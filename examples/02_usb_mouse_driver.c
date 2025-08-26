/*
 * USB鼠标驱动示例
 * 
 * 这是一个USB HID鼠标驱动的完整示例
 * 演示了如何处理中断传输和HID协议
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/usb/input.h>
#include <linux/hid.h>

/* 鼠标数据结构 */
struct usb_mouse {
    char name[128];              /* 设备名称 */
    char phys[64];               /* 物理路径 */
    struct usb_device *udev;     /* USB设备 */
    struct input_dev *dev;       /* 输入设备 */
    struct urb *irq;             /* 中断URB */
    signed char *data;           /* 数据缓冲区 */
    dma_addr_t data_dma;         /* DMA地址 */
};

/* USB鼠标中断处理函数 */
static void usb_mouse_irq(struct urb *urb)
{
    struct usb_mouse *mouse = urb->context;
    signed char *data = mouse->data;
    struct input_dev *dev = mouse->dev;
    int status;
    
    switch (urb->status) {
    case 0:             /* 成功 */
        break;
    case -ECONNRESET:   /* URB被取消 */
    case -ENOENT:
    case -ESHUTDOWN:
        return;
    default:            /* 错误 */
        goto resubmit;
    }
    
    /* 解析鼠标数据包
     * data[0]: 按钮状态 (bit 0=左键, bit 1=右键, bit 2=中键)
     * data[1]: X轴移动量 (有符号)
     * data[2]: Y轴移动量 (有符号)
     * data[3]: 滚轮 (有符号)
     */
    
    /* 报告按钮状态 */
    input_report_key(dev, BTN_LEFT,   data[0] & 0x01);
    input_report_key(dev, BTN_RIGHT,  data[0] & 0x02);
    input_report_key(dev, BTN_MIDDLE, data[0] & 0x04);
    
    /* 报告移动量 */
    input_report_rel(dev, REL_X, data[1]);
    input_report_rel(dev, REL_Y, data[2]);
    
    /* 报告滚轮 */
    if (urb->actual_length > 3)
        input_report_rel(dev, REL_WHEEL, data[3]);
    
    /* 同步事件 */
    input_sync(dev);
    
resubmit:
    /* 重新提交URB */
    status = usb_submit_urb(urb, GFP_ATOMIC);
    if (status)
        dev_err(&mouse->udev->dev,
                "无法重新提交URB (%d)\n", status);
}

/* 打开鼠标设备 */
static int usb_mouse_open(struct input_dev *dev)
{
    struct usb_mouse *mouse = input_get_drvdata(dev);
    
    mouse->irq->dev = mouse->udev;
    if (usb_submit_urb(mouse->irq, GFP_KERNEL))
        return -EIO;
    
    return 0;
}

/* 关闭鼠标设备 */
static void usb_mouse_close(struct input_dev *dev)
{
    struct usb_mouse *mouse = input_get_drvdata(dev);
    
    usb_kill_urb(mouse->irq);
}

