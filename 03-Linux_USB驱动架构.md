# Linux USB驱动架构详解

## 1. Linux USB子系统架构

### 1.1 整体架构图
```
    用户空间应用程序
         ↕ (系统调用)
    ==================
    VFS / 字符设备 / 块设备 / 网络层
         ↕
    USB设备驱动层 (USB Device Driver)
         ↕
    USB Core (USB核心层)
         ↕
    USB主机控制器驱动 (HCD)
         ↕
    USB主机控制器硬件 (EHCI/XHCI/OHCI/UHCI)
```

### 1.2 各层职责

#### USB主机控制器驱动（HCD）
- 管理主机控制器硬件
- 处理根集线器
- 管理传输调度
- 处理中断

#### USB Core
- 管理USB设备和驱动
- 处理设备枚举
- 管理URB（USB Request Block）
- 提供USB API接口

#### USB设备驱动
- 实现特定设备功能
- 处理设备特定协议
- 向上层提供服务

## 2. 关键数据结构

### 2.1 usb_driver - USB驱动结构
```c
struct usb_driver {
    const char *name;           // 驱动名称
    
    /* 设备探测和移除 */
    int (*probe)(struct usb_interface *intf,
                 const struct usb_device_id *id);
    void (*disconnect)(struct usb_interface *intf);
    
    /* 电源管理 */
    int (*suspend)(struct usb_interface *intf, pm_message_t message);
    int (*resume)(struct usb_interface *intf);
    int (*reset_resume)(struct usb_interface *intf);
    
    /* 设备ID表 */
    const struct usb_device_id *id_table;
    
    /* 设备文件操作 */
    const struct file_operations *fops;
    
    /* sysfs属性 */
    const struct attribute_group **dev_groups;
    
    /* 驱动标志 */
    unsigned int no_dynamic_id:1;
    unsigned int supports_autosuspend:1;
    unsigned int disable_hub_initiated_lpm:1;
    unsigned int soft_unbind:1;
};

/* 使用示例 */
static struct usb_driver my_usb_driver = {
    .name       = "my_usb_driver",
    .probe      = my_probe,
    .disconnect = my_disconnect,
    .id_table   = my_id_table,
    .suspend    = my_suspend,
    .resume     = my_resume,
};
```

### 2.2 usb_device - USB设备结构
```c
struct usb_device {
    int devnum;                 // 设备号
    char devpath[16];           // 设备路径
    u32 route;                  // 路由字符串
    enum usb_device_state state;// 设备状态
    enum usb_device_speed speed;// 设备速度
    
    /* 设备描述符 */
    struct usb_device_descriptor descriptor;
    struct usb_host_config *config;    // 当前配置
    struct usb_host_config *actconfig; // 活动配置
    
    /* 端点0 */
    struct usb_host_endpoint *ep0;
    struct usb_host_endpoint *ep_in[16];  // IN端点
    struct usb_host_endpoint *ep_out[16]; // OUT端点
    
    /* 父设备和总线 */
    struct usb_device *parent;
    struct usb_bus *bus;
    struct usb_host_endpoint *ep0;
    
    /* 设备信息 */
    char *product;              // 产品字符串
    char *manufacturer;         // 制造商字符串
    char *serial;               // 序列号
    
    /* 引用计数和锁 */
    struct kref kref;
    struct mutex mutex;
};
```

### 2.3 usb_interface - USB接口结构
```c
struct usb_interface {
    /* 当前接口设置 */
    struct usb_host_interface *altsetting;
    struct usb_host_interface *cur_altsetting;
    unsigned num_altsetting;    // 备用设置数量
    
    /* 接口关联 */
    struct usb_interface_assoc_descriptor *intf_assoc;
    
    /* 驱动信息 */
    int minor;                  // 次设备号
    
    /* 设备模型 */
    struct device dev;
    struct device *usb_dev;
    
    /* 电源管理 */
    atomic_t pm_usage_cnt;
    
    /* 状态标志 */
    enum usb_interface_condition condition;
    unsigned sysfs_files_created:1;
    unsigned ep_devs_created:1;
    unsigned unregistering:1;
    unsigned needs_remote_wakeup:1;
    unsigned needs_altsetting0:1;
    unsigned needs_binding:1;
    unsigned resetting_device:1;
    unsigned authorized:1;
};
```

