#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>

#include "drv_log.h"

#define DRIVER_NAME "wait_event_demo_drv"
#define DEVICE_NAME "wait_event_demo_dev"
#define CLASS_NAME "wait_event_cls"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Cityday");
MODULE_DESCRIPTION("Wait Event Interruptible Demo Driver");
MODULE_VERSION("1.0");

// 设备数据结构
struct wait_event_demo_device {
    struct kobject *kobj;
    
    // 等待队列相关
    wait_queue_head_t waitq;
    int condition;              // 等待条件
    struct mutex lock;          // 保护condition
    
    // 线程相关
    struct task_struct *thread;
    bool thread_running;
    
    // 统计信息
    unsigned long wakeup_count;
    unsigned long wait_count;
};

static struct wait_event_demo_device *demo_dev;
static struct class *wait_event_class;
static struct device *wait_event_device;

// sysfs属性显示函数
static ssize_t condition_show(struct kobject *kobj, struct kobj_attribute *attr,
                             char *buf)
{
    struct wait_event_demo_device *dev = demo_dev;
    int cond;
    
    mutex_lock(&dev->lock);
    cond = dev->condition;
    mutex_unlock(&dev->lock);
    
    return sprintf(buf, "%d\n", cond);
}

// sysfs属性设置函数
static ssize_t condition_store(struct kobject *kobj, struct kobj_attribute *attr,
                              const char *buf, size_t count)
{
    struct wait_event_demo_device *dev = demo_dev;
    int value;
    int ret;
    
    ret = kstrtoint(buf, 10, &value);
    if (ret < 0) {
        DRV_LOG_ERR("Invalid input: %s\n", buf);
        return ret;
    }
    
    mutex_lock(&dev->lock);
    dev->condition = value;
    mutex_unlock(&dev->lock);
    
    // 如果条件变为1，唤醒所有等待者
    if (value == 1) {
        dev->wakeup_count++;
        wake_up_interruptible(&dev->waitq);
        DRV_LOG_INFO("Woke up waiting thread(s), total wakeups: %lu\n", 
                dev->wakeup_count);
    }
    
    return count;
}

static ssize_t thread_status_show(struct kobject *kobj, 
                                 struct kobj_attribute *attr, char *buf)
{
    struct wait_event_demo_device *dev = demo_dev;
    const char *status;
    
    if (dev->thread_running) {
        status = dev->thread ? "running" : "starting";
    } else {
        status = "stopped";
    }
    
    return sprintf(buf, "%s\n", status);
}

static ssize_t stats_show(struct kobject *kobj, struct kobj_attribute *attr,
                         char *buf)
{
    struct wait_event_demo_device *dev = demo_dev;
    
    return sprintf(buf, "Wakeups: %lu\nWaits: %lu\n",
                   dev->wakeup_count, dev->wait_count);
}

static ssize_t trigger_wakeup_store(struct kobject *kobj, 
                                   struct kobj_attribute *attr,
                                   const char *buf, size_t count)
{
    struct wait_event_demo_device *dev = demo_dev;
    int value;
    int ret;
    
    ret = kstrtoint(buf, 10, &value);
    if (ret < 0) {
        DRV_LOG_ERR("Invalid input: %s\n", buf);
        return ret;
    }
    
    // 任何非零值都会触发唤醒
    if (value != 0) {
        mutex_lock(&dev->lock);
        dev->condition = 1;
        mutex_unlock(&dev->lock);
        
        dev->wakeup_count++;
        wake_up_interruptible(&dev->waitq);
        
        DRV_LOG_INFO("Manual wakeup triggered, total: %lu\n", dev->wakeup_count);
    }
    
    return count;
}

// 定义sysfs属性
static struct kobj_attribute condition_attr = 
    __ATTR(condition, 0644, condition_show, condition_store);

static struct kobj_attribute thread_status_attr = 
    __ATTR(thread_status, 0444, thread_status_show, NULL);

static struct kobj_attribute stats_attr = 
    __ATTR(stats, 0444, stats_show, NULL);

static struct kobj_attribute trigger_wakeup_attr = 
    __ATTR(trigger_wakeup, 0200, NULL, trigger_wakeup_store);

static struct attribute *wait_demo_attrs[] = {
    &condition_attr.attr,
    &thread_status_attr.attr,
    &stats_attr.attr,
    &trigger_wakeup_attr.attr,
    NULL,
};

static const struct attribute_group wait_event_demo_attr_group = {
	.attrs = wait_demo_attrs,
};

static const struct attribute_group *wait_event_demo_attr_groups[] = {
	&wait_event_demo_attr_group,
    NULL
};

static struct kobj_type wait_demo_ktype = {
    .sysfs_ops = &kobj_sysfs_ops,
    .default_groups = wait_event_demo_attr_groups,
};

