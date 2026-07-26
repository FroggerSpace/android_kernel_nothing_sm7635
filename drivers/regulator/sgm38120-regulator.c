// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 Willsemi Co. Ltd.
 *
 */

#include <linux/err.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/kernel.h>

#define SGM38120_DEVICE_D1		0x00
#define SGM38120_DEVICE_D2		0x01
#define SGM38120_ILIM			0x02
#define SGM38120_LDO_EN			0x03

#define SGM38120_LDO1_VOUT		0x04
#define SGM38120_LDO2_VOUT		0x05
#define SGM38120_LDO3_VOUT		0x06
#define SGM38120_LDO4_VOUT		0x07
#define SGM38120_LDO5_VOUT		0x08
#define SGM38120_LDO6_VOUT		0x09
#define SGM38120_LDO7_VOUT		0x0a

#define SGM38120_LDO1_LDO2_SEQ		0x0b
#define SGM38120_LDO3_LDO4_SEQ		0x0c
#define SGM38120_LDO5_LDO6_SEQ		0x0d
#define SGM38120_LDO7_SEQ			0x0e
#define SGM38120_SEQ_STATUS			0x0f

#define SGM38120_DISCHARGE_RESISTORS		0x10
#define SGM38120_RESET					0x11
#define SGM38120_REPROGRAMMABLE_I2C_ADDR	0x12

#define SGM38120_LDO1234_COMP	0x13
#define SGM38120_LDO567_COMP		0x14

#define SGM38120_UVP_INT			0x15
#define SGM38120_OCP_INT			0x16
#define SGM38120_TSD_UVLO_INT	0x17
#define SGM38120_UVP_INT_STATUS			0x18
#define SGM38120_OCP_INT_STATUS			0x19
#define SGM38120_TSD_UVLO_INT_STATUS		0x1a
#define SGM38120_SUSD_STATUS				0x1b
#define SGM38120_UVP_INT_MASK			0x1c
#define SGM38120_OCP_INT_MASK			0x1d
#define SGM38120_TSD_UVLO_INT_MASK		0x1e
#define SGM38120_MAX_REG_NO				0x1f

#define SGM38120_VSEL_MASK		0xff
#define SGM38120_DEFAULT_SLAVE_ADDR  0x35
#define ET5917_DEFAULT_SLAVE_ADDR  0x61
#define SGM38120_RESET_NUMBER        318
#define SGM38120_RESET_J_NUMBER      319
#define SGM38120_LDO12_OFFSET      488


#define SGM38120_CHIP_ID      0XD9
#define ET5917_CHIP_ID      0X91

enum sgm38120_regulators {
	SGM38120_REGULATOR_LDO1 = 0,
	SGM38120_REGULATOR_LDO2,
	SGM38120_REGULATOR_LDO3,
	SGM38120_REGULATOR_LDO4,
	SGM38120_REGULATOR_LDO5,
	SGM38120_REGULATOR_LDO6,
	SGM38120_REGULATOR_LDO7,
	SGM38120_MAX_REGULATORS,
};

/* ldo1~7  0-current,1-current */
/*
 *	LDO1	1460000, 2000000
 *	LDO2	1460000, 2000000
 *	LDO3	500000, 700000
 *	LDO4	500000, 700000
 *	LDO5	740000, 950000
 *	LDO6	500000, 700000
 *	LDO7	740000, 950000
 */
static const unsigned int sgm38120_crtable1[] = {1460000, 2000000};
static const unsigned int sgm38120_crtable2[] = {1460000, 2000000};
static const unsigned int sgm38120_crtable3[] = {500000, 700000};
static const unsigned int sgm38120_crtable4[] = {500000, 700000};
static const unsigned int sgm38120_crtable5[] = {740000, 950000};
static const unsigned int sgm38120_crtable6[] = {500000, 700000};
static const unsigned int sgm38120_crtable7[] = {740000, 950000};

