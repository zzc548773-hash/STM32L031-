/**
 ****************************************************************************************************
 * @file        main.c
 * @author      正点原子团队(ALIENTEK) / 气象站 4G MQTT 控制器
 * @version     V1.4
 * @date        2026-07-23
 * @brief       纯净版：仅被动响应云端下发，带 post_reply 订阅，去除所有定时上报和网络重试
 ****************************************************************************************************
 */

#include "./SYSTEM/sys/sys.h"
#include "./SYSTEM/usart/usart.h"
#include "./SYSTEM/delay/delay.h"
#include "./BSP/RS485/rs485.h"
#include "./BSP/DX_CT511/dx_ct511.h"
#include "./BSP/EEPROM/eeprom.h"
#include "string.h"
#include "stdio.h"
#include "stdlib.h"

/* ----------------- 全局变量区 ----------------- */
uint16_t g_sensor_data[8];

/* MQTT 订阅主题 */
char g_sub_topic[128] = {0};
char g_sub_post_reply_topic[128] = {0};

/* ----------------- 函数声明 ----------------- */
void process_mqtt_message(void);

/**
 * @brief  MQTT 消息处理与命令下发解析 (含 set_reply ACK)
 */
void process_mqtt_message(void)
{
    uint8_t *raw_rx_buf = dx_ct511_uart_rx_get_frame();

    if (raw_rx_buf != NULL)
    {
        /* 1. 将接收到的数据拷贝到静态保护区，防止下发 MQTT 时缓冲区被覆盖 */
        static char safe_rx_buf[512];
        memset(safe_rx_buf, 0, sizeof(safe_rx_buf));
        strncpy(safe_rx_buf, (char *)raw_rx_buf, sizeof(safe_rx_buf) - 1);

        uint8_t *rx_buf = (uint8_t *)safe_rx_buf;

        /* 2. 提取 "id":"xxx" 并回发 set_reply ACK */
        char *id_ptr = strstr((char *)rx_buf, "\"id\":\"");
        if (id_ptr != NULL)
        {
            char extracted_id[32] = {0};
            char reply_payload[128] = {0};

            id_ptr += 6; /* 越过 "\"id\":\"" */

            for (int i = 0; i < 31; i++)
            {
                if (id_ptr[i] == '"' || id_ptr[i] == '\0')
                {
                    extracted_id[i] = '\0';
                    break;
                }
                extracted_id[i] = id_ptr[i];
            }

            /* 严格复刻水阀 2.0：收到指令必须先回复 set_reply ACK */
            sprintf(reply_payload, "{\"id\":\"%s\",\"code\":200,\"msg\":\"success\"}", extracted_id);
            char pub_topic[128] = {0};
            sprintf(pub_topic, "$sys/%s/%s/thing/property/set_reply", ONENET_PRODUCT_ID, ONENET_DEVICE_NAME);
            dx_ct511_mqtt_publish(pub_topic, reply_payload);

            /* 3. 校验 License */
            char *license_ptr = strstr((char *)rx_buf, "\"License\":\"");
            if (license_ptr != NULL)
            {
                char extracted_license[32] = {0};
                license_ptr += 11; /* 越过 "\"License\":\"" */

                for (int i = 0; i < 31; i++)
                {
                    if (license_ptr[i] == '"' || license_ptr[i] == '\0')
                    {
                        extracted_license[i] = '\0';
                        break;
                    }
                    extracted_license[i] = license_ptr[i];
                }

                int license_passed = 0;

                /* 若带 NM 前缀则直接写入 EEPROM，偏移量 100 */
                if (strncmp(extracted_license, "NM", 2) == 0)
                {
                    eeprom_write_string(100, extracted_license + 2);
                    license_passed = 1;
                }
                else
                {
                    /* 否则与 EEPROM 比对 */
                    char stored_license[32] = {0};
                    eeprom_read_string(100, stored_license, 32);

                    if ((unsigned char)stored_license[0] != 0xFF && stored_license[0] != '\0')
                    {
                        if (strcmp(extracted_license, stored_license) == 0)
                        {
                            license_passed = 1;
                        }
                    }
                    else
                    {
                        license_passed = 1; /* EEPROM 为空，默认放行 */
                    }
                }

                /* 4. License 校验通过，解析 calibration */
                if (license_passed == 1)
                {
                    /* ----- 处理 calibration == 1 (全量上报) ----- */
                    char *calib_ptr = strstr((char *)rx_buf, "\"calibration\":");
                    if (calib_ptr != NULL)
                    {
                        int calib_val = 0;
                        calib_ptr += 14; /* 越过 "\"calibration\":" */
                        sscanf(calib_ptr, "%d", &calib_val);

                        if (calib_val == 1)
                        {
                            /* 采集 RS485 传感器 */
                            RS485_READ_dizhi_baiye_wsg(1, &g_sensor_data[0]);
                            RS485_READ_dizhi_can_turang(2, &g_sensor_data[4]);

                            float hum_kq = g_sensor_data[0] / 10.0f;
                            float tem_kq = g_sensor_data[1] / 10.0f;
                            float tem_tr = g_sensor_data[4] / 10.0f; /* 修正：buf[0] 为土壤温度 */
                            float hum_tr = g_sensor_data[5] / 10.0f; /* 修正：buf[1] 为土壤湿度 */

                            /* 按照水阀 2.0 格式严格组装：必须带 "value" 嵌套 (注意 hun_TR 是平台确切标识符) */
                            char post_payload[512] = {0};
                            sprintf(post_payload, "{\"id\":\"123\",\"version\":\"1.0\",\"params\":{\"hum_KQ\":{\"value\":%.1f},\"tem_KQ\":{\"value\":%.1f},\"hum_TR\":{\"value\":%.1f},\"tem_TR\":{\"value\":%.1f},\"calibration\":{\"value\":1}}}",
                                    hum_kq, tem_kq, hum_tr, tem_tr);

                            char post_topic[512] = {0};
                            sprintf(post_topic, "$sys/%s/%s/thing/property/post", ONENET_PRODUCT_ID, ONENET_DEVICE_NAME);
                            dx_ct511_mqtt_publish(post_topic, post_payload);
                        }
                    }
                }
            }
        }

        dx_ct511_uart_rx_restart();
    }
}

