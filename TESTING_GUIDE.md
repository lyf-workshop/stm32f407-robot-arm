# 机械臂测试指南 / Robot Arm Testing Guide

## 测试前准备 / Pre-Test Setup

### 硬件连接 Hardware Connections

1. **电源 Power**
   - 给所有 6 个电机驱动器供电（24V 推荐）
   - STM32 开发板供电（USB 或外部 5V）

2. **CAN 总线 CAN Bus**
   ```
   [STM32 PB9(TX)] -----> [CAN收发器] -----> [驱动器1] -----> [驱动器2] ... [驱动器6]
   [STM32 PB8(RX)] -----> [CAN收发器]           ↑                              ↑
                                              120Ω                          120Ω
   ```
   - 总线两端必须接 120Ω 终端电阻
   - 驱动器 CAN 地址设置为 1, 2, 3, 4, 5, 6

3. **串口 UART**
   ```
   [STM32 PA9(TX)]  ----->  [USB转串口 RX]
   [STM32 PA10(RX)] <-----  [USB转串口 TX]
   [GND]            ------  [GND]
   ```

4. **串口工具 Serial Terminal**
   - 波特率：115200
   - 数据位：8
   - 停止位：1
   - 校验：无
   - 推荐工具：PuTTY / Tera Term / Xshell / 串口助手

### 驱动器设置 Motor Driver Configuration

在电机驱动器小屏幕菜单中设置：
1. **CAN 通信**
   - 波特率：500 kbps
   - 地址：分别设为 1, 2, 3, 4, 5, 6
   
2. **细分 Subdivision**
   - 设置为 16 细分（与代码中 `ROBOT_PULSES_PER_REV = 3200` 对应）

3. **到位返回 Reached Response**（可选）
   - 建议暂时关闭，避免总线冲突
   - 调试稳定后可单独开启某一轴测试

---

## 阶段 1：上电验证 Phase 1: Power-On Verification

### 步骤 Steps

1. 烧录固件到 STM32
2. 打开串口工具，连接开发板
3. 复位 STM32（按下复位按钮）

### 预期输出 Expected Output

```
=== Robot Arm Control System ===
Commands:
  rel_rotate <joint_id> <angle>   - Relative rotation
  abs_rotate <joint_id> <angle>   - Absolute rotation
  sync <a1> <a2> <a3> <a4> <a5> <a6> - Sync all joints
  auto <x> <y> <z>                - Cartesian move (IK)
  zero                            - Set current as zero
  stop                            - Emergency stop
  status                          - Show joint angles
  rl_step <d1> <d2> <d3> <d4> <d5> <d6> - RL action step
Ready>
```

### 验证点 Checkpoints

- [x] 串口输出正常
- [x] 电机驱动器指示灯正常（绿灯常亮）
- [x] 无报错信息

---

## 阶段 2：逐轴测试 Phase 2: Individual Joint Testing

### 目的 Purpose

验证每个关节的：
- 运动方向是否正确
- 减速比设置是否准确
- 运动是否平滑

### 测试序列 Test Sequence

#### 关节 0（底座旋转）Joint 0 (Base Rotation)

```
status                  // 记录初始角度 Record initial angle
rel_rotate 0 10         // 顺时针转 10° Rotate 10° CW
status                  // 检查角度是否变为 initial+10
rel_rotate 0 -10        // 逆时针转回 Rotate back
status                  // 应回到初始角度 Should return to initial
```

**观察 Observations:**
- 电机应该平滑转动约 10 度
- 串口应返回 `OK: J0 moved by 10.00 deg`
- `status` 命令应显示角度变化

**常见问题 Troubleshooting:**
- 如果方向相反：修改 `robot_config.c` 中 `JOINT_CONFIG[0].pos_direction`
- 如果角度不对：检查减速比 `JOINT_CONFIG[0].reduction_ratio`

#### 重复测试其他关节 Repeat for Other Joints

```
rel_rotate 1 5          // Joint 1
rel_rotate 2 5          // Joint 2
rel_rotate 3 5          // Joint 3
rel_rotate 4 5          // Joint 4
rel_rotate 5 5          // Joint 5
```

### 验证清单 Checklist

- [ ] 关节 0：方向正确，角度准确
- [ ] 关节 1：方向正确，角度准确
- [ ] 关节 2：方向正确，角度准确
- [ ] 关节 3：方向正确，角度准确
- [ ] 关节 4：方向正确，角度准确
- [ ] 关节 5：方向正确，角度准确

---

## 阶段 3：多轴同步测试 Phase 3: Synchronized Motion Testing

### 目的 Purpose

验证多机同步功能，所有轴应同时启动、同时停止。

### 测试命令 Test Commands

```
sync 90 90 0 0 90 0
```

### 观察 Observations

- 所有 6 个电机应该 **同时启动**
- 所有电机应该 **同时停止**
- 串口返回：`OK: All joints synchronized`

### 预期行为 Expected Behavior

如果某个轴运动距离较长，其他轴会自动降低速度以保证同步到达。

---

## 阶段 4：归零标定 Phase 4: Zero Calibration

### 重要说明 Important

在使用 `auto` 命令（笛卡尔空间控制）之前，必须先进行归零标定，否则逆运动学的绝对坐标无意义。

### 归零步骤 Zero Calibration Steps

