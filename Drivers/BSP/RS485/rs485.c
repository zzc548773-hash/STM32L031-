/**
 ****************************************************************************************************
 * @file        rs485.c
 * @author      正点原子团队(ALIENTEK) / STM32L031
 * @version     V2.1
 * @date        2026-07-23
 * @brief       RS485 驱动代码 (STM32L031G6U6 硬件 UART2 & 内部硬件 CRC)
 ****************************************************************************************************
 */

#include "./BSP/RS485/rs485.h"
#include "./SYSTEM/delay/delay.h"
#include "stm32l0xx_hal.h"
#include <string.h>

UART_HandleTypeDef g_rs458_handler;     /* RS485 串口句柄 (USART2) */
CRC_HandleTypeDef  g_crc_handle;        /* 内部硬件 CRC 句柄 */

#if RS485_EN_RX

uint8_t g_RS485_rx_buf[RS485_REC_LEN];  /* 接收缓冲区 */
uint8_t g_RS485_rx_cnt = 0;             /* 接收字节计数器 */

/**
 * @brief       RS485 串口中断服务函数 (USART2_IRQHandler)
 * @note        直接读取 RDR 寄存器清 RXNE 标志，防止在 ISR 中阻塞或卡死 MCU；
 *              自动清除 ORE/FE/NE/PE 溢出与杂波错误标志。
 */
void RS485_UX_IRQHandler(void)
{
    uint8_t res;

    /* 1. 接收数据中断处理 */
    if (__HAL_UART_GET_FLAG(&g_rs458_handler, UART_FLAG_RXNE) != RESET)
    {
        /* 直接读取 RDR 寄存器，自动清除 RXNE 标志，避免 HAL_UART_Receive 阻塞卡死 */
        res = (uint8_t)(g_rs458_handler.Instance->RDR & 0x00FF);

        if (g_RS485_rx_cnt < RS485_REC_LEN)
        {
            g_RS485_rx_buf[g_RS485_rx_cnt++] = res;
        }
    }

    /* 2. 溢出与总线噪声错误标志自动清除 (防止 UART 挂起停止接收) */
    if (__HAL_UART_GET_FLAG(&g_rs458_handler, UART_FLAG_ORE) != RESET)
    {
        __HAL_UART_CLEAR_FLAG(&g_rs458_handler, UART_CLEAR_OREF);
    }
    if (__HAL_UART_GET_FLAG(&g_rs458_handler, UART_FLAG_FE) != RESET)
    {
        __HAL_UART_CLEAR_FLAG(&g_rs458_handler, UART_CLEAR_FEF);
    }
    if (__HAL_UART_GET_FLAG(&g_rs458_handler, UART_FLAG_NE) != RESET)
    {
        __HAL_UART_CLEAR_FLAG(&g_rs458_handler, UART_CLEAR_NEF);
    }
    if (__HAL_UART_GET_FLAG(&g_rs458_handler, UART_FLAG_PE) != RESET)
    {
        __HAL_UART_CLEAR_FLAG(&g_rs458_handler, UART_CLEAR_PEF);
    }
}

#endif

/**
 * @brief       STM32L031 内部硬件 CRC16 计算函数 (Modbus-16 标准)
 * @param       buf: 待计算数据指针
 * @param       len: 数据长度(字节)
 * @retval      16位 CRC 校验码 (已做高低字节调换，适配工程中的 crch = crc>>8 / crcl = crc&0xFF 打包习惯)
 */
uint16_t GetCRC16(uint8_t *buf, uint16_t len)
{
    uint32_t i;
    uint32_t crc_calc;

    if (buf == NULL || len == 0) return 0;

    /* 1. 复位硬件 CRC 数据寄存器为初始值 (0xFFFF) */
    __HAL_CRC_DR_RESET(&g_crc_handle);

    /* 2. 逐字节写入 DR 寄存器，完全规避 Cortex-M0+ 非 4 字节对齐访问导致的 HardFault 崩溃 */
    for (i = 0; i < len; i++)
    {
        *(__IO uint8_t *)(&g_crc_handle.Instance->DR) = buf[i];
    }

    crc_calc = g_crc_handle.Instance->DR & 0xFFFF;

    /* 3. 翻转高低字节：
     * 硬件 CRC 算出的低字节在低 8 位，而工程后续代码统一使用 ucBuf[6] = crc >> 8; ucBuf[7] = crc & 0xFF; 填充 Modbus 帧。
     * 进行字节翻转后，可完美保证从 PA9/TX 串口发送出去的 Modbus 帧末尾校验字节顺序为 [低字节 LSB, 高字节 MSB]！
     */
    return (uint16_t)(((crc_calc & 0x00FF) << 8) | ((crc_calc & 0xFF00) >> 8));
}

