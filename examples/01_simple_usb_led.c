/*
 * 简单USB LED控制驱动
 * 
 * 这是一个入门级的USB驱动示例，用于控制USB LED设备
 * 支持通过写入设备文件来控制LED的开关
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kref.h>
#include <linux/uaccess.h>
#include <linux/usb.h>
#include <linux/mutex.h>

/* 定义厂商ID和产品ID */
#define USB_LED_VENDOR_ID    0x0416
#define USB_LED_PRODUCT_ID   0x5020

/* 定义USB LED的次设备号基址 */
#define USB_LED_MINOR_BASE   192

/* USB LED设备结构 */
struct usb_led {
    struct usb_device       *udev;           /* USB设备 */
    struct usb_interface    *interface;      /* USB接口 */
    struct mutex            io_mutex;        /* 同步I/O操作 */
    struct kref             kref;            /* 引用计数 */
    __u8                    bulk_out_endpointAddr; /* 批量输出端点地址 */
};

/* USB设备ID表 */
static const struct usb_device_id led_table[] = {
    { USB_DEVICE(USB_LED_VENDOR_ID, USB_LED_PRODUCT_ID) },
    { }  /* 终止符 */
};
MODULE_DEVICE_TABLE(usb, led_table);

/* 前向声明 */
static struct usb_driver led_driver;

/* 删除函数 */
static void led_delete(struct kref *kref)
{
    struct usb_led *dev = container_of(kref, struct usb_led, kref);
    
    usb_put_dev(dev->udev);
    kfree(dev);
}

/* 打开设备 */
static int led_open(struct inode *inode, struct file *file)
{
    struct usb_led *dev;
    struct usb_interface *interface;
    int subminor;
    int retval = 0;
    
    pr_info("LED: open()\n");
    
    subminor = iminor(inode);
    
    /* 查找USB接口 */
    interface = usb_find_interface(&led_driver, subminor);
    if (!interface) {
        pr_err("LED: 无法找到次设备号 %d 的设备\n", subminor);
        retval = -ENODEV;
        goto exit;
    }
    
    /* 获取设备数据 */
    dev = usb_get_intfdata(interface);
    if (!dev) {
        retval = -ENODEV;
        goto exit;
    }
    
    /* 防止自动挂起 */
    retval = usb_autopm_get_interface(interface);
    if (retval)
        goto exit;
    
    /* 增加引用计数 */
    kref_get(&dev->kref);
    
    /* 保存到文件私有数据 */
    file->private_data = dev;
    
exit:
    return retval;
}

/* 释放设备 */
static int led_release(struct inode *inode, struct file *file)
{
    struct usb_led *dev;
    
    pr_info("LED: release()\n");
    
    dev = file->private_data;
    if (dev == NULL)
        return -ENODEV;
    
    /* 允许自动挂起 */
    if (dev->interface)
        usb_autopm_put_interface(dev->interface);
    
    /* 减少引用计数 */
    kref_put(&dev->kref, led_delete);
    
    return 0;
}

/* 写入设备 - 控制LED */
static ssize_t led_write(struct file *file, const char __user *user_buffer,
                        size_t count, loff_t *ppos)
{
    struct usb_led *dev;
    char *buf = NULL;
    int retval = 0;
    int actual_length;
    
    dev = file->private_data;
    
    /* 验证写入大小 */
    if (count == 0)
        goto exit;
    if (count > 64)
        count = 64;
    
    /* 分配缓冲区 */
    buf = kmalloc(count, GFP_KERNEL);
    if (!buf) {
        retval = -ENOMEM;
        goto exit;
    }
    
    /* 从用户空间复制数据 */
    if (copy_from_user(buf, user_buffer, count)) {
        retval = -EFAULT;
        goto error;
    }
    
    /* 获取I/O互斥锁 */
    if (!mutex_lock_interruptible(&dev->io_mutex)) {
        if (!dev->interface) {  /* 断开连接 */
            retval = -ENODEV;
            goto error_unlock;
        }
        
        pr_info("LED: 发送命令 0x%02x\n", buf[0]);
        
        /* 发送控制命令到设备 */
        retval = usb_control_msg(dev->udev,
                                usb_sndctrlpipe(dev->udev, 0),
                                0x09,    /* HID Set_Report */
                                0x21,    /* USB_TYPE_CLASS | USB_RECIP_INTERFACE */
                                0x0200,  /* value: Report Type (Output) | Report ID (0) */
                                0x0000,  /* index: Interface 0 */
                                buf,
                                count,
                                5000);   /* 5秒超时 */
        
        if (retval < 0) {
            dev_err(&dev->interface->dev,
                    "发送控制消息失败: %d\n", retval);
            goto error_unlock;
        }
        
        /* 返回写入的字节数 */
        retval = count;
        
error_unlock:
        mutex_unlock(&dev->io_mutex);
    } else {
        retval = -EINTR;
    }
    
error:
    kfree(buf);
    
exit:
    return retval;
}

