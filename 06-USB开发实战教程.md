# USB驱动开发实战教程

## 🎯 教程目标

本教程将带您一步步完成第一个USB驱动的开发，从零开始到能够正常工作的驱动程序。

## 📋 准备工作

### 开发环境检查
```bash
# 检查内核版本
uname -r

# 检查内核头文件
ls /lib/modules/$(uname -r)/build

# 检查编译工具
gcc --version
make --version

# 检查USB支持
lsmod | grep usb
```

### 创建开发目录
```bash
mkdir ~/usb_driver_workshop
cd ~/usb_driver_workshop
```

## 第一课：Hello USB驱动

### 步骤1：创建最简单的USB驱动

创建文件 `hello_usb.c`：

```c
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/usb.h>

/* 驱动信息 */
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Hello USB Driver Tutorial");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");

/* USB设备ID表 - 先使用一个通用设备作为示例 */
static struct usb_device_id hello_usb_table[] = {
    /* 这里我们先不匹配任何设备，只是为了学习框架 */
    { }  /* 终止符 */
};
MODULE_DEVICE_TABLE(usb, hello_usb_table);

/* 探测函数 - 当设备插入时调用 */
static int hello_usb_probe(struct usb_interface *interface,
                          const struct usb_device_id *id)
{
    struct usb_device *udev = interface_to_usbdev(interface);
    
    pr_info("Hello USB: 设备连接!\n");
    pr_info("  VID: 0x%04x\n", le16_to_cpu(udev->descriptor.idVendor));
    pr_info("  PID: 0x%04x\n", le16_to_cpu(udev->descriptor.idProduct));
    pr_info("  接口号: %d\n", interface->cur_altsetting->desc.bInterfaceNumber);
    
    return 0;  /* 返回0表示成功绑定 */
}

/* 断开函数 - 当设备拔出时调用 */
static void hello_usb_disconnect(struct usb_interface *interface)
{
    pr_info("Hello USB: 设备断开!\n");
}

/* USB驱动结构 */
static struct usb_driver hello_usb_driver = {
    .name       = "hello_usb",           /* 驱动名称 */
    .probe      = hello_usb_probe,       /* 探测函数 */
    .disconnect = hello_usb_disconnect,  /* 断开函数 */
    .id_table   = hello_usb_table,      /* 设备ID表 */
};

/* 模块加载函数 */
static int __init hello_usb_init(void)
{
    int result;
    
    pr_info("Hello USB: 驱动初始化\n");
    
    /* 注册USB驱动 */
    result = usb_register(&hello_usb_driver);
    if (result) {
        pr_err("Hello USB: 注册失败 (%d)\n", result);
        return result;
    }
    
    pr_info("Hello USB: 驱动注册成功\n");
    return 0;
}

/* 模块卸载函数 */
static void __exit hello_usb_exit(void)
{
    pr_info("Hello USB: 驱动卸载\n");
    
    /* 注销USB驱动 */
    usb_deregister(&hello_usb_driver);
}

module_init(hello_usb_init);
module_exit(hello_usb_exit);
```

### 步骤2：创建Makefile

创建文件 `Makefile`：

```makefile
# 模块名称
obj-m := hello_usb.o

# 内核源码路径
KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

# 默认目标
default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

# 清理
clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean

# 安装
install: default
	sudo insmod hello_usb.ko

# 卸载
uninstall:
	sudo rmmod hello_usb

# 查看日志
log:
	dmesg | tail -n 10

.PHONY: default clean install uninstall log
```

### 步骤3：编译和测试

```bash
# 编译驱动
make

# 查看生成的文件
ls -la *.ko

# 加载驱动
sudo insmod hello_usb.ko

# 查看日志
dmesg | tail

# 查看驱动是否加载
lsmod | grep hello_usb

# 卸载驱动
sudo rmmod hello_usb
```

**预期输出**：
```
[12345.678] Hello USB: 驱动初始化
[12345.679] Hello USB: 驱动注册成功
[12346.123] Hello USB: 驱动卸载
```

## 第二课：与真实设备交互

### 步骤1：找到目标设备

```bash
# 插入一个USB设备（如鼠标、键盘、U盘等）
# 查看设备信息
lsusb

# 找到您想要测试的设备，记录VID:PID
# 例如：Bus 001 Device 003: ID 046d:c534 Logitech, Inc.
```

### 步骤2：修改设备ID表

修改 `hello_usb.c` 中的设备ID表：