### 2.4 urb - USB请求块
```c
struct urb {
    /* URB私有数据 */
    struct kref kref;           // 引用计数
    spinlock_t lock;            // 自旋锁
    void *hcpriv;               // HCD私有数据
    
    /* 设备和端点 */
    struct usb_device *dev;     // 目标设备
    struct usb_host_endpoint *ep; // 端点
    unsigned int pipe;          // 端点管道
    unsigned int stream_id;     // 流ID (USB 3.0)
    
    /* 传输缓冲区 */
    void *transfer_buffer;      // 数据缓冲区
    dma_addr_t transfer_dma;    // DMA地址
    struct scatterlist *sg;     // 分散聚集列表
    int num_sgs;                // sg数量
    u32 transfer_buffer_length; // 传输长度
    u32 actual_length;          // 实际传输长度
    
    /* 控制传输专用 */
    unsigned char *setup_packet;// Setup包（控制传输）
    dma_addr_t setup_dma;       // Setup包DMA地址
    
    /* 完成处理 */
    usb_complete_t complete;    // 完成回调函数
    void *context;              // 回调上下文
    
    /* 传输标志和状态 */
    int status;                 // URB状态
    unsigned int transfer_flags;// 传输标志
    
    /* 同步/中断传输专用 */
    int start_frame;            // 起始帧
    int number_of_packets;      // 包数量（同步）
    int interval;               // 轮询间隔
    int error_count;            // 错误计数
    
    /* 同步传输包描述符 */
    struct usb_iso_packet_descriptor iso_frame_desc[0];
};

/* URB传输标志 */
#define URB_SHORT_NOT_OK        0x0001 // 短包错误
#define URB_ISO_ASAP            0x0002 // 同步ASAP
#define URB_NO_TRANSFER_DMA_MAP 0x0004 // 不映射DMA
#define URB_ZERO_PACKET         0x0040 // 发送零长度包
#define URB_NO_INTERRUPT        0x0080 // 不要中断
#define URB_FREE_BUFFER         0x0100 // 自动释放缓冲区
#define URB_DIR_IN              0x0200 // 传输方向IN
#define URB_DIR_OUT             0x0000 // 传输方向OUT
```

