# USB协议规范详解

## 1. USB协议分层模型

### 1.1 协议层次结构
```
应用层
   ↕
功能层（Function Layer）
   ↕  
USB设备层（USB Device Layer）
   ↕
USB逻辑层（USB Logical Layer）
   ↕
USB总线接口层（USB Bus Interface Layer）
   ↕
物理层（Physical Layer）
```

### 1.2 各层职责
- **功能层**：实现设备具体功能
- **设备层**：处理USB标准请求，管理设备状态
- **逻辑层**：管理数据流和端点
- **总线接口层**：处理总线协议、包传输
- **物理层**：电气信号传输

## 2. USB数据包格式

### 2.1 包类型（Packet Types）

#### 令牌包（Token Packets）
```
+------+------+--------+-------+-------+
| SYNC | PID  | ADDR   | ENDP  | CRC5  |
+------+------+--------+-------+-------+
  8位    8位    7位      4位     5位
```

令牌包类型：
- **IN**（0x69）：请求设备发送数据
- **OUT**（0xE1）：主机将发送数据
- **SETUP**（0x2D）：开始控制传输
- **SOF**（0xA5）：帧起始标记

#### 数据包（Data Packets）
```
+------+------+-----------------+--------+
| SYNC | PID  | DATA (0-1024B)  | CRC16  |
+------+------+-----------------+--------+
  8位    8位      0-8192位         16位
```

数据包类型：
- **DATA0**（0xC3）：数据包，PID=0
- **DATA1**（0x4B）：数据包，PID=1
- **DATA2**（0x87）：高速同步传输用
- **MDATA**（0x0F）：高速分割传输用

#### 握手包（Handshake Packets）
```
+------+------+
| SYNC | PID  |
+------+------+
  8位    8位
```

握手包类型：
- **ACK**（0xD2）：成功接收
- **NAK**（0x5A）：设备忙或无数据
- **STALL**（0x1E）：端点停止或不支持请求
- **NYET**（0x96）：尚未准备好（仅高速）

### 2.2 事务类型（Transaction Types）

#### IN事务
```
主机 → 设备：IN令牌包
设备 → 主机：DATA0/1包 或 NAK/STALL
主机 → 设备：ACK（如果收到数据）
```

#### OUT事务
```
主机 → 设备：OUT令牌包
主机 → 设备：DATA0/1包
设备 → 主机：ACK/NAK/STALL
```

#### SETUP事务
```
主机 → 设备：SETUP令牌包
主机 → 设备：DATA0包（8字节）
设备 → 主机：ACK
```

## 3. USB标准请求

### 3.1 Setup包格式
```c
struct usb_ctrlrequest {
    __u8 bRequestType;  // 请求类型
    __u8 bRequest;      // 请求码
    __le16 wValue;      // 值参数
    __le16 wIndex;      // 索引参数
    __le16 wLength;     // 数据长度
} __attribute__ ((packed));
```

### 3.2 bRequestType字段
```
位7：数据传输方向
    0 = 主机到设备（OUT）
    1 = 设备到主机（IN）
    
位6-5：请求类型
    00 = 标准请求
    01 = 类请求
    10 = 厂商请求
    11 = 保留
    
位4-0：接收方
    00000 = 设备
    00001 = 接口
    00010 = 端点
    00011 = 其他
```

### 3.3 标准设备请求
| bRequest | 值 | 名称 | 说明 |
|----------|-----|------|------|
| 0x00 | GET_STATUS | 获取状态 | 获取设备/接口/端点状态 |
| 0x01 | CLEAR_FEATURE | 清除特性 | 清除或禁用特定特性 |
| 0x03 | SET_FEATURE | 设置特性 | 设置或启用特定特性 |
| 0x05 | SET_ADDRESS | 设置地址 | 分配设备地址 |
| 0x06 | GET_DESCRIPTOR | 获取描述符 | 获取指定描述符 |
| 0x07 | SET_DESCRIPTOR | 设置描述符 | 设置描述符（可选） |
| 0x08 | GET_CONFIGURATION | 获取配置 | 获取当前配置值 |
| 0x09 | SET_CONFIGURATION | 设置配置 | 设置设备配置 |
| 0x0A | GET_INTERFACE | 获取接口 | 获取接口设置 |
| 0x0B | SET_INTERFACE | 设置接口 | 选择接口设置 |
| 0x0C | SYNCH_FRAME | 同步帧 | 同步端点帧号 |

