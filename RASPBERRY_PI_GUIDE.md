# 树莓派控制指南

通过树莓派的 UART 串口连接 STM32F407，实现对机械臂的完整控制。

---

## 一、硬件接线

### 接线表

| 树莓派引脚 | GPIO 编号 | 功能 | → | STM32 引脚 | 功能 |
|-----------|----------|------|---|-----------|------|
| Pin 8     | GPIO14   | **TXD** | → | **PA10**  | USART1 RX |
| Pin 10    | GPIO15   | **RXD** | → | **PA9**   | USART1 TX |
| Pin 6/14  | GND      | GND  | → | GND       | GND |

> **注意：TX 接 RX，RX 接 TX（交叉连接）**  
> 两者均为 3.3V 逻辑电平，**无需电平转换**，但必须共地。

### 接线图

```
树莓派                         STM32F407
┌─────────────────┐           ┌─────────────────┐
│  Pin 8  GPIO14  │──────────▶│ PA10  USART1_RX  │
│         (TXD)   │           │                  │
│ Pin 10  GPIO15  │◀──────────│ PA9   USART1_TX  │
│         (RXD)   │           │                  │
│  Pin 6  GND     │───────────│ GND              │
└─────────────────┘           └─────────────────┘
```

### 备用方案：USB 转 TTL 模块

如果不想占用树莓派的硬件 UART，可使用 CH340/CP2102 USB 转 TTL 模块：

```
树莓派 USB  →  USB 转 TTL 模块  →  STM32
                    TXD  ────────▶  PA10 (RX)
                    RXD  ◀────────  PA9  (TX)
                    GND  ──────────  GND
```

脚本中将端口改为 `/dev/ttyUSB0`：
```python
SERIAL_PORT = "/dev/ttyUSB0"
```

---

## 二、树莓派 UART 配置

### 步骤 1：启用 UART 接口

```bash
sudo raspi-config
```

选择路径：`Interface Options` → `Serial Port`

- **"Would you like a login shell to be accessible over the serial port?"** → **No**
- **"Would you like the serial port hardware to be enabled?"** → **Yes**

完成后重启：
```bash
sudo reboot
```

### 步骤 2：确认串口设备

重启后检查可用串口：

```bash
ls -la /dev/ttyAMA* /dev/ttyS* 2>/dev/null
```

| 树莓派型号 | 推荐串口 | 说明 |
|-----------|---------|------|
| Pi 3B / 4B / 5 | `/dev/ttyAMA0` | 硬件 UART（需禁用蓝牙） |
| Pi 3B / 4B 备用 | `/dev/ttyS0` | mini UART（波特率精度低） |
| 所有型号（USB模块）| `/dev/ttyUSB0` | USB 转 TTL |

### 步骤 3（Pi 3B/4B 专用）：禁用蓝牙，释放硬件 UART

树莓派 3B/4B 默认将硬件 UART `/dev/ttyAMA0` 分配给蓝牙，需要手动释放：

```bash
# 编辑 /boot/config.txt（新版系统为 /boot/firmware/config.txt）
sudo nano /boot/config.txt
```

在文件末尾添加：
```ini
# 禁用蓝牙，释放硬件 UART 给 GPIO14/15
dtoverlay=disable-bt
```

禁用蓝牙服务：
```bash
sudo systemctl disable hciuart
sudo reboot
```

重启后 `/dev/ttyAMA0` 即指向 GPIO14/GPIO15 的硬件 UART。

### 步骤 4：添加用户串口权限

```bash
sudo usermod -aG dialout $USER
# 重新登录后生效
```

### 步骤 5：验证串口可用

```bash
# 安装串口调试工具
sudo apt install minicom -y

# 测试连接（确保 STM32 已上电）
minicom -D /dev/ttyAMA0 -b 115200
# 出现 "Robot Arm Control System" 欢迎信息说明连接正常
# 按 Ctrl+A，然后 Q 退出
```

---

## 三、安装 Python 依赖

```bash
pip3 install pyserial
```

---

## 四、运行控制脚本

### 连接测试

```bash
python3 raspberry_pi_control.py --test
```

预期输出：
```
[已连接] /dev/ttyAMA0 @ 115200 baud
[测试] 串口连接测试...
  << Joints: [0.00, 90.00, -90.00, 0.00, 90.00, 0.00] deg
[测试] 通信正常 ✓
       关节角度: ['0.00°', '90.00°', '-90.00°', '0.00°', '90.00°', '0.00°']
```