### 2.5 usb_device_id - 设备ID结构
```c
struct usb_device_id {
    /* 匹配标志 */
    __u16 match_flags;
    
    /* 用于匹配的值 */
    __u16 idVendor;             // 厂商ID
    __u16 idProduct;            // 产品ID
    __u16 bcdDevice_lo;         // 设备版本低
    __u16 bcdDevice_hi;         // 设备版本高
    
    /* 设备类匹配 */
    __u8 bDeviceClass;          // 设备类
    __u8 bDeviceSubClass;       // 设备子类
    __u8 bDeviceProtocol;       // 设备协议
    
    /* 接口类匹配 */
    __u8 bInterfaceClass;       // 接口类
    __u8 bInterfaceSubClass;    // 接口子类
    __u8 bInterfaceProtocol;    // 接口协议
    __u8 bInterfaceNumber;      // 接口号
    
    /* 驱动私有数据 */
    kernel_ulong_t driver_info;
};

/* 匹配标志 */
#define USB_DEVICE_ID_MATCH_VENDOR      0x0001
#define USB_DEVICE_ID_MATCH_PRODUCT     0x0002
#define USB_DEVICE_ID_MATCH_DEV_LO      0x0004
#define USB_DEVICE_ID_MATCH_DEV_HI      0x0008
#define USB_DEVICE_ID_MATCH_DEV_CLASS   0x0010
#define USB_DEVICE_ID_MATCH_DEV_SUBCLASS 0x0020
#define USB_DEVICE_ID_MATCH_DEV_PROTOCOL 0x0040
#define USB_DEVICE_ID_MATCH_INT_CLASS   0x0080
#define USB_DEVICE_ID_MATCH_INT_SUBCLASS 0x0100
#define USB_DEVICE_ID_MATCH_INT_PROTOCOL 0x0200
#define USB_DEVICE_ID_MATCH_INT_NUMBER  0x0400

/* ID表定义宏 */
#define USB_DEVICE(vend, prod) \
    .match_flags = USB_DEVICE_ID_MATCH_VENDOR | \
                   USB_DEVICE_ID_MATCH_PRODUCT, \
    .idVendor = (vend), \
    .idProduct = (prod)

#define USB_DEVICE_VER(vend, prod, lo, hi) \
    .match_flags = USB_DEVICE_ID_MATCH_VENDOR | \
                   USB_DEVICE_ID_MATCH_PRODUCT | \
                   USB_DEVICE_ID_MATCH_DEV_LO | \
                   USB_DEVICE_ID_MATCH_DEV_HI, \
    .idVendor = (vend), \
    .idProduct = (prod), \
    .bcdDevice_lo = (lo), \
    .bcdDevice_hi = (hi)

#define USB_INTERFACE_INFO(cl, sc, pr) \
    .match_flags = USB_DEVICE_ID_MATCH_INT_CLASS | \
                   USB_DEVICE_ID_MATCH_INT_SUBCLASS | \
                   USB_DEVICE_ID_MATCH_INT_PROTOCOL, \
    .bInterfaceClass = (cl), \
    .bInterfaceSubClass = (sc), \
    .bInterfaceProtocol = (pr)

/* 使用示例 */
static const struct usb_device_id my_id_table[] = {
    { USB_DEVICE(0x046d, 0xc534) },           // Logitech设备
    { USB_INTERFACE_INFO(0x03, 0x01, 0x02) }, // HID鼠标
    { }  // 终止符
};
MODULE_DEVICE_TABLE(usb, my_id_table);
```

## 3. USB驱动编程接口

### 3.1 驱动注册与注销
```c
/* 注册USB驱动 */
int usb_register_driver(struct usb_driver *driver, 
                       struct module *owner,
                       const char *mod_name);

/* 简化的注册宏 */
#define usb_register(driver) \
    usb_register_driver(driver, THIS_MODULE, KBUILD_MODNAME)

/* 注销USB驱动 */
void usb_deregister(struct usb_driver *driver);

/* 模块初始化和退出示例 */
static int __init my_usb_init(void)
{
    int ret;
    ret = usb_register(&my_usb_driver);
    if (ret)
        pr_err("Failed to register driver\n");
    return ret;
}

static void __exit my_usb_exit(void)
{
    usb_deregister(&my_usb_driver);
}

module_init(my_usb_init);
module_exit(my_usb_exit);

/* 或使用便捷宏 */
module_usb_driver(my_usb_driver);
```