### 3.4 请求示例

#### 获取设备描述符
```c
struct usb_ctrlrequest get_device_desc = {
    .bRequestType = 0x80,  // IN, 标准, 设备
    .bRequest = 0x06,      // GET_DESCRIPTOR
    .wValue = 0x0100,      // 描述符类型(DEVICE) | 索引(0)
    .wIndex = 0x0000,      // 0
    .wLength = 18          // 设备描述符长度
};
```

#### 设置设备地址
```c
struct usb_ctrlrequest set_address = {
    .bRequestType = 0x00,  // OUT, 标准, 设备
    .bRequest = 0x05,      // SET_ADDRESS
    .wValue = 0x0003,      // 地址3
    .wIndex = 0x0000,      // 0
    .wLength = 0           // 无数据阶段
};
```

## 4. USB描述符详解

### 4.1 设备描述符
```c
/* USB设备描述符结构 - 18字节 */
struct usb_device_descriptor {
    __u8  bLength;              // 0x12 (18字节)
    __u8  bDescriptorType;      // 0x01 (DEVICE)
    __le16 bcdUSB;              // USB版本 (BCD格式)
    __u8  bDeviceClass;         // 类代码
    __u8  bDeviceSubClass;      // 子类代码
    __u8  bDeviceProtocol;      // 协议代码
    __u8  bMaxPacketSize0;      // 端点0最大包大小 (8,16,32,64)
    __le16 idVendor;            // 厂商ID
    __le16 idProduct;           // 产品ID
    __le16 bcdDevice;           // 设备版本 (BCD格式)
    __u8  iManufacturer;        // 厂商字符串索引
    __u8  iProduct;             // 产品字符串索引
    __u8  iSerialNumber;        // 序列号字符串索引
    __u8  bNumConfigurations;   // 配置数量
};

/* 示例：USB键盘设备描述符 */
struct usb_device_descriptor keyboard_device = {
    .bLength = 18,
    .bDescriptorType = 0x01,
    .bcdUSB = 0x0200,          // USB 2.0
    .bDeviceClass = 0x00,      // 在接口描述符中定义
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,
    .bMaxPacketSize0 = 64,
    .idVendor = 0x046d,        // Logitech
    .idProduct = 0xc31c,
    .bcdDevice = 0x0110,       // 版本1.10
    .iManufacturer = 1,
    .iProduct = 2,
    .iSerialNumber = 0,
    .bNumConfigurations = 1
};
```

### 4.2 配置描述符
```c
/* USB配置描述符结构 - 9字节 */
struct usb_config_descriptor {
    __u8  bLength;              // 0x09 (9字节)
    __u8  bDescriptorType;      // 0x02 (CONFIGURATION)
    __le16 wTotalLength;        // 配置信息总长度
    __u8  bNumInterfaces;       // 接口数量
    __u8  bConfigurationValue;  // 配置值
    __u8  iConfiguration;       // 配置字符串索引
    __u8  bmAttributes;         // 配置特性
    __u8  bMaxPower;            // 最大电流 (2mA为单位)
};

/* bmAttributes位定义 */
#define USB_CONFIG_ATT_ONE           (1 << 7)  // 必须为1
#define USB_CONFIG_ATT_SELFPOWER     (1 << 6)  // 自供电
#define USB_CONFIG_ATT_WAKEUP        (1 << 5)  // 支持远程唤醒
#define USB_CONFIG_ATT_BATTERY       (1 << 4)  // 电池供电
```

