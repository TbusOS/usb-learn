/*
 * 简化的USB存储设备驱动示例
 * 
 * 这是一个演示性的USB存储驱动，展示了SCSI命令的处理
 * 实际的USB存储驱动要复杂得多
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/usb.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/completion.h>
#include <scsi/scsi.h>
#include <scsi/scsi_cmnd.h>

/* Bulk-Only Transport协议 */
#define US_BULK_CB_SIGN      0x43425355  /* "USBC" */
#define US_BULK_CS_SIGN      0x53425355  /* "USBS" */

/* CBW标志位 */
#define US_BULK_FLAG_IN      (1 << 7)
#define US_BULK_FLAG_OUT     0

/* CSW状态 */
#define US_BULK_STAT_OK      0
#define US_BULK_STAT_FAIL    1
#define US_BULK_STAT_PHASE   2

/* 命令块封装器（CBW） */
struct bulk_cb_wrap {
    __le32 Signature;              /* "USBC" */
    __u32  Tag;                    /* 唯一标签 */
    __le32 DataTransferLength;     /* 数据传输长度 */
    __u8   Flags;                  /* 方向标志 */
    __u8   Lun;                    /* 逻辑单元号 */
    __u8   Length;                 /* CDB长度 */
    __u8   CDB[16];               /* SCSI命令 */
} __packed;

/* 命令状态封装器（CSW） */
struct bulk_cs_wrap {
    __le32 Signature;              /* "USBS" */
    __u32  Tag;                    /* 必须匹配CBW的Tag */
    __le32 Residue;                /* 未传输的数据 */
    __u8   Status;                 /* 状态 */
} __packed;

/* USB存储设备结构 */
struct usb_storage {
    struct usb_device *udev;       /* USB设备 */
    struct usb_interface *interface; /* USB接口 */
    unsigned int send_bulk_pipe;    /* 批量输出管道 */
    unsigned int recv_bulk_pipe;    /* 批量输入管道 */
    
    /* 传输缓冲区 */
    struct bulk_cb_wrap *cbw;      /* CBW缓冲区 */
    struct bulk_cs_wrap *csw;      /* CSW缓冲区 */
    unsigned char *data_buffer;    /* 数据缓冲区 */
    
    /* 同步 */
    struct mutex io_mutex;
    struct completion command_done;
    
    /* 设备信息 */
    char vendor[9];
    char product[17];
    char serial[9];
    unsigned int tag;              /* 命令标签 */
};

/* 发送SCSI命令 */
static int storage_send_command(struct usb_storage *us,
                               unsigned char *cmd, int cmd_len,
                               unsigned int data_len, int direction)
{
    int result;
    int actual_length;
    
    /* 准备CBW */
    memset(us->cbw, 0, sizeof(struct bulk_cb_wrap));
    us->cbw->Signature = cpu_to_le32(US_BULK_CB_SIGN);
    us->cbw->Tag = ++us->tag;
    us->cbw->DataTransferLength = cpu_to_le32(data_len);
    us->cbw->Flags = (direction == DMA_FROM_DEVICE) ? US_BULK_FLAG_IN : US_BULK_FLAG_OUT;
    us->cbw->Lun = 0;
    us->cbw->Length = cmd_len;
    memcpy(us->cbw->CDB, cmd, cmd_len);
    
    /* 发送CBW */
    result = usb_bulk_msg(us->udev, us->send_bulk_pipe,
                         us->cbw, sizeof(struct bulk_cb_wrap),
                         &actual_length, 5000);
    
    if (result) {
        dev_err(&us->interface->dev, "发送CBW失败: %d\n", result);
        return result;
    }
    
    return 0;
}

/* 传输数据 */
static int storage_transfer_data(struct usb_storage *us,
                                unsigned char *buffer,
                                unsigned int length,
                                int direction)
{
    int result;
    int actual_length;
    unsigned int pipe;
    
    if (length == 0)
        return 0;
    
    /* 选择管道 */
    pipe = (direction == DMA_FROM_DEVICE) ? 
           us->recv_bulk_pipe : us->send_bulk_pipe;
    
    /* 传输数据 */
    result = usb_bulk_msg(us->udev, pipe,
                         buffer, length,
                         &actual_length, 10000);
    
    if (result) {
        dev_err(&us->interface->dev, "数据传输失败: %d\n", result);
        return result;
    }
    
    if (actual_length != length) {
        dev_warn(&us->interface->dev,
                "数据传输不完整: %d/%d\n",
                actual_length, length);
    }
    
    return 0;
}