struct sgm38120 {
	struct device *dev;
	struct regmap *regmap;
	struct regulator_dev *rdev;
	struct gpio_desc *reset_gpio;
	int min_dropout_uv;
	int ldo_vout[7];
	int ldo_en;
};

unsigned int debug_en;
module_param(debug_en, uint, 0644);
// static struct sgm38120 *pDefault_sgm38120 = NULL;

#define SGM_DBG_LOG      0x1
#define SGM_DBG_INT_READ 0x2
#define SGM_DBG(dev, fmt, ...) \
	if (debug_en & SGM_DBG_LOG) dev_info(dev, fmt, ##__VA_ARGS__)

static int sgm38120_regulator_disable_regmap(struct regulator_dev *rdev);
static int sgm38120_regulator_enable_regmap(struct regulator_dev *rdev);
static int sgm38120_regulator_set_voltage_sel_regmap(struct regulator_dev *rdev, unsigned sel);

static const struct regulator_ops sgm38120_reg_ops = {
	.list_voltage		= regulator_list_voltage_linear,
	.map_voltage		= regulator_map_voltage_linear,
	.get_voltage_sel	= regulator_get_voltage_sel_regmap,
	.set_voltage_sel	= sgm38120_regulator_set_voltage_sel_regmap,
	.set_current_limit  = regulator_set_current_limit_regmap,
	.enable			= sgm38120_regulator_enable_regmap,
	.disable		= sgm38120_regulator_disable_regmap,
	.is_enabled		= regulator_is_enabled_regmap,
};

#define SGM38120_DESC(_id, _match, _supply, _min, _max, _step, _vreg, _vmask,	\
	 _ereg, _emask, _enval, _disval, _curr_table, _creg, _cmask, _minsel)	\
	{									\
		.id		= (_id),					\
		.name		= (_match),					\
		.of_match	= of_match_ptr(_match),				\
		.supply_name	= (_supply),					\
		.min_uV		= (_min) * 1000,				\
		.uV_step	= (_step) * 1000,			\
		.n_voltages	= (((_max) - (_min)) / (_step) + _minsel),	\
		.n_current_limits = ARRAY_SIZE(_curr_table),		\
		.regulators_node = of_match_ptr("regulators"),		\
		.type		= REGULATOR_VOLTAGE,			\
		.vsel_reg	= (_vreg),				\
		.vsel_mask	= (_vmask),				\
		.csel_reg	= (_creg),				\
		.csel_mask	= (_cmask),				\
		.enable_reg	= (_ereg),				\
		.enable_mask	= (_emask),				\
		.enable_val     = (_enval),				\
		.disable_val     = (_disval),				\
		.ops		= &sgm38120_reg_ops,			\
		.curr_table = _curr_table,				\
		.linear_min_sel = _minsel,				\
		.owner		= THIS_MODULE,				\
	}

