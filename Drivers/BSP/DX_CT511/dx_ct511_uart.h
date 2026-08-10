/**
 ****************************************************************************************************
 * @file        dx_ct511_uart.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V2.0
 * @date        2026-07-23
 * @brief       DX-CT511 4G模块 (LPUART1) 驱动头文件 (PA2-TX, PA3-RX)
 ****************************************************************************************************
 */

#ifndef __DX_CT511_UART_H
#define __DX_CT511_UART_H

#include "./SYSTEM/sys/sys.h"

/* 引脚定义 */
#define DX_CT511_UART_TX_GPIO_PORT           GPIOA
#define DX_CT511_UART_TX_GPIO_PIN            GPIO_PIN_2
#define DX_CT511_UART_TX_GPIO_CLK_ENABLE()   do{ __HAL_RCC_GPIOA_CLK_ENABLE(); }while(0)

#define DX_CT511_UART_RX_GPIO_PORT           GPIOA
#define DX_CT511_UART_RX_GPIO_PIN            GPIO_PIN_3
#define DX_CT511_UART_RX_GPIO_CLK_ENABLE()   do{ __HAL_RCC_GPIOA_CLK_ENABLE(); }while(0)

/* 定时器定义 (用于超时判定) */
#define DX_CT511_TIM_INTERFACE               TIM2
#define DX_CT511_TIM_IRQn                    TIM2_IRQn
#define DX_CT511_TIM_IRQHandler              TIM2_IRQHandler
#define DX_CT511_TIM_CLK_ENABLE()            do{ __HAL_RCC_TIM2_CLK_ENABLE();}while(0)
#define DX_CT511_TIM_PRESCALER               32000 /* 系统主频32MHz, 分频为1ms */

/* 串口外设定义 (LPUART1) */
#define DX_CT511_UART_INTERFACE              LPUART1
#define DX_CT511_UART_IRQn                   LPUART1_IRQn
#define DX_CT511_UART_IRQHandler             LPUART1_IRQHandler
#define DX_CT511_UART_CLK_ENABLE()           do{ __HAL_RCC_LPUART1_CLK_ENABLE(); }while(0)

/* UART缓冲区大小 */
#define DX_CT511_UART_RX_BUF_SIZE            256
#define DX_CT511_UART_TX_BUF_SIZE            256

/* 函数声明 */
void dx_ct511_uart_printf(char *fmt, ...);
void dx_ct511_uart_rx_restart(void);
uint8_t *dx_ct511_uart_rx_get_frame(void);
uint16_t dx_ct511_uart_rx_get_frame_len(void);
void dx_ct511_uart_init(uint32_t baudrate);
uint8_t dx_ct511_uart_rx_is_idle(void);

#endif
