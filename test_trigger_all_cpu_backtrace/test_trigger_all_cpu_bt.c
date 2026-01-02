// SPDX-License-Identifier: GPL-2.0
/*
 * Simple DebugFS interface to trigger all CPU backtraces
 * Using kernel's trigger_all_cpu_backtrace() function
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/debugfs.h>
#include <linux/string.h>
#include <linux/nmi.h>
#include <linux/cpu.h>

#define DRIVER_NAME "trigger_backtrace"
#define DEBUGFS_FILE_NAME "trigger_all_cpu_backtrace"

static struct dentry *debugfs_dir;
static struct dentry *debugfs_file;

/* DebugFS write callback */
static ssize_t debugfs_write(struct file *file, const char __user *user_buf,
                            size_t count, loff_t *ppos)
{
    char buf[32];
    ssize_t len;
    
    /* Read user input */
    len = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, user_buf, count);
    if (len < 0)
        return len;
    
    buf[len] = '\0';
    
    /* Check if user wrote "trigger" */
    if (strncmp(buf, "trigger", 7) == 0) {
        pr_info("User triggered all CPU backtrace via debugfs\n");
        //trigger_all_cpu_backtrace();
	//arch_cpu_idle_enter();
	arch_cpu_idle();
        pr_info("trigger_all_cpu_backtrace() completed\n");
        return count;
    }
    
    pr_warn("Invalid command. Write 'trigger' to generate backtraces\n");
    return -EINVAL;
}

/* DebugFS file operations */
static const struct file_operations debugfs_fops = {
    .owner = THIS_MODULE,
    .write = debugfs_write,
};

/* Module initialization */
static int __init trigger_backtrace_init(void)
{
    pr_info("Initializing %s driver\n", DRIVER_NAME);
    
    /* Create debugfs directory */
    debugfs_dir = debugfs_create_dir(DRIVER_NAME, NULL);
    if (IS_ERR(debugfs_dir)) {
        pr_err("Failed to create debugfs directory\n");
        return PTR_ERR(debugfs_dir);
    }
    
    /* Create debugfs file */
    debugfs_file = debugfs_create_file(DEBUGFS_FILE_NAME, 0644,
                                      debugfs_dir, NULL, &debugfs_fops);
    if (IS_ERR(debugfs_file)) {
        pr_err("Failed to create debugfs file\n");
        debugfs_remove(debugfs_dir);
        return PTR_ERR(debugfs_file);
    }
    
    pr_info("%s driver initialized successfully\n", DRIVER_NAME);
    pr_info("DebugFS interface: /sys/kernel/debug/%s/%s\n", 
            DRIVER_NAME, DEBUGFS_FILE_NAME);
    
    return 0;
}

/* Module cleanup */
static void __exit trigger_backtrace_exit(void)
{
    /* Remove debugfs entries */
    debugfs_remove(debugfs_file);
    debugfs_remove(debugfs_dir);
    
    pr_info("%s driver unloaded\n", DRIVER_NAME);
}

module_init(trigger_backtrace_init);
module_exit(trigger_backtrace_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("DebugFS interface to trigger all CPU backtraces using kernel function");
MODULE_VERSION("1.0");
