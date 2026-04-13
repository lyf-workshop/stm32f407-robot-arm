# Keil 工程文件添加清单 / Keil Project File Checklist

## 需要添加到工程的文件

打开 Keil MDK-ARM 后，按以下步骤操作：

### 步骤 1：打开工程

双击 `MDK-ARM/STM32F407_CAN_CMD.uvprojx`

### 步骤 2：添加源文件

在左侧 Project 窗口：

**右键 `Application/User` → Add Existing Files to Group 'Application/User'**

逐个添加以下 7 个文件：

```
☐ ../Src/robot_config.c
☐ ../Src/robot_kinematics.c
☐ ../Src/robot_control.c
☐ ../Src/robot_cmd.c
☐ ../Src/robot_sequence.c
☐ ../Src/robot_examples.c
☐ ../Src/usart.c
```

添加后，Project 窗口应显示：

```
📁 Project: STM32F407_CAN_CMD
  📁 Application/User
    📄 main.c
    📄 can.c
    📄 dma.c
    📄 gpio.c
    📄 Emm_V5.c
    📄 stm32f4xx_it.c
    📄 stm32f4xx_hal_msp.c
    📄 system_stm32f4xx.c
    ✨ robot_config.c          ← 新增
    ✨ robot_kinematics.c      ← 新增
    ✨ robot_control.c         ← 新增
    ✨ robot_cmd.c             ← 新增
    ✨ robot_sequence.c        ← 新增
    ✨ robot_examples.c        ← 新增
    ✨ usart.c                 ← 新增
  📁 Drivers
    ...
```

### 步骤 3：编译器选项配置

**菜单：Project → Options for Target → Target 标签页**

- ☐ Use MicroLIB **必须勾选**

**菜单：Project → Options for Target → C/C++ 标签页**

- Language / Code Generation:
  - ☐ C99 Mode (`--c99`)
- Floating Point Hardware:
  - ☐ Use FPU **必须选择**

### 步骤 4：编译

**菜单：Project → Rebuild all target files (F7)**

预期输出：
```
compiling robot_config.c...
compiling robot_kinematics.c...
compiling robot_control.c...
...
linking...
"STM32F407_CAN_CMD.axf" - 0 Error(s), 0 Warning(s).
```

### 步骤 5：下载

**菜单：Flash → Download (F8)**

或使用下载按钮 ⬇️

## 如果遇到编译错误

### Error: undefined reference to `__aeabi_dmul` / `__aeabi_ddiv`

**原因**：未启用 FPU

**解决**：Options for Target → C/C++ → Floating Point Hardware → Use FPU

### Error: undefined reference to `pow` / `sqrt`

**原因**：未启用 MicroLIB

**解决**：Options for Target → Target → Use MicroLIB (勾选)

### Error: multiple definition of `HAL_UART_MspInit`

**原因**：`usart.c` 和 `stm32f4xx_hal_msp.c` 重复定义

**解决**：
1. 打开 `Src/stm32f4xx_hal_msp.c`
2. 如果存在 `HAL_UART_MspInit` 函数，删除或注释掉

### Error: #include "FreeRTOS.h" not found

**原因**：代码中残留了 FreeRTOS 引用（不应该有）

**解决**：检查代码，本移植版本不需要 FreeRTOS

### Warning: implicit declaration of function `printf`

**原因**：未包含 `<stdio.h>`

**解决**：在对应 .c 文件开头添加 `#include <stdio.h>`

## 可选：printf 重定向到 UART

如果需要用 `printf` 调试，在 `usart.c` 末尾添加：

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

然后在代码中可以直接：
```c
printf("Debug: angle = %.2f\n", angle);
```

## 检查清单

编译前确认：

- ☐ 7 个新源文件已添加到工程
- ☐ Use MicroLIB 已勾选
- ☐ Use FPU 已选择
- ☐ C99 Mode 已启用
- ☐ `stm32f4xx_hal_conf.h` 中已启用 `HAL_UART_MODULE_ENABLED`

编译后确认：

- ☐ 0 Error(s)
- ☐ Program Size < 100 KB（典型约 60-70 KB）

下载后确认：

- ☐ 串口输出欢迎信息
- ☐ 可以接收命令（输入 `status` 回车）

---

**完成以上步骤后，项目即可使用！**