static const struct regulator_desc sgm38120_reg[] = {
	SGM38120_DESC(SGM38120_REGULATOR_LDO1, "SGM_LDO1", "vin12", 528, 1504, 8,
		     SGM38120_LDO1_VOUT, SGM38120_VSEL_MASK, SGM38120_LDO_EN, BIT(0),
			 BIT(0), 0,	sgm38120_crtable1, SGM38120_ILIM, BIT(0), 3),
	SGM38120_DESC(SGM38120_REGULATOR_LDO2, "SGM_LDO2", "vin12", 528, 1504, 8,
		     SGM38120_LDO2_VOUT, SGM38120_VSEL_MASK, SGM38120_LDO_EN, BIT(1),
			 BIT(1), 0, sgm38120_crtable2, SGM38120_ILIM, BIT(1), 3),
	SGM38120_DESC(SGM38120_REGULATOR_LDO3, "SGM_LDO3", "vin34", 1500, 3544, 8,
		     SGM38120_LDO3_VOUT, SGM38120_VSEL_MASK, SGM38120_LDO_EN, BIT(2),
			 BIT(2), 0, sgm38120_crtable3, SGM38120_ILIM, BIT(2), 1),
	SGM38120_DESC(SGM38120_REGULATOR_LDO4, "SGM_LDO4", "vin34", 1500, 3544, 8,
		     SGM38120_LDO4_VOUT, SGM38120_VSEL_MASK, SGM38120_LDO_EN, BIT(3),
			 BIT(3), 0, sgm38120_crtable4, SGM38120_ILIM, BIT(3), 1),
	SGM38120_DESC(SGM38120_REGULATOR_LDO5, "SGM_LDO5", "vin5", 1500, 3544, 8,
		     SGM38120_LDO5_VOUT, SGM38120_VSEL_MASK, SGM38120_LDO_EN, BIT(4),
			 BIT(4), 0, sgm38120_crtable5, SGM38120_ILIM, BIT(4), 1),
	SGM38120_DESC(SGM38120_REGULATOR_LDO6, "SGM_LDO6", "vin6", 1500, 3544, 8,
		     SGM38120_LDO6_VOUT, SGM38120_VSEL_MASK, SGM38120_LDO_EN, BIT(5),
			 BIT(5), 0, sgm38120_crtable6, SGM38120_ILIM, BIT(5), 1),
	SGM38120_DESC(SGM38120_REGULATOR_LDO7, "SGM_LDO7", "vin7", 1500, 3544, 8,
		     SGM38120_LDO7_VOUT, SGM38120_VSEL_MASK, SGM38120_LDO_EN, BIT(6),
			 BIT(6), 0, sgm38120_crtable7, SGM38120_ILIM, BIT(6), 1),
};

static const struct regulator_desc et5917_reg[] = {
	SGM38120_DESC(SGM38120_REGULATOR_LDO1, "SGM_LDO1", "vin12", 496, 1800, 8,
		     SGM38120_LDO1_VOUT, SGM38120_VSEL_MASK, SGM38120_LDO_EN, BIT(0),
			 BIT(0), 0,	sgm38120_crtable1, SGM38120_ILIM, BIT(0), 61),
	SGM38120_DESC(SGM38120_REGULATOR_LDO2, "SGM_LDO2", "vin12", 496, 1800, 8,
		     SGM38120_LDO2_VOUT, SGM38120_VSEL_MASK, SGM38120_LDO_EN, BIT(1),
			 BIT(1), 0, sgm38120_crtable2, SGM38120_ILIM, BIT(1), 61),
	SGM38120_DESC(SGM38120_REGULATOR_LDO3, "SGM_LDO3", "vin34", 1372, 3412, 8,
		     SGM38120_LDO3_VOUT, SGM38120_VSEL_MASK, SGM38120_LDO_EN, BIT(2),
			 BIT(2), 0, sgm38120_crtable3, SGM38120_ILIM, BIT(2), 1),
	SGM38120_DESC(SGM38120_REGULATOR_LDO4, "SGM_LDO4", "vin34", 1372, 3412, 8,
		     SGM38120_LDO4_VOUT, SGM38120_VSEL_MASK, SGM38120_LDO_EN, BIT(3),
			 BIT(3), 0, sgm38120_crtable4, SGM38120_ILIM, BIT(3), 1),
	SGM38120_DESC(SGM38120_REGULATOR_LDO5, "SGM_LDO5", "vin5", 1372, 3412, 8,
		     SGM38120_LDO5_VOUT, SGM38120_VSEL_MASK, SGM38120_LDO_EN, BIT(4),
			 BIT(4), 0, sgm38120_crtable5, SGM38120_ILIM, BIT(4), 1),
	SGM38120_DESC(SGM38120_REGULATOR_LDO6, "SGM_LDO6", "vin6", 1372, 3412, 8,
		     SGM38120_LDO6_VOUT, SGM38120_VSEL_MASK, SGM38120_LDO_EN, BIT(5),
			 BIT(5), 0, sgm38120_crtable6, SGM38120_ILIM, BIT(5), 1),
	SGM38120_DESC(SGM38120_REGULATOR_LDO7, "SGM_LDO7", "vin7", 1372, 3412, 8,
		     SGM38120_LDO7_VOUT, SGM38120_VSEL_MASK, SGM38120_LDO_EN, BIT(6),
			 BIT(6), 0, sgm38120_crtable7, SGM38120_ILIM, BIT(6), 1),
};

