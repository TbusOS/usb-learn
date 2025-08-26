# USB调试工具和方法详解

## 1. 软件调试工具

### 1.1 lsusb - 查看USB设备

#### 基本用法
```bash
# 列出所有USB设备
lsusb

# 显示设备树形结构
lsusb -t

# 显示详细信息
lsusb -v

# 查看特定设备
lsusb -d 046d:c534 -v  # 根据VID:PID查看

# 显示设备描述符的原始数据
lsusb -d 046d:c534 -v | grep -A 20 "Device Descriptor"
```

#### lsusb输出解析
```
Bus 001 Device 003: ID 046d:c534 Logitech, Inc. Unifying Receiver
|    |      |       |      |                |
|    |      |       |      |                └── 产品名称
|    |      |       |      └── 产品ID (PID)
|    |      |       └── 厂商ID (VID)
|    |      └── 设备号
|    └── 总线号
└── USB总线
```

#### 详细模式输出示例
```bash
sudo lsusb -v -d 046d:c534

Device Descriptor:
  bLength                18      # 描述符长度
  bDescriptorType         1      # 描述符类型（设备）
  bcdUSB               2.00      # USB版本
  bDeviceClass            0      # 设备类
  bDeviceSubClass         0      # 设备子类
  bDeviceProtocol         0      # 设备协议
  bMaxPacketSize0        64      # 端点0最大包大小
  idVendor           0x046d      # 厂商ID
  idProduct          0xc534      # 产品ID
  bcdDevice           29.01      # 设备版本
  iManufacturer           1      # 制造商字符串索引
  iProduct                2      # 产品字符串索引
  iSerialNumber           0      # 序列号字符串索引
  bNumConfigurations      1      # 配置数量
```

### 1.2 usbmon - USB数据包监控

#### 启用usbmon
```bash
# 加载usbmon模块
sudo modprobe usbmon

# 查看可用的监控接口
ls /sys/kernel/debug/usb/usbmon/
0s  0u  1s  1t  1u  2s  2t  2u  ...
# 0表示所有总线，数字表示特定总线
# s=统计，t=文本格式，u=二进制格式
```

#### 使用文本格式监控
```bash
# 监控所有USB总线
sudo cat /sys/kernel/debug/usb/usbmon/0u

# 监控特定总线
sudo cat /sys/kernel/debug/usb/usbmon/1u

# 保存到文件
sudo cat /sys/kernel/debug/usb/usbmon/0u > usb_trace.txt
```

#### usbmon输出格式
```
URB地址    时间戳 事件 地址 端点 URB状态 数据长度 数据
ffff8801... 123.456 S Ci:1:002:0 s 80 06 0100 0000 0012 18 <
ffff8801... 123.457 C Ci:1:002:0 0 18 = 12010002 00000040 ...

事件类型：
S = 提交URB
C = 完成URB
E = 错误

传输类型：
Ci = 控制输入    Co = 控制输出
Bi = 批量输入    Bo = 批量输出
Ii = 中断输入    Io = 中断输出
Zi = 同步输入    Zo = 同步输出
```

### 1.3 Wireshark USB捕获

#### 安装和配置
```bash
# 安装Wireshark
sudo apt-get install wireshark

# 添加用户到wireshark组
sudo usermod -a -G wireshark $USER

# 设置usbmon权限
sudo setfacl -m u:$USER:r /dev/usbmon*

# 或临时使用root运行
sudo wireshark
```

#### 捕获USB数据包
1. 启动Wireshark
2. 选择usbmon接口（如usbmon0）
3. 开始捕获
4. 执行USB操作
5. 停止捕获并分析

#### 过滤器示例
```
# 根据设备地址过滤
usb.device_address == 3

# 根据端点过滤
usb.endpoint_address == 0x81

# 根据传输类型过滤
usb.transfer_type == URB_CONTROL
usb.transfer_type == URB_BULK
usb.transfer_type == URB_INTERRUPT

# 组合过滤
usb.device_address == 3 && usb.transfer_type == URB_BULK
```

### 1.4 usbutils工具集

