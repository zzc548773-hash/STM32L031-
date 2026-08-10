/**
 ****************************************************************************************************
 * @file        pwr.c
 * @author      姝ｇ偣鍘熷瓙鍥㈤槦(ALIENTEK)
 * @version     V1.0
 * @date        2023-08-01
 * @brief       浣庡姛鑰楁ā寮 椹卞姩浠ｇ爜
 * @license     Copyright (c) 2020-2032, 骞垮窞甯傛槦缈肩數瀛愮戞妧鏈夐檺鍏鍙
 ****************************************************************************************************
 * @attention
 *
 * 瀹為獙骞冲彴:姝ｇ偣鍘熷瓙 M48Z-M3鏈灏忕郴缁熸澘STM32F103鐗
 * 鍦ㄧ嚎瑙嗛:www.yuanzige.com
 * 鎶鏈璁哄潧:www.openedv.com
 * 鍏鍙哥綉鍧:www.alientek.com
 * 璐涔板湴鍧:openedv.taobao.com
 *
 ****************************************************************************************************
 */

#include "./BSP/PWR/pwr.h"


#include "./SYSTEM/delay/delay.h"


/**
 * @brief       浣庡姛鑰楁ā寮忎笅鐨勬寜閿鍒濆嬪寲(鐢ㄤ簬鍞ら啋鐫＄湢妯″紡/鍋滄㈡ā寮)
 * @param       鏃
 * @retval      鏃
 */
void pwr_wkup_key_init(void)
{
    GPIO_InitTypeDef gpio_init_struct;

    PWR_WKUP_GPIO_CLK_ENABLE();                                         /* WKUP鏃堕挓浣胯兘 */

    gpio_init_struct.Pin = PWR_WKUP_GPIO_PIN;                           /* WKUP寮曡剼 */
    gpio_init_struct.Mode = GPIO_MODE_IT_FALLING;                       /* 鏀逛负涓嬮檷娌胯Е鍙戯紙涓插彛璧峰嬩綅涓轰笅闄嶆部锛夛紝浠ヤ究绗涓鏃堕棿鍞ら啋 */
    gpio_init_struct.Pull = GPIO_NOPULL;                                /* 鏀逛负鏃犱笂涓嬫媺锛屾秷闄や笌RX寮曡剼涓婃媺鐢甸樆鐨勫啿绐佸拰婕忕數娴 */
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;                      /* 楂橀 */
    HAL_GPIO_Init(PWR_WKUP_GPIO_PORT, &gpio_init_struct);               /* WKUP寮曡剼鍒濆嬪寲 */

    HAL_NVIC_SetPriority(PWR_WKUP_INT_IRQn, 1, 2);                      /* 鎶㈠崰浼樺厛绾2锛屽瓙浼樺厛绾2 */
    HAL_NVIC_EnableIRQ(PWR_WKUP_INT_IRQn);
}

/**
 * @brief       WK_UP鎸夐敭 澶栭儴涓鏂鏈嶅姟绋嬪簭
 * @param       鏃
 * @retval      鏃
 */
void PWR_WKUP_INT_IRQHandler(void)
{
    HAL_ResumeTick();                                                   /* 鎭㈠嶆淮绛旀椂閽 */
    HAL_GPIO_EXTI_IRQHandler(PWR_WKUP_GPIO_PIN);
}

/**
 * @brief       澶栭儴涓鏂鍥炶皟鍑芥暟
 * @param       GPIO_Pin:涓鏂绾垮紩鑴
 * @note        姝ゅ嚱鏁颁細琚玃WR_WKUP_INT_IRQHandler()璋冪敤
 * @retval      鏃
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == PWR_WKUP_GPIO_PIN)
    {
        /* HAL_GPIO_EXTI_IRQHandler()鍑芥暟宸茬粡涓烘垜浠娓呴櫎浜嗕腑鏂鏍囧織浣嶏紝鎵浠ユ垜浠杩涗簡鍥炶皟鍑芥暟鍙浠ヤ笉鍋氫换浣曚簨 */
    }
}

/**
 * @brief       杩涘叆鍋滄㈡ā寮
 * @param       鏃
 * @retval      鏃
 */
void pwr_enter_stop(void)
{
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_SuspendTick();
    HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);
    sys_stm32_clock_init();
    HAL_ResumeTick();
}


/**
 * @brief       配置未使用引脚为模拟模式，防止漏电
 * @param       无
 * @retval      无
 */
void pwr_unused_gpio_init(void)
{
    GPIO_InitTypeDef gpio_init_struct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* 配置 GPIOA 未使用的引脚为模拟输入模式，无上下拉 */
    gpio_init_struct.Pin = GPIO_PIN_0 | GPIO_PIN_9 | GPIO_PIN_10 | 
                           GPIO_PIN_11 | GPIO_PIN_12;
    gpio_init_struct.Mode = GPIO_MODE_ANALOG;
    gpio_init_struct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &gpio_init_struct);

    /* 配置 GPIOB 未使用的引脚为模拟输入模式，无上下拉 */
    gpio_init_struct.Pin = GPIO_PIN_0 | GPIO_PIN_3 | GPIO_PIN_6 | 
                           GPIO_PIN_7;
    HAL_GPIO_Init(GPIOB, &gpio_init_struct);
}