### 3.2 设备探测和断开
```c
static int my_probe(struct usb_interface *interface,
                   const struct usb_device_id *id)
{
    struct usb_device *udev = interface_to_usbdev(interface);
    struct usb_host_interface *iface_desc;
    struct usb_endpoint_descriptor *endpoint;
    struct my_device *dev;
    int i, retval = -ENOMEM;
    
    /* 分配设备结构 */
    dev = kzalloc(sizeof(*dev), GFP_KERNEL);
    if (!dev)
        return -ENOMEM;
    
    /* 初始化设备 */
    kref_init(&dev->kref);
    mutex_init(&dev->mutex);
    dev->udev = usb_get_dev(udev);
    dev->interface = interface;
    
    /* 解析端点 */
    iface_desc = interface->cur_altsetting;
    for (i = 0; i < iface_desc->desc.bNumEndpoints; ++i) {
        endpoint = &iface_desc->endpoint[i].desc;
        
        if (usb_endpoint_is_bulk_in(endpoint)) {
            dev->bulk_in_size = usb_endpoint_maxp(endpoint);
            dev->bulk_in_endpointAddr = endpoint->bEndpointAddress;
            dev->bulk_in_buffer = kmalloc(dev->bulk_in_size, GFP_KERNEL);
            if (!dev->bulk_in_buffer)
                goto error;
        }
        
        if (usb_endpoint_is_bulk_out(endpoint)) {
            dev->bulk_out_endpointAddr = endpoint->bEndpointAddress;
        }
    }
    
    /* 保存设备指针 */
    usb_set_intfdata(interface, dev);
    
    /* 注册字符设备等 */
    retval = usb_register_dev(interface, &my_class_driver);
    if (retval) {
        usb_set_intfdata(interface, NULL);
        goto error;
    }
    
    dev_info(&interface->dev, "USB device attached\n");
    return 0;
    
error:
    if (dev)
        kref_put(&dev->kref, my_delete);
    return retval;
}

static void my_disconnect(struct usb_interface *interface)
{
    struct my_device *dev;
    
    dev = usb_get_intfdata(interface);
    usb_set_intfdata(interface, NULL);
    
    /* 注销字符设备 */
    usb_deregister_dev(interface, &my_class_driver);
    
    /* 防止更多I/O操作 */
    mutex_lock(&dev->mutex);
    dev->interface = NULL;
    mutex_unlock(&dev->mutex);
    
    /* 减少引用计数 */
    kref_put(&dev->kref, my_delete);
    
    dev_info(&interface->dev, "USB device disconnected\n");
}
```

### 3.3 URB操作
```c
/* 分配URB */
struct urb *usb_alloc_urb(int iso_packets, gfp_t mem_flags);

/* 释放URB */
void usb_free_urb(struct urb *urb);

/* URB引用计数 */
struct urb *usb_get_urb(struct urb *urb);
void usb_put_urb(struct urb *urb);

/* 初始化批量URB */
void usb_fill_bulk_urb(struct urb *urb,
                       struct usb_device *dev,
                       unsigned int pipe,
                       void *transfer_buffer,
                       int buffer_length,
                       usb_complete_t complete_fn,
                       void *context);

/* 初始化控制URB */
void usb_fill_control_urb(struct urb *urb,
                         struct usb_device *dev,
                         unsigned int pipe,
                         unsigned char *setup_packet,
                         void *transfer_buffer,
                         int buffer_length,
                         usb_complete_t complete_fn,
                         void *context);

/* 初始化中断URB */
void usb_fill_int_urb(struct urb *urb,
                      struct usb_device *dev,
                      unsigned int pipe,
                      void *transfer_buffer,
                      int buffer_length,
                      usb_complete_t complete_fn,
                      void *context,
                      int interval);

/* 提交URB */
int usb_submit_urb(struct urb *urb, gfp_t mem_flags);

/* 取消URB */
int usb_unlink_urb(struct urb *urb);  // 异步取消
void usb_kill_urb(struct urb *urb);   // 同步取消

/* URB完成回调示例 */
static void my_urb_complete(struct urb *urb)
{
    struct my_device *dev = urb->context;
    int status = urb->status;
    
    /* 检查状态 */
    if (status) {
        if (!(status == -ENOENT ||
              status == -ECONNRESET ||
              status == -ESHUTDOWN))
            dev_err(&dev->interface->dev,
                    "URB error %d\n", status);
        return;
    }
    
    /* 处理数据 */
    dev->data_len = urb->actual_length;
    memcpy(dev->data, urb->transfer_buffer, dev->data_len);
    
    /* 唤醒等待的进程 */
    complete(&dev->completion);
}

/* 批量传输示例 */
static int my_bulk_transfer(struct my_device *dev, 
                           void *data, int len, int *actual)
{
    struct urb *urb;
    int retval;
    
    /* 分配URB */
    urb = usb_alloc_urb(0, GFP_KERNEL);
    if (!urb)
        return -ENOMEM;
    
    /* 初始化URB */
    usb_fill_bulk_urb(urb, dev->udev,
                     usb_sndbulkpipe(dev->udev, dev->bulk_out_addr),
                     data, len,
                     my_urb_complete, dev);
    
    /* 提交URB */
    retval = usb_submit_urb(urb, GFP_KERNEL);
    if (retval) {
        dev_err(&dev->interface->dev,
                "Failed to submit URB: %d\n", retval);
        goto out;
    }
    
    /* 等待完成 */
    wait_for_completion(&dev->completion);
    *actual = dev->data_len;
    
out:
    usb_free_urb(urb);
    return retval;
}
```