static const struct regmap_range sgm38120_writeable_ranges[] = {
	regmap_reg_range(SGM38120_ILIM, SGM38120_LDO567_COMP),
};

static const struct regmap_range sgm38120_readable_ranges[] = {
	regmap_reg_range(SGM38120_DEVICE_D1, SGM38120_TSD_UVLO_INT_MASK),
};

static const struct regmap_range sgm38120_volatile_ranges[] = {
	regmap_reg_range(SGM38120_DEVICE_D1, SGM38120_TSD_UVLO_INT_MASK),
};

static const struct regmap_access_table sgm38120_writeable_table = {
	.yes_ranges   = sgm38120_writeable_ranges,
	.n_yes_ranges = ARRAY_SIZE(sgm38120_writeable_ranges),
};

static const struct regmap_access_table sgm38120_readable_table = {
	.yes_ranges   = sgm38120_readable_ranges,
	.n_yes_ranges = ARRAY_SIZE(sgm38120_readable_ranges),
};

static const struct regmap_access_table sgm38120_volatile_table = {
	.yes_ranges   = sgm38120_volatile_ranges,
	.n_yes_ranges = ARRAY_SIZE(sgm38120_volatile_ranges),
};

static const struct regmap_config sgm38120_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = SGM38120_MAX_REG_NO,
	.wr_table = &sgm38120_writeable_table,
	.rd_table = &sgm38120_readable_table,
	.cache_type = REGCACHE_RBTREE,
	.volatile_table = &sgm38120_volatile_table,
};

static int sgm38120_regulator_set_voltage_sel_regmap(struct regulator_dev *rdev, unsigned sel)
{
	int rc =0;
	// int ret = 0;
	// uint32_t tmp_data=0;
	struct device *dev = rdev->dev.parent;
	SGM_DBG(dev, "[%s]regulator_set_voltage_sel_regmap sel = 0x%x", rdev->desc->name, sel);
	// rc = regulator_set_voltage_sel_regmap(rdev, sel);

	return rc;
}

static int sgm38120_regulator_disable_regmap(struct regulator_dev *rdev)
{
	struct device *dev = rdev->dev.parent;
	struct i2c_client *client = to_i2c_client(dev);
	struct sgm38120 *sgm38120 = i2c_get_clientdata(client);
	int ret = 0;
	unsigned int val;

	ret = regulator_disable_regmap(rdev);
	if (ret) {
		dev_err(dev, "[%s]Set reg disable error %d", rdev->desc->name, ret);
	} else {
		val = rdev->desc->enable_val;
		if (!val)
			val = rdev->desc->enable_mask;

		sgm38120->ldo_en &= (~val);
		SGM_DBG(dev, "[%s]Set disable val %d", rdev->desc->name, val);
	}
	// if (0 == (sgm38120->ldo_en & 0x7f)) {
	// 	ret = regmap_write(rdev->regmap, SGM38120_LDO_EN, 0);
	// 	if (ret) {
	// 		dev_err(dev, "sysen set 0 error ret %d", ret);
	// 	}
	// }

	return ret;
}