/* 接收状态 */
static int storage_get_status(struct usb_storage *us)
{
    int result;
    int actual_length;
    
    /* 接收CSW */
    result = usb_bulk_msg(us->udev, us->recv_bulk_pipe,
                         us->csw, sizeof(struct bulk_cs_wrap),
                         &actual_length, 5000);
    
    if (result) {
        dev_err(&us->interface->dev, "接收CSW失败: %d\n", result);
        return result;
    }
    
    /* 验证CSW */
    if (actual_length != sizeof(struct bulk_cs_wrap)) {
        dev_err(&us->interface->dev, "CSW长度错误\n");
        return -EIO;
    }
    
    if (le32_to_cpu(us->csw->Signature) != US_BULK_CS_SIGN) {
        dev_err(&us->interface->dev, "CSW签名错误\n");
        return -EIO;
    }
    
    if (us->csw->Tag != us->tag) {
        dev_err(&us->interface->dev, "CSW标签不匹配\n");
        return -EIO;
    }
    
    /* 检查状态 */
    if (us->csw->Status != US_BULK_STAT_OK) {
        dev_err(&us->interface->dev, "命令失败: 状态=%d\n",
                us->csw->Status);
        return -EIO;
    }
    
    return 0;
}

/* 执行SCSI命令 */
static int storage_execute_command(struct usb_storage *us,
                                  unsigned char *cmd, int cmd_len,
                                  void *buffer, unsigned int buf_len,
                                  int direction)
{
    int result;
    
    mutex_lock(&us->io_mutex);
    
    /* 发送命令 */
    result = storage_send_command(us, cmd, cmd_len, buf_len, direction);
    if (result)
        goto out;
    
    /* 传输数据 */
    if (buf_len > 0) {
        result = storage_transfer_data(us, buffer, buf_len, direction);
        if (result)
            goto out;
    }
    
    /* 获取状态 */
    result = storage_get_status(us);
    
out:
    mutex_unlock(&us->io_mutex);
    return result;
}

/* INQUIRY命令 - 获取设备信息 */
static int storage_inquiry(struct usb_storage *us)
{
    unsigned char cmd[6] = {
        INQUIRY,        /* 操作码 */
        0,              /* LUN/EVPD */
        0,              /* 页面码 */
        0,              /* 保留 */
        36,             /* 分配长度 */
        0               /* 控制 */
    };
    unsigned char data[36];
    int result;
    
    result = storage_execute_command(us, cmd, 6,
                                    data, 36, DMA_FROM_DEVICE);
    
    if (result) {
        dev_err(&us->interface->dev, "INQUIRY命令失败\n");
        return result;
    }
    
    /* 解析设备信息 */
    memcpy(us->vendor, &data[8], 8);
    us->vendor[8] = '\0';
    memcpy(us->product, &data[16], 16);
    us->product[16] = '\0';
    
    dev_info(&us->interface->dev,
            "设备: %.8s %.16s\n",
            us->vendor, us->product);
    
    return 0;
}

/* TEST UNIT READY命令 */
static int storage_test_unit_ready(struct usb_storage *us)
{
    unsigned char cmd[6] = {
        TEST_UNIT_READY,
        0, 0, 0, 0, 0
    };
    
    return storage_execute_command(us, cmd, 6, NULL, 0, DMA_NONE);
}

/* READ CAPACITY命令 - 获取容量 */
static int storage_read_capacity(struct usb_storage *us)
{
    unsigned char cmd[10] = {
        READ_CAPACITY,
        0, 0, 0, 0, 0, 0, 0, 0, 0
    };
    unsigned char data[8];
    int result;
    u32 max_lba, block_size;
    
    result = storage_execute_command(us, cmd, 10,
                                    data, 8, DMA_FROM_DEVICE);
    
    if (result) {
        dev_err(&us->interface->dev, "READ_CAPACITY命令失败\n");
        return result;
    }
    
    /* 解析容量信息 */
    max_lba = be32_to_cpup((__be32 *)data);
    block_size = be32_to_cpup((__be32 *)(data + 4));
    
    dev_info(&us->interface->dev,
            "容量: %u块 x %u字节 = %llu MB\n",
            max_lba + 1, block_size,
            ((u64)(max_lba + 1) * block_size) >> 20);
    
    return 0;
}

