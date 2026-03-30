# STM32F407 六轴机械臂控制系统

> **本项目基于 [zero-robotic-arm](https://gitee.com/jmx_123456/zero-robotic-arm) 改进而来。**  
> 原项目作者：[@jmx_123456](https://gitee.com/jmx_123456)  
> 原项目地址：https://gitee.com/jmx_123456/zero-robotic-arm  
> 感谢原作者的开源贡献！

---

## 与原项目的关系

| 项目 | 链接 |
|------|------|
| **原项目（上游）** | https://gitee.com/jmx_123456/zero-robotic-arm |
| **本项目（改进版）** | 当前仓库 |

本项目在原项目的基础上进行了以下改进和移植：

| 改进项 | 说明 |
|--------|------|
| **通信平台** | 从 TTL 串口通信改为 **CAN 总线**多机同步控制 |
| **硬件平台** | 适配 **STM32F407VET6**（HAL 库，Keil MDK-ARM） |
| **裸机化** | 去掉 FreeRTOS 和 MQTT 依赖，改为裸机运行降低复杂度 |
| **LCD 显示** | 新增 **2.8寸 ILI9341 LCD**（FSMC 16位并口）实时显示机械臂状态 |
| **触摸控制** | 新增 **XPT2046 触摸屏**（SPI），支持触摸按钮点动控制 |
| **编码器回读** | 新增 `sync_pos` 和 `calibrate` 指令，从电机编码器读取真实角度 |
| **RL 接口** | 新增 `rl_step` 指令和 Python OpenAI-Gym 风格接口，用于强化学习对接 |
| **串口指令扩展** | 增加 `sync_pos`、`calibrate`、`touch_calib`、`demo_*` 等新指令 |

---

## 功能特性

- **6轴机械臂控制**：通过 CAN 总线 500kbps 控制 6 个 Emm V5 闭环步进电机
- **逆运动学**：Pieper 解析法（DH 参数），支持笛卡尔空间坐标控制
- **多轴同步**：使用 `Emm_V5_Synchronous_motion` 实现多电机同步运动
- **LCD 状态显示**：关节角度、末端坐标、指令、状态实时刷新
- **触摸控制**：XPT2046 电阻式触摸屏，支持虚拟按钮点动控制（J1~J6 +/-）
- **触摸校准**：4点校准算法，自动计算仿射变换矩阵
- **串口命令行**：115200 波特率，支持多种控制命令
- **强化学习接口**：Python `robot_rl_interface.py` 提供 Gym 风格环境

---

## 硬件平台

| 组件 | 规格 |
|------|------|
| 主控 | STM32F407VET6（LQFP100） |
| 电机 | 6× Emm V5 闭环步进电机 |
| 通信 | CAN 总线，500kbps，PB8(RX)/PB9(TX) |
| 串口 | USART1，115200 baud，PA9(TX)/PA10(RX) |
| 显示 | 2.8寸 ILI9341 TFT LCD，320×240，FSMC 16位 |
| 触摸 | XPT2046 电阻式触摸，软件SPI（PD13/PE0/PE2/PE3）|
| 背光 | PA15 GPIO |

---

## 快速上手

### 第 1 步：Keil 工程配置

1. 打开 `MDK-ARM/STM32F407_CAN_CMD.uvprojx`
2. 添加源文件到 **Application/User** 组：
   ```
   ../Src/robot_config.c
   ../Src/robot_kinematics.c
   ../Src/robot_control.c
   ../Src/robot_cmd.c
   ../Src/robot_sequence.c
   ../Src/usart.c
   ```
3. 新建 **LCD** 组，添加：
   ```
   ../Src/lcd.c
   ../Src/lcd_font.c
   ../Src/robot_display.c
   ../Src/touch.c
   ../Src/touch_ui.c
   ../Src/touch_calib.c
   ```
4. 编译器选项：`Target → Use MicroLIB ✅`，`C/C++ → Use FPU ✅`
5. `Project → Rebuild all`（F7）→ `Flash → Download`（F8）

### 第 2 步：硬件连接

```
CAN 总线：
  STM32 PB8(RX) ──┐
  STM32 PB9(TX) ──┤── CAN收发器 ── 驱动器1(addr=1) ── ... ── 驱动器6(addr=6)
  ⚠️ 两端各接 120Ω 终端电阻，驱动器波特率设为 500kbps

串口（USB 转串口）：
  STM32 PA9(TX)  → RX
  STM32 PA10(RX) ← TX
  GND            ─ GND

LCD（已通过 FSMC 引脚自动初始化，直接接线即可）：
  见 LCD_KEIL_SETUP.md 引脚对照表
```

### 第 3 步：串口测试

打开串口工具（115200，发送需附加换行符 `\r\n`）：

```
status              → 显示当前关节角度（软件追踪）
sync_pos            → 从编码器读取真实角度并同步
calibrate 0         → 对比 J0 的软件角度与编码器角度
zero                → 将当前位置设为零点
rel_rotate 0 30     → J0 相对转动 30°
sync 90 90 0 0 90 0 → 6轴同步移动到指定角度
auto 0 -184.5 215.5 → 末端移动到笛卡尔坐标 (x,y,z)
stop                → 急停
touch_calib         → 触摸屏4点校准（在LCD上操作）
```

### 第 4 步：触摸控制测试

1. **选择步长**：触摸底部 `5deg` 按钮（黄色高亮表示已选中）
2. **点动关节**：触摸 `J1+` 使关节1正转5度，触摸 `J1-` 反转5度
3. **归零**：移动到合适位置后，触摸 `ZERO` 按钮
4. **同步角度**：触摸 `STATUS` 按钮从编码器读取真实位置

如果触摸位置不准，串口发送 `touch_calib` 命令，在屏幕上依次点击4个十字完成校准。

---

## 串口命令速查

| 命令 | 说明 |
|------|------|
| `status` | 显示软件追踪的关节角度 |
| `sync_pos` | 从电机编码器读取真实角度并更新 |
| `calibrate <id>` | 显示某轴软件角度 vs 编码器角度对比 |
| `touch_calib` | 触摸屏4点校准（在LCD上点击十字）|
| `rel_rotate <id> <deg>` | 指定关节相对转动 |
| `abs_rotate <id> <deg>` | 指定关节绝对定位 |
| `sync <a1..a6>` | 6轴同步绝对定位 |
| `auto <x> <y> <z>` | 笛卡尔空间逆运动学控制 |
| `zero` | 当前位置设为零点 |
| `stop` | 急停所有电机 |
| `demo_joint` | 关节空间演示 |
| `demo_pick` | 取放物演示 |
| `rl_step <d1..d6>` | 强化学习增量步 |

---

## 强化学习接口（Python）

```bash
pip install pyserial numpy
python robot_rl_interface.py
```

```python
from robot_rl_interface import RobotArmEnv

env = RobotArmEnv(port='COM3')
obs = env.reset()
for _ in range(100):
    action = [1.0, 0, 0, 0, 0, 0]   # 6个关节的角度增量（度）
    obs, reward, done, info = env.step(action)
```

---

## 致谢 / Credits

本项目的运动学算法、DH 参数、机械臂结构参数均来自原项目：

**zero-robotic-arm** by [@jmx_123456](https://gitee.com/jmx_123456)  
🔗 https://gitee.com/jmx_123456/zero-robotic-arm

感谢原作者的开源贡献，本项目的改进工作建立在原项目代码的基础上完成。

---

## 详细文档

- **LCD 配置**：`LCD_KEIL_SETUP.md`
- **触摸控制**：`TOUCH_GUIDE.md`
- **角度标定**：`ROBOT_ARM_GUIDE.md`
- **Keil 工程配置**：`KEIL_SETUP.md`
- **移植说明**：`MIGRATION_SUMMARY.md`
- **测试指南**：`TESTING_GUIDE.md`

---

## License

本项目遵循原项目的开源协议。原项目：https://gitee.com/jmx_123456/zero-robotic-arm
