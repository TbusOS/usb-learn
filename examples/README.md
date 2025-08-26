# USB驱动开发示例代码

这个目录包含了几个USB驱动开发的示例代码，从简单到复杂，帮助您学习Linux USB驱动开发。

## 📁 示例列表

### 1. 01_simple_usb_led.c - 简单USB LED控制驱动
- **难度**：⭐ 入门级
- **功能**：控制USB LED设备的开关
- **重点**：
  - USB驱动基本框架
  - probe/disconnect函数
  - 字符设备接口
  - 控制传输的使用

### 2. 02_usb_mouse_driver.c - USB鼠标驱动
- **难度**：⭐⭐ 初级
- **功能**：处理USB鼠标输入
- **重点**：
  - HID协议处理
  - 中断传输
  - URB的使用
  - 输入子系统接口

### 3. 03_usb_serial_driver.c - USB串口驱动
- **难度**：⭐⭐⭐ 中级
- **功能**：USB转串口功能
- **重点**：
  - 批量传输
  - TTY层接口
  - 异步I/O处理
  - 工作队列的使用

### 4. 04_usb_storage_simple.c - 简化的USB存储驱动
- **难度**：⭐⭐⭐ 中级
- **功能**：访问USB存储设备
- **重点**：
  - SCSI命令处理
  - Bulk-Only Transport协议
  - CBW/CSW处理
  - 错误处理机制

## 🛠️ 编译环境准备

### 安装必要的工具
```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install build-essential linux-headers-$(uname -r)
sudo apt-get install libusb-1.0-0-dev usbutils

# Fedora/RHEL
sudo dnf install kernel-devel kernel-headers
sudo dnf install libusb-devel usbutils

# Arch Linux
sudo pacman -S base-devel linux-headers
sudo pacman -S libusb usbutils
```

### 检查内核配置
```bash
# 确认USB支持已启用
zcat /proc/config.gz | grep CONFIG_USB
```

## 📖 编译说明

### 编译所有驱动
```bash
make
```

### 编译单个驱动
```bash
make 01_simple_usb_led.ko
```

### 清理编译文件
```bash
make clean
```

## 🚀 使用方法

### 1. 加载驱动模块
```bash
# 加载驱动（需要root权限）
sudo insmod 01_simple_usb_led.ko

# 或使用make安装所有
sudo make install
```

### 2. 查看驱动状态
```bash
# 查看已加载的模块
lsmod | grep usb

# 查看内核日志
dmesg | tail -n 20

# 实时监控内核日志
dmesg -w
```

### 3. 测试驱动功能

#### LED驱动测试
```bash
# 写入数据控制LED
echo -n -e '\x01' > /dev/usbled0  # 打开LED
echo -n -e '\x00' > /dev/usbled0  # 关闭LED

# 读取状态
cat /dev/usbled0
```

#### 鼠标驱动测试
```bash
# 查看输入设备
cat /proc/bus/input/devices

# 监控鼠标事件
sudo evtest /dev/input/eventX
```

#### 串口驱动测试
```bash
# 配置串口
stty -F /dev/ttyUSB0 9600

# 发送数据
echo "Hello USB" > /dev/ttyUSB0

# 接收数据
cat /dev/ttyUSB0
```

### 4. 卸载驱动
```bash
# 卸载单个驱动
sudo rmmod 01_simple_usb_led

# 或使用make卸载所有
sudo make uninstall
```

## 🐛 调试技巧

### 1. 启用调试输出
```bash
# 启用USB核心调试
echo 'module usbcore +p' > /sys/kernel/debug/dynamic_debug/control

# 启用特定驱动调试
echo 'module 01_simple_usb_led +p' > /sys/kernel/debug/dynamic_debug/control
```

### 2. 使用usbmon
```bash
# 加载usbmon模块
sudo modprobe usbmon

# 监控USB总线
sudo cat /sys/kernel/debug/usb/usbmon/0u
```

### 3. 查看USB设备信息
```bash
# 列出USB设备
lsusb -v

# 查看设备树
lsusb -t

# 查看特定设备
lsusb -d 0416:5020 -v
```

### 4. 使用Wireshark抓包
```bash
# 安装Wireshark
sudo apt-get install wireshark

# 捕获USB数据包
sudo modprobe usbmon
sudo wireshark
# 选择usbmon0接口
```

## ⚠️ 注意事项

### 安全警告
1. **驱动开发有风险**：错误的驱动可能导致系统崩溃
2. **建议使用虚拟机**：在虚拟机中测试驱动更安全
3. **备份重要数据**：测试前请备份重要文件
4. **谨慎操作硬件**：不当操作可能损坏USB设备

### 权限要求
- 编译驱动不需要root权限
- 加载/卸载驱动需要root权限
- 访问设备文件可能需要特定权限

### 兼容性
- 这些示例基于Linux 5.x/6.x内核
- 不同内核版本API可能有差异
- 需要根据实际内核版本调整代码

## 📚 学习路径

### 初学者路线
1. 先学习 `01_simple_usb_led.c`
   - 理解USB驱动基本结构
   - 掌握probe/disconnect流程
   
2. 然后学习 `02_usb_mouse_driver.c`
   - 理解中断传输
   - 学习URB机制

3. 进阶学习 `03_usb_serial_driver.c`
   - 掌握批量传输
   - 理解异步I/O

4. 最后学习 `04_usb_storage_simple.c`
   - 理解SCSI协议
   - 掌握复杂协议处理

### 练习建议
1. **修改LED驱动**：添加闪烁功能
2. **扩展鼠标驱动**：添加更多按键支持
3. **优化串口驱动**：提高传输效率
4. **完善存储驱动**：添加读写功能

## 🔗 参考资源

### 内核文档
- `/Documentation/usb/` - USB子系统文档
- `/Documentation/driver-api/usb/` - USB驱动API
- `/include/linux/usb.h` - USB核心头文件

### 在线资源
- [Linux USB](http://www.linux-usb.org/)
- [USB.org](https://www.usb.org/)
- [LWN USB文章](https://lwn.net/)

### 推荐书籍
- 《Linux设备驱动程序》第3版
- 《Essential Linux Device Drivers》
- 《USB Complete》

## 🤝 贡献

欢迎提交问题和改进建议！

## 📄 许可证

这些示例代码使用GPL v2许可证。