### 3.4 同步传输函数
```c
/* 控制传输 */
int usb_control_msg(struct usb_device *dev,
                   unsigned int pipe,
                   __u8 request,
                   __u8 requesttype,
                   __u16 value,
                   __u16 index,
                   void *data,
                   __u16 size,
                   int timeout);

/* 批量传输 */
int usb_bulk_msg(struct usb_device *dev,
                unsigned int pipe,
                void *data,
                int len,
                int *actual_length,
                int timeout);

/* 中断传输 */
int usb_interrupt_msg(struct usb_device *dev,
                     unsigned int pipe,
                     void *data,
                     int len,
                     int *actual_length,
                     int timeout);

/* 使用示例 */
static int my_read_register(struct usb_device *udev, 
                           u16 reg, u8 *value)
{
    int ret;
    u8 *buf;
    
    buf = kmalloc(1, GFP_KERNEL);
    if (!buf)
        return -ENOMEM;
    
    ret = usb_control_msg(udev,
                         usb_rcvctrlpipe(udev, 0),
                         0x01,  // 请求码
                         USB_TYPE_VENDOR | USB_DIR_IN,
                         reg,   // wValue
                         0,     // wIndex
                         buf,
                         1,
                         5000); // 5秒超时
    
    if (ret == 1) {
        *value = buf[0];
        ret = 0;
    } else if (ret >= 0) {
        ret = -EIO;
    }
    
    kfree(buf);
    return ret;
}
```

### 3.5 管道操作
```c
/* 创建管道 */
#define usb_sndctrlpipe(dev, endpoint) \
    ((PIPE_CONTROL << 30) | __create_pipe(dev, endpoint))

#define usb_rcvctrlpipe(dev, endpoint) \
    ((PIPE_CONTROL << 30) | __create_pipe(dev, endpoint) | USB_DIR_IN)

#define usb_sndbulkpipe(dev, endpoint) \
    ((PIPE_BULK << 30) | __create_pipe(dev, endpoint))

#define usb_rcvbulkpipe(dev, endpoint) \
    ((PIPE_BULK << 30) | __create_pipe(dev, endpoint) | USB_DIR_IN)

#define usb_sndintpipe(dev, endpoint) \
    ((PIPE_INTERRUPT << 30) | __create_pipe(dev, endpoint))

#define usb_rcvintpipe(dev, endpoint) \
    ((PIPE_INTERRUPT << 30) | __create_pipe(dev, endpoint) | USB_DIR_IN)

#define usb_sndisocpipe(dev, endpoint) \
    ((PIPE_ISOCHRONOUS << 30) | __create_pipe(dev, endpoint))

#define usb_rcvisocpipe(dev, endpoint) \
    ((PIPE_ISOCHRONOUS << 30) | __create_pipe(dev, endpoint) | USB_DIR_IN)
```

## 4. USB字符设备驱动

### 4.1 USB类驱动结构
```c
static struct usb_class_driver my_class_driver = {
    .name =       "usb/my_device%d",
    .fops =       &my_fops,
    .minor_base = USB_MY_MINOR_BASE,
};

/* 文件操作结构 */
static const struct file_operations my_fops = {
    .owner =        THIS_MODULE,
    .read =         my_read,
    .write =        my_write,
    .open =         my_open,
    .release =      my_release,
    .flush =        my_flush,
    .llseek =       noop_llseek,
};

/* 注册/注销USB设备节点 */
int usb_register_dev(struct usb_interface *intf,
                    struct usb_class_driver *class_driver);
                    
void usb_deregister_dev(struct usb_interface *intf,
                       struct usb_class_driver *class_driver);
```