/* USB探测函数 */
static int storage_probe(struct usb_interface *interface,
                        const struct usb_device_id *id)
{
    struct usb_storage *us;
    struct usb_host_interface *iface_desc;
    struct usb_endpoint_descriptor *endpoint;
    int i;
    int result = -ENOMEM;
    
    /* 分配设备结构 */
    us = kzalloc(sizeof(*us), GFP_KERNEL);
    if (!us)
        return -ENOMEM;
    
    /* 分配缓冲区 */
    us->cbw = kmalloc(sizeof(struct bulk_cb_wrap), GFP_KERNEL);
    us->csw = kmalloc(sizeof(struct bulk_cs_wrap), GFP_KERNEL);
    us->data_buffer = kmalloc(512, GFP_KERNEL);
    
    if (!us->cbw || !us->csw || !us->data_buffer)
        goto error;
    
    /* 初始化 */
    us->udev = usb_get_dev(interface_to_usbdev(interface));
    us->interface = interface;
    mutex_init(&us->io_mutex);
    init_completion(&us->command_done);
    
    /* 查找批量端点 */
    iface_desc = interface->cur_altsetting;
    for (i = 0; i < iface_desc->desc.bNumEndpoints; i++) {
        endpoint = &iface_desc->endpoint[i].desc;
        
        if (usb_endpoint_is_bulk_in(endpoint)) {
            us->recv_bulk_pipe = usb_rcvbulkpipe(us->udev,
                    usb_endpoint_num(endpoint));
        }
        
        if (usb_endpoint_is_bulk_out(endpoint)) {
            us->send_bulk_pipe = usb_sndbulkpipe(us->udev,
                    usb_endpoint_num(endpoint));
        }
    }
    
    if (!us->recv_bulk_pipe || !us->send_bulk_pipe) {
        dev_err(&interface->dev, "找不到批量端点\n");
        result = -ENODEV;
        goto error;
    }
    
    /* 保存设备数据 */
    usb_set_intfdata(interface, us);
    
    /* 初始化设备 */
    result = storage_inquiry(us);
    if (result)
        goto error_deregister;
    
    /* 等待设备就绪 */
    for (i = 0; i < 3; i++) {
        result = storage_test_unit_ready(us);
        if (result == 0)
            break;
        msleep(1000);
    }
    
    if (result) {
        dev_err(&interface->dev, "设备未就绪\n");
        goto error_deregister;
    }
    
    /* 读取容量 */
    storage_read_capacity(us);
    
    dev_info(&interface->dev, "USB存储设备已连接\n");
    
    return 0;
    
error_deregister:
    usb_set_intfdata(interface, NULL);
error:
    if (us) {
        kfree(us->cbw);
        kfree(us->csw);
        kfree(us->data_buffer);
        usb_put_dev(us->udev);
        kfree(us);
    }
    return result;
}

/* USB断开函数 */
static void storage_disconnect(struct usb_interface *interface)
{
    struct usb_storage *us = usb_get_intfdata(interface);
    
    if (!us)
        return;
    
    usb_set_intfdata(interface, NULL);
    
    /* 释放资源 */
    kfree(us->cbw);
    kfree(us->csw);
    kfree(us->data_buffer);
    usb_put_dev(us->udev);
    kfree(us);
    
    dev_info(&interface->dev, "USB存储设备已断开\n");
}

/* USB设备ID表 */
static const struct usb_device_id storage_id_table[] = {
    /* Bulk-Only传输，SCSI透明命令集 */
    { USB_INTERFACE_INFO(USB_CLASS_MASS_STORAGE, 0x06, 0x50) },
    { }  /* 终止符 */
};
MODULE_DEVICE_TABLE(usb, storage_id_table);

/* USB驱动结构 */
static struct usb_driver storage_driver = {
    .name       = "usb_storage_simple",
    .probe      = storage_probe,
    .disconnect = storage_disconnect,
    .id_table   = storage_id_table,
};

module_usb_driver(storage_driver);

MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("简化的USB存储驱动示例");
MODULE_LICENSE("GPL");