static int sgm38120_regulator_enable_regmap(struct regulator_dev *rdev)
{
	struct device *dev = rdev->dev.parent;
	struct i2c_client *client = to_i2c_client(dev);
	struct sgm38120 *sgm38120 = i2c_get_clientdata(client);
	int rc = 0;
	int ret = 0;
	unsigned int val;
	int count = 0;
	int tmp_data = 0;
	const int max_read_count = 10;

	// if (0 == (sgm38120->ldo_en & 0x7f)) {
	// 	ret = regmap_write(rdev->regmap, SGM38120_LDO_EN, 0x80);
	// 	if (ret) {
	// 		dev_err(dev, "sysen set 1 error ret %d", ret);
	// 	}
	// }
	ret = regulator_enable_regmap(rdev);
	if (ret) {
		dev_err(dev, "[%s]Set reg enable error %d", rdev->desc->name, ret);
	} else {
		if (rdev->desc->enable_is_inverted) {
			val = rdev->desc->disable_val;
		} else {
			val = rdev->desc->enable_val;
			if (!val)
				val = rdev->desc->enable_mask;
		}
		sgm38120->ldo_en |= val;
		SGM_DBG(dev, "[%s]Set enable val %d", rdev->desc->name, val);
	}

	do {
		ret = regmap_read(rdev->regmap, SGM38120_LDO_EN, &tmp_data);
		if (ret) {
			dev_err(dev, "[%s]i2c LDO_EN read fail %d\n", rdev->desc->name, ret);
		} else {
			if ((tmp_data & sgm38120->ldo_en) == sgm38120->ldo_en) {
				break;
			}
			dev_info(dev, "[%s]wait 1ms for LDO_EN read_data %0x ldo_en 0x%x\n",
				rdev->desc->name, tmp_data, sgm38120->ldo_en);
		}
		usleep_range(1000, 1100);
		count++;
		if (count == (max_read_count / 2)) {
			dev_err(dev, "retry 5 time no enable reg so enable ldo again");
			ret = regulator_enable_regmap(rdev);
			if (ret) {
				dev_err(dev, "[%s]Set reg enable error %d", rdev->desc->name, ret);
				break;
			}
		}
	} while(count < max_read_count);

	if (NULL != rdev && (debug_en & SGM_DBG_INT_READ))
	{
		usleep_range(10000, 11000);
		rc = regmap_read(rdev->regmap, SGM38120_UVP_INT, &tmp_data);
		SGM_DBG(dev, "[%s]SGM38120_UVP_INT = %x ret = %d", rdev->desc->name,tmp_data ,rc);
		rc = regmap_read(rdev->regmap, SGM38120_OCP_INT, &tmp_data);
		SGM_DBG(dev, "[%s]SGM38120_OCP_INT = %x ret = %d", rdev->desc->name,tmp_data ,rc);
		rc = regmap_read(rdev->regmap, SGM38120_TSD_UVLO_INT, &tmp_data);
		SGM_DBG(dev, "[%s]SGM38120_TSD_UVLO_INT = %x ret = %d", rdev->desc->name,tmp_data ,rc);
		rc = regmap_read(rdev->regmap, SGM38120_UVP_INT_STATUS, &tmp_data);
		SGM_DBG(dev, "[%s]SGM38120_UVP_INT = %x ret = %d", rdev->desc->name,tmp_data ,rc);
		rc = regmap_read(rdev->regmap, SGM38120_OCP_INT_STATUS, &tmp_data);
		SGM_DBG(dev, "[%s]SGM38120_OCP_INT = %x ret = %d", rdev->desc->name,tmp_data ,rc);
		rc = regmap_read(rdev->regmap, SGM38120_TSD_UVLO_INT_STATUS, &tmp_data);
		SGM_DBG(dev, "[%s]SGM38120_TSD_UVLO_INT = %x ret = %d", rdev->desc->name,tmp_data ,rc);
	}

	return ret;
}

static void sgm38120_reset(struct sgm38120 *sgm38120)
{
	gpiod_set_value_cansleep(sgm38120->reset_gpio, 0);	//set H
	usleep_range(10000, 11000);
	gpiod_set_value_cansleep(sgm38120->reset_gpio, 1);	//set L
	usleep_range(10000, 11000);
	gpiod_set_value_cansleep(sgm38120->reset_gpio, 0);
	usleep_range(10000, 11000);
}