/**
 * @brief       RS485 初始化 (包含串口引脚配置与硬件 CRC 初始化)
 * @param       baudrate: 波特率 (根据传感器硬件需求配置，如 9600 或 4800)
 * @retval      无
 */
void rs485_init(uint32_t baudrate)
{
    GPIO_InitTypeDef gpio_initure = {0};

    /* 1. 使能 GPIO 与 串口/CRC 外设时钟 */
    RS485_RE_GPIO_CLK_ENABLE();
    RS485_TX_GPIO_CLK_ENABLE();
    RS485_RX_GPIO_CLK_ENABLE();
    RS485_UX_CLK_ENABLE();
    __HAL_RCC_CRC_CLK_ENABLE();

    /* 2. 配置 TX 引脚 (PA9 -> AF4_USART2) */
    gpio_initure.Pin       = RS485_TX_GPIO_PIN;
    gpio_initure.Mode      = GPIO_MODE_AF_PP;
    gpio_initure.Pull      = GPIO_PULLUP;
    gpio_initure.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio_initure.Alternate = GPIO_AF4_USART2;
    HAL_GPIO_Init(RS485_TX_GPIO_PORT, &gpio_initure);

    /* 3. 配置 RX 引脚 (PA10 -> AF4_USART2) */
    gpio_initure.Pin       = RS485_RX_GPIO_PIN;
    gpio_initure.Mode      = GPIO_MODE_AF_PP;
    gpio_initure.Pull      = GPIO_PULLUP;
    gpio_initure.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio_initure.Alternate = GPIO_AF4_USART2;
    HAL_GPIO_Init(RS485_RX_GPIO_PORT, &gpio_initure);

    /* 4. 配置 RE 发送接收控制引脚 (PA8 -> Output Push-Pull) */
    gpio_initure.Pin       = RS485_RE_GPIO_PIN;
    gpio_initure.Mode      = GPIO_MODE_OUTPUT_PP;
    gpio_initure.Pull      = GPIO_PULLUP;
    gpio_initure.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(RS485_RE_GPIO_PORT, &gpio_initure);

    /* 默认初始化为接收模式 */
    RS485_RE(0);

    /* 5. 串口参数配置 (USART2) */
    g_rs458_handler.Instance                    = RS485_UX;
    g_rs458_handler.Init.BaudRate               = baudrate;
    g_rs458_handler.Init.WordLength             = UART_WORDLENGTH_8B;
    g_rs458_handler.Init.StopBits               = UART_STOPBITS_1;
    g_rs458_handler.Init.Parity                 = UART_PARITY_NONE;
    g_rs458_handler.Init.HwFlowCtl              = UART_HWCONTROL_NONE;
    g_rs458_handler.Init.Mode                   = UART_MODE_TX_RX;
    g_rs458_handler.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    HAL_UART_Init(&g_rs458_handler);

#if RS485_EN_RX
    __HAL_UART_ENABLE_IT(&g_rs458_handler, UART_IT_RXNE);
    HAL_NVIC_SetPriority(RS485_UX_IRQn, 1, 3);
    HAL_NVIC_EnableIRQ(RS485_UX_IRQn);
#endif

    /* 6. 初始化 STM32L031 内部硬件 CRC (Modbus-16 标准) */
    g_crc_handle.Instance                     = CRC;
    g_crc_handle.Init.DefaultPolynomialUse    = DEFAULT_POLYNOMIAL_DISABLE;
    g_crc_handle.Init.GeneratingPolynomial    = 0x8005;
    g_crc_handle.Init.CRCLength               = CRC_POLYLENGTH_16B;
    g_crc_handle.Init.InitValue               = 0xFFFF;
    g_crc_handle.Init.InputDataInversionMode  = CRC_INPUTDATA_INVERSION_BYTE;
    g_crc_handle.Init.OutputDataInversionMode = CRC_OUTPUTDATA_INVERSION_ENABLE;
    g_crc_handle.InputDataFormat              = CRC_INPUTDATA_FORMAT_BYTES;
    HAL_CRC_Init(&g_crc_handle);
}