#### usb-devices
```bash
# 显示USB设备详细信息
usb-devices

# 输出示例
T:  Bus=01 Lev=02 Prnt=02 Port=03 Cnt=01 Dev#=  3 Spd=12  MxCh= 0
D:  Ver= 2.00 Cls=00(>ifc ) Sub=00 Prot=00 MxPS=64 #Cfgs=  1
P:  Vendor=046d ProdID=c534 Rev=29.01
S:  Manufacturer=Logitech
S:  Product=USB Receiver
C:  #Ifs= 2 Cfg#= 1 Atr=a0 MxPwr=98mA
I:  If#= 0 Alt= 0 #EPs= 1 Cls=03(HID  ) Sub=01 Prot=01 Driver=usbhid
I:  If#= 1 Alt= 0 #EPs= 1 Cls=03(HID  ) Sub=01 Prot=02 Driver=usbhid
```

#### usbhid-dump
```bash
# 安装
sudo apt-get install usbutils

# 转储HID报告描述符
sudo usbhid-dump -d 046d:c534

# 解析HID描述符
sudo usbhid-dump -d 046d:c534 | grep -A 100 "DESCRIPTOR"
```

### 1.5 内核调试接口

#### debugfs接口
```bash
# 挂载debugfs（通常已自动挂载）
sudo mount -t debugfs none /sys/kernel/debug

# USB调试目录
ls /sys/kernel/debug/usb/

# 查看设备信息
cat /sys/kernel/debug/usb/devices

# EHCI调试寄存器
cat /sys/kernel/debug/usb/ehci/*/registers

# XHCI调试信息
cat /sys/kernel/debug/usb/xhci/*/command-ring
```

#### sysfs接口
```bash
# USB设备sysfs路径
ls /sys/bus/usb/devices/

# 查看设备属性
cat /sys/bus/usb/devices/1-1/idVendor
cat /sys/bus/usb/devices/1-1/idProduct
cat /sys/bus/usb/devices/1-1/bcdDevice

# 查看设备描述符
cat /sys/bus/usb/devices/1-1/descriptors | hexdump -C

# 查看驱动信息
ls -l /sys/bus/usb/devices/1-1:1.0/driver

# 电源管理状态
cat /sys/bus/usb/devices/1-1/power/runtime_status
cat /sys/bus/usb/devices/1-1/power/control
```

### 1.6 dmesg内核日志

#### 查看USB相关日志
```bash
# 查看所有USB日志
dmesg | grep -i usb

# 实时监控
dmesg -w | grep -i usb

# 清除旧日志后监控
sudo dmesg -C
dmesg -w

# 带时间戳
dmesg -T | grep -i usb

# 查看特定设备
dmesg | grep "idVendor=046d"
```

#### 日志级别设置
```bash
# 查看当前日志级别
cat /proc/sys/kernel/printk

# 设置详细日志
echo 8 > /proc/sys/kernel/printk

# 或使用dmesg设置
dmesg -n 8
```

## 2. 动态调试

### 2.1 动态调试控制

#### 启用动态调试
```bash
# 查看可用的调试点
cat /sys/kernel/debug/dynamic_debug/control | grep usb

# 启用USB核心调试
echo 'module usbcore +p' > /sys/kernel/debug/dynamic_debug/control

# 启用特定文件调试
echo 'file drivers/usb/core/hub.c +p' > /sys/kernel/debug/dynamic_debug/control

# 启用特定函数调试
echo 'func usb_submit_urb +p' > /sys/kernel/debug/dynamic_debug/control

# 启用特定行调试
echo 'file drivers/usb/core/urb.c line 350 +p' > /sys/kernel/debug/dynamic_debug/control
```

#### 调试标志
```bash
# p = 打印消息
# f = 包含函数名
# l = 包含行号
# m = 包含模块名
# t = 包含线程ID

# 组合使用
echo 'module usbcore +pfl' > /sys/kernel/debug/dynamic_debug/control
```

### 2.2 ftrace跟踪

