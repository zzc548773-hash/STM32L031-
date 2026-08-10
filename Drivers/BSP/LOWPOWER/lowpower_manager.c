/**
 ****************************************************************************************************
 * @file        lowpower_manager.c
 * @author      正点原子团队(ALIENTEK)参考风格
 * @version     V1.0
 * @date        2026-07-20
 * @brief       低功耗与重连管理器 驱动代码
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

#include "./BSP/LOWPOWER/lowpower_manager.h"
#include "./BSP/RTC/rtc.h"
#include "./BSP/DX_CT511/dx_ct511.h"
#include "./SYSTEM/delay/delay.h"

static uint8_t s_conn_state = CONN_STATE_OK;
static uint32_t s_retry_count = 0;

/**
 * @brief       获取重连退避延时时长
 * @param       retries: 重连失败次数
 * @retval      等待时间（秒）
 */
static uint32_t get_reconnect_delay(uint32_t retries)
{
    if (retries <= 3) return 10;      /* 1~3 次等待 10s */
    if (retries <= 6) return 60;      /* 4~6 次等待 1min (60s) */
    if (retries <= 9) return 300;     /* 7~9 次等待 5min (300s) */
    if (retries <= 12) return 900;    /* 10~12 次等待 15min (900s) */
    return 1800;                      /* 12 次以上等待 30min (1800s) */
}

/**
 * @brief       初始化低功耗管理器
 * @param       无
 * @retval      无
 */
void lowpower_manager_init(void)
{
    s_conn_state = CONN_STATE_OK;
    s_retry_count = 0;
    rtc_init(); /* 初始化 RTC */
}

/**
 * @brief       手动设置连接状态
 * @param       state: 新状态码 (CONN_STATE_OK, CONN_STATE_DISCONN, CONN_STATE_RECONNECTING)
 * @retval      无
 */

uint8_t lowpower_manager_get_state(void)
{
    return s_conn_state;
}
void lowpower_manager_set_state(uint8_t state)
{
    s_conn_state = state;
}

/**
 * @brief       获取低功耗休眠唤醒时长
 * @param       无
 * @retval      唤醒时间（秒）
 */
uint32_t lowpower_manager_get_wakeup_seconds(void)
{
    if (s_conn_state == CONN_STATE_OK)
    {
        return 7200; /* 正常状态下每 2 小时定时唤醒一次查询 */
    }
    else if (s_conn_state == CONN_STATE_RECONNECTING)
    {
        /* 处于退避延时期，返回当前重连退避等待时间 */
        return get_reconnect_delay(s_retry_count);
    }
    return 10; /* 默认安全值 10s */
}

/**
 * @brief       低功耗管理器主轮询服务
 * @note        在 main 函数的 while(1) 循环中调用
 * @param       无
 * @retval      无
 */
void lowpower_manager_poll(void)
{
    uint8_t res;
    uint32_t i;
    /* 1. 处理 RTC 唤醒标志触发 the logic */
    if (g_rtc_wakeup_flag)
    {
        g_rtc_wakeup_flag = 0; /* 清除标志 */
        
        if (s_conn_state == CONN_STATE_RECONNECTING)
        {
            /* 延时到期，准备重新连接 */
            s_conn_state = CONN_STATE_DISCONN;
        }
        else if (s_conn_state == CONN_STATE_OK)
        {
            /* 定期 2 小时唤醒检查连接状态 */
            if (dx_ct511_check_mqtt_status() == 0)
            {
                /* 确认掉线，触发重连 */
                s_conn_state = CONN_STATE_DISCONN;
            }
        }
    } /* 2. 重连业务逻辑 */
    if (s_conn_state == CONN_STATE_DISCONN)
    {
        s_retry_count++;
        
        /* 连续失败达 12 次以上，执行软件重启模块 */
        if (s_retry_count > 12)
        {
            /* 向 4G 模块发送软件重启指令 */
            dx_ct511_send_cmd("AT+RESET", "OK", 2000);
            
            /* 等待 4G 模块重启（循环发送 AT，每隔 1s 查询一次，最多等待 30 秒） */
            for (i = 0; i < 30; i++)
            {
                delay_ms(1000);
                if (dx_ct511_send_cmd("AT", "OK", 500) == DX_CT511_EOK)
                {
										delay_ms(3000);
                    break;
                }
            }
        }/* 尝试发起 MQTT 连接 */
        res = dx_ct511_mqtt_connect_onenet(ONENET_DEVICE_NAME, ONENET_PRODUCT_ID, ONENET_TOKEN);
        if (res == DX_CT511_EOK)
        {
            /* 连接成功！ */
            s_retry_count = 0;
            s_conn_state = CONN_STATE_OK; 
            /* 发送低功耗自动休眠指令，让 4G 模块也自动进入睡眠以省电 */
            dx_ct511_send_cmd("AT+SYSSLEEP=1", "OK", 1000);
        }
        else
        {
            /* 连接失败，进入退避等待阶段 */
            s_conn_state = CONN_STATE_RECONNECTING;
        }
    }
}