### 4.3 接口描述符
```c
/* USB接口描述符结构 - 9字节 */
struct usb_interface_descriptor {
    __u8  bLength;              // 0x09 (9字节)
    __u8  bDescriptorType;      // 0x04 (INTERFACE)
    __u8  bInterfaceNumber;     // 接口编号
    __u8  bAlternateSetting;    // 备用设置值
    __u8  bNumEndpoints;        // 端点数量（不含端点0）
    __u8  bInterfaceClass;      // 接口类代码
    __u8  bInterfaceSubClass;   // 接口子类代码
    __u8  bInterfaceProtocol;   // 接口协议代码
    __u8  iInterface;           // 接口字符串索引
};

/* HID键盘接口示例 */
struct usb_interface_descriptor hid_keyboard = {
    .bLength = 9,
    .bDescriptorType = 0x04,
    .bInterfaceNumber = 0,
    .bAlternateSetting = 0,
    .bNumEndpoints = 1,        // 一个中断端点
    .bInterfaceClass = 0x03,   // HID类
    .bInterfaceSubClass = 0x01,// Boot Interface
    .bInterfaceProtocol = 0x01,// 键盘
    .iInterface = 0
};
```

### 4.4 端点描述符
```c
/* USB端点描述符结构 - 7字节 */
struct usb_endpoint_descriptor {
    __u8  bLength;              // 0x07 (7字节)
    __u8  bDescriptorType;      // 0x05 (ENDPOINT)
    __u8  bEndpointAddress;     // 端点地址
    __u8  bmAttributes;         // 端点属性
    __le16 wMaxPacketSize;      // 最大包大小
    __u8  bInterval;            // 轮询间隔
};

/* bEndpointAddress位定义 */
#define USB_ENDPOINT_NUMBER_MASK    0x0f  // 端点号掩码
#define USB_ENDPOINT_DIR_MASK       0x80  // 方向掩码
#define USB_DIR_OUT                 0     // 到设备
#define USB_DIR_IN                  0x80  // 到主机

/* bmAttributes位定义 */
#define USB_ENDPOINT_XFERTYPE_MASK  0x03  // 传输类型掩码
#define USB_ENDPOINT_XFER_CONTROL   0     // 控制传输
#define USB_ENDPOINT_XFER_ISOC      1     // 同步传输
#define USB_ENDPOINT_XFER_BULK      2     // 批量传输
#define USB_ENDPOINT_XFER_INT       3     // 中断传输

/* 同步端点的同步类型 */
#define USB_ENDPOINT_SYNC_NONE      (0 << 2)  // 无同步
#define USB_ENDPOINT_SYNC_ASYNC     (1 << 2)  // 异步
#define USB_ENDPOINT_SYNC_ADAPTIVE  (2 << 2)  // 自适应
#define USB_ENDPOINT_SYNC_SYNC      (3 << 2)  // 同步

/* 中断端点示例 */
struct usb_endpoint_descriptor interrupt_ep = {
    .bLength = 7,
    .bDescriptorType = 0x05,
    .bEndpointAddress = 0x81,  // EP1 IN
    .bmAttributes = 0x03,      // 中断传输
    .wMaxPacketSize = 8,       // 8字节
    .bInterval = 10            // 10ms轮询间隔
};
```

### 4.5 字符串描述符
```c
/* USB字符串描述符结构 */
struct usb_string_descriptor {
    __u8  bLength;              // 描述符长度
    __u8  bDescriptorType;      // 0x03 (STRING)
    __le16 wData[1];            // Unicode字符串
};

/* 字符串描述符索引0 - 语言ID */
static const struct {
    __u8  bLength;
    __u8  bDescriptorType;
    __le16 wLANGID;
} lang_desc = {
    .bLength = 4,
    .bDescriptorType = 0x03,
    .wLANGID = 0x0409          // 英语(美国)
};

/* 制造商字符串示例 */
static const struct {
    __u8  bLength;
    __u8  bDescriptorType;
    __le16 wString[8];
} manufacturer_string = {
    .bLength = 2 + 2 * 8,
    .bDescriptorType = 0x03,
    .wString = {
        'A', 'c', 'm', 'e', ' ', 'I', 'n', 'c'
    }
};
```

## 5. HID类协议

