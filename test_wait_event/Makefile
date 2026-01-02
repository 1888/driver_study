obj-m += wait_event_demo.o

KERNEL_DIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) clean

install:
	sudo insmod wait_event_demo.ko

remove:
	sudo rmmod wait_event_demo

test_wakeup:
	echo 1 | sudo tee /sys/kernel/wait_event_demo/trigger_wakeup

show_stats:
	cat /sys/kernel/wait_event_demo/stats

show_status:
	cat /sys/kernel/wait_event_demo/thread_status

debug:
	dmesg | tail -20