```c
/* 替换为您的设备VID:PID */
static struct usb_device_id hello_usb_table[] = {
    { USB_DEVICE(0x046d, 0xc534) },  /* 替换为实际的VID:PID */
    { }  /* 终止符 */
};
MODULE_DEVICE_TABLE(usb, hello_usb_table);
```

### 步骤3：增强probe函数

```c
static int hello_usb_probe(struct usb_interface *interface,
                          const struct usb_device_id *id)
{
    struct usb_device *udev = interface_to_usbdev(interface);
    struct usb_host_interface *iface_desc;
    struct usb_endpoint_descriptor *endpoint;
    int i;
    
    pr_info("Hello USB: 设备连接!\n");
    pr_info("  设备信息:\n");
    pr_info("    VID: 0x%04x\n", le16_to_cpu(udev->descriptor.idVendor));
    pr_info("    PID: 0x%04x\n", le16_to_cpu(udev->descriptor.idProduct));
    pr_info("    制造商: %s\n", udev->manufacturer ? udev->manufacturer : "未知");
    pr_info("    产品: %s\n", udev->product ? udev->product : "未知");
    pr_info("    速度: %s\n", 
            (udev->speed == USB_SPEED_HIGH) ? "高速(480Mbps)" :
            (udev->speed == USB_SPEED_FULL) ? "全速(12Mbps)" :
            (udev->speed == USB_SPEED_LOW) ? "低速(1.5Mbps)" : "超高速");
    
    /* 分析接口信息 */
    iface_desc = interface->cur_altsetting;
    pr_info("  接口信息:\n");
    pr_info("    接口号: %d\n", iface_desc->desc.bInterfaceNumber);
    pr_info("    类: 0x%02x\n", iface_desc->desc.bInterfaceClass);
    pr_info("    子类: 0x%02x\n", iface_desc->desc.bInterfaceSubClass);
    pr_info("    协议: 0x%02x\n", iface_desc->desc.bInterfaceProtocol);
    pr_info("    端点数: %d\n", iface_desc->desc.bNumEndpoints);
    
    /* 分析端点信息 */
    for (i = 0; i < iface_desc->desc.bNumEndpoints; i++) {
        endpoint = &iface_desc->endpoint[i].desc;
        pr_info("    端点%d: 地址=0x%02x, 类型=%s, 方向=%s, 最大包=%d\n",
                i,
                endpoint->bEndpointAddress,
                usb_endpoint_type(endpoint) == USB_ENDPOINT_XFER_BULK ? "批量" :
                usb_endpoint_type(endpoint) == USB_ENDPOINT_XFER_INT ? "中断" :
                usb_endpoint_type(endpoint) == USB_ENDPOINT_XFER_ISOC ? "同步" : "控制",
                usb_endpoint_dir_in(endpoint) ? "IN" : "OUT",
                usb_endpoint_maxp(endpoint));
    }
    
    /* 这里我们不真正绑定设备，避免冲突 */
    return -ENODEV;  /* 让其他驱动处理这个设备 */
}
```

### 步骤4：测试真实设备

```bash
# 重新编译
make clean && make

# 加载驱动
sudo insmod hello_usb.ko

# 现在插拔您的USB设备，观察日志
dmesg -w

# 或者如果设备已经插着，可以先拔出再插入
```

**预期输出**：
```
Hello USB: 设备连接!
  设备信息:
    VID: 0x046d
    PID: 0xc534
    制造商: Logitech
    产品: USB Receiver
    速度: 全速(12Mbps)
  接口信息:
    接口号: 0
    类: 0x03
    子类: 0x01
    协议: 0x02
    端点数: 1
    端点0: 地址=0x81, 类型=中断, 方向=IN, 最大包=8
```

## 第三课：实现字符设备接口

### 步骤1：增加字符设备支持

创建新文件 `char_usb.c`：

