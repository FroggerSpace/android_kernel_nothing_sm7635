#include <linux/io.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/of_address.h>
#include <linux/module.h>
#include <linux/reboot.h>
#include <linux/panic_notifier.h>
#include <linux/nothing_restart_handler.h>

static unsigned shared_imem_restart_info_size = 0x80;

struct nothing_restart_notify {
	struct notifier_block panic_nb;
	struct notifier_block reboot_nb;
};

struct nothing_restart_info {
	char magic[4]; /* 'S' 'S' 0xDE 0xAD */
	char data[];
};

static struct nothing_restart_info __iomem *restart_info;
static unsigned rst_msg_size;
static char *kernel_panic_msg_buf;
static char *reboot_msg_buf;

void set_restart_msg(const char *msg)
{
	char magic[4];

	if (!restart_info || rst_msg_size == 0)
		return;
        if (magic[0] == 'S' && magic[1] == 'S') {
		return;
	}

	/* Restart info magic: SS\xDE\xAD, System DEAD */
	magic[0] = magic[1] = 'S';
	magic[2] = 0xDE;
	magic[3] = 0xAD;

	/* Write magic first */
	memcpy_toio(restart_info, magic, 4);

	/* Write message data */
	if (msg) {
		pr_err("%s: %s \n", __func__, msg);
		memcpy_toio((char __iomem *)restart_info + 4, msg, 
			    min_t(size_t, strlen(msg), rst_msg_size - 1));
	} else {
		memcpy_toio((char __iomem *)restart_info + 4, "Unknown", 7);
	}

	mb();
}
EXPORT_SYMBOL(set_restart_msg);

static int nothing_notifier_panic(struct notifier_block *this, unsigned long event,
			      void *ptr)
{
	pr_warn("%s: Get panic notify", __func__);
	
	if (!kernel_panic_msg_buf)
		return NOTIFY_OK;
	
	strcpy(kernel_panic_msg_buf, "Kernel Panic");
	if (ptr && rst_msg_size > 1)
		snprintf(kernel_panic_msg_buf, rst_msg_size - 1, "%s", (char *)ptr);
	set_restart_msg(kernel_panic_msg_buf);

	return NOTIFY_OK;
}

static int nothing_notifier_reboot(struct notifier_block *this, unsigned long event,
			      void *ptr)
{
	pr_warn("%s: Get reboot notify", __func__);
	
	if (!reboot_msg_buf)
		return NOTIFY_OK;
	
	strcpy(reboot_msg_buf, "Reboot");
	if (ptr && rst_msg_size > 8)
		snprintf(reboot_msg_buf, rst_msg_size - 1, "Reboot: %s", (char *)ptr);
	set_restart_msg(reboot_msg_buf);

	return NOTIFY_OK;
}

static int nothing_restart_info_probe(struct platform_device *pdev)
{
	struct device_node *np;
	unsigned nothing_rst_info_size;
	int ret = 0;
	struct nothing_restart_notify *restart_notify;

	restart_notify = devm_kzalloc(&pdev->dev, sizeof(*restart_notify), GFP_KERNEL);
	if (!restart_notify) {
		pr_err("%s: Unable to allocate memory for restart_notify\n", __func__);
		return 0;
	}

	np = of_find_compatible_node(NULL, NULL,
				"nothing,msm-imem-restart_info");
	if (!np) {
		pr_err("unable to find imem nothing,imem-restart-info\n");
		return 0;
	} else {
		restart_info = of_iomap(np, 0);
		if (!restart_info) {
			pr_err("%s: nothing restart_info not valid\n", __func__);
			return 0;
		} else {
			ret = of_property_read_u32(np, "info_size", &nothing_rst_info_size);
			if (ret != 0) {
				pr_err("%s: Failed to find info_size property in nothing restart info device node %d\n",
						__func__, ret);
				return 0;
			}

			/* Update global size variable based on device tree */
			shared_imem_restart_info_size = nothing_rst_info_size;

		}
	}

	if (nothing_rst_info_size < offsetof(struct nothing_restart_info, data))
	{
		pr_err("%s: invalid nothing_rst_info_size %d\n", __func__, nothing_rst_info_size);
		return 0;
	}
	rst_msg_size = nothing_rst_info_size - offsetof(struct nothing_restart_info, data);

	/* Allocate buffers for panic and reboot messages */
	kernel_panic_msg_buf = devm_kmalloc(&pdev->dev, rst_msg_size, GFP_KERNEL);
	if (!kernel_panic_msg_buf) {
		pr_err("%s: Unable to allocate kernel_panic_msg_buf\n", __func__);
		return -ENOMEM;
	}

	reboot_msg_buf = devm_kmalloc(&pdev->dev, rst_msg_size, GFP_KERNEL);
	if (!reboot_msg_buf) {
		pr_err("%s: Unable to allocate reboot_msg_buf\n", __func__);
		return -ENOMEM;
	}

	memset_io(restart_info, 0x00, nothing_rst_info_size);
	wmb();
	set_restart_msg("Unknown");

	restart_notify->panic_nb.notifier_call = nothing_notifier_panic;
	restart_notify->panic_nb.priority = INT_MAX;
	atomic_notifier_chain_register(&panic_notifier_list,
				       &restart_notify->panic_nb);

	restart_notify->reboot_nb.notifier_call = nothing_notifier_reboot;
	restart_notify->reboot_nb.priority = 255;
	register_reboot_notifier(&restart_notify->reboot_nb);

	platform_set_drvdata(pdev, restart_notify);
	pr_info("%s OK\n", __func__);
	return 0;
}

static int nothing_restart_info_remove(struct platform_device *pdev)
{
	struct nothing_restart_notify *restart_notify = platform_get_drvdata(pdev);

	atomic_notifier_chain_unregister(&panic_notifier_list,
					 &restart_notify->panic_nb);
	unregister_reboot_notifier(&restart_notify->reboot_nb);

	if (restart_info)
		iounmap(restart_info);

	return 0;
}

static const struct of_device_id of_nothing_restart_info_match[] = {
	{ .compatible = "qcom,msm-imem", },
	{},
};
MODULE_DEVICE_TABLE(of, of_nothing_restart_info_match);

static struct platform_driver nothing_restart_info_driver = {
	.probe = nothing_restart_info_probe,
	.remove = nothing_restart_info_remove,
	.driver = {
		.name = "nothing-restart-info",
		.of_match_table = of_nothing_restart_info_match,
	},
};

static int __init nothing_restart_info_driver_init(void)
{
	pr_warn("nothing_restart_info_driver_init\n");

	return platform_driver_register(&nothing_restart_info_driver);
}

module_init(nothing_restart_info_driver_init);

static void __exit nothing_restart_info_driver_exit(void)
{
	return platform_driver_unregister(&nothing_restart_info_driver);
}
module_exit(nothing_restart_info_driver_exit);

MODULE_DESCRIPTION("Nothing Restart Info Driver");
MODULE_LICENSE("GPL v2");