#### 设置ftrace
```bash
# 进入ftrace目录
cd /sys/kernel/debug/tracing

# 查看可用的跟踪器
cat available_tracers

# 设置函数跟踪器
echo function > current_tracer

# 设置USB相关函数过滤
echo 'usb_*' > set_ftrace_filter
echo 'hub_*' >> set_ftrace_filter

# 开始跟踪
echo 1 > tracing_on

# 查看跟踪结果
cat trace

# 停止跟踪
echo 0 > tracing_on
```

#### 跟踪特定事件
```bash
# 查看USB事件
ls events/usb/

# 启用USB事件跟踪
echo 1 > events/usb/enable

# 或选择特定事件
echo 1 > events/usb/usb_submit_urb/enable
echo 1 > events/usb/usb_complete_urb/enable
```

## 3. 驱动调试技巧

### 3.1 printk调试

#### 在驱动中添加调试信息
```c
/* 定义调试宏 */
#ifdef DEBUG
#define DBG(fmt, args...) \
    printk(KERN_DEBUG "usb_driver: " fmt, ## args)
#else
#define DBG(fmt, args...)
#endif

/* 使用不同级别 */
pr_emerg("紧急消息\n");    // KERN_EMERG
pr_alert("警报消息\n");    // KERN_ALERT
pr_crit("严重消息\n");     // KERN_CRIT
pr_err("错误消息\n");      // KERN_ERR
pr_warn("警告消息\n");     // KERN_WARNING
pr_notice("通知消息\n");   // KERN_NOTICE
pr_info("信息消息\n");     // KERN_INFO
pr_debug("调试消息\n");    // KERN_DEBUG

/* 设备相关的打印 */
dev_dbg(&interface->dev, "设备调试信息\n");
dev_info(&interface->dev, "设备信息\n");
dev_warn(&interface->dev, "设备警告\n");
dev_err(&interface->dev, "设备错误\n");
```

### 3.2 数据包内容打印

```c
/* 打印数据包内容 */
static void print_hex_dump_bytes(const char *prefix,
                                const void *buf, size_t len)
{
    print_hex_dump(KERN_DEBUG, prefix,
                  DUMP_PREFIX_OFFSET, 16, 1,
                  buf, len, true);
}

/* 在URB完成函数中使用 */
static void urb_complete(struct urb *urb)
{
    if (urb->status == 0) {
        dev_dbg(&urb->dev->dev, "URB完成，数据：\n");
        print_hex_dump_bytes("", DUMP_PREFIX_OFFSET,
                           urb->transfer_buffer,
                           urb->actual_length);
    }
}
```

### 3.3 错误处理调试

```c
/* 详细的错误处理 */
static int my_usb_probe(struct usb_interface *intf,
                       const struct usb_device_id *id)
{
    int ret;
    
    ret = usb_control_msg(...);
    if (ret < 0) {
        dev_err(&intf->dev, "控制消息失败: %d\n", ret);
        
        switch (ret) {
        case -ETIMEDOUT:
            dev_err(&intf->dev, "请求超时\n");
            break;
        case -EPIPE:
            dev_err(&intf->dev, "端点停止\n");
            break;
        case -ENODEV:
            dev_err(&intf->dev, "设备已断开\n");
            break;
        default:
            dev_err(&intf->dev, "未知错误: %d\n", ret);
        }
        return ret;
    }
}
```

## 4. 硬件调试工具

### 4.1 USB协议分析仪

#### 商业分析仪
- **Beagle USB 480/5000/12**：专业USB分析仪
- **Ellisys USB Explorer**：高端分析仪
- **LeCroy USB Protocol Analyzer**：专业级工具

#### 使用方法
1. 连接分析仪到USB总线
2. 配置触发条件
3. 捕获数据包
4. 分析协议细节
5. 导出分析报告

### 4.2 示波器调试

#### 测量要点
- **D+/D-差分信号**：检查信号质量
- **VBUS电压**：确认5V供电
- **上拉电阻**：验证速度检测
- **眼图分析**：高速信号质量

#### 示波器设置
```
- 带宽：至少100MHz（USB 2.0）
- 采样率：至少1GS/s
- 探头：差分探头最佳
- 触发：边沿触发或协议触发
```

