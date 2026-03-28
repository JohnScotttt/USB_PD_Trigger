# IAP升级固件设计框架

## 目标

在最大利用当前已有框架的前提下，将当前的固件支持通过IAP进行更新，构建IAP相关代码。

## MCU官方例程

项目绝对地址位于D:/repos/EVT/EXAM/IAP，参考其跳转和中断实现，确保正确性。

## 通讯方式

使用当前的USB HID（Cherry USB）框架下载新固件，同时保留扩展其他方式的余地。

进入Bootloader之后采用新的PID来区分模式。

## 下载及覆盖方式

采用边下载边覆盖的方式，节省ROM空间，但不得使用外部存储（即使硬件具备）。

## 进入Bootloader方式

支持指令进入（见下文），也考虑通过上电时已有按键的按下与否来进入（保留接口，暂时未确认使用哪个已有按钮）。

如果APP区失效，则自动进入Bootloader。

## 保险措施

即使APP区完全失效（例如断电引起的覆盖失败），Bootloader也能被正确启动并下载安装固件，但不得使用外部存储（即使硬件具备）。

添加校验保证固件安装正确。

## 指令操作

支持通过当前的HID指令体系进入Bootloader，支持通过HID获取固件下载和安装进度。

## 空间管理

Bootloader使用12KB空间。

## 文件结构

Bootloader部分代码独立创建"Bootloader"文件夹，相关代码创建在其中。

PlatformIO创建独立的环境来编译。

## 命名和管理规则

Bootloader相关文件命名规则均参考原先固件代码的命名规则，Bootloader版本存放于config.h中。

## 脚本支持

添加Python脚本支持在IAP模式下写入固件。

## 安装流程

先通过WCHISPTool清空Flash写入Bootloader区，复位后进入Bootloader，通过Python脚本写入APP区，复位后正常进入APP。

后续如果更新APP固件就进入Bootloader，通过Python脚本写入APP区，复位后正常进入APP。

