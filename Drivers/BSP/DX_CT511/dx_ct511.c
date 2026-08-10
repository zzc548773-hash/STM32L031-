/**
****************************************************************************************************
* @file        dx_ct511.c
* @author      正点原子(ALIENTEK)参考风格
* @version     V1.0
* @date        2026-06-30
* @brief       DX-CT511 4G模块核心控制驱动
****************************************************************************************************
*/
#include <string.h>
#include <stdio.h>
#include "./BSP/DX_CT511/dx_ct511.h"
#include "./SYSTEM/delay/delay.h"
volatile uint8_t g_dtr_lock = 0;
/**
* @brief       DX-CT511 模块初始化
* @param       无
* @retval      无
*/
void dx_ct511_init(void)
{
    GPIO_InitTypeDef gpio_init_struct;
    DX_CT511_DTR_GPIO_CLK_ENABLE();
    DX_CT511_RST_GPIO_CLK_ENABLE();

    /* DTR引脚初始化，默认拉高允许休眠 */
    gpio_init_struct.Pin = DX_CT511_DTR_GPIO_PIN;
    gpio_init_struct.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init_struct.Pull = GPIO_PULLUP;
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(DX_CT511_DTR_GPIO_PORT, &gpio_init_struct);
    DX_CT511_DTR(1); /* Default HIGH to allow sleep */

    /* RST引脚初始化 */
    gpio_init_struct.Pin = DX_CT511_RST_GPIO_PIN;
    HAL_GPIO_Init(DX_CT511_RST_GPIO_PORT, &gpio_init_struct);
    DX_CT511_RST(1); /* 默认使能模块 */

    /* 4G 模组直接由电池供电，去除了控制电源引脚 */

    /* 初始化底层串口, 波特率 115200 */
    dx_ct511_uart_init(115200);

    /* 不断发送 AT 指令，等待模块开机响应 */
    while (dx_ct511_send_cmd("AT", "OK", 1000) != 0)
    {
        delay_ms(500); // 没等到就等 500ms 再试，直到模块开机
    }
}

/**
 * @brief       DX-CT511 模块复位重启 (死机恢复用)
 * @param       无
 * @retval      无
 */
void dx_ct511_power_cycle(void)
{
    /* 拉低 RST 复位 */
    DX_CT511_RST(0);
    delay_ms(1000); /* 彻底复位等待 */

    /* 重新拉高使能 */
    DX_CT511_RST(1);

    /* 等待模块开机初始化完成 */
    delay_ms(3000);
    dx_ct511_uart_rx_restart();
}

/**
* @brief       发送 AT 指令并等待预期应答
* @param       cmd      : 要发送的指令 (注意自行加上 \r\n，如果没有的话这里补充)
* @param       ack      : 期待接收到的应答字符串 (比如 "OK")，传NULL表示不等待应答
* @param       timeout  : 等待超时时间 (单位 ms)
* @retval      0:成功; 1:错误/未收到预期应答; 2:超时
*/
uint8_t dx_ct511_send_cmd(char *cmd, char *ack, uint32_t timeout)
{
    uint8_t res = DX_CT511_ETIMEOUT;
    uint32_t start_time;

    if (g_dtr_lock == 0)
    {
        DX_CT511_DTR(0);
        delay_ms(20);
    }

    dx_ct511_uart_rx_restart();

    if (cmd != NULL && strlen(cmd) > 0)
    {
        dx_ct511_uart_printf("%s\r\n", cmd);
    }

    if (ack == NULL)
    {
        if (g_dtr_lock == 0) DX_CT511_DTR(1);
        return DX_CT511_EOK;
    }

    start_time = HAL_GetTick();

    while ((HAL_GetTick() - start_time) < timeout)
    {
        uint8_t *rx_buf = dx_ct511_uart_rx_get_frame();
        if (rx_buf != NULL)
        {
            if (strstr((const char *)rx_buf, ack) != NULL)
            {
                res = DX_CT511_EOK;
                break;
            }
            dx_ct511_uart_rx_restart();
        }
        delay_ms(10);
    }

    if (g_dtr_lock == 0)
    {
        DX_CT511_DTR(1);
    }

    return res;
}

