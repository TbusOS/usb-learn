# USB驱动示例Makefile
#
# 使用方法：
#   make              - 编译所有模块
#   make clean        - 清理编译文件
#   make install      - 安装模块（需要root权限）
#   make uninstall    - 卸载模块（需要root权限）

# 检查是否在内核模块编译环境
ifneq ($(KERNELRELEASE),)

# 内核模块列表
obj-m += 01_simple_usb_led.o
obj-m += 02_usb_mouse_driver.o
obj-m += 03_usb_serial_driver.o
obj-m += 04_usb_storage_simple.o

else

# 内核源码路径（默认为当前运行的内核）
KERNELDIR ?= /lib/modules/$(shell uname -r)/build

# 当前目录
PWD := $(shell pwd)

# 模块名称
MODULES := 01_simple_usb_led.ko \
           02_usb_mouse_driver.ko \
           03_usb_serial_driver.ko \
           04_usb_storage_simple.ko

# 默认目标
default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

# 清理目标
clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
	rm -f *.o *.ko *.mod.c *.mod.o *.symvers *.order

# 安装模块
install: default
	@echo "安装USB驱动模块..."
	@for mod in $(MODULES); do \
		if [ -f $$mod ]; then \
			echo "安装 $$mod"; \
			sudo insmod $$mod || true; \
		fi; \
	done
	@echo "模块安装完成"
	@lsmod | grep -E "usb_led|usbmouse|usb_serial|usb_storage" || true

# 卸载模块
uninstall:
	@echo "卸载USB驱动模块..."
	-sudo rmmod 01_simple_usb_led 2>/dev/null || true
	-sudo rmmod 02_usb_mouse_driver 2>/dev/null || true
	-sudo rmmod 03_usb_serial_driver 2>/dev/null || true
	-sudo rmmod 04_usb_storage_simple 2>/dev/null || true
	@echo "模块卸载完成"

# 显示加载的模块
show:
	@echo "当前加载的USB驱动模块："
	@lsmod | grep -E "usb_led|usbmouse|usb_serial|usb_storage" || echo "没有加载相关模块"

# 查看内核日志
log:
	@echo "最近的USB驱动日志："
	@dmesg | tail -n 50 | grep -i usb || true

# 帮助信息
help:
	@echo "USB驱动编译系统使用说明："
	@echo "  make         - 编译所有驱动模块"
	@echo "  make clean   - 清理编译生成的文件"
	@echo "  make install - 安装驱动模块（需要sudo）"
	@echo "  make uninstall - 卸载驱动模块（需要sudo）"
	@echo "  make show    - 显示当前加载的模块"
	@echo "  make log     - 查看USB相关的内核日志"
	@echo "  make help    - 显示此帮助信息"
	@echo ""
	@echo "单独编译某个模块："
	@echo "  make 01_simple_usb_led.ko"
	@echo "  make 02_usb_mouse_driver.ko"
	@echo "  make 03_usb_serial_driver.ko"
	@echo "  make 04_usb_storage_simple.ko"
	@echo ""
	@echo "调试提示："
	@echo "  - 使用 dmesg -w 实时查看内核日志"
	@echo "  - 使用 lsusb -v 查看USB设备详细信息"
	@echo "  - 使用 udevadm monitor 监控USB事件"

# 单个模块编译规则
%.ko: %.c
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

.PHONY: default clean install uninstall show log help

endif
