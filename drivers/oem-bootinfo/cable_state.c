#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/gpio.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/pinctrl/consumer.h>


static const struct of_device_id cable_state_of_match[] = {
	{ .compatible = "gpio,cable_state", },
	{},
};

static int cable_state_probe(struct platform_device *pdev){
	int ret = 0;
	//struct device_node *node = NULL;
	struct pinctrl_state *pins_default = NULL;
	static struct pinctrl *cable_state_pinctrl;

	//use pinctrl set default state
    cable_state_pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(cable_state_pinctrl)) {
		ret = PTR_ERR(cable_state_pinctrl);
        printk("%s: get cable_state_pinctrl fail.\n", __func__);
		return ret;
	}

	pins_default = pinctrl_lookup_state(cable_state_pinctrl, "default");
	if (IS_ERR(pins_default)) {
		ret = PTR_ERR(pins_default);
        printk("%s: lookup default pinctrl fail.\n", __func__);
		return ret;
	}

	pinctrl_select_state(cable_state_pinctrl, pins_default);
    devm_pinctrl_put(cable_state_pinctrl);
	return 0;
}

static struct platform_driver cable_state_driver = {
	.probe = cable_state_probe,
	.driver = {
		.name = "cable_state_driver",
		.owner	= THIS_MODULE,
		.of_match_table = cable_state_of_match,
	},
};

static int __init cable_state_mod_init(void)
{
	int ret = 0;
	ret = platform_driver_register(&cable_state_driver);
 	if (ret)
 		printk("%s: cable_state platform_driver_register error:(%d)\n", __func__, ret);

 	return ret;
}

static void __exit cable_state_mod_exit(void)
{
	platform_driver_unregister(&cable_state_driver);
}

module_init(cable_state_mod_init);
module_exit(cable_state_mod_exit);
MODULE_LICENSE("GPL");
