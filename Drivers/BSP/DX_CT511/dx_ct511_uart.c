/**
 ****************************************************************************************************
 * @file        dx_ct511_uart.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V2.0
 * @date        2026-07-23
 * @brief       DX-CT511 4G模块 (LPUART1) 驱动实现 (PA2-TX, PA3-RX)
 ****************************************************************************************************
 */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "./BSP/DX_CT511/dx_ct511_uart.h"

UART_HandleTypeDef g_dx_ct511_uart_handle;
TIM_HandleTypeDef  g_dx_ct511_tim_handle;

static uint8_t g_dx_ct511_uart_rx_buf[DX_CT511_UART_RX_BUF_SIZE];
static uint16_t g_dx_ct511_uart_rx_len = 0;
static uint8_t g_dx_ct511_uart_rx_frame_cplt = 0;
static uint8_t g_dx_ct511_uart_rx_data = 0;

void dx_ct511_uart_rx_restart(void)
{
    g_dx_ct511_uart_rx_len = 0;
    g_dx_ct511_uart_rx_frame_cplt = 0;
    memset(g_dx_ct511_uart_rx_buf, 0, DX_CT511_UART_RX_BUF_SIZE);
}

uint8_t *dx_ct511_uart_rx_get_frame(void)
{
    if (g_dx_ct511_uart_rx_frame_cplt)
    {
        return g_dx_ct511_uart_rx_buf;
    }
    return NULL;
}

uint16_t dx_ct511_uart_rx_get_frame_len(void)
{
    if (g_dx_ct511_uart_rx_frame_cplt)
    {
        return g_dx_ct511_uart_rx_len;
    }
    return 0;
}

void dx_ct511_uart_printf(char *fmt, ...)
{
    va_list ap;
    uint16_t len;
    static uint8_t tx_buf[DX_CT511_UART_TX_BUF_SIZE];

    va_start(ap, fmt);
    vsprintf((char *)tx_buf, fmt, ap);
    va_end(ap);

    len = strlen((const char *)tx_buf);
    HAL_UART_Transmit(&g_dx_ct511_uart_handle, tx_buf, len, HAL_MAX_DELAY);
}

