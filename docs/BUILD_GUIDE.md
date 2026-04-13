# 编译配置指南 / Build Configuration Guide

## Keil MDK-ARM 工程配置

### 添加源文件到工程 Add Source Files to Project

打开 `MDK-ARM/STM32F407_CAN_CMD.uvprojx`，在 Keil IDE 中：

#### 1. 在 Application/User 组中添加以下文件：

**右键 Application/User → Add Existing Files to Group 'Application/User'**

添加以下文件：
- `../Src/robot_config.c`
- `../Src/robot_kinematics.c`
- `../Src/robot_control.c`
- `../Src/robot_cmd.c`
- `../Src/robot_sequence.c`
- `../Src/usart.c`

#### 2. 验证头文件包含路径

**Project → Options for Target → C/C++ → Include Paths**

确保包含：
- `..\Inc`
- `..\Drivers\STM32F4xx_HAL_Driver\Inc`
- `..\Drivers\CMSIS\Device\ST\STM32F4xx\Include`
- `..\Drivers\CMSIS\Include`

（通常默认已配置）

### 编译器设置 Compiler Settings

**Project → Options for Target → C/C++**

- Optimization: `-O1` 或 `-O2` (推荐 -O1 便于调试)
- C99 Mode: 勾选 `--c99`
- Warnings: 建议启用 `-Wall`

**Language / Code Generation:**
- Use MicroLIB: 勾选（减小代码体积，支持 printf）

### 浮点运算支持 Floating Point Support

**Project → Options for Target → C/C++ → Floating Point Hardware**

- 选择：`Use FPU`（STM32F407 自带 FPU，显著加速运动学计算）

### 编译步骤 Build Steps

1. Clean Project: `Project → Clean Targets`
2. Rebuild All: `Project → Rebuild all target files` (F7)
3. 检查编译输出，确保 0 errors

### 预期编译输出 Expected Build Output

```
Build target 'STM32F407_CAN_CMD'
compiling robot_config.c...
compiling robot_kinematics.c...
compiling robot_control.c...
compiling robot_cmd.c...
compiling robot_sequence.c...
compiling usart.c...
compiling main.c...
...
linking...
Program Size: Code=XXXXX RO-data=XXXX RW-data=XXX ZI-data=XXXX
"STM32F407_CAN_CMD.axf" - 0 Error(s), 0 Warning(s).
Build Time Elapsed:  XX:XX.XX
```

---

## 常见编译错误及解决 Common Build Errors

### Error: undefined reference to `pow`

**原因**：未链接数学库

**解决方法**：
1. Options for Target → Linker → Misc controls
2. 添加：`--library_type=microlib`
3. 或在 C/C++ → Define 中添加：`ARM_MATH_CM4`

### Error: `#include <math.h>` not found

**原因**：MicroLIB 未启用

**解决方法**：
1. Options for Target → Target
2. 勾选 `Use MicroLIB`

### Error: multiple definition of `HAL_UART_MspInit`

**原因**：`usart.c` 和 `stm32f4xx_hal_msp.c` 中重复定义

**解决方法**：
删除 `stm32f4xx_hal_msp.c` 中的 `HAL_UART_MspInit` 函数（如果存在）

### Warning: implicit declaration of function `printf`

**原因**：未包含 `<stdio.h>`

**解决方法**：
在需要 printf 的文件头部添加：`#include <stdio.h>`

---

## 下载配置 Download Configuration

**Project → Options for Target → Debug**

- Use: `ST-Link Debugger`（或 CMSIS-DAP / J-Link）
- Settings → Flash Download → 勾选 `Reset and Run`

---

## 代码大小估算 Code Size Estimation

基于类似配置的预估：

| 模块 | Flash 占用 | RAM 占用 |
|------|-----------|---------|
| HAL Driver | ~40 KB | ~2 KB |
| CAN + Emm_V5 | ~8 KB | ~1 KB |
| Robot Kinematics | ~6 KB | ~500 B |
| Robot Control | ~4 KB | ~200 B |
| UART + Command Parser | ~3 KB | ~300 B |
| **Total** | **~61 KB** | **~4 KB** |

STM32F407VET6 容量：512 KB Flash / 128 KB RAM，空间充裕。

---

## 调试技巧 Debugging Tips

### 使用 printf 调试

需要重定向 `printf` 到 UART1：

在 `usart.c` 末尾添加：

```c
#ifdef __GNUC__
#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
#define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif

PUTCHAR_PROTOTYPE
{
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 0xFFFF);
    return ch;
}
```

然后在代码中可以直接使用：
```c
printf("IK result: %.2f\n", joint_angles[0]);
```

### Keil 仿真调试

- 设置断点在 `RobotKin_Inverse` 函数
- 观察 `g_robot_kinematics.result` 数组
- 检查 IK 解是否合理

---

## 后续升级路径 Upgrade Path

### 添加 FreeRTOS

当前为裸机版本，若需要升级到完整的原项目架构（支持多任务并发）：

1. 用 CubeMX 开启 FreeRTOS Middleware
2. 参考原项目 `robot.c` 中的任务创建代码
3. 将命令解析改为任务队列机制

### 添加轨迹规划

当前为即时运动，若需要平滑轨迹：

1. 开启 TIM2（10ms 周期中断）
2. 实现梯形速度规划算法
3. 在定时器中断中逐点推送插值

### 集成 Python RL

参考原项目 `Deep_RL/train_robot_arm.py`：
1. 搭建 MuJoCo 仿真环境
2. 训练 TD3 策略
3. 用 Python 脚本通过 `rl_step` 命令控制实体机械臂

---

## 参考文档 References

- [Keil MDK 用户手册](http://www.keil.com/support/man/docs/uv4/)
- [STM32 HAL 库使用指南](https://www.st.com/resource/en/user_manual/um1725-description-of-stm32f4-hal-and-lowlayer-drivers-stmicroelectronics.pdf)
- [零一造物 ZERO 机械臂](https://gitee.com/dearxie/zero-robotic-arm)
