/**
 ****************************************************************************************************
 * @file        lowpower_manager.h
 * @author      正点原子团队(ALIENTEK)参考风格
 * @version     V1.0
 * @date        2026-07-20
 * @brief       低功耗与重连管理器 头文件
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

#ifndef __LOWPOWER_MANAGER_H
#define __LOWPOWER_MANAGER_H

#include "./SYSTEM/sys/sys.h"

/* 状态机状态定义 */
#define CONN_STATE_OK            0    /* 连接正常 */
#define CONN_STATE_DISCONN       1    /* 连接断开，需要重新开始重连 */
#define CONN_STATE_RECONNECTING  2    /* 处于重连延时等待期 */

/* 外部函数声明 */
void lowpower_manager_init(void);
void lowpower_manager_poll(void);
void lowpower_manager_set_state(uint8_t state);
uint8_t lowpower_manager_get_state(void);
uint32_t lowpower_manager_get_wakeup_seconds(void);

#endif
