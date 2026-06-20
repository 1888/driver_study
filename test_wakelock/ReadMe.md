# Wakelock API 测试流程文档

本驱动用于演示 Linux 内核电源管理中 `wakeup_source` 相关接口的使用，包括 `__pm_stay_awake`、`__pm_relax` 和 `__pm_wakeup_event`。驱动创建了名称为 `my_wake_source` 的唤醒源，并通过 sysfs 接口提供测试触发功能。

## 1. 编译
确保已安装当前内核的编译环境（kernel headers）：

```bash
make
```

## 2. 运行测试
加载驱动模块后，可以通过 sysfs 接口手动触发 `manual` (对应 `__pm_stay_awake`/`__pm_relax`) 或 `event` (对应 `__pm_wakeup_event`) 测试：

```bash
sudo insmod test_wakelock.ko
# 手动触发测试
echo manual | sudo tee /sys/kernel/demo_wakelock/trigger
# 或者触发事件测试
echo event | sudo tee /sys/kernel/demo_wakelock/trigger
```

## 3. 验证驱动状态
通过 `dmesg` 查看内核日志，确认接口调用流程：

```bash
dmesg | tail -n 10
```

通过 `debugfs` 查看唤醒源的统计信息：

```bash
sudo cat /sys/kernel/debug/wakeup_sources | grep my_wake_source
```
比如：
```bash
cityday@qemuubunt2404server:~/test_wakelock$ sudo cat /sys/kernel/debug/wakeup_sources 
name		active_count	event_count	wakeup_count	expire_count	active_since	total_time	max_time	last_change	prevent_suspend_time
my_wake_source	0		0		0		0		0		0		0		0		0
```

## 4. 电源管理测试
要测试 Wakelock 是否成功阻止了系统进入 Suspend（休眠）：

1. **设置休眠模式**（例如深度睡眠）：  
   cat /sys/power/mem_sleep命令可以查看系统挂起（suspend-to-RAM)时采用哪种休眠机制。如下面的例子，表明系统挂起支持：
   * s2idle模式 - 现代轻量待机（S0 低功耗空闲，Modern Standby）
     * CPU 彻底断电停止运行，主板进入 ACPI S3 低功耗状态，整机功耗极低
     * 唤醒流程：触发唤醒信号 → 主板上电复位 → 固件恢复内存上下文 → 内核恢复运行
     * 唤醒命令：在qemu monitor中输入唤醒命令：system_wakeup
* deep模式 - 传统深度挂起（S3 睡眠，ACPI S3
  * CPU 不会完全断电，整机功耗比 deep (S3) 略高，但唤醒速度极快
  * 唤醒流程：无需主板完整上电，CPU 从空闲状态直接恢复，毫秒级唤醒
  * 唤醒命令：虚拟机里键盘鼠标无法直接唤醒 s2idle，虚拟化层屏蔽了普通设备中断，只能用 QEMU Monitor 命令强制唤醒。在qemu monitor中模拟短按电源键：system_powerdown

当前采用的是deep模式。但是虚拟机中两种睡眠实际效果区别很小，虚拟化层会拦截真实硬件断电，仅模拟睡眠状态。
```bash
# []框住的为当前使用的模式
cityday@qemuubunt2404server:~/test_wakelock$ cat /sys/power/mem_sleep
s2idle [deep]
# 可以同通过写入s2idle，切换系统挂起时采用s2idle模式
cityday@qemuubunt2404server:~/test_wakelock$ echo s2idle | sudo tee /sys/power/mem_sleep
s2idle
cityday@qemuubunt2404server:~/test_wakelock$ cat /sys/power/mem_sleep
[s2idle] deep
# 可以同通过写入deep，切换系统挂起时采用deep模式
cityday@qemuubunt2404server:~/test_wakelock$ echo deep | sudo tee /sys/power/mem_sleep
deep
cityday@qemuubunt2404server:~/test_wakelock$ cat /sys/power/mem_sleep
s2idle [deep]
```
2. **触发休眠**：
向/sys/power/state写入mem_sleep中当前设置的模式，即可让系统休眠。
```bash
cityday@qemuubunt2404server:~/test_wakelock$ cat /sys/power/mem_sleep
s2idle [deep]
cityday@qemuubunt2404server:~/test_wakelock$ echo mem | sudo tee /sys/power/state
mem #卡在这里
```
此时已经进入S3, 虚拟机中已不能操作。可以在qemu monitor中通过命令info status查看到suspended的状态。此时可以通过qume monitor中输入system_wakeup命令唤醒虚拟机。
```bash
(qemu) info status 
VM status: paused (suspended)
(qemu) system_wakeup 
```
唤醒后，虚拟机从刚才卡住的地方退出，可以继续操作了。
```bash
cityday@qemuubunt2404server:~/test_wakelock$ echo mem | sudo tee /sys/power/state
mem
cityday@qemuubunt2404server:~/test_wakelock$  
```
3. **验证pm_stay_awake期间不能进入system suspend**
sudo cat /sys/kernel/debug/wakeup_sources 

```bash
# 通过 sysfs 接口手动拉住wakelock (对应 __pm_stay_awake），30s之后会释放wakelock（__pm_relax)
echo manual | sudo tee /sys/kernel/demo_wakelock/trigger
```
30s内在另一个terminal中输入system suspend的命令。

通过 sysfs 接口手动触发 `manual` (对应 `__pm_stay_awake`/`__pm_relax`) 或 `event` (对应 `__pm_wakeup_event`) 测试：

```bash
sudo insmod test_wakelock.ko
# 手动触发测试
echo manual | sudo tee /sys/kernel/demo_wakelock/trigger
# 或者触发事件测试
echo event | sudo tee /sys/kernel/demo_wakelock/trigger
```


* **预期行为**：
    * 在执行 `manual` 测试时，系统保持唤醒 30 秒（默认）。
    * 在执行 `event` 测试时，系统保持唤醒 30 秒（默认，通过 `__pm_wakeup_event`）。
    * 锁释放后，系统应能正常进入休眠。

## 5. 卸载
```bash
sudo rmmod test_wakelock
```