/**
 * @brief       RS485 发送数据
 * @param       buf: 发送数据首地址
 * @param       len: 发送字节数
 * @retval      无
 */
void rs485_send_data(uint8_t *buf, uint8_t len)
{
    /* 发送前清空接收计数器与接收缓冲区 */
    g_RS485_rx_cnt = 0;
    memset(g_RS485_rx_buf, 0, sizeof(g_RS485_rx_buf));

    RS485_RE(1);    /* 切换为发送状态 */
    HAL_UART_Transmit(&g_rs458_handler, buf, len, 1000);

    /* 关键等待：确保最后一个字节完全脱离移位寄存器从 PA9 发出，防止提前拉低 RE 关断 485 导致尾部 CRC 丢失 */
    while (__HAL_UART_GET_FLAG(&g_rs458_handler, UART_FLAG_TC) == RESET);

    RS485_RE(0);    /* 切换为接收状态 */
}

/**
 * @brief       RS485 接收数据
 * @param       buf: 接收数据首地址
 * @param       len: 接收数据长度
 * @retval      无
 */
void rs485_receive_data(uint8_t *buf, uint8_t *len)
{
    uint8_t rxlen = g_RS485_rx_cnt;
    uint8_t i = 0;

    delay_ms(10);

    if (rxlen == g_RS485_rx_cnt && rxlen != 0)
    {
        for (i = 0; i < rxlen; i++)
        {
            buf[i] = g_RS485_rx_buf[i];
        }

        *len = g_RS485_rx_cnt;
        g_RS485_rx_cnt = 0;
    }
}

/**
 * @brief       RS485 读写操作统一打包发送
 */
void RS485_RW_Opr(uint8_t ucAddr, uint8_t ucCmd, uint16_t ucReg, uint16_t uiDate)
{
    uint16_t crc;
    uint8_t crcl, crch;
    uint8_t ucBuf[8];

    ucBuf[0] = ucAddr;
    ucBuf[1] = ucCmd;
    ucBuf[2] = ucReg >> 8;
    ucBuf[3] = ucReg & 0xFF;
    ucBuf[4] = uiDate >> 8;
    ucBuf[5] = uiDate & 0xFF;

    crc      = GetCRC16(ucBuf, 6);
    crch     = (uint8_t)(crc >> 8);
    crcl     = (uint8_t)(crc & 0xFF);

    ucBuf[6] = crch;
    ucBuf[7] = crcl;

    rs485_send_data(ucBuf, 8);
}

/**
 * @brief       修改传感器地址校验帧
 */
uint8_t dizhi_shengchengzhen(uint8_t dizhi_jiu, uint8_t dizhi_xin, uint8_t *buf)
{
    uint16_t crc;
    uint8_t crcl, crch;
    uint8_t ucBuf[8];

    ucBuf[0] = dizhi_jiu;
    ucBuf[1] = WRITE;
    ucBuf[2] = 0x0100 >> 8;
    ucBuf[3] = 0x00;
    ucBuf[4] = dizhi_xin >> 8;
    ucBuf[5] = dizhi_xin & 0xFF;

    crc      = GetCRC16(ucBuf, 6);
    crch     = (uint8_t)(crc >> 8);
    crcl     = (uint8_t)(crc & 0xFF);

    ucBuf[6] = crch;
    ucBuf[7] = crcl;

    for (int i = 0; i < 8; i++)
    {
        if (ucBuf[i] != buf[i])
        {
            return 0;
        }
    }
    return 1;
}

/**
 * @brief       设置土壤传感器地址
 */
