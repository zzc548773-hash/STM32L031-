/**
 ****************************************************************************************************
 * @file        rs485.h
 * @author      正点原子团队(ALIENTEK) / STM32L031
 * @version     V2.0
 * @date        2026-07-23
 * @brief       RS485 驱动代码 (STM32L031G6U6)
 ****************************************************************************************************
 */

#ifndef __RS485_H
#define __RS485_H

#include "./SYSTEM/sys/sys.h"

/******************************************************************************************/
/* RS485 引脚与串口定义 (STM32L031G6U6: PA9-TX, PA10-RX, PA8-RE) */

#define RS485_RE_GPIO_PORT                  GPIOA
#define RS485_RE_GPIO_PIN                   GPIO_PIN_8
#define RS485_RE_GPIO_CLK_ENABLE()          do{ __HAL_RCC_GPIOA_CLK_ENABLE(); }while(0)

#define RS485_TX_GPIO_PORT                  GPIOA
#define RS485_TX_GPIO_PIN                   GPIO_PIN_9
#define RS485_TX_GPIO_CLK_ENABLE()          do{ __HAL_RCC_GPIOA_CLK_ENABLE(); }while(0)

#define RS485_RX_GPIO_PORT                  GPIOA
#define RS485_RX_GPIO_PIN                   GPIO_PIN_10
#define RS485_RX_GPIO_CLK_ENABLE()          do{ __HAL_RCC_GPIOA_CLK_ENABLE(); }while(0)

#define RS485_UX                            USART2
#define RS485_UX_IRQn                       USART2_IRQn
#define RS485_UX_IRQHandler                 USART2_IRQHandler
#define RS485_UX_CLK_ENABLE()               do{ __HAL_RCC_USART2_CLK_ENABLE(); }while(0)

/******************************************************************************************/

/* 控制 RS485_RE 引脚, 收发切换
 * RS485_RE = 0: 接收模式
 * RS485_RE = 1: 发送模式
 */ 
#define RS485_RE(x)   do{ x ? \
                          HAL_GPIO_WritePin(RS485_RE_GPIO_PORT, RS485_RE_GPIO_PIN, GPIO_PIN_SET) : \
                          HAL_GPIO_WritePin(RS485_RE_GPIO_PORT, RS485_RE_GPIO_PIN, GPIO_PIN_RESET); \
                      }while(0)

#define RS485_REC_LEN               64          /* 最大接收字节数 */
#define RS485_EN_RX                 1           /* 使能接收中断 */
#define READ                        0x03
#define WRITE                       0x06

extern uint8_t g_RS485_rx_buf[RS485_REC_LEN];   /* 接收缓冲区 */
extern uint8_t g_RS485_rx_cnt;                  /* 接收数据计数器 */

void rs485_init(uint32_t baudrate);                 /* RS485 初始化 */
void rs485_send_data(uint8_t *buf, uint8_t len);    /* RS485 发送数据 */
void rs485_receive_data(uint8_t *buf, uint8_t *len);/* RS485 接收数据 */
uint16_t GetCRC16(uint8_t *buf, uint16_t len);       /* STM32L031 硬件 CRC16 计算 */

void RS485_RW_Opr(uint8_t ucAddr, uint8_t ucCmd, uint16_t ucReg, uint16_t uiDate);
uint8_t dizhi_shengchengzhen(uint8_t dizhi_jiu, uint8_t dizhi_xin, uint8_t *buf);
uint8_t RS485_set_dizhi_turang(uint8_t dizhi_jiu, uint8_t mubiao);
void RS485_READ_dizhi_can(uint8_t dizhi, uint16_t *buf);
void RS485_READ_dizhi_can_turang(uint8_t dizhi, uint16_t *buf);
void RS485_READ_dizhi_baiye_wsg(uint8_t dizhi, uint16_t *buf);
void RS485_READ_dizhi_can_jianda(uint8_t dizhi, uint16_t *buf, uint16_t a);
void RS485_READ_dizhi_can_jianda_2(uint8_t dizhi, uint16_t *buf, uint16_t a);

#endif