uint8_t dx_ct511_mqtt_connect_onenet(char *device_name, char *product_id, char *token)
{
    char cmd_buf[512] = {0};
    uint8_t res;

    /* 1. 等待网络注册成功 (CEREG: 1=注册本地, 5=注册漫游), 最多等待 5 次 (~15秒) */
    {
        uint8_t cereg_retry = 0;
        while (cereg_retry < 5)  /* 最多等待 ~15秒，快速失败由退避机制接管 */
        {
            if (dx_ct511_send_cmd("AT+CEREG?", ",1", 1000) == DX_CT511_EOK ||
                dx_ct511_send_cmd("AT+CEREG?", ",5", 1000) == DX_CT511_EOK)
            {
                break; /* 网络已注册，继续 */
            }
            delay_ms(1000);
            cereg_retry++;
        }
        if (cereg_retry >= 5) return DX_CT511_ETIMEOUT;  /* 网络未就绪，快速返回让退避机制重试 */
    }

    /* 2. 配置 MQTT 客户端信息 */
    sprintf(cmd_buf, "AT+MCONFIG=\"%s\",\"%s\",\"%s\"", device_name, product_id, token);
    res = dx_ct511_send_cmd(cmd_buf, "OK", 2000);
    if (res != DX_CT511_EOK) return res;

    /* 3. 启动 MQTT 连接 (连接到移动云) */
    res = dx_ct511_send_cmd("AT+MIPSTART=\"mqtts.heclouds.com\",1883,4", "+MIPSTART: SUCCESS", 5000);
    if (res != DX_CT511_EOK) return res;

    /* 4. 发起连接建立请求 (KeepAlive=120s) */
    res = dx_ct511_send_cmd("AT+MCONNECT=1,120", "+MCONNECT: SUCCESS", 8000);
    return res;
}

/**
 * @brief       MQTT 发布消息 (上报数据)
 * @param       topic  : 发布的主题，例如 "$sys/ProductID/DeviceName/thing/property/post"
 * @param       payload: JSON 等格式的数据体
 * @retval      0:成功; 其他:失败
 */
uint8_t dx_ct511_mqtt_publish(char *topic, char *payload)
{
    char cmd_buf[512] = {0};
    uint8_t res;
    uint16_t payload_len = strlen(payload);

    /* 锁定 DTR 为低电平，禁止 send_cmd 内部拉高 DTR */
    delay_ms(20);

    /* 1. 发送 MPUBEX 指令，包含主题和长度 */
    sprintf(cmd_buf, "AT+MPUBEX=\"%s\",0,0,%d", topic, payload_len);
    res = dx_ct511_send_cmd(cmd_buf, ">", 2000);

    /* 2. 在收到 ">" 提示符后，发送实际的 payload */
    dx_ct511_uart_rx_restart();
    dx_ct511_uart_printf("%s", payload); // 不带\r\n

    /* 等待 OK 响应 */
    uint32_t start_time = HAL_GetTick();
    res = DX_CT511_ETIMEOUT;

    while ((HAL_GetTick() - start_time) < 3000)
    {
        uint8_t *rx_buf = dx_ct511_uart_rx_get_frame();
        if (rx_buf != NULL)
        {
            if (strstr((const char *)rx_buf, "OK") != NULL)
            {
                res = DX_CT511_EOK;
                break;
            }
            dx_ct511_uart_rx_restart();
        }
        delay_ms(10);
    }

    dx_ct511_uart_rx_restart();
    return res;
}

uint8_t dx_ct511_mqtt_subscribe(char *topic)
{
    char cmd_buf[128];
    sprintf(cmd_buf, "AT+MSUB=\"%s\",0", topic);
    return dx_ct511_send_cmd(cmd_buf, "+MSUB: SUCCESS", 5000);
}

/**
 * @brief       检查 MQTT 连接状态
 * @note        通过发送 AT+MQTTSTATU? 指令查询
 * @param       无
 * @retval      0: 未连接, 1: 已连接
 */
uint8_t dx_ct511_check_mqtt_status(void)
{
    if (dx_ct511_send_cmd("AT+MQTTSTATU?", "+MQTTSTATU: 1", 2000) == DX_CT511_EOK)
    {
        return 1;
    }
    return 0;
}
