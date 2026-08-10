/**
 ****************************************************************************************************
 * @file        rtc.c
 * @author      正点原子团队(ALIENTEK)参考风格
 * @version     V2.1
 * @date        2026-07-20
 * @brief       RTC 驱动代码 (STM32L031) - 使用外部 32.768kHz 低速晶振 (LSE)
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

#include "./BSP/RTC/rtc.h"

RTC_HandleTypeDef g_rtc_handle;
volatile uint8_t g_rtc_wakeup_flag = 0;

/**
 * @brief       RTC初始化
 * @note        使用外部低速晶振 LSE (32.768kHz) 作为 RTC 时钟源，精度高于内部 LSI
 * @param       无
 * @retval      0: 成功, 1: 失败
 */
uint8_t rtc_init(void)
{
    g_rtc_handle.Instance = RTC;
    g_rtc_handle.Init.HourFormat = RTC_HOURFORMAT_24;
    /* LSE = 32768Hz（精确）：
     * 1Hz 频率时钟计算公式：ck_spre = RTCCLK / ((AsynchPrediv + 1) * (SynchPrediv + 1))
     * 标准分频：AsynchPrediv = 127, SynchPrediv = 255
     * 验证：32768 / (128 * 256) = 1Hz，完全精确
     */
    g_rtc_handle.Init.AsynchPrediv = 127;
    g_rtc_handle.Init.SynchPrediv = 255;
    g_rtc_handle.Init.OutPut = RTC_OUTPUT_DISABLE;
    g_rtc_handle.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
    g_rtc_handle.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;

    if (HAL_RTC_Init(&g_rtc_handle) != HAL_OK)
    {
        return 1;
    }

    /* 开启 RTC Wakeup 中断优先级并使能 */
    HAL_NVIC_SetPriority(RTC_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(RTC_IRQn);

    return 0;
}

/**
 * @brief       RTC 底层驱动，时钟配置
 * @note        此函数会被 HAL_RTC_Init() 自动调用
 * @param       hrtc: RTC句柄
 * @retval      无
 */
void HAL_RTC_MspInit(RTC_HandleTypeDef *hrtc)
{
    RCC_OscInitTypeDef rcc_oscinitstruct = {0};
    RCC_PeriphCLKInitTypeDef rcc_periphclkinitstruct = {0};

    /* 1. 使能电源接口时钟和备份域写入保护解锁 */
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();

    /* 2. 开启并使能 LSE 外部低速晶振 (32.768kHz，接 PC14/PC15) */
    rcc_oscinitstruct.OscillatorType = RCC_OSCILLATORTYPE_LSE;
    rcc_oscinitstruct.LSEState = RCC_LSE_ON;
    rcc_oscinitstruct.PLL.PLLState = RCC_PLL_NONE;
    HAL_RCC_OscConfig(&rcc_oscinitstruct);

    /* 3. 选择 LSE 作为 RTC 的时钟源 */
    rcc_periphclkinitstruct.PeriphClockSelection = RCC_PERIPHCLK_RTC;
    rcc_periphclkinitstruct.RTCClockSelection = RCC_RTCCLKSOURCE_LSE;
    HAL_RCCEx_PeriphCLKConfig(&rcc_periphclkinitstruct);

    /* 4. 使能 RTC 外设时钟 */
    __HAL_RCC_RTC_ENABLE();
}

/**
 * @brief       设置 RTC 唤醒定时器时间
 * @note        使用 1Hz 的 16 位定时器时钟源 (RTC_WAKEUPCLOCK_CK_SPRE_16BITS)，即 1 计数 = 1 秒
 * @param       seconds: 唤醒间隔时间（秒），范围 1 ~ 65536
 * @retval      无
 */
void rtc_set_wakeup(uint32_t seconds)
{
    if (seconds == 0) return;

    /* 在配置唤醒计时器前，必须先关闭它 */
    HAL_RTCEx_DeactivateWakeUpTimer(&g_rtc_handle);

    /* 清除可能悬起的唤醒中断标志 */
    __HAL_RTC_WAKEUPTIMER_CLEAR_FLAG(&g_rtc_handle, RTC_FLAG_WUTF);

    /* 重新使能并设置 RTC 唤醒定时器：计数器值范围 0 ~ 65535。
     * 由于计数是从配置值倒计时到 0，因此计数器参数应设为 seconds - 1
     */
    HAL_RTCEx_SetWakeUpTimer_IT(&g_rtc_handle, seconds - 1, RTC_WAKEUPCLOCK_CK_SPRE_16BITS);
}

/**
 * @brief       关闭 RTC 唤醒定时器
 * @param       无
 * @retval      无
 */
void rtc_deactivate_wakeup(void)
{
    HAL_RTCEx_DeactivateWakeUpTimer(&g_rtc_handle);
    __HAL_RTC_WAKEUPTIMER_CLEAR_FLAG(&g_rtc_handle, RTC_FLAG_WUTF);
}

/**
 * @brief       RTC 中断服务函数
 * @param       无
 * @retval      无
 */
void RTC_IRQHandler(void)
{
    HAL_RTCEx_WakeUpTimerIRQHandler(&g_rtc_handle);
}

/**
 * @brief       RTC 唤醒事件回调函数
 * @note        当唤醒定时器计数到 0 时，该回调函数由 HAL_RTCEx_WakeUpTimerIRQHandler 调用
 * @param       hrtc: RTC 句柄指针
 * @retval      无
 */
void HAL_RTCEx_WakeUpTimerEventCallback(RTC_HandleTypeDef *hrtc)
{
    g_rtc_wakeup_flag = 1; /* 触发唤醒事件标志 */
}