### 5.1 HID描述符
```c
/* HID描述符结构 */
struct hid_descriptor {
    __u8  bLength;              // 描述符长度
    __u8  bDescriptorType;      // 0x21 (HID)
    __le16 bcdHID;              // HID版本
    __u8  bCountryCode;         // 国家代码
    __u8  bNumDescriptors;      // 下级描述符数量
    __u8  bDescriptorType0;     // 下级描述符类型
    __le16 wDescriptorLength0;  // 下级描述符长度
    /* 可能有更多下级描述符 */
} __attribute__ ((packed));

/* HID鼠标描述符示例 */
static const struct hid_descriptor mouse_hid = {
    .bLength = 9,
    .bDescriptorType = 0x21,   // HID
    .bcdHID = 0x0111,          // HID 1.11
    .bCountryCode = 0,         // 不支持国家代码
    .bNumDescriptors = 1,
    .bDescriptorType0 = 0x22,  // Report
    .wDescriptorLength0 = 52   // Report描述符长度
};
```

### 5.2 HID Report描述符
```c
/* HID鼠标Report描述符示例 */
static const __u8 mouse_report_desc[] = {
    0x05, 0x01,    // Usage Page (Generic Desktop)
    0x09, 0x02,    // Usage (Mouse)
    0xA1, 0x01,    // Collection (Application)
    0x09, 0x01,    //   Usage (Pointer)
    0xA1, 0x00,    //   Collection (Physical)
    
    // 按钮
    0x05, 0x09,    //     Usage Page (Button)
    0x19, 0x01,    //     Usage Minimum (1)
    0x29, 0x03,    //     Usage Maximum (3)
    0x15, 0x00,    //     Logical Minimum (0)
    0x25, 0x01,    //     Logical Maximum (1)
    0x95, 0x03,    //     Report Count (3)
    0x75, 0x01,    //     Report Size (1)
    0x81, 0x02,    //     Input (Data,Var,Abs)
    
    // 填充位
    0x95, 0x01,    //     Report Count (1)
    0x75, 0x05,    //     Report Size (5)
    0x81, 0x03,    //     Input (Const,Var,Abs)
    
    // X,Y轴
    0x05, 0x01,    //     Usage Page (Generic Desktop)
    0x09, 0x30,    //     Usage (X)
    0x09, 0x31,    //     Usage (Y)
    0x15, 0x81,    //     Logical Minimum (-127)
    0x25, 0x7F,    //     Logical Maximum (127)
    0x75, 0x08,    //     Report Size (8)
    0x95, 0x02,    //     Report Count (2)
    0x81, 0x06,    //     Input (Data,Var,Rel)
    
    0xC0,          //   End Collection
    0xC0           // End Collection
};

/* 鼠标数据报告格式 */
struct mouse_report {
    __u8 buttons;   // 位0-2: 按钮1-3
    __s8 x;         // X轴移动量
    __s8 y;         // Y轴移动量
} __attribute__((packed));
```

### 5.3 HID类请求
| bRequest | 值 | 名称 | 说明 |
|----------|-----|------|------|
| 0x01 | GET_REPORT | 获取报告 | 获取输入/输出/特性报告 |
| 0x02 | GET_IDLE | 获取空闲率 | 获取空闲率 |
| 0x03 | GET_PROTOCOL | 获取协议 | 获取协议(Boot/Report) |
| 0x09 | SET_REPORT | 设置报告 | 设置输出/特性报告 |
| 0x0A | SET_IDLE | 设置空闲率 | 设置空闲率 |
| 0x0B | SET_PROTOCOL | 设置协议 | 设置协议(Boot/Report) |

## 6. Mass Storage类协议

### 6.1 Mass Storage类概述
- **BBB**（Bulk-Only Transport）：仅使用批量传输
- **CBI**（Control/Bulk/Interrupt）：使用控制、批量和中断传输
- **SCSI**命令集：大多数USB存储设备使用SCSI命令

### 6.2 BBB协议

#### Command Block Wrapper (CBW)
```c
struct bulk_cb_wrap {
    __le32 Signature;           // 'USBC' (0x43425355)
    __u32  Tag;                 // 命令标签
    __le32 DataTransferLength;  // 数据传输长度
    __u8   Flags;               // 位7: 方向(0=Out, 1=In)
    __u8   Lun;                 // 逻辑单元号
    __u8   Length;              // CDB长度(1-16)
    __u8   CDB[16];            // SCSI命令块
} __attribute__((packed));
```

