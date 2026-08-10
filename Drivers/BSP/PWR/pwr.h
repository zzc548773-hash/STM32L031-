/**
 ****************************************************************************************************
 * @file        pwr.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2023-08-01
 * @brief       低功耗模� 驱动代码
 * @license     Copyright (c) 2020-2032, 广州市星翼电子�技有限��
 ****************************************************************************************************
 * @attention
 *
 * 实验平台:正点原子 M48Z-M3�小系统板STM32F103�
 * 在线视�:www.yuanzige.com
 * ��论坛:www.openedv.com
 * �司网�:www.alientek.com
 * �买地�:openedv.taobao.com
 *
 ****************************************************************************************************
 */

#ifndef __PWR_H
#define __PWR_H

#include "./SYSTEM/sys/sys.h"


/******************************************************************************************/
/* PWR WKUP 按键 引脚和中� 定义 
 * 我们通过WK_UP按键唤醒 MCU,  因�必须定义这�按键及其对应的中�服务函数 
 */

#define PWR_WKUP_GPIO_PORT              GPIOA
#define PWR_WKUP_GPIO_PIN               GPIO_PIN_0
#define PWR_WKUP_GPIO_CLK_ENABLE()      do{ __HAL_RCC_GPIOA_CLK_ENABLE(); }while(0)   /* PA口时钟使� */
  
#define PWR_WKUP_INT_IRQn               EXTI0_1_IRQn
#define PWR_WKUP_INT_IRQHandler         EXTI0_1_IRQHandler

/******************************************************************************************/

void pwr_pvd_init(uint32_t pls); /* PVD电压�测初始化函数 */
void pwr_wkup_key_init(void);    /* 唤醒按键初�化 */
void pwr_enter_sleep(void);      /* 进入睡眠模式 */
void pwr_enter_stop(void);       /* 进入停�模� */

void pwr_unused_gpio_init(void); /* ����δʹ������Ϊģ��ģʽ����ֹ©�� */

#endif




















