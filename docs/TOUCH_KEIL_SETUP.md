# 触摸控制功能 - Keil 工程配置补充

## 新增文件（触摸功能）

在原有LCD功能的基础上，新增以下文件：

| 文件 | 功能 |
|------|------|
| `Inc/touch.h` | XPT2046触摸驱动头文件 |
| `Inc/touch_ui.h` | 触摸按钮UI头文件 |
| `Inc/touch_calib.h` | 触摸校准头文件 |
| `Src/touch.c` | XPT2046软件SPI驱动实现 |
| `Src/touch_ui.c` | 虚拟按钮界面（点动控制）|
| `Src/touch_calib.c` | 4点校准算法实现 |

## Keil 配置步骤

### 添加源文件到 LCD 组

在 Keil 的 Project 窗口中，右键 `LCD` 组 → **Add Existing Files**，添加：
- `../Src/touch.c`
- `../Src/touch_ui.c`
- `../Src/touch_calib.c`

### 编译选项（无需修改）

继续使用之前的设置：
- `Target → Use MicroLIB` ✅
- `C/C++ → Use FPU` ✅

## 代码集成点

### 1. 主程序 `main.c`

新增初始化：
```c
#include "touch.h"
#include "touch_ui.h"
#include "touch_calib.h"

int main(void) {
    // ... 其他初始化 ...
    Touch_Init();        // 初始化XPT2046触摸控制器
    TouchUI_Init();      // 绘制触摸按钮界面
    
    while (1) {
        // 触摸轮询（50ms周期）
        // 自动处理触摸事件 → 调用机械臂控制函数
    }
}
```

### 2. 命令解析 `robot_cmd.c`

新增串口命令：
```c
#include "touch_calib.h"

// 在 execute_command 中添加：
if (strcmp(cmd, "touch_calib") == 0) {
    TouchCalib_Start();  // 启动触摸校准流程
}
```

## 编译验证

编译后如果出现以下错误，参考解决方案：

| 错误信息 | 原因 | 解决 |
|----------|------|------|
| `Undefined symbol Touch_Init` | touch.c 未加入工程 | 将 `Src/touch.c` 加入 LCD 组 |
| `Undefined symbol TouchUI_Init` | touch_ui.c 未加入工程 | 将 `Src/touch_ui.c` 加入 LCD 组 |
| `Undefined symbol TouchCalib_Start` | touch_calib.c 未加入工程 | 将 `Src/touch_calib.c` 加入 LCD 组 |
| `Undefined reference to memcpy` | MicroLIB未启用 | 确认 Target → Use MicroLIB ✅ |

## 功能测试

### 测试1：触摸检测
1. 烧录固件，观察LCD显示触摸按钮界面
2. 用手指触摸屏幕任意位置，观察是否有响应
3. 如果无响应，执行下一步校准

### 测试2：触摸校准
1. 串口发送 `touch_calib`
2. LCD显示第1个红色十字（左上角），用触摸笔或指甲点击十字中心
3. 依次点击4个十字（左上 → 右上 → 左下 → 右下）
4. 显示 "Calibration Complete!"，触摸屏幕任意位置返回

### 测试3：按钮控制
1. 触摸底部 `5deg` 按钮，观察是否变为黄色高亮
2. 触摸 `J1+` 按钮，观察关节1是否正转5度
3. 触摸 `ZERO` 按钮，观察状态显示是否变为 "OK: Zero set"（绿色）

## 性能参数

| 参数 | 值 | 说明 |
|------|-----|------|
| 触摸采样率 | 20Hz（50ms周期）| 在 main.c 主循环中 |
| SPI时钟 | ~500kHz | 软件SPI（可调整 spi_delay）|
| 触摸分辨率 | 12-bit（0~4095）| XPT2046 ADC精度 |
| 按钮尺寸 | 80×30 像素 | 适合手指点击 |
| 防抖 | 上升沿检测 | 仅在按下瞬间触发一次 |

## 引脚占用情况

| 引脚 | 原用途 | 新用途（触摸） | 冲突？ |
|------|--------|---------------|-------|
| PD13 | 未使用 | T_CLK | ✅ 无冲突 |
| PE0  | 未使用 | T_CS  | ✅ 无冲突 |
| PE2  | 未使用 | T_MOSI | ✅ 无冲突 |
| PE3  | 未使用 | T_MISO | ✅ 无冲突 |

## 故障排查

### 触摸无反应
- 检查FPC排线是否插紧（LCD排线有34针，包含触摸信号）
- 用万用表测量 T_CS (PE0) 是否有高电平
- 串口发送 `status` 确认主程序正常运行

### 触摸位置严重偏移
- 执行 `touch_calib` 重新校准
- 检查LCD方向设置（当前代码为横屏320×240）
- 如果校准后仍偏移，检查 `touch.c` 中 `s_default_calib` 系数

### 按钮误触发（一次点击响应多次）
- 增加防抖时间：修改 `main.c` 中触摸轮询周期从50ms改为100ms
- 或在 `touch.c` 中增加 `Touch_IsPressed()` 的判断阈值

## 下一步扩展建议

如果需要更丰富的交互，可以参考以下方向：
1. **滑块控件**：连续拖动调节角度（需要实现触摸跟踪）
2. **虚拟键盘**：输入精确数值（需要绘制0-9数字键盘）
3. **多页面切换**：触摸TAB切换"手动控制"/"演示"/"设置"页面
4. **触摸示教**：长按"记录"按钮保存轨迹点

---

详细使用方法见 `TOUCH_GUIDE.md`。
