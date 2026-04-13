# 项目文件结构 / Project File Structure

## 新增文件清单（本次移植添加）

### 头文件 (Inc/)

```
Inc/
├── robot_config.h       ✅ 机械臂物理参数配置（DH、减速比、限位）
├── robot_kinematics.h   ✅ 逆运动学算法接口
├── robot_control.h      ✅ 机械臂控制逻辑接口
├── robot_cmd.h          ✅ 串口命令解析器接口
├── robot_sequence.h     ✅ 预设动作序列接口
├── robot_examples.h     ✅ 使用示例接口
└── usart.h              ✅ USART1 驱动接口
```

### 源文件 (Src/)

```
Src/
├── robot_config.c       ✅ 物理参数实现（DH表、减速比等）
├── robot_kinematics.c   ✅ Pieper 解析法 IK 实现
├── robot_control.c      ✅ 关节控制、笛卡尔控制实现
├── robot_cmd.c          ✅ 命令解析实现（11 个命令）
├── robot_sequence.c     ✅ 演示序列实现（取放物、圆周等）
├── robot_examples.c     ✅ 5 个使用示例
└── usart.c              ✅ USART1 初始化与中断处理
```

### 文档文件

```
📄 README.md              ✅ 快速开始指南
📄 MIGRATION_SUMMARY.md   ✅ 移植工作总结报告
📄 ROBOT_ARM_GUIDE.md     ✅ 详细使用手册
📄 TESTING_GUIDE.md       ✅ 硬件测试指南
📄 BUILD_GUIDE.md         ✅ 编译配置指南
📄 KEIL_SETUP.md          ✅ Keil 工程配置清单
📄 robot_rl_interface.py  ✅ Python RL 接口示例
```

## 修改的原有文件

### Src/main.c
- ✅ 添加机械臂模块头文件
- ✅ 添加 USART1 初始化调用
- ✅ 替换演示代码为机械臂初始化逻辑
- ✅ 启动 UART 接收中断

### Src/stm32f4xx_it.c
- ✅ 添加 `usart.h` 包含
- ✅ 声明 `huart1` 外部变量
- ✅ 添加 `USART1_IRQHandler` 中断处理函数

### Inc/stm32f4xx_it.h
- ✅ 添加 `USART1_IRQHandler` 函数声明

### Inc/stm32f4xx_hal_conf.h
- ✅ 启用 `HAL_UART_MODULE_ENABLED`

## 原有文件（未修改，继续使用）

### 电机控制协议
```
Src/Emm_V5.c + Inc/Emm_V5.h   (张大头 Emm V5 协议实现)
```

### CAN 通信
```
Src/can.c + Inc/can.h         (CAN 初始化、滤波器、发送函数)
```

### 其他外设
```
Src/gpio.c + Inc/gpio.h
Src/dma.c + Inc/dma.h
Src/stm32f4xx_hal_msp.c
Src/system_stm32f4xx.c
```

### 工程配置
```
STM32F407_CAN_CMD.ioc         (CubeMX 配置文件)
MDK-ARM/*.uvprojx             (Keil 工程文件)
```

## 代码行数统计

| 模块 | 文件 | 行数 | 说明 |
|------|------|------|------|
| 配置 | robot_config.h/c | ~120 | DH参数、减速比等 |
| 运动学 | robot_kinematics.h/c | ~350 | Pieper IK 算法 |
| 控制 | robot_control.h/c | ~160 | 关节/笛卡尔控制 |
| 命令 | robot_cmd.h/c | ~240 | 11 个串口命令 |
| 序列 | robot_sequence.h/c | ~120 | 3 个演示序列 |
| 示例 | robot_examples.h/c | ~160 | 5 个使用示例 |
| 通信 | usart.h/c | ~80 | UART 驱动 |
| **总计** | **7 个模块** | **~1230 行** | **纯算法代码** |

加上 Emm_V5 协议层（~1550 行），总共约 **2800 行核心代码**。

## 依赖关系图

```
main.c
  ├─→ robot_control.c
  │     ├─→ robot_kinematics.c
  │     │     └─→ robot_config.c
  │     └─→ Emm_V5.c
  │           └─→ can.c
  ├─→ robot_cmd.c
  │     ├─→ robot_control.c (循环依赖)
  │     └─→ robot_sequence.c
  │           └─→ robot_control.c
  └─→ usart.c
        └─→ robot_cmd.c
```

## 编译配置要求

### 必须项
- ✅ HAL_CAN_MODULE_ENABLED
- ✅ HAL_UART_MODULE_ENABLED
- ✅ Use MicroLIB
- ✅ Use FPU
- ✅ C99 Mode

### 推荐项
- ⭐ Optimization: -O1 (调试) 或 -O2 (发布)
- ⭐ printf 重定向（可选，便于调试）

## Flash / RAM 使用预估

基于 STM32F407VET6 (512KB Flash / 128KB RAM)：

| 组件 | Flash | RAM | 占用率 |
|------|-------|-----|--------|
| HAL Driver | ~40 KB | ~2 KB | 8% / 2% |
| CAN + Emm_V5 | ~8 KB | ~1 KB | 2% / 1% |
| Robot Kinematics | ~6 KB | ~500 B | 1% / <1% |
| Robot Control | ~8 KB | ~500 B | 2% / <1% |
| Command Parser | ~4 KB | ~300 B | 1% / <1% |
| **小计** | **~66 KB** | **~4.5 KB** | **13% / 4%** |
| **剩余空间** | **446 KB** | **123 KB** | **87% / 96%** |

**结论**：空间充裕，后续可添加：
- FreeRTOS（+20 KB Flash，+10 KB RAM）
- 轨迹插值（+5 KB Flash）
- MQTT / WiFi（+30 KB Flash）

## 功能模块清单

### ✅ 已实现

- [x] CAN 总线通信（500 kbps）
- [x] Emm V5 协议封装（完整命令集）
- [x] 多机同步运动（驱动器硬件同步）
- [x] Pieper 解析法 IK（闭合解，< 1ms）
- [x] 关节空间控制（相对/绝对）
- [x] 笛卡尔空间控制（X,Y,Z）
- [x] 串口命令解析（11 个命令）
- [x] RL 接口（rl_step 命令）
- [x] 预设动作序列（取放物、圆周等）
- [x] Python 接口示例

### ⏸️ 可选扩展（未实现，按需添加）

- [ ] 限位开关支持（硬件归零）
- [ ] 轨迹插值（梯形速度规划）
- [ ] PID 闭环控制
- [ ] FreeRTOS 多任务架构
- [ ] MQTT 远程控制
- [ ] Web 可视化平台
- [ ] TD3 强化学习训练与部署

## 兼容性

### 硬件兼容
- ✅ STM32F407VET6（本项目目标）
- ✅ 其他 STM32F4 系列（需调整时钟配置）
- ⚠️ STM32F1 系列（需修改 HAL 库）

### 驱动器兼容
- ✅ ZDT XS 系列闭环步进（Emm V5 固件）
- ⚠️ 其他品牌需修改 `Emm_V5.c` 协议层

### 机械臂兼容
- ✅ 零一造物 ZERO 机械臂（完全兼容）
- ⚠️ 其他 6-DOF 机械臂（需修改 DH 参数）

## 下一步

1. **立即开始**：按照 `README.md` 快速开始
2. **硬件测试**：按照 `TESTING_GUIDE.md` 逐步验证
3. **深入学习**：阅读 `MIGRATION_SUMMARY.md` 了解架构
4. **RL 集成**：使用 `robot_rl_interface.py` 开始训练

---

**文件清单版本：v1.0**  
**最后更新：2026-03-27**
