# 快速开始指南 / Quick Start Guide

## 5 分钟快速上手

### 第 1 步：编译固件 (2 分钟)

1. 打开 Keil MDK-ARM
2. 打开工程：`MDK-ARM/STM32F407_CAN_CMD.uvprojx`
3. **重要**：添加新文件到工程（右键 Application/User 组 → Add Existing Files）：
   ```
   ../Src/robot_config.c
   ../Src/robot_kinematics.c
   ../Src/robot_control.c
   ../Src/robot_cmd.c
   ../Src/robot_sequence.c
   ../Src/usart.c
   ```
4. **重要**：启用编译器选项：
   - Target → Use MicroLIB ✅
   - C/C++ → Use FPU ✅
5. 编译：`Project → Rebuild all` (F7)
6. 下载：`Flash → Download` (F8)

### 第 2 步：连接硬件 (2 分钟)

**CAN 总线：**
```
STM32 PB8(RX) ─┐
STM32 PB9(TX) ─┤→ CAN收发器 → 驱动器1(地址1) → ... → 驱动器6(地址6)
```
- ⚠️ 两端接 120Ω 终端电阻
- ⚠️ 驱动器波特率设为 500kbps

**串口：**
```
STM32 PA9(TX)  → USB转串口 RX
STM32 PA10(RX) ← USB转串口 TX
GND            ─ GND
```

### 第 3 步：测试 (1 分钟)

1. 打开串口工具（115200 波特率）
2. 复位 STM32，看到欢迎信息
3. 输入命令测试：
   ```
   status              # 查看当前关节角度
   rel_rotate 0 10     # 关节 0 转 10 度
   ```

## 命令速查表

| 命令 | 示例 | 说明 |
|------|------|------|
| `status` | `status` | 显示当前关节角度 |
| `rel_rotate` | `rel_rotate 0 10` | 关节 0 相对转动 10° |
| `sync` | `sync 90 90 0 0 90 0` | 所有关节同步到指定角度 |
| `auto` | `auto 0 -184.5 215.5` | 末端移动到 (x,y,z) 坐标 |
| `zero` | `zero` | 将当前位置设为零点 |
| `stop` | `stop` | 急停所有电机 |

## 演示序列

执行内置演示动作：

```
demo_joint     # 关节空间演示
demo_pick      # 取放物演示
demo_circle    # 圆周运动演示
```

## 强化学习接入（Python）

```bash
pip install pyserial numpy
python robot_rl_interface.py
```

或在代码中：

```python
from robot_rl_interface import RobotArmEnv

env = RobotArmEnv(port='COM3')
obs = env.reset()

for step in range(100):
    action = [1.0, 0, 0, 0, 0, 0]  # 关节角度增量
    obs, reward, done, info = env.step(action)
    print(f"State: {obs}")
```

## 常见问题

### 串口没有输出？
- 检查 TX/RX 是否交叉连接
- 确认波特率 115200

### 电机不转？
- 检查 CAN 接线（CANH、CANL）
- 确认终端电阻（120Ω）
- 检查驱动器地址（1~6）

### IK 失败？
- 先执行 `zero` 命令归零
- 确认目标点在工作空间内
- 检查 DH 参数是否与实际机械臂一致

## 详细文档

- **使用手册**：`ROBOT_ARM_GUIDE.md`
- **测试指南**：`TESTING_GUIDE.md`
- **编译配置**：`BUILD_GUIDE.md`
- **移植总结**：`MIGRATION_SUMMARY.md`

## 原项目链接

- Gitee：https://gitee.com/dearxie/zero-robotic-arm
- B站：https://www.bilibili.com/video/BV1d63Dz7ERN/

## 技术支持

QQ 交流群：1041684044

---

**开始使用吧！🚀**