### 4.2 文件操作实现
```c
static int my_open(struct inode *inode, struct file *file)
{
    struct my_device *dev;
    struct usb_interface *interface;
    int subminor;
    int retval = 0;
    
    subminor = iminor(inode);
    
    /* 获取接口 */
    interface = usb_find_interface(&my_usb_driver, subminor);
    if (!interface) {
        pr_err("Can't find device for minor %d\n", subminor);
        return -ENODEV;
    }
    
    /* 获取设备数据 */
    dev = usb_get_intfdata(interface);
    if (!dev)
        return -ENODEV;
    
    /* 增加引用计数 */
    retval = usb_autopm_get_interface(interface);
    if (retval)
        return retval;
    
    /* 增加使用计数 */
    kref_get(&dev->kref);
    
    /* 保存到文件私有数据 */
    file->private_data = dev;
    
    return 0;
}

static ssize_t my_read(struct file *file, char __user *buffer,
                      size_t count, loff_t *ppos)
{
    struct my_device *dev = file->private_data;
    int retval;
    int actual_length;
    
    if (!dev->interface)
        return -ENODEV;
    
    /* 执行USB批量读 */
    retval = usb_bulk_msg(dev->udev,
                         usb_rcvbulkpipe(dev->udev, dev->bulk_in_addr),
                         dev->bulk_in_buffer,
                         min(dev->bulk_in_size, count),
                         &actual_length,
                         10000);
    
    if (retval) {
        dev_err(&dev->interface->dev,
                "Bulk read failed: %d\n", retval);
        return retval;
    }
    
    /* 复制到用户空间 */
    if (copy_to_user(buffer, dev->bulk_in_buffer, actual_length))
        return -EFAULT;
    
    return actual_length;
}

static ssize_t my_write(struct file *file, const char __user *buffer,
                       size_t count, loff_t *ppos)
{
    struct my_device *dev = file->private_data;
    int retval;
    int actual_length;
    char *buf;
    
    if (!dev->interface)
        return -ENODEV;
    
    /* 分配缓冲区 */
    buf = kmalloc(count, GFP_KERNEL);
    if (!buf)
        return -ENOMEM;
    
    /* 从用户空间复制 */
    if (copy_from_user(buf, buffer, count)) {
        kfree(buf);
        return -EFAULT;
    }
    
    /* 执行USB批量写 */
    retval = usb_bulk_msg(dev->udev,
                         usb_sndbulkpipe(dev->udev, dev->bulk_out_addr),
                         buf,
                         count,
                         &actual_length,
                         10000);
    
    kfree(buf);
    
    if (retval) {
        dev_err(&dev->interface->dev,
                "Bulk write failed: %d\n", retval);
        return retval;
    }
    
    return actual_length;
}

static int my_release(struct inode *inode, struct file *file)
{
    struct my_device *dev = file->private_data;
    
    if (!dev)
        return -ENODEV;
    
    /* 允许自动挂起 */
    usb_autopm_put_interface(dev->interface);
    
    /* 减少引用计数 */
    kref_put(&dev->kref, my_delete);
    
    return 0;
}
```

## 5. USB电源管理

### 5.1 运行时电源管理
```c
/* 获取接口（阻止自动挂起）*/
int usb_autopm_get_interface(struct usb_interface *intf);
int usb_autopm_get_interface_async(struct usb_interface *intf);

/* 释放接口（允许自动挂起）*/
void usb_autopm_put_interface(struct usb_interface *intf);
void usb_autopm_put_interface_async(struct usb_interface *intf);

/* 设置自动挂起延迟 */
static int my_probe(struct usb_interface *interface,
                   const struct usb_device_id *id)
{
    /* ... */
    
    /* 设置自动挂起延迟为2秒 */
    pm_runtime_set_autosuspend_delay(&interface->dev, 2000);
    usb_enable_autosuspend(interface_to_usbdev(interface));
    
    return 0;
}
```