1. **手动调整机械臂到已知位姿**
   - 推荐姿态：所有关节在机械零位（通常是初始伸直状态）
   - 或参考 `T_0_6_RESET` 矩阵对应的姿态

2. **发送归零命令**
   ```
   zero
   ```
   
3. **验证**
   ```
   status
   ```
   应显示当前定义的角度（通常为初始值）

---

## 阶段 5：逆运动学验证 Phase 5: Inverse Kinematics Verification

### 前提条件 Prerequisites

- [x] 已完成归零标定
- [x] 机械臂处于已知初始位姿

### 测试点位 Test Positions

#### 测试点 1：正前方 Front Position

```
auto 0 -184.5 215.5
```

**预期行为:**
- 机械臂末端应移动到该坐标位置
- 各关节协调运动
- 串口返回：`OK: Moved to (0.0, -184.5, 215.5)`

#### 测试点 2：侧方 Side Position

```
auto 100 -150 200
```

#### 测试点 3：回初始位 Return

```
auto 0 -184.5 215.5
```

### 失败排查 Troubleshooting

如果返回 `Error: IK failed or move out of range`：

1. **位置超出工作空间**
   - 减小 X/Y/Z 值
   - 确保目标点在机械臂可达范围内

2. **DH 参数不匹配**
   - 检查 `robot_config.c` 中的 `D_H` 参数
   - 与实际机械臂尺寸对比
   - 若尺寸不同，需要重新测量并修改

3. **关节限位问题**
   - 检查 `JOINT_LIMITS` 设置
   - 某个关节可能达到限位

---

## 阶段 6：强化学习接口测试 Phase 6: RL Interface Testing

### RL 命令测试 RL Command Test

```
rl_step 1.0 0.0 0.0 0.0 0.0 0.0
```

### 预期输出 Expected Output

```
state 91.00 90.00 -90.00 0.00 90.00 0.00
```

返回格式：`state <θ1> <θ2> <θ3> <θ4> <θ5> <θ6>`

### Python 集成示例 Python Integration Example

```python
import serial
import time

ser = serial.Serial('COM3', 115200, timeout=1)
time.sleep(2)  # Wait for board reset

def send_rl_action(delta_angles):
    """Send RL action and get state feedback"""
    cmd = f"rl_step {' '.join(map(str, delta_angles))}\r\n"
    ser.write(cmd.encode())
    
    # Read response
    response = ser.readline().decode().strip()
    if response.startswith('state'):
        angles = list(map(float, response.split()[1:]))
        return angles
    return None

# Example: move joint 0 by 1 degree
state = send_rl_action([1.0, 0, 0, 0, 0, 0])
print(f"Current state: {state}")
```

---

## 常见问题 FAQ

### Q1: 串口没有输出
- 检查 TX/RX 是否交叉连接
- 确认波特率 115200
- 检查 USB 驱动是否安装

### Q2: 电机不转
- 检查 CAN 总线接线（CANH、CANL）
- 确认终端电阻已接（120Ω × 2）
- 检查驱动器地址设置（1~6）
- 确认驱动器波特率为 500 kbps

### Q3: 方向反了
修改 `robot_config.c` 中对应关节的 `pos_direction`：
```c
{90.0f, MOTOR_DIR_CW, ...}  // 改为 MOTOR_DIR_CCW
```

### Q4: 角度不准确
检查并修改 `robot_config.c` 中的减速比：
```c
const float GEAR_RATIO[6] = {50.0f, 50.89f, ...};
```

### Q5: IK 总是失败
- 确认 DH 参数与实际机械臂一致
- 目标点可能超出工作空间
- 尝试更接近当前位置的点

---

## 性能指标参考 Performance Reference

基于原项目（零一造物 ZERO 机械臂）：

- **工作空间半径**: 约 400-500 mm
- **重复定位精度**: ±2° (受行星减速器旷量影响)
- **IK 求解速度**: < 1 ms (STM32F407 @ 168MHz)
- **CAN 通信延迟**: 2-5 ms per motor
- **末端负载**: < 500g（3D 打印结构刚性限制）

---

## 下一步开发建议 Next Steps

### 1. 添加轨迹规划 Trajectory Planning

当前实现为点到点运动，可以改进为连续轨迹：
- 梯形速度规划
- 线性/圆弧插值
- TIM2 定时器（10ms）周期更新

### 2. 集成 FreeRTOS

原项目使用 FreeRTOS 多任务架构，后期可升级：
- UART 命令任务
- 运动控制任务
- MQTT 通信任务（可选）

### 3. 强化学习训练 RL Training

参考原项目 Deep_RL 目录：
- MuJoCo 物理仿真
- TD3 算法训练
- Sim-to-Real 迁移

### 4. 添加限位开关支持

原项目支持硬件限位开关，实现自动归零：
- GPIO 读取限位开关状态
- `hard_reset` 命令自动找限位

---

## 参考资料 References

- [零一造物 ZERO 机械臂开源项目](https://gitee.com/dearxie/zero-robotic-arm)
- [ZDT Emm V5 协议手册](https://zhangdatou.taobao.com)
- [STM32F407 数据手册](https://www.st.com/en/microcontrollers-microprocessors/stm32f407-417.html)

---

## 技术支持 Support

如有问题，欢迎加入零一造物技术交流群：
- QQ 群：1041684044
- QQ 2群：1029351597
