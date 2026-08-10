/**
 ****************************************************************************************************
 * @file        rtc.h
 * @author      正点原子团队(ALIENTEK)参考风格
 * @version     V2.0
 * @date        2026-07-20
 * @brief       RTC 驱动代码 (STM32L031)
 * @license     Copyright (c) 2020-2032, 广州市星翼电子科技有限公司
 ****************************************************************************************************
 * @attention
 *
 * 实验平台:正点原子 M48Z-M3小系统板STM32L031
 * 在线视频:www.yuanzige.com
 * 技术论坛:www.openedv.com
 * 公司官网:www.alientek.com
 * 购买地址:openedv.taobao.com
 *
 ****************************************************************************************************
 */

#ifndef __RTC_H
#define __RTC_H

#include "./SYSTEM/sys/sys.h"

extern RTC_HandleTypeDef g_rtc_handle;
extern volatile uint8_t g_rtc_wakeup_flag;

uint8_t rtc_init(void);
void rtc_set_wakeup(uint32_t seconds);
void rtc_deactivate_wakeup(void);

#endif