### 5.2 系统挂起/恢复
```c
static int my_suspend(struct usb_interface *intf, pm_message_t message)
{
    struct my_device *dev = usb_get_intfdata(intf);
    
    if (!dev)
        return 0;
    
    /* 停止正在进行的传输 */
    usb_kill_anchored_urbs(&dev->submitted);
    
    return 0;
}

static int my_resume(struct usb_interface *intf)
{
    struct my_device *dev = usb_get_intfdata(intf);
    int retval = 0;
    
    if (!dev)
        return 0;
    
    /* 重新初始化硬件 */
    retval = my_init_hardware(dev);
    if (retval)
        dev_err(&intf->dev, "Resume failed\n");
    
    return retval;
}

static int my_reset_resume(struct usb_interface *intf)
{
    struct my_device *dev = usb_get_intfdata(intf);
    
    /* 设备已复位，需要完全重新初始化 */
    return my_init_hardware(dev);
}
```

### 5.3 远程唤醒
```c
/* 启用远程唤醒 */
static int my_probe(struct usb_interface *interface,
                   const struct usb_device_id *id)
{
    /* ... */
    
    /* 标记需要远程唤醒 */
    interface->needs_remote_wakeup = 1;
    
    /* 或在运行时设置 */
    device_set_wakeup_enable(&interface->dev, 1);
    
    return 0;
}
```

## 6. USB错误处理

### 6.1 URB错误码
```c
/* URB状态码 */
#define -EINPROGRESS    /* URB仍在处理 */
#define -ENOENT         /* URB被unlink */
#define -ECONNRESET     /* URB被unlink */
#define -ESHUTDOWN      /* 设备已断开 */
#define -EPIPE          /* 端点停止 */
#define -ECOMM          /* 传输期间错误 */
#define -ENOSR          /* 缓冲区不足 */
#define -EOVERFLOW      /* babble错误 */
#define -EREMOTEIO      /* 短读且URB_SHORT_NOT_OK */
#define -ENODEV         /* 设备已移除 */
#define -EXDEV          /* 同步传输部分错误 */
#define -ETIMEDOUT      /* 传输超时 */

/* 错误处理示例 */
static void my_urb_complete(struct urb *urb)
{
    int status = urb->status;
    
    switch (status) {
    case 0:
        /* 成功 */
        break;
    case -ECONNRESET:
    case -ENOENT:
    case -ESHUTDOWN:
        /* URB被取消 */
        return;
    case -EPIPE:
        /* 端点停止，需要清除 */
        usb_clear_halt(urb->dev, urb->pipe);
        break;
    default:
        /* 其他错误 */
        dev_err(&urb->dev->dev, "URB错误: %d\n", status);
        break;
    }
}
```

### 6.2 错误恢复
```c
/* 清除端点停止状态 */
int usb_clear_halt(struct usb_device *dev, int pipe);

/* 复位设备 */
int usb_reset_device(struct usb_device *udev);
int usb_reset_configuration(struct usb_device *dev);
int usb_reset_endpoint(struct usb_device *dev, unsigned int epaddr);

/* 错误恢复示例 */
static int my_error_recovery(struct my_device *dev)
{
    int retval;
    
    /* 尝试清除停止状态 */
    retval = usb_clear_halt(dev->udev,
                           usb_sndbulkpipe(dev->udev, 
                                         dev->bulk_out_addr));
    if (retval) {
        /* 清除失败，尝试复位设备 */
        retval = usb_reset_device(dev->udev);
        if (retval) {
            dev_err(&dev->interface->dev,
                    "设备复位失败\n");
            return retval;
        }
        
        /* 重新初始化 */
        retval = my_init_hardware(dev);
    }
    
    return retval;
}
```

## 7. USB调试技术

### 7.1 内核调试宏
```c
/* USB调试宏 */
#define dev_dbg(dev, format, ...)  /* 调试信息 */
#define dev_info(dev, format, ...) /* 一般信息 */
#define dev_warn(dev, format, ...) /* 警告信息 */
#define dev_err(dev, format, ...)  /* 错误信息 */

/* 使用示例 */
static int my_probe(struct usb_interface *interface,
                   const struct usb_device_id *id)
{
    struct usb_device *udev = interface_to_usbdev(interface);
    
    dev_info(&interface->dev, "探测设备 %04x:%04x\n",
             le16_to_cpu(udev->descriptor.idVendor),
             le16_to_cpu(udev->descriptor.idProduct));
    
    dev_dbg(&interface->dev, "接口号: %d\n",
            interface->cur_altsetting->desc.bInterfaceNumber);
    
    return 0;
}
```