uint8_t RS485_set_dizhi_turang(uint8_t dizhi_jiu, uint8_t mubiao)
{
    uint8_t num = 0, z = 0;
    uint16_t crc;
    uint8_t crcl, crch;
    uint8_t ucBuf[11], ucBuf_2[11];

    ucBuf[0] = dizhi_jiu;
    ucBuf[1] = 0x10;
    ucBuf[2] = 0;
    ucBuf[3] = 0x80;
    ucBuf[4] = 0;
    ucBuf[5] = 1;
    ucBuf[6] = 2;
    ucBuf[7] = 0;
    ucBuf[8] = mubiao;

    crc      = GetCRC16(ucBuf, 9);
    crch     = (uint8_t)(crc >> 8);
    crcl     = (uint8_t)(crc & 0xFF);
    ucBuf[9] = crch;
    ucBuf[10]= crcl;

    rs485_send_data(ucBuf, 11);

    while (!num)
    {
        rs485_receive_data(ucBuf_2, &num);
        z++;
        delay_ms(100);
        if (z == 50) break;
    }

    crc      = GetCRC16(ucBuf, 6);
    crch     = (uint8_t)(crc >> 8);
    crcl     = (uint8_t)(crc & 0xFF);
    ucBuf[6] = crch;
    ucBuf[7] = crcl;

    if (num == 8)
    {
        for (int i = 0; i < 8; i++)
        {
            if (ucBuf[i] != ucBuf_2[i])
            {
                z = 80;
            }
        }
        if (z != 80)
        {
            return 0;
        }
    }
    return 1;
}

/**
 * @brief       读取 CAN/常规 传感器数据
 */
void RS485_READ_dizhi_can(uint8_t dizhi, uint16_t *buf)
{
    uint8_t num = 0, z = 0;
    uint16_t crc;
    uint8_t crcl, crch;
    uint8_t ucBuf[8], ucBuf_2[10] = {0};

    ucBuf[0] = dizhi;
    ucBuf[1] = READ;
    ucBuf[2] = 0x00;
    ucBuf[3] = 0x00;
    ucBuf[4] = 0x00;
    ucBuf[5] = 2;

    crc      = GetCRC16(ucBuf, 6);
    crch     = (uint8_t)(crc >> 8);
    crcl     = (uint8_t)(crc & 0xFF);
    ucBuf[6] = crch;
    ucBuf[7] = crcl;

    rs485_send_data(ucBuf, 8);

    while (!num)
    {
        rs485_receive_data(ucBuf_2, &num);
        z++;
        delay_ms(100);
        if (z == 10) break;
    }

    buf[0] = (ucBuf_2[3] << 8) + ucBuf_2[4];
    buf[1] = (ucBuf_2[5] << 8) + ucBuf_2[6];
}

/**
 * @brief       读取 土壤 传感器数据
 */
void RS485_READ_dizhi_can_turang(uint8_t dizhi, uint16_t *buf)
{
    uint8_t num = 0, z = 0;
    uint16_t crc;
    uint8_t crcl, crch;
    uint8_t ucBuf[8], ucBuf_2[64] = {0};

    ucBuf[0] = dizhi;
    ucBuf[1] = READ;
    ucBuf[2] = 0x00;
    ucBuf[3] = 0x00;
    ucBuf[4] = 0x00;
    ucBuf[5] = 2;

    crc      = GetCRC16(ucBuf, 6);
    crch     = (uint8_t)(crc >> 8);
    crcl     = (uint8_t)(crc & 0xFF);
    ucBuf[6] = crch;
    ucBuf[7] = crcl;

    rs485_send_data(ucBuf, 8);

    while (!num)
    {
        rs485_receive_data(ucBuf_2, &num);
        z++;
        delay_ms(100);
        if (z == 10) break;
    }

    buf[0] = (ucBuf_2[3] << 8) + ucBuf_2[4];
    buf[1] = (ucBuf_2[5] << 8) + ucBuf_2[6];
}

/**
 * @brief       读取 百叶盒温湿度拐 传感器数据
 */
