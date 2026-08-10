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
    DX_CT511_PWR_GPIO_CLK_ENABLE();

    /* DTR引脚初始化，默认拉低不影响功能 */
    gpio_init_struct.Pin = DX_CT511_DTR_GPIO_PIN;
    gpio_init_struct.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init_struct.Pull = GPIO_PULLUP;
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(DX_CT511_DTR_GPIO_PORT, &gpio_init_struct);
    DX_CT511_DTR(0); 

    /* RST/VIN_EN引脚初始化 */
    gpio_init_struct.Pin = DX_CT511_RST_GPIO_PIN;
    HAL_GPIO_Init(DX_CT511_RST_GPIO_PORT, &gpio_init_struct);
    DX_CT511_RST(1); /* 默认使能模块 */

    /* 6V总电源控制初始化 (如果电机那里已经初始化过，这里会覆盖但状态一致) */
    gpio_init_struct.Pin = DX_CT511_PWR_GPIO_PIN;
    HAL_GPIO_Init(DX_CT511_PWR_GPIO_PORT, &gpio_init_struct);
    DX_CT511_PWR(1); /* 默认打开 6V 供电 */

    /* 初始化底层串口, 波特率 115200 */
    dx_ct511_uart_init(115200);
    
    /* 循环发送 AT 指令，等待模块开机响应 */
    while(dx_ct511_send_cmd("AT", "OK", 1000) != 0)
    {
        delay_ms(500); // 没等到就等 500ms 再试，直到模块开机
    }
}

/**
 * @brief       DX-CT511 模块断电重启 (死机恢复用)
 * @note        此操作会短暂拉低 PA7，导致共用 6V 的电机短暂断电
 * @param       无
 * @retval      无
 */
void dx_ct511_power_cycle(void)
{
    /* 断电 */
    DX_CT511_PWR(0);
    DX_CT511_RST(0);
    delay_ms(1000); /* 彻底放电等待 */

    /* 重新上电 */
    DX_CT511_PWR(1);
    delay_ms(100);
    DX_CT511_RST(1);
    
    /* 等待模块开机初始化完成 */
    delay_ms(3000); 
    dx_ct511_uart_rx_restart();
}

/**
 * @brief       发送 AT 指令并等待预期应答
 * @param       cmd      : 要发送的指令 (注意自行加上 \r\n，如果没有的话这里补充)
 * @param       ack      : 期待接收到的应答字符串 (比如 "OK")，传NULL表示不等待应答
 * @param       wait_time: 等待超时时间 (单位 ms)
 * @retval      0:成功; 1:错误/未收到预期应答; 2:超时
 */
uint8_t dx_ct511_send_cmd(char *cmd, char *ack, uint32_t wait_time)
{
    uint8_t *rx_buf;
    uint32_t start_time;

    dx_ct511_uart_rx_restart();

    /* 发送命令，自动补充 \r\n 如果没有的话 */
    if(strstr(cmd, "\r\n"))
    {
        dx_ct511_uart_printf("%s", cmd);
    }
    else
    {
        dx_ct511_uart_printf("%s\r\n", cmd);
    }

    if (ack == NULL)
    {
        return DX_CT511_EOK;
    }

    start_time = HAL_GetTick();
    while ((HAL_GetTick() - start_time) < wait_time)
    {
        rx_buf = dx_ct511_uart_rx_get_frame();
        if (rx_buf != NULL)
        {
            if (strstr((const char *)rx_buf, ack) != NULL)
            {
                return DX_CT511_EOK; /* 收到期待的应答 */
            }
            /* 如果收到其他数据，可以选择重新开始接收继续等，这里简单处理重新接收 */
            dx_ct511_uart_rx_restart();
        }
        delay_ms(10);
    }

    return DX_CT511_ETIMEOUT;
}

/**
 * @brief       配置并连接 OneNET 平台 (MQTT)
 * @param       device_name: 设备名
 * @param       product_id : 产品ID
 * @param       token      : 鉴权Token
 * @retval      0:成功; 其他:失败
 */
uint8_t dx_ct511_mqtt_connect_onenet(char *device_name, char *product_id, char *token)
{
    char cmd_buf[512]={0};
    uint8_t res;

    /* 1. 开启网络 */
    res = dx_ct511_send_cmd("AT+NETOPEN", "OK", 5000);
    if(res != DX_CT511_EOK && res != DX_CT511_ERROR) 
    {
        // 如果已经打开可能会返回 ERROR 等，暂时继续往下走
    }

    /* 2. 配置 MQTT 客户端信息 */
    sprintf(cmd_buf, "AT+MCONFIG=\"%s\",\"%s\",\"%s\"", device_name, product_id, token);
    res = dx_ct511_send_cmd(cmd_buf, "OK", 2000);
    if(res != DX_CT511_EOK) return res;

    /* 3. 启动 MQTT 连接 (连接到移动云) */
    res = dx_ct511_send_cmd("AT+MIPSTART=\"mqtts.heclouds.com\",1883,4", "OK", 3000);
    if(res != DX_CT511_EOK) return res;

    /* 4. 发起连接建立请求 (KeepAlive=60s) */
    res = dx_ct511_send_cmd("AT+MCONNECT=1,60", "OK", 3000);
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
    char cmd_buf[512]={0};
    uint8_t res;
    uint16_t payload_len = strlen(payload);

    /* 1. 发送 MPUBEX 指令声明要发送的数据 */
    sprintf(cmd_buf, "AT+MPUBEX=\"%s\",0,0,%d", topic, payload_len);
    res = dx_ct511_send_cmd(cmd_buf, ">", 2000);
    if(res != DX_CT511_EOK) return res;

    /* 2. 在出现 ">" 提示符后，发送实际的 payload */
    dx_ct511_uart_rx_restart();
    dx_ct511_uart_printf("%s", payload); // 不带\r\n

    /* 等待 OK 返回 */
    uint32_t start_time = HAL_GetTick();
    while ((HAL_GetTick() - start_time) < 3000)
    {
        uint8_t *rx_buf = dx_ct511_uart_rx_get_frame();
        if (rx_buf != NULL)
        {
            if (strstr((const char *)rx_buf, "OK") != NULL)
            {
                return DX_CT511_EOK;
            }
            dx_ct511_uart_rx_restart();
        }
        delay_ms(10);
    }
    return DX_CT511_ETIMEOUT;
}

/**
 * @brief       MQTT 订阅主题 (接收平台下发控制)
 * @param       topic  : 订阅的主题，例如 "$sys/ProductID/DeviceName/thing/property/set"
 * @retval      0:成功; 其他:失败
 */
uint8_t dx_ct511_mqtt_subscribe(char *topic)
{
    char cmd_buf[128];
    sprintf(cmd_buf, "AT+MSUB=\"%s\",0", topic);
    return dx_ct511_send_cmd(cmd_buf, "OK", 3000);
}
