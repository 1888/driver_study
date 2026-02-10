#ifndef __DRV_LOG_H__
#define __DRV_LOG_H__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/smp.h>

/* 日志级别定义 */
typedef enum {
    DRV_LOG_LEVEL_PROHIBITED = 0,
    DRV_LOG_LEVEL_ERROR   = 1,
    DRV_LOG_LEVEL_WARNING = 2,
    DRV_LOG_LEVEL_INFO    = 3,
    DRV_LOG_LEVEL_DEBUG   = 4,
} drv_log_level_t;

/* 编译时配置默认级别 */
#ifndef DRV_CURRENT_LOG_LEVEL
#define DRV_CURRENT_LOG_LEVEL DRV_LOG_LEVEL_DEBUG
#endif

/* 获取当前线程的PID */
static inline pid_t get_current_thread_id(void)
{
    return current->pid;
}

/* 获取当前CPU核心ID */
static inline unsigned int get_current_cpu_id(void)
{
    return smp_processor_id();
}

/* 获取当前线程名称 */
static inline const char *get_current_thread_name(void)
{
    return current->comm;
}

/* 检查是否应该打印该级别的日志 */
static inline bool should_log(drv_log_level_t level)
{
    return (level <= DRV_CURRENT_LOG_LEVEL);
}

/* 统一格式日志打印函数 */
static inline void drv_log_print(drv_log_level_t level,
                                const char *func,
                                int line,
                                const char *fmt, ...)
{
    if (!should_log(level))
        return;

    char log_buf[256];
    int offset = 0;
    va_list args;
    
    /* 模块名称 */
    offset += snprintf(log_buf + offset, sizeof(log_buf) - offset,
                      "[%s]", KBUILD_MODNAME);
    
    /* 线程信息: PID:thread_name */
    offset += snprintf(log_buf + offset, sizeof(log_buf) - offset,
                      "[%d:%s]",
                      get_current_thread_id(),
                      get_current_thread_name());
    
    /* CPU信息 */
    offset += snprintf(log_buf + offset, sizeof(log_buf) - offset,
                      "(cpu-%u)",
                      get_current_cpu_id());
    
    /* 函数名 */
    offset += snprintf(log_buf + offset, sizeof(log_buf) - offset,
                      "[%s]",
                      func);
    
    /* 行号 */
    offset += snprintf(log_buf + offset, sizeof(log_buf) - offset,
                      "[%d]",
                      line);
    
    /* 消息内容 */
    va_start(args, fmt);
    offset += vsnprintf(log_buf + offset, sizeof(log_buf) - offset,
                       fmt, args);
    va_end(args);
    
    /* 根据级别选择打印函数 */
    switch (level) {
        case DRV_LOG_LEVEL_ERROR:
            pr_err("%s", log_buf);
            break;
        case DRV_LOG_LEVEL_WARNING:
            pr_warn("%s", log_buf);
            break;
        case DRV_LOG_LEVEL_INFO:
            pr_info("%s", log_buf);
            break;
        case DRV_LOG_LEVEL_DEBUG:
            pr_debug("%s", log_buf);
            break;
        default:
            break;
    }
}

/* 基础日志宏 */
#define DRV_LOG_ERR(fmt, ...) \
    drv_log_print(DRV_LOG_LEVEL_ERROR, __func__, __LINE__, fmt, ##__VA_ARGS__)

#define DRV_LOG_WARN(fmt, ...) \
    drv_log_print(DRV_LOG_LEVEL_WARNING, __func__, __LINE__, fmt, ##__VA_ARGS__)

#define DRV_LOG_INFO(fmt, ...) \
    drv_log_print(DRV_LOG_LEVEL_INFO, __func__, __LINE__, fmt, ##__VA_ARGS__)

#define DRV_LOG_DBG(fmt, ...) \
    drv_log_print(DRV_LOG_LEVEL_DEBUG, __func__, __LINE__, fmt, ##__VA_ARGS__)

/* 十六进制dump宏 */
#define DRV_LOG_HEX_DUMP(ptr, len) \
    do { \
        if (should_log(DRV_LOG_LEVEL_DEBUG)) { \
            print_hex_dump(KERN_DEBUG, "", \
                          DUMP_PREFIX_OFFSET, 16, 1, \
                          ptr, len, true); \
        } \
    } while (0)

/* 带前缀的十六进制dump */
#define DRV_LOG_HEX_DUMP_PREFIX(prefix, ptr, len) \
    do { \
        if (should_log(DRV_LOG_LEVEL_DEBUG)) { \
            print_hex_dump(KERN_DEBUG, prefix, \
                          DUMP_PREFIX_OFFSET, 16, 1, \
                          ptr, len, true); \
        } \
    } while (0)

#endif /* __DRV_LOG_H__ */