void RS485_READ_dizhi_baiye_wsg(uint8_t dizhi, uint16_t *buf)
{
    uint8_t num = 0, z = 0;
    uint16_t crc;
    uint8_t crcl, crch;
    uint8_t ucBuf[8], ucBuf_2[64] = {0};

    ucBuf[0] = dizhi;
    ucBuf[1] = READ;
    ucBuf[2] = 0x01;
    ucBuf[3] = 0xF4;
    ucBuf[4] = 0x00;
    ucBuf[5] = 2;

    crc      = GetCRC16(ucBuf, 6);
    crch     = (uint8_t)(crc >> 8);
    crcl     = (uint8_t)(crc & 0xFF);
    ucBuf[6] = crch;
    ucBuf[7] = crcl;

    rs485_send_data(ucBuf, 8);

    while (!num)
    {
        rs485_receive_data(ucBuf_2, &num);
        z++;
        delay_ms(100);
        if (z == 10) break;
    }

    buf[0] = (ucBuf_2[3] << 8) + ucBuf_2[4];
    buf[1] = (ucBuf_2[5] << 8) + ucBuf_2[6];
}

/**
 * @brief       读取 建大/特定 传感器数据 (通道1)
 */
void RS485_READ_dizhi_can_jianda(uint8_t dizhi, uint16_t *buf, uint16_t a)
{
    uint8_t num = 0, z = 0;
    uint16_t crc;
    uint8_t crcl, crch;
    uint8_t ucBuf[8], ucBuf_2[64] = {0};

    ucBuf[0] = dizhi;
    ucBuf[1] = READ;
    ucBuf[2] = 0x00;
    ucBuf[3] = 0x00;
    ucBuf[4] = 0x00;
    ucBuf[5] = 1;

    crc      = GetCRC16(ucBuf, 6);
    crch     = (uint8_t)(crc >> 8);
    crcl     = (uint8_t)(crc & 0xFF);
    ucBuf[6] = crch;
    ucBuf[7] = crcl;

    rs485_send_data(ucBuf, 8);

    while (!num)
    {
        rs485_receive_data(ucBuf_2, &num);
        z++;
        delay_ms(100);
        if (z == 10) break;
    }

    buf[a] = (ucBuf_2[3] << 8) + ucBuf_2[4];
}

/**
 * @brief       读取 建大/特定 传感器数据 (通道2)
 */
void RS485_READ_dizhi_can_jianda_2(uint8_t dizhi, uint16_t *buf, uint16_t a)
{
    uint8_t num = 0, z = 0;
    uint16_t crc;
    uint8_t crcl, crch;
    uint8_t ucBuf[8], ucBuf_2[64] = {0};

    ucBuf[0] = dizhi;
    ucBuf[1] = READ;
    ucBuf[2] = 0x00;
    ucBuf[3] = 0x02;
    ucBuf[4] = 0x00;
    ucBuf[5] = 1;

    crc      = GetCRC16(ucBuf, 6);
    crch     = (uint8_t)(crc >> 8);
    crcl     = (uint8_t)(crc & 0xFF);
    ucBuf[6] = crch;
    ucBuf[7] = crcl;

    rs485_send_data(ucBuf, 8);

    while (!num)
    {
        rs485_receive_data(ucBuf_2, &num);
        z++;
        delay_ms(100);
        if (z == 10) break;
    }

    buf[a] = (ucBuf_2[3] << 8) + ucBuf_2[4];
}

/**
 * @brief       雨量传感器清零
 */
uint8_t RS485_yuliang_lost(uint8_t dizhi)
{
    uint8_t num = 0, z = 0;
    uint16_t crc;
    uint8_t crcl, crch;
    uint8_t ucBuf[8], ucBuf_2[64] = {0};

    ucBuf[0] = dizhi;
    ucBuf[1] = WRITE;
    ucBuf[2] = 0x00;
    ucBuf[3] = 0x00;
    ucBuf[4] = 0x00;
    ucBuf[5] = 0x5A;

    crc      = GetCRC16(ucBuf, 6);
    crch     = (uint8_t)(crc >> 8);
    crcl     = (uint8_t)(crc & 0xFF);
    ucBuf[6] = crch;
    ucBuf[7] = crcl;

    rs485_send_data(ucBuf, 8);

    while (!num)
    {
        rs485_receive_data(ucBuf_2, &num);
        z++;
        delay_ms(100);
        if (z == 10) return 1;
    }

    return 0;
}
