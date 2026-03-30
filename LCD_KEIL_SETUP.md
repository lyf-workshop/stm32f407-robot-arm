# LCD 屏幕功能 Keil 工程配置说明

## 新增文件清单

| 文件 | 位置 |
|------|------|
| `Inc/lcd.h` | LCD驱动头文件 |
| `Inc/lcd_font.h` | 8×16字体头文件 |
| `Inc/robot_display.h` | 机械臂状态UI头文件 |
| `Src/lcd.c` | ILI9341驱动（FSMC初始化+绘图） |
| `Src/lcd_font.c` | ASCII 8×16位图字体数据 |
| `Src/robot_display.c` | 机械臂状态UI实现 |

---

## Keil Project Manager 操作步骤

### 第1步：新建文件组

在 Keil 的 Project 窗口中：
1. 右键点击最顶层项目名 → **Manage Project Items**
2. 在 Groups 栏中点击 **Add...** 新建组，命名为 `LCD`
3. 确认后关闭

### 第2步：添加源文件

在 Project Manager 中，展开 `LCD` 组，将以下三个文件加入：
- `../Src/lcd.c`
- `../Src/lcd_font.c`
- `../Src/robot_display.c`

（或在 Project 视图中直接右键 LCD 组 → **Add Existing Files**，选择上述文件）

### 第3步：确认 Include Path

打开 **Options for Target → C/C++ → Include Paths**，确认已包含：
```
..\Inc
```
（之前已配置过，应该已有，无需重复添加）

### 第4步：无需添加 HAL_SRAM 驱动

> ⚠️ 本LCD驱动**直接操作FSMC寄存器**（`FSMC_Bank1->BTCR`），
> **不使用** `HAL_SRAM`，因此：
> - **不需要**在 `stm32f4xx_hal_conf.h` 中启用 `HAL_SRAM_MODULE_ENABLED`
> - **不需要**添加 `stm32f4xx_hal_sram.c` 到工程

---

## 编译后验证

编译时如果出现以下错误，参考对应解决方案：

| 错误信息 | 原因 | 解决 |
|----------|------|------|
| `Undefined symbol FSMC_Bank1` | 未包含MCU头文件 | 确认 `stm32f4xx_hal.h` 已被包含（main.h中已有） |
| `Undefined symbol LCD_Font8x16` | lcd_font.c 未加入工程 | 将 `Src/lcd_font.c` 加入 LCD 组 |
| `Undefined symbol RobotDisplay_Init` | robot_display.c 未加入工程 | 将 `Src/robot_display.c` 加入 LCD 组 |
| `Undefined symbol LCD_Init` | lcd.c 未加入工程 | 将 `Src/lcd.c` 加入 LCD 组 |

---

## 硬件连接确认

根据引脚分配表，2.8寸屏连接关系（**已全部在lcd.c中软件配置，无需手动跳线**）：

| LCD信号 | STM32引脚 | 功能 |
|---------|-----------|------|
| CS      | PD7       | FSMC_NE1 片选 |
| WR      | PD5       | FSMC_NWE 写使能 |
| RD      | PD4       | FSMC_NOE 读使能 |
| RS(DC)  | PD11      | FSMC_A16 命令/数据 |
| RESET   | NRST      | 接MCU复位脚 |
| BL      | PA15      | 背光（高电平点亮）|
| D0-D15  | PD14,PD15,PD0,PD1,PE7-PE15,PD8-PD10 | 16位数据总线 |

---

## 显示效果预览

烧录运行后，屏幕显示：

```
┌──────────────────────────────────────┐
│ ROBOT ARM CONTROL               [IDL]│  ← 深蓝底白字，右侧状态徽标
├──────────────────────────────────────┤
│ J1:+000.00   J4:+000.00             │
│ J2:+000.00   J5:+000.00             │  ← 绿色关节角度，200ms刷新
│ J3:+000.00   J6:+000.00             │
├──────────────────────────────────────┤
│ X:+0000.0  Y:+0000.0  Z:+0000.0    │  ← 青色末端坐标
├──────────────────────────────────────┤
│ Cmd:---                             │  ← 最后收到的串口指令
│ Sts:---                             │  ← 执行结果（绿=OK，红=Error）
└──────────────────────────────────────┘
```

发送串口指令后：
- 状态徽标切换为 `[RUN]`（绿色），执行完成后回到 `[IDL]`
- `Cmd:` 行实时显示输入的指令
- `Sts:` 行显示 OK 或错误信息
