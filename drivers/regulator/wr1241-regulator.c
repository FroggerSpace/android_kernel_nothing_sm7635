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

#define WR1241_DEVICE_D1		0x00
#define WR1241_DEVICE_D2		0x19
#define WR1241_ILIM			0x01
#define WR1241_LDO_EN			0x0E

#define WR1241_LDO1_VOUT		0x03
#define WR1241_LDO2_VOUT		0x04
#define WR1241_LDO3_VOUT		0x05
#define WR1241_LDO4_VOUT		0x06

#define WR1241_LDO1_LDO2_SEQ		0x0A
#define WR1241_LDO3_LDO4_SEQ		0x0B
#define WR1241_SEQ_STATUS			0x0f

#define WR1241_VSEL_MASK		0xff

enum wr1241_regulators {
	WR1241_REGULATOR_LDO1 = 0,
	WR1241_REGULATOR_LDO2,
	WR1241_REGULATOR_LDO3,
	WR1241_REGULATOR_LDO4,
	WR1241_MAX_REGULATORS,
};

unsigned int debug_en;
module_param(debug_en, uint, 0644);

#define WR1241_DBG_LOG      0x1
#define WR1241_DBG(rdev, fmt, ...) \
	if (debug_en & WR1241_DBG_LOG) dev_info(rdev->dev.parent, "[%s]"fmt, rdev->desc->name, ##__VA_ARGS__)
#define WR1241_ERR(rdev, fmt, ...) \
	dev_err(rdev->dev.parent, "[%s]"fmt, rdev->desc->name, ##__VA_ARGS__)

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
static const unsigned int wr1241_crtable1[] = {1460000, 2000000};
static const unsigned int wr1241_crtable2[] = {1460000, 2000000};
static const unsigned int wr1241_crtable3[] = {500000, 700000};
static const unsigned int wr1241_crtable4[] = {500000, 700000};

struct wr1241 {
	struct device *dev;
	struct regmap *regmap;
	struct regulator_dev *rdev;
	struct gpio_desc *reset_gpio;
	int min_dropout_uv;
	int ldo_vout[7];
	int ldo_en;
};

static int wr1241_regulator_disable_regmap(struct regulator_dev *rdev);
static int wr1241_regulator_enable_regmap(struct regulator_dev *rdev);
static int wr1241_regulator_set_voltage_sel_regmap(struct regulator_dev *rdev, unsigned sel);

static const struct regulator_ops wr1241_reg_ops = {
	.list_voltage		= regulator_list_voltage_linear,
	.map_voltage		= regulator_map_voltage_linear,
	.get_voltage_sel	= regulator_get_voltage_sel_regmap,
	.set_voltage_sel	= wr1241_regulator_set_voltage_sel_regmap,
	// .set_current_limit  = regulator_set_current_limit_regmap,
	.enable			= wr1241_regulator_enable_regmap,
	.disable		= wr1241_regulator_disable_regmap,
	.is_enabled		= regulator_is_enabled_regmap,
};

#define WR1241_DESC(_id, _match, _supply, _min, _max, _step, _vreg, _vmask,	\
	 _ereg, _emask, _enval, _disval, _curr_table, _creg, _cmask, _minsel)	\
	{									\
		.id		= (_id),					\
		.name		= (_match),					\
		.of_match	= of_match_ptr(_match),				\
		.supply_name	= (_supply),					\
		.min_uV		= (_min),				\
		.uV_step	= (_step),			\
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
		.ops		= &wr1241_reg_ops,			\
		.curr_table = _curr_table,				\
		.linear_min_sel = _minsel,				\
		.owner		= THIS_MODULE,				\
	}

static const struct regulator_desc wr1241_reg[] = {
	WR1241_DESC(WR1241_REGULATOR_LDO1, "WR_LDO1", "vin12", 600 * 1000, 2130 * 1000, 6 * 1000,
		     WR1241_LDO1_VOUT, WR1241_VSEL_MASK, WR1241_LDO_EN, BIT(0),
			 BIT(0), 0,	wr1241_crtable1, WR1241_ILIM, BIT(0), 1),
	WR1241_DESC(WR1241_REGULATOR_LDO2, "WR_LDO2", "vin12", 600 * 1000, 2130 * 1000, 6 * 1000,
		     WR1241_LDO2_VOUT, WR1241_VSEL_MASK, WR1241_LDO_EN, BIT(1),
			 BIT(1), 0, wr1241_crtable2, WR1241_ILIM, BIT(1), 1),
	WR1241_DESC(WR1241_REGULATOR_LDO3, "WR_LDO3", "vin34", 1200 * 1000, 4387500, 12500,
		     WR1241_LDO3_VOUT, WR1241_VSEL_MASK, WR1241_LDO_EN, BIT(2),
			 BIT(2), 0, wr1241_crtable3, WR1241_ILIM, BIT(2), 1),
	WR1241_DESC(WR1241_REGULATOR_LDO4, "WR_LDO4", "vin34", 1200 * 1000, 4387500, 12500,
		     WR1241_LDO4_VOUT, WR1241_VSEL_MASK, WR1241_LDO_EN, BIT(3),
			 BIT(3), 0, wr1241_crtable4, WR1241_ILIM, BIT(3), 1),
};

static const struct regulator_desc wr1241_uw_reg[] = {
	WR1241_DESC(WR1241_REGULATOR_LDO1, "WR_LDO1_UW", "vin12", 600 * 1000, 2130 * 1000, 6 * 1000,
		     WR1241_LDO1_VOUT, WR1241_VSEL_MASK, WR1241_LDO_EN, BIT(0),
			 BIT(0), 0,	wr1241_crtable1, WR1241_ILIM, BIT(0), 1),
	WR1241_DESC(WR1241_REGULATOR_LDO2, "WR_LDO2_UW", "vin12", 600 * 1000, 2130 * 1000, 6 * 1000,
		     WR1241_LDO2_VOUT, WR1241_VSEL_MASK, WR1241_LDO_EN, BIT(1),
			 BIT(1), 0, wr1241_crtable2, WR1241_ILIM, BIT(1), 1),
	WR1241_DESC(WR1241_REGULATOR_LDO3, "WR_LDO3_UW", "vin34", 1200 * 1000, 4387500, 12500,
		     WR1241_LDO3_VOUT, WR1241_VSEL_MASK, WR1241_LDO_EN, BIT(2),
			 BIT(2), 0, wr1241_crtable3, WR1241_ILIM, BIT(2), 1),
	WR1241_DESC(WR1241_REGULATOR_LDO4, "WR_LDO4_UW", "vin34", 1200 * 1000, 4387500, 12500,
		     WR1241_LDO4_VOUT, WR1241_VSEL_MASK, WR1241_LDO_EN, BIT(3),
			 BIT(3), 0, wr1241_crtable4, WR1241_ILIM, BIT(3), 1),
};

static const struct regmap_range wr1241_writeable_ranges[] = {
	regmap_reg_range(WR1241_ILIM, WR1241_DEVICE_D2),
};

static const struct regmap_range wr1241_readable_ranges[] = {
	regmap_reg_range(WR1241_DEVICE_D1, WR1241_DEVICE_D2),
};

static const struct regmap_range wr1241_volatile_ranges[] = {
	regmap_reg_range(WR1241_DEVICE_D1, WR1241_DEVICE_D2),
};

static const struct regmap_access_table wr1241_writeable_table = {
	.yes_ranges   = wr1241_writeable_ranges,
	.n_yes_ranges = ARRAY_SIZE(wr1241_writeable_ranges),
};

static const struct regmap_access_table wr1241_readable_table = {
	.yes_ranges   = wr1241_readable_ranges,
	.n_yes_ranges = ARRAY_SIZE(wr1241_readable_ranges),
};

static const struct regmap_access_table wr1241_volatile_table = {
	.yes_ranges   = wr1241_volatile_ranges,
	.n_yes_ranges = ARRAY_SIZE(wr1241_volatile_ranges),
};

static const struct regmap_config wr1241_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = WR1241_DEVICE_D2,
	.wr_table = &wr1241_writeable_table,
	.rd_table = &wr1241_readable_table,
	.cache_type = REGCACHE_RBTREE,
	.volatile_table = &wr1241_volatile_table,
};