/* 读取设备状态 */
static ssize_t led_read(struct file *file, char __user *user_buffer,
                       size_t count, loff_t *ppos)
{
    struct usb_led *dev;
    char status[8];
    int retval = 0;
    int actual_length;
    
    dev = file->private_data;
    
    if (count < 8) {
        retval = -EINVAL;
        goto exit;
    }
    
    /* 获取I/O互斥锁 */
    if (!mutex_lock_interruptible(&dev->io_mutex)) {
        if (!dev->interface) {  /* 断开连接 */
            retval = -ENODEV;
            goto error_unlock;
        }
        
        /* 读取设备状态 */
        retval = usb_control_msg(dev->udev,
                                usb_rcvctrlpipe(dev->udev, 0),
                                0x01,    /* HID Get_Report */
                                0xa1,    /* USB_TYPE_CLASS | USB_RECIP_INTERFACE | USB_DIR_IN */
                                0x0100,  /* value: Report Type (Input) | Report ID (0) */
                                0x0000,  /* index: Interface 0 */
                                status,
                                sizeof(status),
                                5000);   /* 5秒超时 */
        
        if (retval < 0) {
            dev_err(&dev->interface->dev,
                    "读取状态失败: %d\n", retval);
            goto error_unlock;
        }
        
        actual_length = retval;
        
        /* 复制到用户空间 */
        if (copy_to_user(user_buffer, status, actual_length)) {
            retval = -EFAULT;
            goto error_unlock;
        }
        
        retval = actual_length;
        
error_unlock:
        mutex_unlock(&dev->io_mutex);
    } else {
        retval = -EINTR;
    }
    
exit:
    return retval;
}

/* 文件操作结构 */
static const struct file_operations led_fops = {
    .owner   = THIS_MODULE,
    .read    = led_read,
    .write   = led_write,
    .open    = led_open,
    .release = led_release,
    .llseek  = noop_llseek,
};

/* USB类驱动 */
static struct usb_class_driver led_class = {
    .name       = "usb/led%d",
    .fops       = &led_fops,
    .minor_base = USB_LED_MINOR_BASE,
};

/* 设备探测函数 */
static int led_probe(struct usb_interface *interface,
                    const struct usb_device_id *id)
{
    struct usb_led *dev = NULL;
    struct usb_host_interface *iface_desc;
    struct usb_endpoint_descriptor *endpoint;
    int i;
    int retval = -ENOMEM;
    
    pr_info("LED: 探测设备 %04x:%04x\n",
            id->idVendor, id->idProduct);
    
    /* 分配设备结构 */
    dev = kzalloc(sizeof(*dev), GFP_KERNEL);
    if (!dev) {
        dev_err(&interface->dev, "内存不足\n");
        goto error;
    }
    
    /* 初始化设备结构 */
    kref_init(&dev->kref);
    mutex_init(&dev->io_mutex);
    
    dev->udev = usb_get_dev(interface_to_usbdev(interface));
    dev->interface = interface;
    
    /* 获取接口描述符 */
    iface_desc = interface->cur_altsetting;
    pr_info("LED: 接口有 %d 个端点\n",
            iface_desc->desc.bNumEndpoints);
    
    /* 查找批量输出端点 */
    for (i = 0; i < iface_desc->desc.bNumEndpoints; ++i) {
        endpoint = &iface_desc->endpoint[i].desc;
        
        if (usb_endpoint_is_bulk_out(endpoint)) {
            dev->bulk_out_endpointAddr = endpoint->bEndpointAddress;
            pr_info("LED: 找到批量输出端点 0x%02x\n",
                    dev->bulk_out_endpointAddr);
            break;
        }
    }
    
    /* 如果没有批量端点，某些LED设备只使用控制传输 */
    
    /* 保存设备指针 */
    usb_set_intfdata(interface, dev);
    
    /* 注册设备 */
    retval = usb_register_dev(interface, &led_class);
    if (retval) {
        dev_err(&interface->dev,
                "无法获取次设备号\n");
        usb_set_intfdata(interface, NULL);
        goto error;
    }
    
    /* 打印设备信息 */
    dev_info(&interface->dev,
            "USB LED设备已连接: 次设备号 %d\n",
            interface->minor);
    
    return 0;
    
error:
    if (dev)
        kref_put(&dev->kref, led_delete);
    return retval;
}

/* 设备断开函数 */
static void led_disconnect(struct usb_interface *interface)
{
    struct usb_led *dev;
    int minor = interface->minor;
    
    pr_info("LED: 断开连接\n");
    
    dev = usb_get_intfdata(interface);
    usb_set_intfdata(interface, NULL);
    
    /* 注销设备 */
    usb_deregister_dev(interface, &led_class);
    
    /* 防止更多I/O操作 */
    mutex_lock(&dev->io_mutex);
    dev->interface = NULL;
    mutex_unlock(&dev->io_mutex);
    
    /* 减少引用计数 */
    kref_put(&dev->kref, led_delete);
    
    dev_info(&interface->dev, "USB LED #%d 已断开\n", minor);
}

/* USB驱动结构 */
static struct usb_driver led_driver = {
    .name       = "usbled",
    .probe      = led_probe,
    .disconnect = led_disconnect,
    .id_table   = led_table,
    .supports_autosuspend = 1,
};

/* 模块初始化 */
static int __init usb_led_init(void)
{
    int retval;
    
    pr_info("USB LED驱动初始化\n");
    
    /* 注册USB驱动 */
    retval = usb_register(&led_driver);
    if (retval)
        pr_err("USB LED驱动注册失败: %d\n", retval);
    
    return retval;
}

/* 模块退出 */
static void __exit usb_led_exit(void)
{
    pr_info("USB LED驱动退出\n");
    
    /* 注销USB驱动 */
    usb_deregister(&led_driver);
}

module_init(usb_led_init);
module_exit(usb_led_exit);

MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("USB LED控制驱动");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");