/**
 * @brief  网络状态监测与假死自愈处理 (每 1 分钟检测一次 AT+CEREG?)
 */
void check_network_status(void)
{
    static uint32_t last_check_tick = 0;
    if ((HAL_GetTick() - last_check_tick) >= 60000)
    {
        last_check_tick = HAL_GetTick();

        /* 发送 AT+CEREG? 检查 4G 模组串口与基站响应 */
        if (dx_ct511_send_cmd("AT+CEREG?", "OK", 1000) != DX_CT511_EOK)
        {
            /* 若 4G 模组死机无响应，硬件复位并重新连接拉回在线状态 */
            dx_ct511_power_cycle();
            dx_ct511_init();
            while (dx_ct511_mqtt_connect_onenet(ONENET_DEVICE_NAME, ONENET_PRODUCT_ID, ONENET_TOKEN) != DX_CT511_EOK)
            {
                delay_ms(1000);
            }
            dx_ct511_mqtt_subscribe(g_sub_topic);
            delay_ms(500);
            dx_ct511_mqtt_subscribe(g_sub_post_reply_topic);
        }
    }
}

int main(void)
{
    HAL_Init();                                 /* 初始化 HAL 库 */
    sys_stm32_clock_init();                     /* 设置系统时钟为 16MHz */
    delay_init(16);                             /* 初始化延时 */
    usart_init(115200);                         /* 初始化串口1 */
    rs485_init(9600);                           /* 初始化 RS485 及硬件 CRC (9600) */

    /* 初始化 4G 模块 */
    dx_ct511_init();

    /* 连接 OneNet MQTT 平台 */
    while (dx_ct511_mqtt_connect_onenet(ONENET_DEVICE_NAME, ONENET_PRODUCT_ID, ONENET_TOKEN) != DX_CT511_EOK)
    {
        delay_ms(1000);
    }

    delay_ms(1000);

    /* 订阅下发命令 Topic */
    sprintf(g_sub_topic, "$sys/%s/%s/thing/property/set", ONENET_PRODUCT_ID, ONENET_DEVICE_NAME);
    dx_ct511_mqtt_subscribe(g_sub_topic);
    
    delay_ms(500);

    /* 额外订阅 post_reply 主题 */
    sprintf(g_sub_post_reply_topic, "$sys/%s/%s/thing/property/post_reply", ONENET_PRODUCT_ID, ONENET_DEVICE_NAME);
    dx_ct511_mqtt_subscribe(g_sub_post_reply_topic);

    while (1)
    {
        /* 1. 在主循环查询是否有 MQTT 下发数据 */
        process_mqtt_message();
        
        /* 2. 定期 (每 1 分钟) 监测 4G 网络响应，防假死自愈 */
        check_network_status();

        delay_ms(50);
    }
}