static int wr1241_regulator_set_voltage_sel_regmap(struct regulator_dev *rdev, unsigned sel)
{
	int rc =0;
	WR1241_DBG(rdev, "regulator_set_voltage_sel_regmap sel = 0x%x", sel);
	rc = regulator_set_voltage_sel_regmap(rdev, sel);

	return rc;
}

static int wr1241_regulator_disable_regmap(struct regulator_dev *rdev)
{
	struct device *dev = rdev->dev.parent;
	struct i2c_client *client = to_i2c_client(dev);
	struct wr1241 *wr1241 = i2c_get_clientdata(client);
	int ret = 0;
	unsigned int val;

	ret = regulator_disable_regmap(rdev);
	if (ret) {
		WR1241_ERR(rdev, "Set reg disable error %d", ret);
	} else {
		val = rdev->desc->enable_val;
		if (!val)
			val = rdev->desc->enable_mask;

		wr1241->ldo_en &= (~val);
		WR1241_DBG(rdev, "Set disable val %d", val);
	}
	// if (0 == (wr1241->ldo_en & 0x7f)) {
	// 	ret = regmap_write(rdev->regmap, WR1241_LDO_EN, 0);
	// 	if (ret) {
	// 		dev_err(dev, "sysen set 0 error ret %d", ret);
	// 	}
	// }

	return ret;
}