```c
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/usb.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>

#define USB_VENDOR_ID    0x046d  /* 替换为您的VID */
#define USB_PRODUCT_ID   0xc534  /* 替换为您的PID */

/* 设备私有数据结构 */
struct my_usb_device {
    struct usb_device *udev;        /* USB设备 */
    struct usb_interface *interface; /* USB接口 */
    struct mutex io_mutex;          /* I/O互斥锁 */
    char *buffer;                   /* 数据缓冲区 */
    size_t buffer_size;             /* 缓冲区大小 */
};

/* 文件操作函数 */
static int my_usb_open(struct inode *inode, struct file *file)
{
    struct usb_interface *interface;
    struct my_usb_device *dev;
    int subminor;
    
    subminor = iminor(inode);
    
    /* 通过次设备号找到USB接口 */
    interface = usb_find_interface(&my_usb_driver, subminor);
    if (!interface) {
        pr_err("找不到次设备号 %d 的USB接口\n", subminor);
        return -ENODEV;
    }
    
    /* 获取设备私有数据 */
    dev = usb_get_intfdata(interface);
    if (!dev) {
        return -ENODEV;
    }
    
    /* 保存到文件私有数据 */
    file->private_data = dev;
    
    pr_info("设备已打开\n");
    return 0;
}

static int my_usb_release(struct inode *inode, struct file *file)
{
    pr_info("设备已关闭\n");
    return 0;
}

static ssize_t my_usb_read(struct file *file, char __user *user_buffer,
                          size_t count, loff_t *ppos)
{
    struct my_usb_device *dev = file->private_data;
    char message[] = "Hello from USB device!\n";
    size_t message_len = strlen(message);
    
    if (*ppos >= message_len)
        return 0;  /* EOF */
    
    if (count > message_len - *ppos)
        count = message_len - *ppos;
    
    if (copy_to_user(user_buffer, message + *ppos, count))
        return -EFAULT;
    
    *ppos += count;
    pr_info("读取了 %zu 字节\n", count);
    
    return count;
}

static ssize_t my_usb_write(struct file *file, const char __user *user_buffer,
                           size_t count, loff_t *ppos)
{
    struct my_usb_device *dev = file->private_data;
    char buffer[256];
    size_t len;
    
    len = min(count, sizeof(buffer) - 1);
    
    if (copy_from_user(buffer, user_buffer, len))
        return -EFAULT;
    
    buffer[len] = '\0';
    pr_info("收到用户数据: %s", buffer);
    
    return len;
}

/* 文件操作结构 */
static const struct file_operations my_usb_fops = {
    .owner   = THIS_MODULE,
    .open    = my_usb_open,
    .release = my_usb_release,
    .read    = my_usb_read,
    .write   = my_usb_write,
    .llseek  = default_llseek,
};

/* USB类驱动 */
static struct usb_class_driver my_usb_class = {
    .name       = "myusb%d",
    .fops       = &my_usb_fops,
    .minor_base = USB_DYNAMIC_MINORS,  /* 动态分配次设备号 */
};

/* 前向声明 */
static struct usb_driver my_usb_driver;

/* 探测函数 */
static int my_usb_probe(struct usb_interface *interface,
                       const struct usb_device_id *id)
{
    struct usb_device *udev = interface_to_usbdev(interface);
    struct my_usb_device *dev;
    int retval = 0;
    
    pr_info("USB设备探测开始\n");
    
    /* 分配私有数据结构 */
    dev = kzalloc(sizeof(*dev), GFP_KERNEL);
    if (!dev)
        return -ENOMEM;
    
    /* 初始化设备结构 */
    dev->udev = usb_get_dev(udev);
    dev->interface = interface;
    mutex_init(&dev->io_mutex);
    
    /* 分配缓冲区 */
    dev->buffer_size = 1024;
    dev->buffer = kmalloc(dev->buffer_size, GFP_KERNEL);
    if (!dev->buffer) {
        retval = -ENOMEM;
        goto error;
    }
    
    /* 保存私有数据到接口 */
    usb_set_intfdata(interface, dev);
    
    /* 注册字符设备 */
    retval = usb_register_dev(interface, &my_usb_class);
    if (retval) {
        pr_err("无法注册USB设备\n");
        usb_set_intfdata(interface, NULL);
        goto error;
    }
    
    pr_info("USB设备已注册，次设备号: %d\n", interface->minor);
    return 0;
    
error:
    if (dev) {
        kfree(dev->buffer);
        usb_put_dev(dev->udev);
        kfree(dev);
    }
    return retval;
}

/* 断开函数 */
static void my_usb_disconnect(struct usb_interface *interface)
{
    struct my_usb_device *dev;
    
    pr_info("USB设备断开\n");
    
    dev = usb_get_intfdata(interface);
    usb_set_intfdata(interface, NULL);
    
    /* 注销字符设备 */
    usb_deregister_dev(interface, &my_usb_class);
    
    /* 清理资源 */
    if (dev) {
        mutex_destroy(&dev->io_mutex);
        kfree(dev->buffer);
        usb_put_dev(dev->udev);
        kfree(dev);
    }
}

/* USB设备ID表 */
static struct usb_device_id my_usb_table[] = {
    { USB_DEVICE(USB_VENDOR_ID, USB_PRODUCT_ID) },
    { }
};
MODULE_DEVICE_TABLE(usb, my_usb_table);

/* USB驱动结构 */
static struct usb_driver my_usb_driver = {
    .name       = "my_usb_char",
    .probe      = my_usb_probe,
    .disconnect = my_usb_disconnect,
    .id_table   = my_usb_table,
};

module_usb_driver(my_usb_driver);

MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("USB Character Device Tutorial");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");
```

