# Wakelock API 测试流程文档

本驱动用于演示 Linux 内核电源管理中 `wakeup_source` 相关接口的使用，包括 `__pm_stay_awake`、`__pm_relax` 和 `__pm_wakeup_event`。

## 1. 编译
确保已安装当前内核的编译环境（kernel headers）：

```bash
make
```

## 2. 加载模块
加载驱动模块后，驱动会自动运行 `test_manual_wakelock` 和 `test_event_wakelock` 函数：

```bash
sudo insmod test_wakelock.ko
```

## 3. 验证驱动状态
通过 `dmesg` 查看内核日志，确认接口调用流程：

```bash
dmesg | tail -n 10
```

通过 `debugfs` 查看唤醒源的统计信息：

```bash
sudo cat /sys/kernel/debug/wakeup_sources | grep demo_wakelock
```

## 4. 电源管理测试
要测试 Wakelock 是否成功阻止了系统进入 Suspend（休眠）：

1. **设置休眠模式**（例如深度睡眠）：
   ```bash
   echo mem | sudo tee /sys/power/mem_sleep
   ```

2. **触发休眠**：
   ```bash
   echo mem | sudo tee /sys/power/state
   ```

* **预期行为**：
    * 在执行 `__pm_stay_awake` 到 `__pm_relax` 的 5 秒期间，系统执行休眠命令时，命令会阻塞或直接返回失败。
    * 在 `__pm_wakeup_event` 触发后的 3 秒内，系统同样无法进入休眠。
    * 锁释放后，系统应能正常进入休眠。

## 5. 卸载
```bash
sudo rmmod test_wakelock
```