static int wr1241_regulator_enable_regmap(struct regulator_dev *rdev)
{
	struct device *dev = rdev->dev.parent;
	struct i2c_client *client = to_i2c_client(dev);
	struct wr1241 *wr1241 = i2c_get_clientdata(client);
	int ret = 0;
	unsigned int val;
	int count = 0;
	int tmp_data = 0;
	const int max_read_count = 10;

	// if (0 == (wr1241->ldo_en & 0x7f)) {
	// 	ret = regmap_write(rdev->regmap, WR1241_LDO_EN, 0x80);
	// 	if (ret) {
	// 		dev_err(dev, "sysen set 1 error ret %d", ret);
	// 	}
	// }
	ret = regulator_enable_regmap(rdev);
	if (ret) {
		WR1241_ERR(rdev, "Set reg enable error %d", ret);
	} else {
		if (rdev->desc->enable_is_inverted) {
			val = rdev->desc->disable_val;
		} else {
			val = rdev->desc->enable_val;
			if (!val)
				val = rdev->desc->enable_mask;
		}
		wr1241->ldo_en |= val;
		WR1241_DBG(rdev, "Set enable val %d", val);
	}

	do {
		ret = regmap_read(rdev->regmap, WR1241_LDO_EN, &tmp_data);
		if (ret) {
			WR1241_ERR(rdev, "i2c LDO_EN read fail %d\n", ret);
		} else {
			if ((tmp_data & wr1241->ldo_en) == wr1241->ldo_en) {
				break;
			}
			dev_info(dev, "wait 1ms for LDO_EN read_data %0x ldo_en 0x%x\n",
				tmp_data, wr1241->ldo_en);
		}
		usleep_range(1000, 1100);
		count++;
		if (count == (max_read_count / 2)) {
			WR1241_ERR(rdev, "retry 5 time no enable reg so enable ldo again");
			ret = regulator_enable_regmap(rdev);
			if (ret) {
				WR1241_ERR(rdev, "Set reg enable error %d", ret);
				break;
			}
		}
	} while(count < max_read_count);

	return ret;
}





static void wr1241_reset(struct wr1241 *wr1241)
{
	gpiod_set_value_cansleep(wr1241->reset_gpio, 0);	//set H
	usleep_range(10000, 11000);
	gpiod_set_value_cansleep(wr1241->reset_gpio, 1);	//set L
	usleep_range(10000, 11000);
	gpiod_set_value_cansleep(wr1241->reset_gpio, 0);
	usleep_range(10000, 11000);
}
#if 0
static void wr1241_i2c_disable(struct wr1241 *wr1241)
{
	gpiod_set_value_cansleep(wr1241->reset_gpio, 1);	//set L
	dev_info(wr1241->dev, "set dev i2c disable\n");
	usleep_range(10000, 11000);
}

