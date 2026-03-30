# ⚡ 快速参考卡 / Quick Reference Card

## 📌 串口命令速查

### 基础命令
```
status                        # 查看当前角度
zero                          # 归零标定
stop                          # 急停
```

### 单轴控制
```
rel_rotate 0 10               # 关节0相对转10°
abs_rotate 1 90               # 关节1转到90°
```

### 多轴同步
```
sync 90 90 0 0 90 0           # 所有关节同步运动
```

### 笛卡尔控制
```
auto 0 -184.5 215.5           # 末端移动到(x,y,z)
```

### 演示序列
```
demo_joint                    # 关节空间演示
demo_pick                     # 取放物演示
demo_circle                   # 圆周运动演示
```

### 强化学习
```
rl_step 1.0 0 0 0 0 0         # RL动作执行
```

---

## 🔌 硬件快速接线

### CAN 总线
```
STM32 PB8(RX) ─┐
STM32 PB9(TX) ─┤─ CAN收发器 ─ 驱动器1~6
               └─ 120Ω 终端电阻（两端各一个）
```

### 串口
```
STM32 PA9(TX)  → USB转串口 RX
STM32 PA10(RX) ← USB转串口 TX
GND            ─ GND
```

### 驱动器设置
- 地址：1, 2, 3, 4, 5, 6
- 波特率：500 kbps
- 细分：16

---

## ⚙️ Keil 编译配置

### 添加文件（Application/User 组）
```
☑ robot_config.c
☑ robot_kinematics.c
☑ robot_control.c
☑ robot_cmd.c
☑ robot_sequence.c
☑ robot_examples.c
☑ usart.c
```

### 编译器选项
```
Target → Use MicroLIB          ☑
C/C++  → Use FPU               ☑
C/C++  → C99 Mode              ☑
```

---

## 🐍 Python RL 接口

### 安装
```bash
pip install pyserial numpy
```

### 使用
```python
from robot_rl_interface import RobotArmEnv

env = RobotArmEnv(port='COM3')
obs = env.reset()

for _ in range(100):
    action = [1.0, 0, 0, 0, 0, 0]  # 角度增量
    obs, reward, done, info = env.step(action)
```

---

## 🔍 故障排查速查

| 问题 | 解决方法 |
|------|---------|
| 串口无输出 | 检查 TX/RX 交叉、波特率 115200 |
| 电机不转 | 检查 CAN 接线、终端电阻、地址 |
| 编译错误：pow | 启用 Use MicroLIB |
| 编译错误：浮点 | 启用 Use FPU |
| IK 失败 | 先 `zero` 归零、检查工作空间 |
| 方向反 | 修改 `robot_config.c` 的 pos_direction |

---

## 📏 关键参数

### DH 参数（mm）
```
J1: {0, 0, 0}
J2: {0, π/2, 0}
J3: {200, π, 0}        ← a2 = 200mm
J4: {47.63, -π/2, -184.5}  ← a3/d4
J5: {0, π/2, 0}
J6: {0, π/2, 0}
```

### 减速比
```
J1~J4: 50~51
J5: 26.85
J6: 51
```

### 工作空间
```
半径：~450mm
高度：-100 ~ 400mm
```

---

## 📖 完整文档

| 需求 | 文档 |
|------|------|
| 快速开始 | `README.md` |
| Keil 配置 | `KEIL_SETUP.md` |
| 详细使用 | `ROBOT_ARM_GUIDE.md` |
| 硬件测试 | `TESTING_GUIDE.md` |
| 移植总结 | `MIGRATION_SUMMARY.md` |

---

## 💾 备份提示

建议在第一次编译前备份原始工程：
```
复制整个文件夹 → 重命名为 STM32F407_CAN_CMD_backup
```

---

**打印此卡片，放在桌边，随时查阅！📌**