#### Command Status Wrapper (CSW)
```c
struct bulk_cs_wrap {
    __le32 Signature;           // 'USBS' (0x53425355)
    __u32  Tag;                 // 必须与CBW的Tag匹配
    __le32 Residue;             // 未传输的数据量
    __u8   Status;              // 命令执行状态
} __attribute__((packed));

/* Status值 */
#define USB_STATUS_PASS     0x00  // 命令成功
#define USB_STATUS_FAIL     0x01  // 命令失败
#define USB_STATUS_PHASE    0x02  // 阶段错误
```

### 6.3 SCSI命令示例
```c
/* INQUIRY命令 - 获取设备信息 */
struct scsi_inquiry_cmd {
    __u8  opcode;               // 0x12
    __u8  evpd;                 // 0
    __u8  page_code;            // 0
    __u8  reserved;
    __u8  allocation_length;    // 36
    __u8  control;
} __attribute__((packed));

/* READ(10)命令 - 读取数据 */
struct scsi_read10_cmd {
    __u8  opcode;               // 0x28
    __u8  flags;
    __be32 lba;                 // 逻辑块地址
    __u8  group_number;
    __be16 transfer_length;     // 传输块数
    __u8  control;
} __attribute__((packed));

/* WRITE(10)命令 - 写入数据 */
struct scsi_write10_cmd {
    __u8  opcode;               // 0x2A
    __u8  flags;
    __be32 lba;                 // 逻辑块地址
    __u8  group_number;
    __be16 transfer_length;     // 传输块数
    __u8  control;
} __attribute__((packed));
```

## 7. CDC类协议

### 7.1 CDC类概述
CDC（Communication Device Class）用于网络适配器、调制解调器、串口等通信设备。

### 7.2 CDC子类
- **ACM**（Abstract Control Model）：虚拟串口
- **ECM**（Ethernet Control Model）：以太网
- **NCM**（Network Control Model）：高速网络
- **OBEX**：对象交换

### 7.3 CDC ACM描述符
```c
/* CDC头部功能描述符 */
struct usb_cdc_header_desc {
    __u8  bLength;              // 5
    __u8  bDescriptorType;      // 0x24 (CS_INTERFACE)
    __u8  bDescriptorSubType;   // 0x00 (Header)
    __le16 bcdCDC;              // CDC版本
} __attribute__((packed));

/* CDC调用管理功能描述符 */
struct usb_cdc_call_mgmt_descriptor {
    __u8  bLength;              // 5
    __u8  bDescriptorType;      // 0x24
    __u8  bDescriptorSubType;   // 0x01 (Call Management)
    __u8  bmCapabilities;       // 能力位图
    __u8  bDataInterface;       // 数据接口号
} __attribute__((packed));

/* CDC ACM功能描述符 */
struct usb_cdc_acm_descriptor {
    __u8  bLength;              // 4
    __u8  bDescriptorType;      // 0x24
    __u8  bDescriptorSubType;   // 0x02 (ACM)
    __u8  bmCapabilities;       // 能力位图
} __attribute__((packed));

/* CDC联合功能描述符 */
struct usb_cdc_union_desc {
    __u8  bLength;              // 5+
    __u8  bDescriptorType;      // 0x24
    __u8  bDescriptorSubType;   // 0x06 (Union)
    __u8  bMasterInterface0;    // 控制接口
    __u8  bSlaveInterface0;     // 数据接口
} __attribute__((packed));
```

## 8. USB 3.0新增特性

### 8.1 SuperSpeed增强
- **双单工通信**：独立的发送和接收通道
- **流协议**：支持多个逻辑流
- **突发传输**：一次可传输多个包

### 8.2 新增描述符