static void wr1241_i2c_enable(struct wr1241 *wr1241)
{
	int i, ret;
	int tmp_data;

	gpiod_set_value_cansleep(wr1241->reset_gpio, 0);	//set H
	dev_info(wr1241->dev, "set dev i2c enable\n");
	usleep_range(10000, 11000);
	for(i = 0; i < 3; i++)
	{
		ret = regmap_read(wr1241->regmap, WR1241_DEVICE_D2, &tmp_data);
		if (ret) {
			dev_err(wr1241->dev, "i2c enable set error, start rest retry %d\n", i);
			wr1241_reset(wr1241);
		} else {
			dev_info(wr1241->dev, "i2c enable set success, I2C ADDR REG: 0x%x\n", tmp_data);
			break;
		}
	}
}
#endif
static int wr1241_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct regulator_config config = {};
	struct regulator_dev *rdev;
	const struct regulator_desc *regulators;
	struct wr1241 *wr1241;
	int ret, i;
	unsigned int gpio_number;
	// unsigned short current_addr;
	// int tmp_data;
	static atomic_t probe_cnt = ATOMIC_INIT(0);
	int sleep_cnt = 0;
	// unsigned int chip_id;
	const char *ldo_type;

	wr1241 = devm_kzalloc(dev, sizeof(struct wr1241), GFP_KERNEL);
	if (!wr1241)
		return -ENOMEM;

	if (1 < atomic_add_return(1, &probe_cnt)) {
		//wait for other probe
		do {
			msleep(10);
			sleep_cnt++;
			dev_info(dev, "wait 10ms for other wr1241 probe cnt: %d\n", sleep_cnt);
			if (1 >= atomic_read(&probe_cnt))
			{
				dev_info(dev, "wait success for other wr1241 probe\n");
				break;
			}
		} while(sleep_cnt < 50);
	}

	wr1241->ldo_en = 0;
	wr1241->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(wr1241->reset_gpio)) {
		ret = PTR_ERR(wr1241->reset_gpio);
		dev_err(dev, "failed to request reset GPIO: %d\n", ret);
		return ret;
	}
	gpio_number = desc_to_gpio(wr1241->reset_gpio);
	dev_info(dev, "RESET GPIO number is %u\n", gpio_number);

	wr1241_reset(wr1241);

	regulators = wr1241_reg;

	i2c_set_clientdata(client, wr1241);
	wr1241->dev = dev;
	wr1241->regmap = devm_regmap_init_i2c(client, &wr1241_regmap_config);
	if (IS_ERR(wr1241->regmap)) {
		ret = PTR_ERR(wr1241->regmap);
		dev_err(dev, "Failed to allocate register map: %d\n", ret);
		atomic_sub(1, &probe_cnt);
		return ret;
	}

	config.dev = &client->dev;
	config.regmap = wr1241->regmap;

	if (0 == of_property_read_string(dev->of_node, "ldo-type", &ldo_type))
	{
		if (0 == strncmp("UW", ldo_type, 2))
		{
			dev_info(&client->dev, "use UW LDO regulator desc");
			regulators = wr1241_uw_reg;
		}
	}

	/* Instantiate the regulators */
	for (i = 0; i < WR1241_MAX_REGULATORS; i++) {
		rdev = devm_regulator_register(&client->dev,
					       &regulators[i], &config);
		if (IS_ERR(rdev)) {
			dev_err(&client->dev,
				"failed to register %d regulator\n", i);
			atomic_sub(1, &probe_cnt);
			return PTR_ERR(rdev);
		}
	}



	/*Inital related mask for interrupt here*/

	for (i = 0; i < WR1241_MAX_REGULATORS; i++)
	{
		regmap_read(wr1241->regmap, WR1241_LDO1_VOUT + i,
			&wr1241->ldo_vout[i]);
		dev_info(dev, "wr1241 WR_LDO%d value is 0x%x\n", i, wr1241->ldo_vout[i]);
	}
	atomic_sub(1, &probe_cnt);

	return 0;
}

static void wr1241_regulator_shutdown(struct i2c_client *client)
{
	struct wr1241 *wr1241 = i2c_get_clientdata(client);
	int ret = 0;
	if (system_state == SYSTEM_POWER_OFF)
		ret = regmap_write(wr1241->regmap, WR1241_LDO_EN, 0x80);
}

static int __maybe_unused wr1241_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct wr1241 *wr1241 = i2c_get_clientdata(client);
	int i;
	int ret = 0;

	ret = regmap_read(wr1241->regmap, WR1241_LDO_EN, &wr1241->ldo_en);
	for (i = 0; i < WR1241_MAX_REGULATORS; i++)
		regmap_read(wr1241->regmap, WR1241_LDO1_VOUT + i,
			    &wr1241->ldo_vout[i]);

	return 0;
}

static int __maybe_unused wr1241_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct wr1241 *wr1241 = i2c_get_clientdata(client);
	int i;
	int ret =0;

	// wr1241_reset(wr1241);
	for (i = 0; i < WR1241_MAX_REGULATORS; i++)
		regmap_write(wr1241->regmap, WR1241_LDO1_VOUT + i,
			     wr1241->ldo_vout[i]);
	ret = regmap_write(wr1241->regmap, WR1241_LDO_EN, wr1241->ldo_en);

	return 0;
}

static SIMPLE_DEV_PM_OPS(wr1241_pm_ops, wr1241_suspend, wr1241_resume);

static const struct i2c_device_id wr1241_i2c_id[] = {
	{ "wr1241", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, wr1241_i2c_id);

static const struct of_device_id wr1241_of_match[] = {
	{ .compatible = "nothing,wr1241" },
	{}
};
MODULE_DEVICE_TABLE(of, wr1241_of_match);

static struct i2c_driver wr1241_i2c_driver = {
	.driver = {
		.name = "wr1241",
		.of_match_table = of_match_ptr(wr1241_of_match),
		.pm = &wr1241_pm_ops,
	},
	.id_table = wr1241_i2c_id,
	.probe	= wr1241_i2c_probe,
	.shutdown = wr1241_regulator_shutdown,
};

module_i2c_driver(wr1241_i2c_driver);

MODULE_DESCRIPTION("WR1241 regulator driver");
MODULE_LICENSE("GPL");
