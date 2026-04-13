# 触摸控制功能实现总结

## 功能概述

为2.8寸LCD屏幕（ILI9341 + XPT2046）添加触摸控制功能，用户可以直接在屏幕上点击按钮控制机械臂的6个关节，无需通过串口输入命令。

## 新增文件

### 驱动层
- `Inc/touch.h` / `Src/touch.c`：XPT2046触摸控制器驱动
  - 软件SPI实现（PD13/PE0/PE2/PE3）
  - 12位ADC采样 + 3次中值滤波降噪
  - 仿射变换校准（6参数矩阵）
  - 触摸状态读取和坐标映射

### UI层
- `Inc/touch_ui.h` / `Src/touch_ui.c`：触摸按钮界面
  - 虚拟按钮布局（4列×6行，每个80×30像素）
  - 关节控制按钮：J1~J6，每个关节有 +/- 两个按钮
  - 功能按钮：ZERO（归零）、STOP（急停）、STATUS（同步角度）
  - 步长选择器：1°/5°/10°/30°（底部栏，黄色高亮当前选中）
  - 按钮事件分发和视觉反馈

### 校准模块
- `Inc/touch_calib.h` / `Src/touch_calib.c`：触摸校准
  - 4点校准状态机（左上、右上、左下、右下）
  - 屏幕引导UI（红色十字+进度提示）
  - 仿射变换系数计算（从原始ADC映射到LCD像素坐标）
  - 串口命令 `touch_calib` 触发

## 集成修改

### main.c
```c
#include "touch.h"
#include "touch_ui.h"
#include "touch_calib.h"

int main(void) {
    // 初始化部分
    Touch_Init();      // 初始化XPT2046（软件SPI + GPIO配置）
    TouchUI_Init();    // 绘制按钮界面
    
    // 主循环
    while (1) {
        // 每50ms轮询触摸状态
        if (Touch_Read(&touch_state)) {
            // 判断是否在校准模式
            if (TouchCalib_GetState() != IDLE) {
                // 校准模式：将原始ADC值喂给校准状态机
                TouchCalib_ProcessTouch(raw_x, raw_y);
            } else {
                // 正常模式：检测按钮点击（上升沿）
                TouchUI_Event_t event = TouchUI_HandleTouch(x, y);
                switch (event) {
                    case BTN_EVENT_J1_PLUS:
                        RobotCtrl_MoveJointRelative(0, step, 30, 50);
                        break;
                    // ... 其他按钮处理 ...
                }
            }
        }
    }
}
```

### robot_cmd.c
添加串口命令：
```c
#include "touch_calib.h"

if (strcmp(cmd, "touch_calib") == 0) {
    TouchCalib_Start();  // 启动校准流程
}
```

## 使用流程

### 1. 编译配置
在Keil工程的 `LCD` 组中添加3个新文件：
- `../Src/touch.c`
- `../Src/touch_ui.c`
- `../Src/touch_calib.c`

编译选项无需修改（继续使用 MicroLIB + FPU）。

### 2. 烧录固件
编译后下载到STM32F407，LCD自动显示触摸按钮界面。

### 3. 触摸校准（首次使用推荐）
串口发送：
```
touch_calib
```
按屏幕提示依次点击4个红色十字（左上 → 右上 → 左下 → 右下），完成后自动保存校准参数。

### 4. 触摸控制
- **选择步长**：点击底部 `5deg` 按钮（黄色表示选中）
- **点动关节**：点击 `J1+` 使关节1正转5度
- **归零**：点击 `ZERO` 将当前位置设为零点
- **急停**：点击 `STOP` 紧急停止所有电机
- **同步角度**：点击 `STATUS` 从编码器读取真实位置

## 技术特性

| 特性 | 实现方式 |
|------|---------|
| SPI通信 | 软件SPI（~500kHz），兼容性好，无需占用硬件SPI |
| 噪声抑制 | 3次采样取中值，避免单次误差 |
| 防抖 | 上升沿检测（仅在按下瞬间触发一次）|
| 轮询频率 | 50ms（20Hz），兼顾响应速度和CPU占用 |
| 坐标变换 | 仿射变换（6参数），支持旋转/缩放/平移 |
| 校准存储 | 当前存储在RAM，断电丢失（可扩展FLASH持久化）|
| 按钮尺寸 | 80×30像素，适合手指点击 |

## 引脚占用

| 引脚 | 原用途 | 新用途 | 冲突？ |
|------|--------|--------|-------|
| PD13 | 空闲 | T_CLK（SPI时钟）| 无 ✅ |
| PE0  | 空闲 | T_CS（片选）| 无 ✅ |
| PE2  | 空闲 | T_MOSI | 无 ✅ |
| PE3  | 空闲 | T_MISO | 无 ✅ |

T_PEN（触摸中断引脚）未使用，采用轮询模式。

## 性能指标

- **触摸响应延迟**：50ms（轮询周期）+ 1ms（SPI读取）
- **坐标精度**：12位（0~4095），映射到320×240屏幕
- **CPU占用**：每50ms约1ms SPI通信，占用率 < 2%
- **内存占用**：约2KB（代码段 + 按钮结构体 + 校准系数）

## 故障排查

### 触摸无响应
1. 检查FPC排线连接是否牢固
2. 用万用表测量 PE0（T_CS）是否有高电平
3. 串口发送 `status` 确认主程序未卡死

### 触摸位置偏移
1. 执行 `touch_calib` 重新校准
2. 确认LCD方向（代码设定横屏320×240）
3. 如果仍偏移，调整 `touch.c` 中 `s_default_calib` 系数

### 按钮误触发
1. 增加轮询周期：修改 `main.c` 中 50ms → 100ms
2. 增加防抖延迟：修改 `touch_calib.c` 中 300ms → 500ms

## 后续扩展方向

- **滑块控件**：拖动连续调节角度
- **虚拟键盘**：输入精确数值
- **多页面UI**：触摸TAB切换不同功能页面
- **示教模式**：触摸记录关键帧，保存轨迹
- **校准持久化**：将校准参数存入STM32内部FLASH

---

详细使用方法见 `TOUCH_GUIDE.md`  
Keil工程配置见 `TOUCH_KEIL_SETUP.md`
