/**
 ****************************************************************************************************
 * @file        sys.c
 * @author      正点原子团队(ALIENTEK) / 气象站 HSI 16MHz 主时钟驱动
 * @version     V1.2
 * @date        2026-07-25
 * @brief       纯净高效 HSI 16MHz 时钟初始化 (移除 HSE，防重复配置锁死)
 ****************************************************************************************************
 */

#include "./SYSTEM/sys/sys.h"

void sys_nvic_set_vector_table(uint32_t baseaddr, uint32_t offset)
{
    SCB->VTOR = baseaddr | (offset & (uint32_t)0xFFFFFE00);
}

void sys_wfi_set(void)
{
    __ASM volatile("wfi");
}

void sys_intx_disable(void)
{
    __ASM volatile("cpsid i");
}

void sys_intx_enable(void)
{
    __ASM volatile("cpsie i");
}

void sys_msr_msp(uint32_t addr)
{
    __set_MSP(addr);
}

void sys_standby(void)
{
    __HAL_RCC_PWR_CLK_ENABLE();
    SET_BIT(PWR->CR, PWR_CR_PDDS);
}

void sys_soft_reset(void)
{
    NVIC_SystemReset();
}

/**
 * @brief       纯净高效 HSI (内部 16MHz 高速 RC 振荡器) 时钟初始化函数
 */
void sys_stm32_clock_init(void)
{
    HAL_StatusTypeDef ret = HAL_ERROR;
    RCC_OscInitTypeDef rcc_osc_init = {0};
    RCC_ClkInitTypeDef rcc_clk_init = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    /* 1. 检查内部 HSI 16MHz 是否已经处于就绪状态 */
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_HSIRDY) == RESET)
    {
        /* 若未就绪，配置并使能 HSI 16MHz */
        rcc_osc_init.OscillatorType = RCC_OSCILLATORTYPE_HSI;
        rcc_osc_init.HSIState = RCC_HSI_ON;
        rcc_osc_init.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
        rcc_osc_init.PLL.PLLState = RCC_PLL_NONE;
        ret = HAL_RCC_OscConfig(&rcc_osc_init);
    }
    else
    {
        /* HSI 已经成功就绪，无需重复执行 OscConfig 触发 HAL 保护报错 */
        ret = HAL_OK;
    }

    /* 2. 将系统主时钟 (SYSCLK) 切换/设置为 HSI 16MHz */
    if (ret == HAL_OK)
    {
        rcc_clk_init.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
        rcc_clk_init.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
        rcc_clk_init.AHBCLKDivider = RCC_SYSCLK_DIV1;
        rcc_clk_init.APB1CLKDivider = RCC_HCLK_DIV1;
        rcc_clk_init.APB2CLKDivider = RCC_HCLK_DIV1;
        ret = HAL_RCC_ClockConfig(&rcc_clk_init, FLASH_LATENCY_0);
    }

    /* 3. 极罕见电路硬件故障容错：若 HSI 配置失败，保底降级为内部 MSI (2.097MHz) 保证单片机不死锁 */
    if (ret != HAL_OK)
    {
        rcc_osc_init.OscillatorType = RCC_OSCILLATORTYPE_MSI;
        rcc_osc_init.MSIState = RCC_MSI_ON;
        rcc_osc_init.MSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
        rcc_osc_init.MSIClockRange = RCC_MSIRANGE_5; /* ~2.097MHz */
        rcc_osc_init.PLL.PLLState = RCC_PLL_NONE;
        HAL_RCC_OscConfig(&rcc_osc_init);

        rcc_clk_init.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
        rcc_clk_init.SYSCLKSource = RCC_SYSCLKSOURCE_MSI;
        rcc_clk_init.AHBCLKDivider = RCC_SYSCLK_DIV1;
        rcc_clk_init.APB1CLKDivider = RCC_HCLK_DIV1;
        rcc_clk_init.APB2CLKDivider = RCC_HCLK_DIV1;
        HAL_RCC_ClockConfig(&rcc_clk_init, FLASH_LATENCY_0);
    }
}
