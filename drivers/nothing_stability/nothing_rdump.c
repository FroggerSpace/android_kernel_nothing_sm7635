#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include <linux/kernel.h>

#define DRIVER_NAME "nt_rdump"

static void __iomem *imem_base;
static u64 lba_val;

/* Module parameter callbacks to read/write LBA value */
static int lba_param_set(const char *val, const struct kernel_param *kp)
{
    unsigned long long tmp;
    int ret = kstrtoull(val, 10, &tmp);
    if (ret)
        return ret;

    if (imem_base) {
        writeq(tmp, imem_base);
        lba_val = tmp;
    }
    return 0;
}

static int lba_param_get(char *buffer, const struct kernel_param *kp)
{
    u64 tmp = 0;
    if (imem_base)
        tmp = readq(imem_base);
    return sprintf(buffer, "%llu\n", tmp);
}

static const struct kernel_param_ops lba_param_ops = {
    .set = lba_param_set,
    .get = lba_param_get,
};
module_param_cb(lba_addr, &lba_param_ops, &lba_val, 0644);
MODULE_PARM_DESC(lba_addr, "LBA address stored in OCIMEM via DT reg");

static int __init mrdump_lba_init(void)
{
    struct device_node *np;
    struct resource res;
    int ret;

    np = of_find_compatible_node(NULL, NULL, "nothing,lba_addr");
    if (!np) {
        pr_err(DRIVER_NAME ": device node not found\n");
        return -ENODEV;
    }

    ret = of_address_to_resource(np, 0, &res);
    if (ret) {
        pr_err(DRIVER_NAME ": failed to get reg resource\n");
        of_node_put(np);
        return ret;
    }

    imem_base = ioremap(res.start, resource_size(&res));
    if (!imem_base) {
        pr_err(DRIVER_NAME ": ioremap failed\n");
        of_node_put(np);
        return -ENOMEM;
    }

    pr_info(DRIVER_NAME ": mapped IMEM at %pa size %llx\n", &res.start, resource_size(&res));
    of_node_put(np);
    return 0;
}

static void __exit mrdump_lba_exit(void)
{
    if (imem_base)
        iounmap(imem_base);
}

module_init(mrdump_lba_init);
module_exit(mrdump_lba_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("<BSP_CORE@nothing.tech>");
MODULE_DESCRIPTION("LBA Address IMEM driver");