void dx_ct511_uart_init(uint32_t baudrate)
{
    GPIO_InitTypeDef gpio_init_struct;
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
    UART_WakeUpTypeDef WakeUpSelection;

    DX_CT511_UART_TX_GPIO_CLK_ENABLE();
    DX_CT511_UART_RX_GPIO_CLK_ENABLE();
    DX_CT511_UART_CLK_ENABLE();
    DX_CT511_TIM_CLK_ENABLE();

    /* 配置 LPUART1 时钟源为 HSI16 (支持 STOP 模式唤醒) */
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_LPUART1;
    PeriphClkInit.Lpuart1ClockSelection = RCC_LPUART1CLKSOURCE_HSI;
    HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit);

    /* GPIO 配置 TX (PA2 -> AF6_LPUART1) */
    gpio_init_struct.Pin = DX_CT511_UART_TX_GPIO_PIN;
    gpio_init_struct.Mode = GPIO_MODE_AF_PP;
    gpio_init_struct.Pull = GPIO_PULLUP;
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio_init_struct.Alternate = GPIO_AF6_LPUART1;
    HAL_GPIO_Init(DX_CT511_UART_TX_GPIO_PORT, &gpio_init_struct);

    /* GPIO 配置 RX (PA3 -> AF6_LPUART1) */
    gpio_init_struct.Pin = DX_CT511_UART_RX_GPIO_PIN;
    gpio_init_struct.Alternate = GPIO_AF6_LPUART1;
    HAL_GPIO_Init(DX_CT511_UART_RX_GPIO_PORT, &gpio_init_struct);

    /* LPUART1 参数配置 */
    g_dx_ct511_uart_handle.Instance = DX_CT511_UART_INTERFACE;
    g_dx_ct511_uart_handle.Init.BaudRate = baudrate;
    g_dx_ct511_uart_handle.Init.WordLength = UART_WORDLENGTH_8B;
    g_dx_ct511_uart_handle.Init.StopBits = UART_STOPBITS_1;
    g_dx_ct511_uart_handle.Init.Parity = UART_PARITY_NONE;
    g_dx_ct511_uart_handle.Init.Mode = UART_MODE_TX_RX;
    g_dx_ct511_uart_handle.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    g_dx_ct511_uart_handle.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&g_dx_ct511_uart_handle);

    /* 配置 STOP 模式唤醒源 */
    WakeUpSelection.WakeUpEvent = UART_WAKEUP_ON_STARTBIT;
    HAL_UARTEx_StopModeWakeUpSourceConfig(&g_dx_ct511_uart_handle, WakeUpSelection);
    HAL_UARTEx_EnableStopMode(&g_dx_ct511_uart_handle);

    /* 中断配置 */
    HAL_NVIC_SetPriority(DX_CT511_UART_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(DX_CT511_UART_IRQn);
    __HAL_UART_ENABLE_IT(&g_dx_ct511_uart_handle, UART_IT_RXNE);
    __HAL_UART_ENABLE_IT(&g_dx_ct511_uart_handle, UART_IT_WUF);

    /* TIM 配置 */
    g_dx_ct511_tim_handle.Instance = DX_CT511_TIM_INTERFACE;
    g_dx_ct511_tim_handle.Init.Prescaler = DX_CT511_TIM_PRESCALER - 1;
    g_dx_ct511_tim_handle.Init.CounterMode = TIM_COUNTERMODE_UP;
    g_dx_ct511_tim_handle.Init.Period = 50 - 1;
    g_dx_ct511_tim_handle.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    HAL_TIM_Base_Init(&g_dx_ct511_tim_handle);

    HAL_NVIC_SetPriority(DX_CT511_TIM_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(DX_CT511_TIM_IRQn);
}

void DX_CT511_UART_IRQHandler(void)
{
    if (__HAL_UART_GET_FLAG(&g_dx_ct511_uart_handle, UART_FLAG_RXNE) != RESET)
    {
        g_dx_ct511_uart_rx_data = (uint8_t)(g_dx_ct511_uart_handle.Instance->RDR & 0x00FF);
        
        if (g_dx_ct511_uart_rx_frame_cplt == 0)
        {
            if (g_dx_ct511_uart_rx_len < DX_CT511_UART_RX_BUF_SIZE)
            {
                g_dx_ct511_uart_rx_buf[g_dx_ct511_uart_rx_len++] = g_dx_ct511_uart_rx_data;
                __HAL_TIM_SET_COUNTER(&g_dx_ct511_tim_handle, 0);
                if (g_dx_ct511_uart_rx_len == 1)
                {
                    HAL_TIM_Base_Start_IT(&g_dx_ct511_tim_handle);
                }
            }
        }
    }
    
    if (__HAL_UART_GET_FLAG(&g_dx_ct511_uart_handle, UART_FLAG_ORE) != RESET)
    {
        __HAL_UART_CLEAR_FLAG(&g_dx_ct511_uart_handle, UART_CLEAR_OREF);
    }
    if (__HAL_UART_GET_FLAG(&g_dx_ct511_uart_handle, UART_FLAG_FE) != RESET)
    {
        __HAL_UART_CLEAR_FLAG(&g_dx_ct511_uart_handle, UART_CLEAR_FEF);
    }
    if (__HAL_UART_GET_FLAG(&g_dx_ct511_uart_handle, UART_FLAG_PE) != RESET)
    {
        __HAL_UART_CLEAR_FLAG(&g_dx_ct511_uart_handle, UART_CLEAR_PEF);
    }
    if (__HAL_UART_GET_FLAG(&g_dx_ct511_uart_handle, UART_FLAG_NE) != RESET)
    {
        __HAL_UART_CLEAR_FLAG(&g_dx_ct511_uart_handle, UART_CLEAR_NEF);
    }
    if (__HAL_UART_GET_FLAG(&g_dx_ct511_uart_handle, UART_FLAG_WUF) != RESET)
    {
        __HAL_UART_CLEAR_FLAG(&g_dx_ct511_uart_handle, UART_CLEAR_WUF);
    }
}

void DX_CT511_TIM_IRQHandler(void)
{
    if (__HAL_TIM_GET_FLAG(&g_dx_ct511_tim_handle, TIM_FLAG_UPDATE) != RESET)
    {
        __HAL_TIM_CLEAR_FLAG(&g_dx_ct511_tim_handle, TIM_FLAG_UPDATE);
        HAL_TIM_Base_Stop_IT(&g_dx_ct511_tim_handle);
        
        if (g_dx_ct511_uart_rx_len > 0)
        {
            g_dx_ct511_uart_rx_frame_cplt = 1;
        }
    }
}

uint8_t dx_ct511_uart_rx_is_idle(void)
{
    if (g_dx_ct511_uart_rx_len > 0)
    {
        return 0;
    }
    return 1;
}
