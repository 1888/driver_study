#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/pm.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/sysfs.h>

#include "drv_log.h"


#define PM_STAY_AWAKE_TIME_SEC		(30)
#define WS_NAME				"my_wake_source"

static struct wakeup_source *demo_ws;
static struct kobject *demo_kobj;
static int timeout_sec = PM_STAY_AWAKE_TIME_SEC;

/* 测试 1: 使用 __pm_stay_awake (手动控制) */
static void test_manual_wakelock(void)
{
	pr_info("demo: Holding wakelock manually.\n");
	__pm_stay_awake(demo_ws);
}

/* 释放接口: 使用 __pm_relax (手动控制) */
static void test_relax_wakelock(void)
{
	pr_info("demo: Releasing wakelock manually.\n");
	__pm_relax(demo_ws);
}

/* 测试 2: 使用 __pm_wakeup_event (自动超时释放) */
static void test_event_wakelock(void)
{
	pr_info("demo: Triggering %d ms event wakelock...\n", timeout_sec * 1000);
	/* 保持指定的毫秒数后自动释放 */
	__pm_wakeup_event(demo_ws, timeout_sec * 1000);
}

static ssize_t trigger_store(struct kobject *kobj, struct kobj_attribute *attr,
				 const char *buf, size_t count)
{
	if (strncmp(buf, "manual", 6) == 0)
		test_manual_wakelock();
	else if (strncmp(buf, "relax", 5) == 0)
		test_relax_wakelock();
	else if (strncmp(buf, "event", 5) == 0)
		test_event_wakelock();
	else
		return -EINVAL;
	return count;
}

static ssize_t timeout_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", timeout_sec);
}

static ssize_t timeout_store(struct kobject *kobj, struct kobj_attribute *attr,
				 const char *buf, size_t count)
{
	int ret = kstrtoint(buf, 10, &timeout_sec);
	if (ret < 0)
		return ret;
	return count;
}

static struct kobj_attribute trigger_attr = __ATTR(trigger, 0200, NULL, trigger_store);
static struct kobj_attribute timeout_attr = __ATTR(timeout, 0644, timeout_show, timeout_store);

static struct attribute *demo_attrs[] = {
	&trigger_attr.attr,
	&timeout_attr.attr,
	NULL,
};

static struct attribute_group demo_attr_group = {
	.attrs = demo_attrs,
};

static int __init demo_init(void)
{
	int ret;
	demo_ws = wakeup_source_register(NULL, WS_NAME);
	if (!demo_ws)
		return -ENOMEM;

	demo_kobj = kobject_create_and_add("demo_wakelock", kernel_kobj);
	if (!demo_kobj) {
		wakeup_source_unregister(demo_ws);
		return -ENOMEM;
	}

	ret = sysfs_create_group(demo_kobj, &demo_attr_group);
	if (ret) {
		kobject_put(demo_kobj);
		wakeup_source_unregister(demo_ws);
		return ret;
	}

	return 0;
}

static void __exit demo_exit(void)
{
	if (demo_kobj) {
		sysfs_remove_group(demo_kobj, &demo_attr_group);
		kobject_put(demo_kobj);
	}
	if (demo_ws)
		wakeup_source_unregister(demo_ws);
}

module_init(demo_init);
module_exit(demo_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("cityday");
MODULE_DESCRIPTION("Wakelock API Usage Demo");
