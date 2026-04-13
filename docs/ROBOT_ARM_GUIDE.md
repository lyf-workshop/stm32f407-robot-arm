# Robot Arm Control System - User Guide

## Overview

This project implements a 6-DOF robot arm control system based on STM32F407 + CAN bus + ZDT Emm V5 closed-loop stepper motor drivers. The core algorithms are ported from the [zero-robotic-arm](https://gitee.com/dearxie/zero-robotic-arm) open-source project.

## Features

- **Inverse Kinematics**: Pieper analytical method (deterministic, fast)
- **Joint Space Control**: Direct joint angle control with synchronization
- **Cartesian Space Control**: End-effector position control (X,Y,Z)
- **UART Command Interface**: Serial command parser for testing and RL integration
- **RL Ready**: Pre-defined `rl_step` command for reinforcement learning interface

## Hardware Setup

### Required Components

- STM32F407VET6 development board
- 6x ZDT XS series closed-loop stepper motors (Emm V5 firmware)
- CAN transceiver (PB8=RX, PB9=TX)
- UART-USB adapter (PA9=TX, PA10=RX)

### CAN Bus Wiring

- Motor addresses: 1, 2, 3, 4, 5, 6
- CAN baud rate: 500 kbps
- Topology: Linear bus with 120Ω termination resistors at both ends

### UART Connection

- Baud rate: 115200
- Data bits: 8
- Stop bits: 1
- Parity: None

## Building the Project

1. Open `STM32F407_CAN_CMD.ioc` with STM32CubeMX (optional, only if you need to modify peripheral configurations)
2. Open `MDK-ARM/STM32F407_CAN_CMD.uvprojx` with Keil MDK
3. Build the project (F7)
4. Download to target (F8)

## Serial Commands

### Basic Commands

#### `rel_rotate <joint_id> <angle>`
Rotate a joint by relative angle (degrees).

Example:
```
rel_rotate 0 10.0    // Rotate joint 0 by 10 degrees
rel_rotate 1 -15.5   // Rotate joint 1 by -15.5 degrees
```

#### `abs_rotate <joint_id> <angle>`
Rotate a joint to absolute angle (degrees).

Example:
```
abs_rotate 2 90.0    // Move joint 2 to 90 degrees
```

#### `sync <a1> <a2> <a3> <a4> <a5> <a6>`
Move all joints to target angles simultaneously (multi-motor synchronization).

Example:
```
sync 90 90 0 0 90 0   // All joints to specified angles
```

#### `auto <x> <y> <z>`
Move end-effector to Cartesian position (mm). Inverse kinematics is calculated automatically.

Example:
```
auto 0 -184.5 215.5   // Move to position (0, -184.5, 215.5) mm
auto 100 -150 300     // Move to position (100, -150, 300) mm
```

#### `zero`
Set current position as zero reference point. Call this after manually moving the arm to mechanical zero position.

Example:
```
zero
```

#### `stop`
Emergency stop all motors.

Example:
```
stop
```

#### `status`
Display current joint angles.

Example:
```
status
```
Response:
```
Joints: [90.00, 90.00, -90.00, 0.00, 90.00, 0.00] deg
```

### Reinforcement Learning Interface

#### `rl_step <d1> <d2> <d3> <d4> <d5> <d6>`
Apply joint angle increments (for RL action execution). Returns current state after motion.

Example:
```
rl_step 0.5 -0.3 0.2 0.0 0.1 0.0
```
Response:
```
state 90.50 89.70 -89.80 0.00 90.10 0.00
```

This command is designed for integration with Python RL frameworks:
- Receive action from RL agent (6 joint angle deltas)
- Execute motion
- Return observation (current joint angles) for reward calculation

## Testing Procedure

### Phase 1: Power-On Verification

1. Connect power supply to motor drivers
2. Connect CAN bus (verify termination resistors)
3. Connect UART-USB adapter
4. Download firmware
5. Open serial terminal (115200 baud)
6. You should see welcome message:
   ```
   === Robot Arm Control System ===
   Commands:
   ...
   Ready>
   ```

### Phase 2: Individual Joint Testing

Test each joint one by one to verify:
- Motor direction correctness
- Gear reduction ratio accuracy
- Movement smoothness

Example test sequence:
```
rel_rotate 0 10      // Joint 0: rotate 10 deg CW
status               // Check angle changed
rel_rotate 0 -10     // Joint 0: rotate back
rel_rotate 1 5       // Joint 1: rotate 5 deg
...
```

### Phase 3: Synchronized Motion

Test multi-motor synchronization:
```
sync 90 90 0 0 90 0  // All joints move together
```

All motors should start and stop simultaneously.

### Phase 4: Inverse Kinematics

Test Cartesian space control:
```
auto 0 -184.5 215.5   // Forward position
auto 100 -150 200     // Move to another position
```

The arm should move smoothly to the target with correct joint coordination.

### Phase 5: RL Interface (Optional)

Test reinforcement learning command:
```
rl_step 1.0 0.0 0.0 0.0 0.0 0.0
```

Should return current state after motion.

## Troubleshooting

### No serial output
- Check UART connections (TX/RX crossed)
- Verify baud rate (115200)
- Check USB-UART driver installation

### Motors not moving
- Verify CAN bus wiring and termination
- Check motor power supply
- Check motor addresses (should be 1-6)
- Verify CAN baud rate matches (500 kbps)

### Wrong movement direction
- Adjust `pos_direction` in `JOINT_CONFIG` in `robot_config.c`
- Recompile and download

### IK fails (auto command returns error)
- Target position may be out of reachable workspace
- Check joint angle limits in `robot_config.c`
- Verify DH parameters match your physical robot

## DH Parameters

If your robot arm dimensions differ from the zero-robotic-arm project, you need to update DH parameters in `robot_config.c`:

```c
const float D_H[6][4] = {
    {a,  alpha,  d,  theta_offset},  // Unit: mm for a/d, radians for angles
    ...
};
```

Measure your robot and update accordingly.

## Future Enhancements

### Reinforcement Learning Integration

The original zero-robotic-arm project provides a complete TD3 RL training framework:
- Simulation environment: MuJoCo + URDF
- Training script: `train_robot_arm.py` (Stable-Baselines3)
- Deployment: Python script sends `rl_step` commands via UART

See: `https://gitee.com/dearxie/zero-robotic-arm/tree/master/5.%20Deep_RL`

### Trajectory Planning

Current implementation uses immediate motion commands. For smooth trajectories:
- Add trapezoidal velocity profiling
- Implement linear/circular interpolation
- Use TIM2 periodic updates (10ms) for continuous motion

## Credits

Core algorithms based on [零一造物_ZERO机械臂](https://gitee.com/dearxie/zero-robotic-arm) by dearxie.
Motor control via ZDT Emm V5 protocol.

## License

GPL-2.0 (inherited from zero-robotic-arm project)