#if 0
static void sgm38120_i2c_disable(struct sgm38120 *sgm38120)
{
	gpiod_set_value_cansleep(sgm38120->reset_gpio, 1);	//set L
	dev_info(sgm38120->dev, "set dev i2c disable\n");
	usleep_range(10000, 11000);
}

static void sgm38120_i2c_enable(struct sgm38120 *sgm38120)
{
	int i, ret;
	int tmp_data;

	gpiod_set_value_cansleep(sgm38120->reset_gpio, 0);	//set H
	dev_info(sgm38120->dev, "set dev i2c enable\n");
	usleep_range(10000, 11000);
	for(i = 0; i < 3; i++)
	{
		ret = regmap_read(sgm38120->regmap, SGM38120_REPROGRAMMABLE_I2C_ADDR, &tmp_data);
		if (ret) {
			dev_err(sgm38120->dev, "i2c enable set error, start rest retry %d\n", i);
			sgm38120_reset(sgm38120);
		} else {
			dev_info(sgm38120->dev, "i2c enable set success, I2C ADDR REG: 0x%x\n", tmp_data);
			break;
		}
	}
}
#endif
static int sgm38120_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct regulator_config config = {};
	struct regulator_dev *rdev;
	const struct regulator_desc *regulators;
	struct sgm38120 *sgm38120;
	int ret, i;
	unsigned int gpio_number;
	unsigned short current_addr;
	int tmp_data;
	static atomic_t probe_cnt = ATOMIC_INIT(0);
	int sleep_cnt = 0;
	int chip_id;
	bool is_et5917 = false;

	sgm38120 = devm_kzalloc(dev, sizeof(struct sgm38120), GFP_KERNEL);
	if (!sgm38120)
		return -ENOMEM;

	if (1 < atomic_add_return(1, &probe_cnt)) {
		//wait for other probe
		do {
			msleep(10);
			sleep_cnt++;
			dev_info(dev, "wait 10ms for other sgm38120 probe cnt: %d\n", sleep_cnt);
			if (1 >= atomic_read(&probe_cnt))
			{
				dev_info(dev, "wait success for other sgm38120 probe\n");
				break;
			}
		} while(sleep_cnt < 50);
	}

	sgm38120->ldo_en = 0;
	sgm38120->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(sgm38120->reset_gpio)) {
		ret = PTR_ERR(sgm38120->reset_gpio);
		dev_err(dev, "failed to request reset GPIO: %d\n", ret);
		return ret;
	}
	gpio_number = desc_to_gpio(sgm38120->reset_gpio);
	dev_info(dev, "RESET GPIO number is %u\n", gpio_number);

	// if (NULL != pDefault_sgm38120)
	// {
	// 	sgm38120_i2c_disable(pDefault_sgm38120);
	// 	dev_info(dev, "Disable default sgm38120 I2C start\n");
	// }

	sgm38120_reset(sgm38120);

	chip_id = i2c_smbus_read_byte_data(client, SGM38120_DEVICE_D1);

	dev_info(dev, "chip_id 0x%x\n", chip_id);

	if (chip_id < 0 || chip_id == ET5917_CHIP_ID)
	{
		current_addr   = client->addr;
		client->addr   = ET5917_DEFAULT_SLAVE_ADDR;

		dev_info(dev, "Et5917 probe\n");

		chip_id = i2c_smbus_read_byte_data(client, SGM38120_DEVICE_D1);
		dev_info(dev, "et5917 chip_id 0x%x\n", chip_id);
		if (0 > chip_id)
		{
			dev_err(dev, "Read i2c addr 0x%x failed!", client->addr);
		}

		if (0 > i2c_smbus_write_byte_data(client, SGM38120_REPROGRAMMABLE_I2C_ADDR, 0x02))
		{
			dev_err(dev, "write i2c addr 0x%x failed!", client->addr);
		}
		is_et5917 = true;

		regulators = et5917_reg;
		client->addr = current_addr;
	}
	else
	{
		dev_info(dev, "sgm38120 probe\n");
		regulators = sgm38120_reg;
	}

	// if (NULL != pDefault_sgm38120)
	// {
	// 	sgm38120_i2c_enable(pDefault_sgm38120);
	// 	dev_info(dev, "Disable default sgm38120 I2C end\n");
	// }

	i2c_set_clientdata(client, sgm38120);
	sgm38120->dev = dev;
	sgm38120->regmap = devm_regmap_init_i2c(client, &sgm38120_regmap_config);
	if (IS_ERR(sgm38120->regmap)) {
		ret = PTR_ERR(sgm38120->regmap);
		dev_err(dev, "Failed to allocate register map: %d\n", ret);
		atomic_sub(1, &probe_cnt);
		return ret;
	}

	config.dev = &client->dev;
	config.regmap = sgm38120->regmap;

	/* Instantiate the regulators */
	for (i = 0; i < SGM38120_MAX_REGULATORS; i++) {
		rdev = devm_regulator_register(&client->dev,
					       &regulators[i], &config);
		if (IS_ERR(rdev)) {
			dev_err(&client->dev,
				"failed to register %d regulator\n", i);
			atomic_sub(1, &probe_cnt);
			return PTR_ERR(rdev);
		}
		// if (0 == of_property_read_u32(rdev->dev.of_node, "init-voltage", &tmp_data))
		// {
		// 	dev_err(&client->dev,"[%d]init-voltage %d",i,tmp_data);
		// }
	}

	// if (NULL == pDefault_sgm38120)
	// {
	// 	pDefault_sgm38120 = sgm38120;
	// }

	/*Inital related mask for interrupt here*/
	ret = regmap_read(sgm38120->regmap, SGM38120_UVP_INT_MASK, &tmp_data);
	dev_info(dev, "SGM38120_UVP_INT_MASK = %x ret = %d",tmp_data ,ret);
	ret = regmap_read(sgm38120->regmap, SGM38120_OCP_INT_MASK, &tmp_data);
	dev_info(dev, "SGM38120_OCP_INT_MASK = %x ret = %d",tmp_data ,ret);
	ret = regmap_read(sgm38120->regmap, SGM38120_TSD_UVLO_INT_MASK, &tmp_data);
	dev_info(dev, "SGM38120_TSD_UVLO_INT_MASK = %x ret = %d",tmp_data ,ret);
	ret = regmap_read(sgm38120->regmap, SGM38120_UVP_INT, &tmp_data);
	dev_info(dev, "SGM38120_UVP_INT = %x ret = %d",tmp_data ,ret);
	ret = regmap_read(sgm38120->regmap, SGM38120_OCP_INT, &tmp_data);
	dev_info(dev, "SGM38120_OCP_INT = %x ret = %d",tmp_data ,ret);
	ret = regmap_read(sgm38120->regmap, SGM38120_TSD_UVLO_INT, &tmp_data);
	dev_info(dev, "SGM38120_TSD_UVLO_INT = %x ret = %d",tmp_data ,ret);
	regmap_write(sgm38120->regmap, SGM38120_UVP_INT_MASK, 0);
	regmap_write(sgm38120->regmap, SGM38120_OCP_INT_MASK, 0);
	regmap_write(sgm38120->regmap, SGM38120_TSD_UVLO_INT_MASK, 0);
	dev_info(dev, "sgm38120 enable sys_en");
	regmap_write(sgm38120->regmap, SGM38120_LDO_EN, 0x80);
	if (is_et5917)
	{
		dev_info(dev, "et5917 set LDO1 and LDO2");
		regmap_write(sgm38120->regmap, SGM38120_LDO1_VOUT, 0x83);
		regmap_write(sgm38120->regmap, SGM38120_LDO2_VOUT, 0x83);
		regmap_write(sgm38120->regmap, SGM38120_LDO3_VOUT, 0xb2);
		regmap_write(sgm38120->regmap, SGM38120_LDO4_VOUT, 0x35);
		regmap_write(sgm38120->regmap, SGM38120_LDO5_VOUT, 0xD8);
		regmap_write(sgm38120->regmap, SGM38120_LDO6_VOUT, 0xCb);
		regmap_write(sgm38120->regmap, SGM38120_LDO7_VOUT, 0xb2);
	}
	else
	{
		regmap_write(sgm38120->regmap, SGM38120_LDO1_VOUT, 0x45);
		regmap_write(sgm38120->regmap, SGM38120_LDO2_VOUT, 0x45);
		regmap_write(sgm38120->regmap, SGM38120_LDO3_VOUT, 0xa2);
		regmap_write(sgm38120->regmap, SGM38120_LDO4_VOUT, 0x25);
		regmap_write(sgm38120->regmap, SGM38120_LDO5_VOUT, 0xC8);
		regmap_write(sgm38120->regmap, SGM38120_LDO6_VOUT, 0xbb);
		regmap_write(sgm38120->regmap, SGM38120_LDO7_VOUT, 0xa2);
	}

	for (i = 0; i < ARRAY_SIZE(sgm38120->ldo_vout); i++)
	{
		regmap_read(sgm38120->regmap, SGM38120_LDO1_VOUT + i,
			&sgm38120->ldo_vout[i]);
		dev_info(dev, "sgm38120 SGM_LDO%d value is 0x%x\n", i, sgm38120->ldo_vout[i]);
	}
	atomic_sub(1, &probe_cnt);

	return 0;
}