### 步骤2：编译和测试

```bash
# 编译
make clean
obj-m := char_usb.o
make

# 加载驱动
sudo insmod char_usb.ko

# 查看设备文件
ls -l /dev/myusb*

# 测试设备
echo "Hello USB!" > /dev/myusb0
cat /dev/myusb0

# 查看日志
dmesg | tail -n 20
```

## 第四课：实现真实的USB通信

### 步骤1：添加URB支持

创建 `urb_usb.c`：

```c
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/usb.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/completion.h>

#define USB_VENDOR_ID    0x046d
#define USB_PRODUCT_ID   0xc534

struct my_urb_device {
    struct usb_device *udev;
    struct usb_interface *interface;
    struct urb *int_urb;               /* 中断URB */
    unsigned char *int_buffer;         /* 中断数据缓冲区 */
    size_t int_buffer_size;
    __u8 int_endpoint_addr;            /* 中断端点地址 */
    struct completion int_completion;   /* 完成信号 */
    struct mutex io_mutex;
};

/* URB完成回调函数 */
static void int_callback(struct urb *urb)
{
    struct my_urb_device *dev = urb->context;
    int status = urb->status;
    int i;
    
    switch (status) {
    case 0:
        /* 成功接收数据 */
        pr_info("URB完成，接收到 %d 字节数据:\n", urb->actual_length);
        
        /* 打印接收到的数据（十六进制） */
        for (i = 0; i < urb->actual_length; i++) {
            pr_cont("%02x ", dev->int_buffer[i]);
        }
        pr_cont("\n");
        
        complete(&dev->int_completion);
        break;
        
    case -ECONNRESET:
    case -ENOENT:
    case -ESHUTDOWN:
        /* URB被取消 */
        pr_info("URB被取消\n");
        return;
        
    default:
        /* 其他错误 */
        pr_err("URB错误: %d\n", status);
        break;
    }
    
    /* 重新提交URB（如果需要持续接收） */
    // usb_submit_urb(urb, GFP_ATOMIC);
}

/* 发送中断传输 */
static int send_interrupt_transfer(struct my_urb_device *dev)
{
    int retval;
    
    if (!dev->int_urb)
        return -ENODEV;
    
    /* 初始化完成信号 */
    init_completion(&dev->int_completion);
    
    /* 提交URB */
    retval = usb_submit_urb(dev->int_urb, GFP_KERNEL);
    if (retval) {
        pr_err("提交URB失败: %d\n", retval);
        return retval;
    }
    
    pr_info("URB已提交，等待数据...\n");
    
    /* 等待URB完成（最多等待5秒） */
    if (!wait_for_completion_timeout(&dev->int_completion, 5 * HZ)) {
        pr_warn("URB超时\n");
        usb_kill_urb(dev->int_urb);
        return -ETIMEDOUT;
    }
    
    return 0;
}

static int my_urb_probe(struct usb_interface *interface,
                       const struct usb_device_id *id)
{
    struct usb_device *udev = interface_to_usbdev(interface);
    struct my_urb_device *dev;
    struct usb_host_interface *iface_desc;
    struct usb_endpoint_descriptor *endpoint;
    int retval = 0;
    int i;
    
    pr_info("URB USB设备探测\n");
    
    /* 分配设备结构 */
    dev = kzalloc(sizeof(*dev), GFP_KERNEL);
    if (!dev)
        return -ENOMEM;
    
    dev->udev = usb_get_dev(udev);
    dev->interface = interface;
    mutex_init(&dev->io_mutex);
    init_completion(&dev->int_completion);
    
    /* 查找中断端点 */
    iface_desc = interface->cur_altsetting;
    for (i = 0; i < iface_desc->desc.bNumEndpoints; i++) {
        endpoint = &iface_desc->endpoint[i].desc;
        
        if (usb_endpoint_is_int_in(endpoint)) {
            dev->int_endpoint_addr = endpoint->bEndpointAddress;
            dev->int_buffer_size = usb_endpoint_maxp(endpoint);
            pr_info("找到中断输入端点: 0x%02x, 最大包: %zu\n",
                    dev->int_endpoint_addr, dev->int_buffer_size);
            break;
        }
    }
    
    if (!dev->int_endpoint_addr) {
        pr_err("未找到中断端点\n");
        retval = -ENODEV;
        goto error;
    }
    
    /* 分配中断缓冲区 */
    dev->int_buffer = kmalloc(dev->int_buffer_size, GFP_KERNEL);
    if (!dev->int_buffer) {
        retval = -ENOMEM;
        goto error;
    }
    
    /* 分配URB */
    dev->int_urb = usb_alloc_urb(0, GFP_KERNEL);
    if (!dev->int_urb) {
        retval = -ENOMEM;
        goto error;
    }
    
    /* 初始化中断URB */
    usb_fill_int_urb(dev->int_urb, udev,
                     usb_rcvintpipe(udev, dev->int_endpoint_addr),
                     dev->int_buffer,
                     dev->int_buffer_size,
                     int_callback,
                     dev,
                     endpoint->bInterval);
    
    /* 保存设备数据 */
    usb_set_intfdata(interface, dev);
    
    /* 测试发送一个中断传输 */
    retval = send_interrupt_transfer(dev);
    if (retval && retval != -ETIMEDOUT) {
        pr_err("测试传输失败: %d\n", retval);
        /* 不算致命错误，继续 */
    }
    
    pr_info("URB USB设备初始化完成\n");
    return 0;
    
error:
    if (dev) {
        usb_free_urb(dev->int_urb);
        kfree(dev->int_buffer);
        usb_put_dev(dev->udev);
        kfree(dev);
    }
    return retval;
}

static void my_urb_disconnect(struct usb_interface *interface)
{
    struct my_urb_device *dev;
    
    pr_info("URB USB设备断开\n");
    
    dev = usb_get_intfdata(interface);
    usb_set_intfdata(interface, NULL);
    
    if (dev) {
        /* 取消所有URB */
        usb_kill_urb(dev->int_urb);
        
        /* 释放资源 */
        usb_free_urb(dev->int_urb);
        kfree(dev->int_buffer);
        usb_put_dev(dev->udev);
        kfree(dev);
    }
}

static struct usb_device_id my_urb_table[] = {
    { USB_DEVICE(USB_VENDOR_ID, USB_PRODUCT_ID) },
    { }
};
MODULE_DEVICE_TABLE(usb, my_urb_table);

static struct usb_driver my_urb_driver = {
    .name       = "my_urb_usb",
    .probe      = my_urb_probe,
    .disconnect = my_urb_disconnect,
    .id_table   = my_urb_table,
};

module_usb_driver(my_urb_driver);

MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("USB URB Tutorial");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");
```

