#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/pm.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
/* #include <linux/pm_wakeup.h> */

#include "drv_log.h"

#define PM_STAY_AWAKE_TIME_SEC		(30)
#define PM_WAKEUP_EVT_TIME_SEC		(30)
#define WS_NAME				"my_wake_source"

static struct wakeup_source *demo_ws;

/* 测试 1: 使用 __pm_stay_awake 和 __pm_relax (手动控制) */
static void test_manual_wakelock(void)
{
	pr_info("demo: Holding wakelock manually...\n");
	__pm_stay_awake(demo_ws);
	
	/* 模拟一段耗时工作 */
	ssleep(PM_STAY_AWAKE_TIME_SEC); 
	
	pr_info("demo: Releasing wakelock manually.\n");
	__pm_relax(demo_ws);
}

/* 测试 2: 使用 __pm_wakeup_event (自动超时释放) */
static void test_event_wakelock(void)
{
	pr_info("demo: Triggering 3s event wakelock...\n");
	/* 保持 3000 毫秒后自动释放 */
	__pm_wakeup_event(demo_ws, PM_WAKEUP_EVT_TIME_SEC);
}

static int __init demo_init(void)
{
	demo_ws = wakeup_source_register(NULL, WS_NAME);
	if (!demo_ws)
		return -ENOMEM;
	
	/* 示例运行 */
	test_manual_wakelock();
	test_event_wakelock();
	
	return 0;
}

static void __exit demo_exit(void)
{
	if (demo_ws)
		wakeup_source_unregister(demo_ws);
}

module_init(demo_init);
module_exit(demo_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("cityday");
MODULE_DESCRIPTION("Wakelock API Usage Demo");