/* 设备探测函数 */
static int usb_mouse_probe(struct usb_interface *intf,
                          const struct usb_device_id *id)
{
    struct usb_device *dev = interface_to_usbdev(intf);
    struct usb_host_interface *interface;
    struct usb_endpoint_descriptor *endpoint;
    struct usb_mouse *mouse;
    struct input_dev *input_dev;
    int pipe, maxp;
    int error = -ENOMEM;
    
    /* 获取接口描述符 */
    interface = intf->cur_altsetting;
    
    /* 检查端点数量 */
    if (interface->desc.bNumEndpoints != 1)
        return -ENODEV;
    
    /* 获取端点描述符 */
    endpoint = &interface->endpoint[0].desc;
    
    /* 验证是否为中断输入端点 */
    if (!usb_endpoint_is_int_in(endpoint))
        return -ENODEV;
    
    /* 创建管道 */
    pipe = usb_rcvintpipe(dev, endpoint->bEndpointAddress);
    maxp = usb_maxpacket(dev, pipe, usb_pipeout(pipe));
    
    /* 分配鼠标结构 */
    mouse = kzalloc(sizeof(struct usb_mouse), GFP_KERNEL);
    if (!mouse)
        goto fail1;
    
    /* 分配数据缓冲区 */
    mouse->data = usb_alloc_coherent(dev, 8, GFP_ATOMIC, &mouse->data_dma);
    if (!mouse->data)
        goto fail2;
    
    /* 分配URB */
    mouse->irq = usb_alloc_urb(0, GFP_KERNEL);
    if (!mouse->irq)
        goto fail3;
    
    /* 分配输入设备 */
    input_dev = input_allocate_device();
    if (!input_dev)
        goto fail4;
    
    mouse->udev = usb_get_dev(dev);
    mouse->dev = input_dev;
    
    /* 设置设备名称和路径 */
    if (dev->manufacturer)
        strlcpy(mouse->name, dev->manufacturer, sizeof(mouse->name));
    
    if (dev->product) {
        if (dev->manufacturer)
            strlcat(mouse->name, " ", sizeof(mouse->name));
        strlcat(mouse->name, dev->product, sizeof(mouse->name));
    }
    
    if (!strlen(mouse->name))
        snprintf(mouse->name, sizeof(mouse->name),
                 "USB Mouse %04x:%04x",
                 le16_to_cpu(dev->descriptor.idVendor),
                 le16_to_cpu(dev->descriptor.idProduct));
    
    usb_make_path(dev, mouse->phys, sizeof(mouse->phys));
    strlcat(mouse->phys, "/input0", sizeof(mouse->phys));
    
    /* 设置输入设备属性 */
    input_dev->name = mouse->name;
    input_dev->phys = mouse->phys;
    usb_to_input_id(dev, &input_dev->id);
    input_dev->dev.parent = &intf->dev;
    
    /* 设置事件类型 */
    input_dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_REL);
    
    /* 设置按键 */
    input_dev->keybit[BIT_WORD(BTN_MOUSE)] = 
        BIT_MASK(BTN_LEFT) | BIT_MASK(BTN_RIGHT) | BIT_MASK(BTN_MIDDLE);
    
    /* 设置相对坐标 */
    input_dev->relbit[0] = 
        BIT_MASK(REL_X) | BIT_MASK(REL_Y) | BIT_MASK(REL_WHEEL);
    
    /* 设置驱动数据 */
    input_set_drvdata(input_dev, mouse);
    
    /* 设置打开/关闭函数 */
    input_dev->open = usb_mouse_open;
    input_dev->close = usb_mouse_close;
    
    /* 初始化中断URB */
    usb_fill_int_urb(mouse->irq, dev, pipe, mouse->data,
                     (maxp > 8 ? 8 : maxp),
                     usb_mouse_irq, mouse, endpoint->bInterval);
    mouse->irq->transfer_dma = mouse->data_dma;
    mouse->irq->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
    
    /* 注册输入设备 */
    error = input_register_device(mouse->dev);
    if (error)
        goto fail5;
    
    /* 保存设备指针 */
    usb_set_intfdata(intf, mouse);
    
    dev_info(&intf->dev, "USB鼠标已连接: %s\n", mouse->phys);
    
    return 0;
    
fail5:
    input_free_device(input_dev);
fail4:
    usb_free_urb(mouse->irq);
fail3:
    usb_free_coherent(dev, 8, mouse->data, mouse->data_dma);
fail2:
    kfree(mouse);
fail1:
    return error;
}

/* 设备断开函数 */
static void usb_mouse_disconnect(struct usb_interface *intf)
{
    struct usb_mouse *mouse = usb_get_intfdata(intf);
    
    usb_set_intfdata(intf, NULL);
    if (mouse) {
        /* 停止URB */
        usb_kill_urb(mouse->irq);
        
        /* 注销输入设备 */
        input_unregister_device(mouse->dev);
        
        /* 释放URB */
        usb_free_urb(mouse->irq);
        
        /* 释放缓冲区 */
        usb_free_coherent(interface_to_usbdev(intf), 8,
                         mouse->data, mouse->data_dma);
        
        /* 释放设备结构 */
        kfree(mouse);
    }
    
    dev_info(&intf->dev, "USB鼠标已断开\n");
}

/* USB设备ID表 */
static const struct usb_device_id usb_mouse_id_table[] = {
    { USB_INTERFACE_INFO(USB_INTERFACE_CLASS_HID,
                        USB_INTERFACE_SUBCLASS_BOOT,
                        USB_INTERFACE_PROTOCOL_MOUSE) },
    { }  /* 终止符 */
};
MODULE_DEVICE_TABLE(usb, usb_mouse_id_table);

/* USB驱动结构 */
static struct usb_driver usb_mouse_driver = {
    .name       = "usbmouse",
    .probe      = usb_mouse_probe,
    .disconnect = usb_mouse_disconnect,
    .id_table   = usb_mouse_id_table,
};

module_usb_driver(usb_mouse_driver);

MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("USB鼠标驱动");
MODULE_LICENSE("GPL");