### 交互式控制台

```bash
python3 raspberry_pi_control.py
```

进入交互模式后可直接输入命令：

```
robot> status
  << Joints: [0.00, 90.00, -90.00, 0.00, 90.00, 0.00] deg
robot> rel_rotate 0 15
  << OK: J0 moved by 15.00 deg
robot> sync_pos
  << OK: Synced [15.00, 90.02, -89.97, 0.01, 90.03, 0.01] deg
robot> stop
  << OK: All motors stopped
robot> quit
```

### 运行演示

```bash
python3 raspberry_pi_control.py --demo
```

### 指定串口/波特率

```bash
python3 raspberry_pi_control.py --port /dev/ttyUSB0 --baud 115200
```

---

## 五、在自己的脚本中使用

```python
from raspberry_pi_control import RobotArmController

# 使用 with 语法，自动关闭串口
with RobotArmController(port="/dev/ttyAMA0") as robot:

    # 从编码器同步真实位置
    angles = robot.sync_pos()
    print(f"当前角度: {angles}")

    # 单轴绝对运动
    robot.abs_rotate(0, 45.0)   # 关节 0 转到 45°

    # 六轴同步运动
    robot.sync([45, 90, -90, 0, 90, 0])

    # 笛卡尔运动（IK 自动求解）
    robot.move_to_pose(x=150, y=0, z=200)

    # 紧急停止
    robot.stop()
```

### 用于强化学习

```python
from raspberry_pi_control import RobotArmController
import numpy as np

with RobotArmController(port="/dev/ttyAMA0", verbose=False) as robot:
    # 设置零点
    robot.zero()

    for step in range(100):
        # 产生随机增量动作（各轴 ±2°）
        deltas = np.random.uniform(-2, 2, size=6).tolist()

        # 执行并获取新状态
        new_angles = robot.rl_step(deltas)
        if new_angles is None:
            print("触发限位，停止")
            break

        print(f"Step {step}: {[f'{a:.1f}' for a in new_angles]}")
```

---

## 六、完整命令参考

| 命令 | 说明 | 示例 |
|------|------|------|
| `status` | 查询软件关节角度 | `robot.status()` |
| `sync_pos` | 从编码器读取实际角度 | `robot.sync_pos()` |
| `rel_rotate <j> <deg>` | 关节相对转动 | `robot.rel_rotate(0, 10.0)` |
| `abs_rotate <j> <deg>` | 关节绝对转动 | `robot.abs_rotate(2, -45.0)` |
| `sync <a0..a5>` | 六轴同步到目标角度 | `robot.sync([0,90,-90,0,90,0])` |
| `auto <x> <y> <z>` | 笛卡尔目标（IK） | `robot.move_to_pose(150,0,200)` |
| `rl_step <d0..d5>` | RL 步进（增量） | `robot.rl_step([1,0,0,0,0,0])` |
| `zero` | 设置当前为零点 | `robot.zero()` |
| `stop` | 紧急停止 | `robot.stop()` |
| `calibrate <j>` | 对比软件与编码器角度 | `robot.calibrate(0)` |
| `demo_joint` | 关节空间演示 | `robot.demo_joint()` |
| `demo_pick` | 抓取放置演示 | `robot.demo_pick()` |
| `demo_circle` | 圆弧轨迹演示 | `robot.demo_circle()` |

---

## 七、常见问题排查

### 串口打不开

```
[错误] 无法打开串口 /dev/ttyAMA0
```

- 确认已运行 `raspi-config` 启用 Serial
- 确认用户在 `dialout` 组：`groups $USER`
- 如使用 USB 模块，确认设备存在：`ls /dev/ttyUSB*`

### 无响应 / 乱码

- 检查 TX/RX 是否接反（TX→RX，RX→TX）
- 确认共地（GND 已连接）
- Pi 3B/4B：确认 `/dev/ttyAMA0` 没有被蓝牙占用，已按第三步配置

### Pi 3B/4B 用 `/dev/ttyS0` 波特率不准

- mini UART 依赖 CPU 频率，会导致波特率漂移
- 推荐固定 CPU 频率：在 `/boot/config.txt` 中添加 `core_freq=250`
- 或使用 USB 转 TTL 模块彻底回避此问题

### STM32 重启后需要重新连接

脚本初始化时已添加 `time.sleep(1.5)` 等待 STM32 启动，如仍超时可将其改大：

```python
# raspberry_pi_control.py 第 68 行
time.sleep(3.0)   # 加长等待时间
```