// 内核线程函数
static int wait_event_demo_thread(void *data)
{
    struct wait_event_demo_device *dev = data;
    int ret = 0;
    int wait_cycle = 0;
    
    // 允许SIGKILL信号
    allow_signal(SIGKILL);
    
    DRV_LOG_INFO("Wait event demo thread started, PID: %d\n", current->pid);
    
    while (!kthread_should_stop()) {
        wait_cycle++;
        
        // 重置条件
        mutex_lock(&dev->lock);
        dev->condition = 0;
        mutex_unlock(&dev->lock);
        
        DRV_LOG_INFO("Thread: Waiting for condition (cycle %d)...\n", wait_cycle);
        dev->wait_count++;
        
        // 使用wait_event_interruptible等待
        ret = wait_event_interruptible(dev->waitq, 
                                       (dev->condition != 0) || 
                                       kthread_should_stop());
        
        // 检查中断原因
        if (ret == -ERESTARTSYS) {
            /*
             * 检查是否为致命信号
             * 如果是致命信号，应该退出线程
             * 否则可以继续等待
             */
            if (fatal_signal_pending(current)) {
                DRV_LOG_INFO("Thread: Received fatal signal, exiting\n");
                break;
            }
            
            DRV_LOG_INFO("Thread: Interrupted by non-fatal signal, continuing...\n");
            continue;
        }
        
        if (kthread_should_stop()) {
            DRV_LOG_INFO("Thread: Received stop request\n");
            break;
        }
        
        // 正常唤醒
        DRV_LOG_INFO("Thread: Woken up! Condition = %d\n", dev->condition);
        
        // 模拟一些处理工作
        msleep_interruptible(100);
    }
    
    DRV_LOG_INFO("Wait demo thread exiting\n");
    return 0;
}

// 创建设备
static int __init wait_event_demo_init(void)
{
    int ret = 0;
    
    DRV_LOG_INFO("Wait Event Demo Driver Initializing...\n");
    
    // 分配设备结构
    demo_dev = kzalloc(sizeof(*demo_dev), GFP_KERNEL);
    if (!demo_dev) {
        DRV_LOG_ERR("Failed to allocate device structure\n");
        return -ENOMEM;
    }
    
    // 初始化等待队列
    init_waitqueue_head(&demo_dev->waitq);
    mutex_init(&demo_dev->lock);
    demo_dev->condition = 0;
    demo_dev->thread_running = false;
    demo_dev->wakeup_count = 0;
    demo_dev->wait_count = 0;
    
    // 到/sys/kernel/目录下创建sysfs kobject，名为“wait_event_demo“
    /* 常用的全局 kobject 指针
     * kernel_kobj;      // /sys/kernel/
     * fs_kobj;          // /sys/fs/
     * dev_kobj;         // /sys/dev/
     * power_kobj;       // /sys/power/
     * m_kobj;          // /sys/kernel/mm/
     */
    demo_dev->kobj = kobject_create_and_add("wait_event_demo", kernel_kobj);
    if (!demo_dev->kobj) {
        DRV_LOG_ERR("Failed to create wait_event_demo kobject\n");
        ret = -ENOMEM;
        goto err_kobj;
    }
    
    // 设置ktype
    demo_dev->kobj->ktype = &wait_demo_ktype;
    
    // 创建内核线程
    demo_dev->thread = kthread_create(wait_event_demo_thread, demo_dev, 
                                     "wait_event_demo_thread");
    if (IS_ERR(demo_dev->thread)) {
        ret = PTR_ERR(demo_dev->thread);
        DRV_LOG_ERR("Failed to create kernel thread: %d\n", ret);
        goto err_thread;
    }
    
    demo_dev->thread_running = true;
    wake_up_process(demo_dev->thread);
    
    // 创建设备类和设备（可选，用于完整性）
    wait_event_class = class_create(CLASS_NAME);
    if (IS_ERR(wait_event_class)) {
        DRV_LOG_WARN("Failed to create class, continuing without device node\n");
        wait_event_class = NULL;
    } else {
        wait_event_device = device_create(wait_event_class, NULL, 
                                   MKDEV(0, 0), NULL, DEVICE_NAME);
        if (IS_ERR(wait_event_device)) {
            DRV_LOG_WARN("Failed to create device\n");
            wait_event_device = NULL;
        }
    }
    
    DRV_LOG_INFO("Wait Event Demo Driver Loaded Successfully\n");
    DRV_LOG_INFO("Sysfs interface at: /sys/kernel/wait_event_demo/\n");
    DRV_LOG_INFO("Use 'echo 1 > /sys/kernel/wait_event_demo/trigger_wakeup' to wake thread\n");
    
    return 0;

err_thread:
    kobject_put(demo_dev->kobj);
err_kobj:
    kfree(demo_dev);
    return ret;
}

// 清理函数
static void __exit wait_event_demo_exit(void)
{
    DRV_LOG_INFO("Wait Event Demo Driver Exiting...\n");
    
    if (demo_dev) {
        // 停止线程
        if (demo_dev->thread && demo_dev->thread_running) {
            demo_dev->thread_running = false;
            
            // 先设置停止标志
            kthread_stop(demo_dev->thread);
            
            // 确保线程被唤醒以检查停止标志
            mutex_lock(&demo_dev->lock);
            demo_dev->condition = 1;
            mutex_unlock(&demo_dev->lock);
            wake_up_interruptible(&demo_dev->waitq);
            
            // 等待线程退出
            if (demo_dev->thread) {
                int ret = kthread_stop(demo_dev->thread);
                if (ret == -EINTR)
                    DRV_LOG_INFO("Thread stopped by signal\n");
            }
        }
        
        // 清理sysfs
        if (demo_dev->kobj)
            kobject_put(demo_dev->kobj);
        
        // 清理设备节点
        if (wait_event_device)
            device_destroy(wait_event_class, MKDEV(0, 0));
        if (wait_event_class)
            class_destroy(wait_event_class);
        
        // 释放内存
        kfree(demo_dev);
        demo_dev = NULL;
    }
    
    DRV_LOG_INFO("Wait Event Demo Driver Unloaded\n");
}

module_init(wait_event_demo_init);
module_exit(wait_event_demo_exit);