### 4.3 逻辑分析仪

#### 连接方法
```
逻辑分析仪通道分配：
- CH0: D+
- CH1: D-
- CH2: VBUS
- CH3: GND（参考）
```

#### Sigrok/PulseView使用
```bash
# 安装
sudo apt-get install sigrok pulseview

# 启动PulseView
pulseview

# 配置：
1. 选择设备
2. 设置采样率（至少24MHz）
3. 添加USB协议解码器
4. 开始捕获
```

## 5. 常见问题调试

### 5.1 设备无法枚举

#### 调试步骤
```bash
# 1. 检查内核日志
dmesg | tail -n 50

# 2. 检查USB控制器
lspci | grep USB
lsusb -t

# 3. 检查电源
# 查看设备是否获得足够电源
cat /sys/bus/usb/devices/*/power/level

# 4. 尝试其他端口或Hub

# 5. 禁用自动挂起
echo on > /sys/bus/usb/devices/*/power/control

# 6. 重置USB控制器
echo -n "0000:00:14.0" > /sys/bus/pci/drivers/xhci_hcd/unbind
echo -n "0000:00:14.0" > /sys/bus/pci/drivers/xhci_hcd/bind
```

### 5.2 传输错误

#### 常见错误码
```c
-EINPROGRESS : URB正在处理
-ENOENT      : URB被unlink取消
-ECONNRESET  : URB被unlink取消
-EPIPE       : 端点停止（STALL）
-EOVERFLOW   : babble错误
-ENODEV      : 设备已移除
-ETIMEDOUT   : 传输超时
-ESHUTDOWN   : 设备或主机控制器关闭
```

#### 调试方法
```bash
# 启用URB调试
echo 'func usb_submit_urb +p' > /sys/kernel/debug/dynamic_debug/control
echo 'func usb_complete_urb +p' > /sys/kernel/debug/dynamic_debug/control

# 监控URB状态
cat /sys/kernel/debug/usb/usbmon/0u | grep "status="
```

### 5.3 性能问题

#### 性能分析
```bash
# 使用perf分析
sudo perf record -a -g
sudo perf report

# 查看中断统计
cat /proc/interrupts | grep usb

# 查看USB带宽使用
cat /sys/kernel/debug/usb/devices | grep "B:"
```

#### 优化建议
1. 使用批量传输而非控制传输
2. 增大URB缓冲区大小
3. 使用多个URB队列
4. 启用Scatter-Gather DMA
5. 调整端点轮询间隔

## 6. 自动化测试

### 6.1 USB测试脚本

```bash
#!/bin/bash
# USB设备测试脚本

# 设备信息
VID="046d"
PID="c534"

# 检查设备是否存在
check_device() {
    lsusb -d ${VID}:${PID} > /dev/null 2>&1
    return $?
}

# 获取设备路径
get_device_path() {
    for dev in /sys/bus/usb/devices/*; do
        if [ -f "$dev/idVendor" ]; then
            vendor=$(cat "$dev/idVendor")
            product=$(cat "$dev/idProduct")
            if [ "$vendor" = "$VID" ] && [ "$product" = "$PID" ]; then
                echo "$dev"
                return 0
            fi
        fi
    done
    return 1
}

# 测试设备
test_device() {
    echo "开始USB设备测试..."
    
    if check_device; then
        echo "[✓] 设备已连接"
        
        dev_path=$(get_device_path)
        if [ $? -eq 0 ]; then
            echo "[✓] 设备路径: $dev_path"
            
            # 读取设备信息
            echo "设备信息:"
            echo "  厂商ID: $(cat $dev_path/idVendor)"
            echo "  产品ID: $(cat $dev_path/idProduct)"
            echo "  版本: $(cat $dev_path/bcdDevice)"
            echo "  速度: $(cat $dev_path/speed)"
        fi
    else
        echo "[✗] 设备未找到"
        return 1
    fi
}

# 执行测试
test_device
```

### 6.2 内核测试模块

