
#include <linux/errcode.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/reboot.h>
#include <linux/string.h>
#include <video/mmp_disp.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <asm/io.h>
//#include <soc/qcom/scm.h>
#include <asm/uaccess.h>
#include <linux/ioport.h>
#include <linux/gpio.h>

static struct kobject *errcode_kobj = NULL;

#define CREATE_DEVICE_ATTR(_name)                       \
		&_name##_attr.attr

#define CREATE_DEVICE_INFO(_name)                       \
extern char _name[32];                                  \
static ssize_t _name##_show(struct kobject *kobj,       \
		struct kobj_attribute *attr, char * buf)        \
{                                                       \
	char *s = buf;                                      \
	s += sprintf(s, "%s\n", _name);                     \
	return (s - buf);                                   \
}                                                       \
                                                        \
static ssize_t _name##_store(struct kobject *kobj,       \
		struct kobj_attribute *attr, const char * buf, size_t n)    \
{                                                       \
	return n;                                           \
}                                                       \
                                                        \
static struct kobj_attribute _name##_attr = {           \
	.attr = {                                           \
		.name = #_name,                                 \
		.mode = 0644,                                   \
	},                                                  \
	.show = _name##_show,                               \
	.store = _name##_store,                             \
}


/* error code begin*/
unsigned long  errcode_lcd = 0x701000;
unsigned long  errcode_tp = 0x702000;
EXPORT_SYMBOL(errcode_tp);
unsigned long  errcode_ddr = 0x703000;
EXPORT_SYMBOL(errcode_ddr);
unsigned long  errcode_sensor = 0x705000;
EXPORT_SYMBOL(errcode_sensor);
unsigned long  errcode_vib = 0x706000;
unsigned long errcode_audio = 0x707000;
EXPORT_SYMBOL(errcode_audio);
unsigned long  errcode_fp = 0x708000;
EXPORT_SYMBOL(errcode_fp);
unsigned long  errcode_wifi = 0x801000;
EXPORT_SYMBOL(errcode_wifi);
unsigned long  errcode_bt = 0x802000;
EXPORT_SYMBOL(errcode_bt);
unsigned long  errcode_gps = 0x803000;
EXPORT_SYMBOL(errcode_gps);
unsigned long  errcode_nfc = 0x804000;
EXPORT_SYMBOL(errcode_nfc);
unsigned long  errcode_fm = 0x805000;
EXPORT_SYMBOL(errcode_fm);
unsigned long errcode_charger = 0x709000;
unsigned long errcode_usb = 0x710000;
unsigned long errcode_memory = 0x704000;

unsigned long  errcode_cam_back = 0x000000;
EXPORT_SYMBOL(errcode_cam_back);
unsigned long  errcode_cam_front = 0x000000;
EXPORT_SYMBOL(errcode_cam_front);
unsigned long  errcode_cam_uw = 0x000000;
EXPORT_SYMBOL(errcode_cam_uw);
unsigned long  errcode_cam_tele = 0x000000;
EXPORT_SYMBOL(errcode_cam_tele);

void clear_errorcode_value(unsigned long *code_type)
{
	if (!code_type)
		return;
	*code_type &= 0xFFF000UL;
}

