# STM32L031 基础工程

基于 STM32L031 系列单片机的基础开发工程，使用 HAL 库与 Keil MDK (ARMCC/ARMCLANG) 开发环境。

## 目录结构

- `Drivers/`: STM32L0xx HAL 固件库及 CMSIS 驱动文件
- `Middlewares/`: 中间件库
- `Projects/MDK-ARM/`: Keil MDK5 项目工程文件 (`atk_l031.uvprojx`)
- `User/`: 用户应用层源码 (`main.c`, `stm32l0xx_it.c`, 驱动及外设模块源码)
- `keilkill.bat`: Keil 临时编译垃圾文件清理脚本

## 开发环境
- **芯片型号**: STM32L031G6Ux
- **IDE**: Keil uVision 5 (MDK-ARM)
- **库版本**: STM32L0xx HAL Library