static void sgm38120_regulator_shutdown(struct i2c_client *client)
{
	struct sgm38120 *sgm38120 = i2c_get_clientdata(client);
	int ret = 0;
	if (system_state == SYSTEM_POWER_OFF)
		ret = regmap_write(sgm38120->regmap, SGM38120_LDO_EN, 0x80);
}

static int __maybe_unused sgm38120_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct sgm38120 *sgm38120 = i2c_get_clientdata(client);
	int i;
	int ret = 0;

	ret = regmap_read(sgm38120->regmap, SGM38120_LDO_EN, &sgm38120->ldo_en);
	for (i = 0; i < ARRAY_SIZE(sgm38120->ldo_vout); i++)
		regmap_read(sgm38120->regmap, SGM38120_LDO1_VOUT + i,
			    &sgm38120->ldo_vout[i]);

	return 0;
}

static int __maybe_unused sgm38120_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct sgm38120 *sgm38120 = i2c_get_clientdata(client);
	int i;
	int ret =0;

	// sgm38120_reset(sgm38120);
	for (i = 0; i < ARRAY_SIZE(sgm38120->ldo_vout); i++)
		regmap_write(sgm38120->regmap, SGM38120_LDO1_VOUT + i,
			     sgm38120->ldo_vout[i]);
	ret = regmap_write(sgm38120->regmap, SGM38120_LDO_EN, sgm38120->ldo_en);

	return 0;
}

static SIMPLE_DEV_PM_OPS(sgm38120_pm_ops, sgm38120_suspend, sgm38120_resume);

static const struct i2c_device_id sgm38120_i2c_id[] = {
	{ "sgm38120", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, sgm38120_i2c_id);

static const struct of_device_id sgm38120_of_match[] = {
	{ .compatible = "nothing,sgm38120" },
	{}
};
MODULE_DEVICE_TABLE(of, sgm38120_of_match);

static struct i2c_driver sgm38120_i2c_driver = {
	.driver = {
		.name = "sgm38120",
		.of_match_table = of_match_ptr(sgm38120_of_match),
		.pm = &sgm38120_pm_ops,
	},
	.id_table = sgm38120_i2c_id,
	.probe	= sgm38120_i2c_probe,
	.shutdown = sgm38120_regulator_shutdown,
};

module_i2c_driver(sgm38120_i2c_driver);

MODULE_DESCRIPTION("SGM38120 regulator driver");
MODULE_LICENSE("GPL");
