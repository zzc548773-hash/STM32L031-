/**
 ****************************************************************************************************
 * @file        dx_ct511.h
 * @author      正点原子(ALIENTEK)参��格
 * @version     V1.0
 * @date        2026-06-30
 * @brief       DX-CT511 4G模块核心控制驱动
 ****************************************************************************************************
 */

#ifndef __DX_CT511_H
#define __DX_CT511_H

#include "./SYSTEM/sys/sys.h"
#include "./BSP/DX_CT511/dx_ct511_uart.h"

/* OneNET 平台鉴权信息 */
#define ONENET_DEVICE_NAME               "NMQXAACCC00005"
#define ONENET_PRODUCT_ID                "76s6o0LGb8"
#define ONENET_TOKEN                     "version=2018-10-31&res=products%2F76s6o0LGb8%2Fdevices%2FNMQXAACCC00005&et=3488948919&method=md5&sign=99z02TG2RWbXI0fNZii4Jw%3D%3D"

/* 引脚定义 */
#define DX_CT511_DTR_GPIO_PORT           GPIOA
#define DX_CT511_DTR_GPIO_PIN            GPIO_PIN_1
#define DX_CT511_DTR_GPIO_CLK_ENABLE()   do{ __HAL_RCC_GPIOA_CLK_ENABLE(); }while(0)

#define DX_CT511_RST_GPIO_PORT           GPIOA
#define DX_CT511_RST_GPIO_PIN            GPIO_PIN_4
#define DX_CT511_RST_GPIO_CLK_ENABLE()   do{ __HAL_RCC_GPIOA_CLK_ENABLE(); }while(0)



/* IO控制� */
#define DX_CT511_DTR(x)                  do{ x ? \
                                             HAL_GPIO_WritePin(DX_CT511_DTR_GPIO_PORT, DX_CT511_DTR_GPIO_PIN, GPIO_PIN_SET) : \
                                             HAL_GPIO_WritePin(DX_CT511_DTR_GPIO_PORT, DX_CT511_DTR_GPIO_PIN, GPIO_PIN_RESET); \
                                         }while(0)

#define DX_CT511_RST(x)                  do{ x ? \
                                             HAL_GPIO_WritePin(DX_CT511_RST_GPIO_PORT, DX_CT511_RST_GPIO_PIN, GPIO_PIN_SET) : \
                                             HAL_GPIO_WritePin(DX_CT511_RST_GPIO_PORT, DX_CT511_RST_GPIO_PIN, GPIO_PIN_RESET); \
                                         }while(0)



/* 状�代� */
#define DX_CT511_EOK         0             /* 成功 */
#define DX_CT511_ERROR       1             /* 通用错� */
#define DX_CT511_ETIMEOUT    2             /* 超时 */

/* 核心函数 */
void dx_ct511_init(void);
void dx_ct511_power_cycle(void);
uint8_t dx_ct511_send_cmd(char *cmd, char *ack, uint32_t wait_time);

/* MQTT & OneNET 相关功能 */
uint8_t dx_ct511_mqtt_connect_onenet(char *device_name, char *product_id, char *token);
uint8_t dx_ct511_mqtt_publish(char *topic, char *payload);
uint8_t dx_ct511_mqtt_subscribe(char *topic);
uint8_t dx_ct511_check_mqtt_status(void);

#endif