```c
/* USB测试模块 test_usb.c */
#include <linux/module.h>
#include <linux/usb.h>

static int test_probe(struct usb_interface *intf,
                     const struct usb_device_id *id)
{
    struct usb_device *udev = interface_to_usbdev(intf);
    struct usb_host_interface *alt = intf->cur_altsetting;
    int i;
    
    pr_info("测试设备探测: %04x:%04x\n",
            le16_to_cpu(udev->descriptor.idVendor),
            le16_to_cpu(udev->descriptor.idProduct));
    
    /* 打印接口信息 */
    pr_info("接口 %d:\n", alt->desc.bInterfaceNumber);
    pr_info("  类: 0x%02x\n", alt->desc.bInterfaceClass);
    pr_info("  子类: 0x%02x\n", alt->desc.bInterfaceSubClass);
    pr_info("  协议: 0x%02x\n", alt->desc.bInterfaceProtocol);
    pr_info("  端点数: %d\n", alt->desc.bNumEndpoints);
    
    /* 打印端点信息 */
    for (i = 0; i < alt->desc.bNumEndpoints; i++) {
        struct usb_endpoint_descriptor *ep = &alt->endpoint[i].desc;
        pr_info("  端点 0x%02x:\n", ep->bEndpointAddress);
        pr_info("    类型: %s\n",
                usb_endpoint_type(ep) == USB_ENDPOINT_XFER_BULK ? "批量" :
                usb_endpoint_type(ep) == USB_ENDPOINT_XFER_INT ? "中断" :
                usb_endpoint_type(ep) == USB_ENDPOINT_XFER_ISOC ? "同步" : "控制");
        pr_info("    方向: %s\n",
                usb_endpoint_dir_in(ep) ? "IN" : "OUT");
        pr_info("    最大包: %d\n", usb_endpoint_maxp(ep));
    }
    
    return -ENODEV;  // 不真正绑定设备
}

static void test_disconnect(struct usb_interface *intf)
{
    pr_info("测试设备断开\n");
}

static struct usb_device_id test_table[] = {
    { USB_DEVICE(0x046d, 0xc534) },
    { }
};
MODULE_DEVICE_TABLE(usb, test_table);

static struct usb_driver test_driver = {
    .name = "usb_test",
    .probe = test_probe,
    .disconnect = test_disconnect,
    .id_table = test_table,
};

module_usb_driver(test_driver);
MODULE_LICENSE("GPL");
```

## 7. 调试检查清单

### 7.1 问题诊断流程

```
1. 硬件连接
   □ USB线缆是否完好
   □ 接口是否清洁
   □ 供电是否正常
   
2. 系统识别
   □ lsusb能否看到设备
   □ dmesg有无错误信息
   □ /sys/bus/usb/devices/是否存在
   
3. 驱动加载
   □ 驱动模块是否加载（lsmod）
   □ 驱动是否匹配（ID表）
   □ probe函数是否被调用
   
4. 数据传输
   □ URB提交是否成功
   □ 数据包是否正确
   □ 传输速率是否正常
   
5. 错误处理
   □ 错误码是否正确处理
   □ 超时设置是否合理
   □ 重试机制是否工作
```

### 7.2 性能优化检查

```
1. 传输效率
   □ 使用合适的传输类型
   □ URB大小是否优化
   □ 是否使用DMA
   
2. CPU使用
   □ 中断处理是否高效
   □ 是否有不必要的轮询
   □ 工作队列使用是否合理
   
3. 内存使用
   □ 缓冲区分配是否合理
   □ 是否有内存泄漏
   □ DMA映射是否正确
   
4. 电源管理
   □ 自动挂起是否启用
   □ 唤醒机制是否正常
   □ 电源状态转换是否正确
```

## 总结

USB调试需要综合使用多种工具和方法：

1. **软件工具**：熟练使用lsusb、usbmon、Wireshark等
2. **内核接口**：掌握debugfs、sysfs、ftrace的使用
3. **驱动调试**：合理使用printk和动态调试
4. **硬件工具**：必要时使用协议分析仪或示波器
5. **系统方法**：建立规范的调试流程和检查清单

通过系统的调试方法，可以快速定位和解决USB驱动开发中的问题。