### 7.2 usbmon调试
```bash
# 加载usbmon模块
sudo modprobe usbmon

# 查看USB总线
ls /sys/kernel/debug/usb/usbmon/

# 捕获总线0（所有总线）
sudo cat /sys/kernel/debug/usb/usbmon/0u

# 使用tcpdump捕获
sudo tcpdump -i usbmon0 -w usb.pcap

# 使用wireshark分析
wireshark usb.pcap
```

### 7.3 动态调试
```bash
# 启用USB核心动态调试
echo 'module usbcore +p' > /sys/kernel/debug/dynamic_debug/control

# 启用特定驱动调试
echo 'module my_driver +p' > /sys/kernel/debug/dynamic_debug/control

# 启用特定文件调试
echo 'file drivers/usb/core/hub.c +p' > /sys/kernel/debug/dynamic_debug/control

# 启用特定函数调试
echo 'func usb_submit_urb +p' > /sys/kernel/debug/dynamic_debug/control
```

## 8. USB Gadget驱动

### 8.1 Gadget驱动架构
```
    Gadget Function Driver
           ↕
    Gadget Composite Layer
           ↕
    Gadget UDC Driver
           ↕
    USB Device Controller Hardware
```

### 8.2 Gadget驱动示例
```c
#include <linux/usb/gadget.h>
#include <linux/usb/composite.h>

/* Gadget功能驱动 */
struct f_my_gadget {
    struct usb_function function;
    struct usb_ep *in_ep;
    struct usb_ep *out_ep;
};

static int my_gadget_bind(struct usb_configuration *c,
                         struct usb_function *f)
{
    struct f_my_gadget *my_gadget = func_to_my_gadget(f);
    struct usb_composite_dev *cdev = c->cdev;
    int ret;
    
    /* 分配端点 */
    my_gadget->in_ep = usb_ep_autoconfig(cdev->gadget, 
                                        &fs_in_desc);
    if (!my_gadget->in_ep)
        return -ENODEV;
    
    my_gadget->out_ep = usb_ep_autoconfig(cdev->gadget,
                                         &fs_out_desc);
    if (!my_gadget->out_ep)
        return -ENODEV;
    
    return 0;
}

static void my_gadget_unbind(struct usb_configuration *c,
                            struct usb_function *f)
{
    /* 清理资源 */
}

static int my_gadget_setup(struct usb_function *f,
                          const struct usb_ctrlrequest *ctrl)
{
    /* 处理类特定请求 */
    return -EOPNOTSUPP;
}

static int my_gadget_set_alt(struct usb_function *f,
                            unsigned intf, unsigned alt)
{
    struct f_my_gadget *my_gadget = func_to_my_gadget(f);
    
    /* 启用端点 */
    return usb_ep_enable(my_gadget->in_ep);
}

static void my_gadget_disable(struct usb_function *f)
{
    struct f_my_gadget *my_gadget = func_to_my_gadget(f);
    
    /* 禁用端点 */
    usb_ep_disable(my_gadget->in_ep);
    usb_ep_disable(my_gadget->out_ep);
}
```

## 总结

Linux USB驱动架构提供了完整的USB支持：

1. **分层架构**：清晰的层次划分，各司其职
2. **URB机制**：异步传输的核心
3. **丰富的API**：简化驱动开发
4. **电源管理**：完善的PM支持
5. **错误处理**：健壮的错误恢复机制
6. **调试工具**：强大的调试能力

作为驱动开发者，需要重点掌握：
- USB驱动的probe/disconnect流程
- URB的创建、提交和处理
- 各种传输类型的使用方法
- 错误处理和恢复机制
- 调试技术和工具使用

通过理解这些概念和掌握相关API，您就能开发出高质量的USB设备驱动。