static ssize_t errcode_lcd_show(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{
	char *s = buf;
	s += sprintf(s, "%lx\n",errcode_lcd);
	clear_errorcode_value(&errcode_lcd);
	return (s - buf);
}
static ssize_t errcode_lcd_store(struct kobject *kobj, struct kobj_attribute *attr, const char * buf, size_t n)
{
	return n;
}
static struct kobj_attribute errcode_lcd_attr = {
	.attr = {
		.name = "errcode_lcd",
		.mode = 0644,
	},
	.show =&errcode_lcd_show,
	.store= &errcode_lcd_store,
};

static ssize_t errcode_tp_show(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{
	char *s = buf;
	s += sprintf(s, "%lx\n",errcode_tp);
	return (s - buf);
}
static ssize_t errcode_tp_store(struct kobject *kobj, struct kobj_attribute *attr, const char * buf, size_t n)
{
	return n;
}
static struct kobj_attribute errcode_tp_attr = {
	.attr = {
		.name = "errcode_tp",
		.mode = 0644,
	},
	.show =&errcode_tp_show,
	.store= &errcode_tp_store,
};

static ssize_t errcode_ddr_show(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{
	char *s = buf;
	s += sprintf(s, "%lx\n",errcode_ddr);
	return (s - buf);
}
static ssize_t errcode_ddr_store(struct kobject *kobj, struct kobj_attribute *attr, const char * buf, size_t n)
{
	return n;
}
static struct kobj_attribute errcode_ddr_attr = {
	.attr = {
		.name = "errcode_ddr",
		.mode = 0644,
	},
	.show =&errcode_ddr_show,
	.store= &errcode_ddr_store,
};

static ssize_t errcode_memory_show(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{
	char *s = buf;
	s += sprintf(s, "%lx\n",errcode_memory);
	return (s - buf);
}
static ssize_t errcode_memory_store(struct kobject *kobj, struct kobj_attribute *attr, const char * buf, size_t n)
{
	return n;
}
static struct kobj_attribute errcode_memory_attr = {
	.attr = {
		.name = "errcode_memory",
		.mode = 0644,
	},
	.show =&errcode_memory_show,
	.store= &errcode_memory_store,
};

static ssize_t errcode_sensor_show(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{
	char *s = buf;
	s += sprintf(s, "%lx\n",errcode_sensor);
	return (s - buf);
}
static ssize_t errcode_sensor_store(struct kobject *kobj, struct kobj_attribute *attr, const char * buf, size_t n)
{
	return n;
}
static struct kobj_attribute errcode_sensor_attr = {
	.attr = {
		.name = "errcode_sensor",
		.mode = 0644,
	},
	.show =&errcode_sensor_show,
	.store= &errcode_sensor_store,
};

static ssize_t errcode_vib_show(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{
	char *s = buf;
	s += sprintf(s, "%lx\n",errcode_vib);
	return (s - buf);
}
static ssize_t errcode_vib_store(struct kobject *kobj, struct kobj_attribute *attr, const char * buf, size_t n)
{
	return n;
}
static struct kobj_attribute errcode_vib_attr = {
	.attr = {
		.name = "errcode_vib",
		.mode = 0644,
	},
	.show =&errcode_vib_show,
	.store= &errcode_vib_store,
};

static ssize_t errcode_audio_show(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{
	char *s = buf;
	s += sprintf(s, "%lx\n",errcode_audio);
	return (s - buf);
}
static ssize_t errcode_audio_store(struct kobject *kobj, struct kobj_attribute *attr, const char * buf, size_t n)
{
	int ret = 0;
	ret = kstrtoul(buf, 0, &errcode_audio);
	printk("errcode_audio = 0x%lx, buf = %s, ret:%d\n", errcode_audio, buf, ret);
	return n;
}
static struct kobj_attribute errcode_audio_attr = {
	.attr = {
		.name = "errcode_audio",
		.mode = 0664,
	},
	.show =&errcode_audio_show,
	.store= &errcode_audio_store,
};

static ssize_t errcode_fp_show(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{
	char *s = buf;
	s += sprintf(s, "%lx\n",errcode_fp);
	return (s - buf);
}
static ssize_t errcode_fp_store(struct kobject *kobj, struct kobj_attribute *attr, const char * buf, size_t n)
{
        int ret = 0;
	ret = kstrtoul(buf, 0, &errcode_fp);
	printk("errcode_fp = 0x%lx, buf = %s, ret:%d\n", errcode_fp, buf, ret);
	return n;
}
static struct kobj_attribute errcode_fp_attr = {
	.attr = {
		.name = "errcode_fp",
		.mode = 0644,
	},
	.show =&errcode_fp_show,
	.store= &errcode_fp_store,
};

static ssize_t errcode_usb_show(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{
	char *s = buf;
	s += sprintf(s, "%lx\n",errcode_usb);
	return (s - buf);
}
static ssize_t errcode_usb_store(struct kobject *kobj, struct kobj_attribute *attr, const char * buf, size_t n)
{
	return n;
}
static struct kobj_attribute errcode_usb_attr = {
	.attr = {
		.name = "errcode_usb",
		.mode = 0644,
	},
	.show =&errcode_usb_show,
	.store= &errcode_usb_store,
};

static ssize_t errcode_charger_show(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{
	char *s = buf;
	s += sprintf(s, "%lx\n",errcode_charger);
	return (s - buf);
}
static ssize_t errcode_charger_store(struct kobject *kobj, struct kobj_attribute *attr, const char * buf, size_t n)
{
	return n;
}
static struct kobj_attribute errcode_charger_attr = {
	.attr = {
		.name = "errcode_charger",
		.mode = 0644,
	},
	.show =&errcode_charger_show,
	.store= &errcode_charger_store,
};
/* error code end*/

//shangfei add to record wcn errorcode 20240709 begin
static ssize_t errcode_wifi_show(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{
	char *s = buf;
	s += sprintf(s, "%lx\n",errcode_wifi);
	return (s - buf);
}

static ssize_t errcode_wifi_store(struct kobject *kobj, struct kobj_attribute *attr, const char * buf, size_t n)
{
	return n;
}
static struct kobj_attribute errcode_wifi_attr = {
	.attr = {
		.name = "errcode_wifi",
		.mode = 0644,
	},
	.show =&errcode_wifi_show,
	.store= &errcode_wifi_store,
};

static ssize_t errcode_bt_show(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{
	char *s = buf;
	s += sprintf(s, "%lx\n",errcode_bt);
	return (s - buf);
}

static ssize_t errcode_bt_store(struct kobject *kobj, struct kobj_attribute *attr, const char * buf, size_t n)
{
	return n;
}
static struct kobj_attribute errcode_bt_attr = {
	.attr = {
		.name = "errcode_bt",
		.mode = 0644,
	},
	.show =&errcode_bt_show,
	.store= &errcode_bt_store,
};

static ssize_t errcode_gps_show(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{
	char *s = buf;
	s += sprintf(s, "%lx\n",errcode_gps);
	return (s - buf);
}

static ssize_t errcode_gps_store(struct kobject *kobj, struct kobj_attribute *attr, const char * buf, size_t n)
{
	return n;
}
static struct kobj_attribute errcode_gps_attr = {
	.attr = {
		.name = "errcode_gps",
		.mode = 0644,
	},
	.show =&errcode_gps_show,
	.store= &errcode_gps_store,
};

static ssize_t errcode_nfc_show(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{
	char *s = buf;
	s += sprintf(s, "%lx\n",errcode_nfc);
	return (s - buf);
}

static ssize_t errcode_nfc_store(struct kobject *kobj, struct kobj_attribute *attr, const char * buf, size_t n)
{
	return n;
}
static struct kobj_attribute errcode_nfc_attr = {
	.attr = {
		.name = "errcode_nfc",
		.mode = 0644,
	},
	.show =&errcode_nfc_show,
	.store= &errcode_nfc_store,
};

static ssize_t errcode_fm_show(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{
	char *s = buf;
	s += sprintf(s, "%lx\n",errcode_fm);
	return (s - buf);
}

static ssize_t errcode_fm_store(struct kobject *kobj, struct kobj_attribute *attr, const char * buf, size_t n)
{
	return n;
}
static struct kobj_attribute errcode_fm_attr = {
	.attr = {
		.name = "errcode_fm",
		.mode = 0644,
	},
	.show =&errcode_fm_show,
	.store= &errcode_fm_store,
};
//shangfei add to record wcn errorcode 20240709 begin

static ssize_t errcode_cam_back_show(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{
	char *s = buf;
	s += sprintf(s, "%lx\n", errcode_cam_back);
	return (s - buf);
}

static ssize_t errcode_cam_back_store(struct kobject *kobj, struct kobj_attribute *attr, const char * buf, size_t n)
{
	int ret = 0;
	unsigned long long temp = 0x0;

	ret = kstrtoull(buf, 0 , &temp);
	if (ret < 0)
		return ret;
	if (temp != (unsigned long)temp)
		return -ERANGE;
	errcode_cam_back = (temp & 0xFFFFF) | CAM_MODULE1;

	return n;
}

static struct kobj_attribute errcode_cam_back_attr = {
	.attr = {
		.name = "errcode_cam_back",
		.mode = 0644,
	},
	.show =&errcode_cam_back_show,
	.store= &errcode_cam_back_store,
};

static ssize_t errcode_cam_front_show(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{
	char *s = buf;
	s += sprintf(s, "%lx\n", errcode_cam_front);
	return (s - buf);
}

static ssize_t errcode_cam_front_store(struct kobject *kobj, struct kobj_attribute *attr, const char * buf, size_t n)
{
	int ret = 0;
	unsigned long long temp = 0x0;

	ret = kstrtoull(buf, 0 , &temp);
	if (ret < 0)
		return ret;
	if (temp != (unsigned long)temp)
		return -ERANGE;
	errcode_cam_front = (temp & 0xFFFFF) | CAM_MODULE2;

	return n;
}

static struct kobj_attribute errcode_cam_front_attr = {
	.attr = {
		.name = "errcode_cam_front",
		.mode = 0644,
	},
	.show =&errcode_cam_front_show,
	.store= &errcode_cam_front_store,
};

static ssize_t errcode_cam_uw_show(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{
        char *s = buf;
        s += sprintf(s, "%lx\n", errcode_cam_uw);
        return (s - buf);
}
static ssize_t errcode_cam_uw_store(struct kobject *kobj, struct kobj_attribute *attr, const char * buf, size_t n)
{
        int ret = 0;
        unsigned long long temp = 0x0;
        ret = kstrtoull(buf, 0 , &temp);
        if (ret < 0)
                return ret;
        if (temp != (unsigned long)temp)
                return -ERANGE;
        errcode_cam_uw = (temp & 0xFFFFF) | CAM_MODULE4;
        return n;
}
static struct kobj_attribute errcode_cam_uw_attr = {
        .attr = {
                .name = "errcode_cam_uw",
                .mode = 0644,
        },
        .show =&errcode_cam_uw_show,
        .store= &errcode_cam_uw_store,
};

static ssize_t errcode_cam_tele_show(struct kobject *kobj, struct kobj_attribute *attr, char * buf)
{
        char *s = buf;
        s += sprintf(s, "%lx\n", errcode_cam_tele);
        return (s - buf);
}
static ssize_t errcode_cam_tele_store(struct kobject *kobj, struct kobj_attribute *attr, const char * buf, size_t n)
{
        int ret = 0;
        unsigned long long temp = 0x0;
        ret = kstrtoull(buf, 0 , &temp);
        if (ret < 0)
                return ret;
        if (temp != (unsigned long)temp)
                return -ERANGE;
        errcode_cam_tele = (temp & 0xFFFFF) | CAM_MODULE5;
        return n;
}
static struct kobj_attribute errcode_cam_tele_attr = {
        .attr = {
                .name = "errcode_cam_tele",
                .mode = 0644,
        },
        .show =&errcode_cam_tele_show,
        .store= &errcode_cam_tele_store,
};

static struct attribute * g[] = {
	&errcode_lcd_attr.attr,
	&errcode_tp_attr.attr,
	&errcode_ddr_attr.attr,
	&errcode_memory_attr.attr,
	&errcode_sensor_attr.attr,
	&errcode_vib_attr.attr,
	&errcode_audio_attr.attr,
	&errcode_fp_attr.attr,
	&errcode_usb_attr.attr,
	&errcode_charger_attr.attr,
	&errcode_wifi_attr.attr,
	&errcode_bt_attr.attr,
	&errcode_gps_attr.attr,
	&errcode_nfc_attr.attr,
	&errcode_fm_attr.attr,
	&errcode_cam_back_attr.attr,
	&errcode_cam_front_attr.attr,
	&errcode_cam_uw_attr.attr,
	&errcode_cam_tele_attr.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = g,
};


static int __init errcode_init(void)
{
	int ret = -ENOMEM;
	
	//printk("%s,line=%d\n",__func__,__LINE__);  

	errcode_kobj = kobject_create_and_add("ontim_errcode", NULL);

	if (errcode_kobj == NULL) {
		printk("errcode_init: kobject_create_and_add failed\n");
		goto fail;
	}

	ret = sysfs_create_group(errcode_kobj, &attr_group);
	if (ret) {
		printk("errcode_init: sysfs_create_group failed\n");
		goto sys_fail;
	}

	return ret;
sys_fail:
	kobject_del(errcode_kobj);
fail:
	return ret;
}

static void __exit errcode_exit(void)
{
	if (errcode_kobj) {
		sysfs_remove_group(errcode_kobj, &attr_group);
		kobject_del(errcode_kobj);
	}
}

arch_initcall(errcode_init);
module_exit(errcode_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Error Code collector");