### 步骤2：测试URB通信

```bash
# 编译
make clean
obj-m := urb_usb.o
make

# 注意：这个驱动会和系统驱动冲突
# 需要先解绑现有驱动
lsusb -t  # 找到设备位置，如1-1:1.0

# 解绑现有驱动（小心操作！）
echo "1-1:1.0" | sudo tee /sys/bus/usb/drivers/usbhid/unbind

# 加载我们的驱动
sudo insmod urb_usb.ko

# 观察日志
dmesg | tail -n 20

# 重新绑定原驱动
echo "1-1:1.0" | sudo tee /sys/bus/usb/drivers/usbhid/bind

# 卸载我们的驱动
sudo rmmod urb_usb
```

## 调试技巧总结

### 1. 日志调试
```bash
# 实时查看内核日志
dmesg -w

# 过滤USB相关日志
dmesg | grep -i usb

# 清除旧日志
sudo dmesg -C
```

### 2. 设备信息查看
```bash
# 查看USB设备
lsusb -v

# 查看设备树
lsusb -t

# 查看特定设备
lsusb -s 001:003 -v  # 总线:设备号
```

### 3. 调试设置
```bash
# 启用动态调试
echo 'module usbcore +p' | sudo tee /sys/kernel/debug/dynamic_debug/control
echo 'module your_driver +p' | sudo tee /sys/kernel/debug/dynamic_debug/control
```

## 下一步学习建议

1. **深入理解URB**：学习不同类型URB的使用
2. **错误处理**：完善错误处理和恢复机制
3. **性能优化**：使用多个URB提高传输效率
4. **用户接口**：结合sysfs、proc等接口
5. **电源管理**：实现挂起/恢复功能

这个实战教程带您从零开始构建了USB驱动的基本框架。通过这些练习，您应该对USB驱动开发有了直观的理解。接下来可以尝试更复杂的项目，如HID设备驱动或存储设备驱动。