#### SuperSpeed端点伴随描述符
```c
struct usb_ss_ep_comp_descriptor {
    __u8  bLength;              // 6
    __u8  bDescriptorType;      // 0x30 (SS_ENDPOINT_COMP)
    __u8  bMaxBurst;            // 最大突发大小
    __u8  bmAttributes;         // 属性
    __le16 wBytesPerInterval;   // 每个服务间隔的字节数
} __attribute__((packed));
```

#### BOS（二进制对象存储）描述符
```c
struct usb_bos_descriptor {
    __u8  bLength;              // 5
    __u8  bDescriptorType;      // 0x0F (BOS)
    __le16 wTotalLength;        // BOS总长度
    __u8  bNumDeviceCaps;       // 设备能力数量
} __attribute__((packed));
```

### 8.3 USB 3.0链接电源管理
- **U0**：正常工作状态
- **U1**：低功耗状态，快速恢复
- **U2**：更低功耗，恢复较慢
- **U3**：挂起状态

## 9. USB Type-C和PD

### 9.1 Type-C特性
- **可反插**：没有正反面
- **多协议支持**：USB、DisplayPort、Thunderbolt等
- **更高功率**：最高100W（20V/5A）

### 9.2 Power Delivery协议
```c
/* PD消息头 */
struct pd_message_header {
    __le16 header;
    /* 位定义：
     * 0-4:   消息类型
     * 5-7:   数据对象数量
     * 8:     消息ID
     * 9-11:  端口电源角色等
     * 12-14: 规范版本
     * 15:    扩展消息
     */
} __attribute__((packed));

/* 电源数据对象(PDO) */
struct power_data_object {
    __le32 pdo;
    /* 不同类型的PDO有不同的位定义 */
} __attribute__((packed));
```

## 10. USB调试协议分析

### 10.1 枚举过程抓包分析
```
时间     方向    类型       数据
--------------------------------------------
0.000    H→D    RESET      [复位信号]
0.010    H→D    SETUP      80 06 00 01 00 00 40 00
                           [GET_DESCRIPTOR Device 64字节]
0.011    D→H    DATA0      12 01 00 02 00 00 00 40...
                           [设备描述符前8字节]
0.012    H→D    ACK        
0.020    H→D    RESET      [第二次复位]
0.030    H→D    SETUP      00 05 03 00 00 00 00 00
                           [SET_ADDRESS 地址=3]
0.031    D→H    ACK
0.032    H→D    IN         [状态阶段]
0.033    D→H    DATA1      [0长度数据]
0.034    H→D    ACK
```

### 10.2 批量传输抓包分析
```
# 写入操作（OUT）
0.100    H→D    OUT        EP2
0.101    H→D    DATA0      [512字节数据]
0.102    D→H    ACK
0.103    H→D    OUT        EP2  
0.104    H→D    DATA1      [512字节数据]
0.105    D→H    ACK

# 读取操作（IN）
0.200    H→D    IN         EP1
0.201    D→H    DATA0      [512字节数据]
0.202    H→D    ACK
0.203    H→D    IN         EP1
0.204    D→H    NAK        [设备无数据]
```

### 10.3 错误处理示例
```
# CRC错误
0.300    H→D    OUT        EP2
0.301    H→D    DATA0      [数据+错误CRC]
0.302           [无响应 - CRC错误被忽略]
0.310    H→D    OUT        EP2 [重传]
0.311    H→D    DATA0      [数据+正确CRC]
0.312    D→H    ACK

# STALL响应
0.400    H→D    SETUP      [非法请求]
0.401    D→H    STALL      [端点停止]
0.402    H→D    SETUP      [CLEAR_FEATURE清除STALL]
0.403    D→H    ACK
```

## 总结

USB协议规范涵盖了从物理层到应用层的完整定义：

1. **数据包层**：定义了令牌、数据、握手包格式
2. **事务层**：规定了IN/OUT/SETUP事务流程
3. **传输层**：定义了四种传输类型
4. **设备框架**：描述符体系和标准请求
5. **类规范**：HID、Mass Storage、CDC等标准类

作为驱动开发者，重点掌握：
- 描述符的组织和解析
- 标准请求的处理流程
- 各种传输类型的使用场景
- 具体设备类的协议规范

理解这些协议细节是开发高质量USB驱动的基础。
