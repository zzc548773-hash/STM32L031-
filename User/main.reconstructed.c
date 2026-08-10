/**
 ****************************************************************************************************
 * @file        main.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2020-04-20
 * @brief       跑马灯 实验
 * @license     Copyright (c) 2020-2032, 广州市星翼电子科技有限公司
 ****************************************************************************************************
 * @attention
 *
 * 实验平台:正点原子 STM32L031开发板
 * 在线视频:www.yuanzige.com
 * 技术论坛:www.openedv.com
 * 公司网址:www.alientek.com
 * 购买地址:openedv.taobao.com
 *
 ****************************************************************************************************
 */

#include "./SYSTEM/sys/sys.h"
#include "./SYSTEM/delay/delay.h"
#include "./BSP/PWR/pwr.h"
#include "./BSP/IWDG/iwdg.h"
#include "./SYSTEM/usart/usart.h"
#include "./BSP/RTC/rtc.h"
#include "./BSP/MT6701/mt6701.h"
#include "./BSP/MOTOR/motor.h"
#include <string.h>
#include "./BSP/EEPROM/eeprom.h"
#include "./BSP/DX_CT511/dx_ct511.h"
#include "./BSP/ADC/adc.h"

int16_t angle_raw,adc_val;;
float angle_deg;
uint8_t err,res;
void motor_angle(int angle_mode, float angle);
static float get_cw_dist(float target, float current);
volatile uint32_t my_number;

/* 处理收到的MQTT数据并回复 */
void process_mqtt_message(void)
{
    uint8_t *raw_rx_buf = dx_ct511_uart_rx_get_frame();
    if (raw_rx_buf != NULL)
    {
        /* 备份接收到的 JSON 数据，防止底层 AT 指令发回复时覆盖了全局 RX 缓冲区 */
        static char safe_rx_buf[512];
        memset(safe_rx_buf, 0, sizeof(safe_rx_buf));
        strncpy(safe_rx_buf, (char *)raw_rx_buf, sizeof(safe_rx_buf) - 1);
        
        uint8_t *rx_buf = (uint8_t *)safe_rx_buf;
        
        /* 判断是否包含 "id":" */
        char *id_ptr = strstr((char *)rx_buf, "\"id\":\"");
        if (id_ptr != NULL)
        {
            char extracted_id[32] = {0};
            char reply_payload[128]={0};
            
            id_ptr += 6; /* 移动指针跳过 "id":" 这6个字符 */
            
            /* 提取双引号内的 ID 字符串 */
            for (int i = 0; i < 31; i++)
            {
                if (id_ptr[i] == '"' || id_ptr[i] == '\0') 
                {
                    extracted_id[i] = '\0';
                    break;
                }
                extracted_id[i] = id_ptr[i];
            }
            
            /* 拼接回复用的 JSON 字符串并立刻发送 ACK */
            sprintf(reply_payload, "{\"id\":\"%s\",\"code\":200,\"msg\":\"success\"}", extracted_id);
            char pub_topic[128]={0};
            sprintf(pub_topic, "$sys/%s/%s/thing/property/set_reply", ONENET_PRODUCT_ID, ONENET_DEVICE_NAME);
            dx_ct511_mqtt_publish(pub_topic, reply_payload);
            
            /* =================== 下发命令业务逻辑解析 =================== */
            
            /* 1. 查找 License */
            char *license_ptr = strstr((char *)rx_buf, "\"License\":\"");
            if (license_ptr != NULL)
            {
                char extracted_license[32] = {0};
                license_ptr += 11; // 跳过 "\"License\":\""
                
                for (int i = 0; i < 31; i++) 
                {
                    if (license_ptr[i] == '"' || license_ptr[i] == '\0') 
                    {
                        extracted_license[i] = '\0';
                        break;
                    }
                    extracted_license[i] = license_ptr[i];
                }
                
                int license_passed = 0; /* 0表示未通过，1表示通过 */
                
                /* 判断是否有 NM 前缀 */
                if (strncmp(extracted_license, "NM", 2) == 0)
                {
                    /* 有 NM 前缀，设定用户编号并存储到 100 地址 */
                    eeprom_write_string(100, extracted_license + 2);
                    license_passed = 1; /* 设定新编号也算通过，继续往下执行控制命令 */
                }
                else
                {
                    /* 没有 NM 前缀，判断与存储的是否一致 */
                    char stored_license[32] = {0};
                    eeprom_read_string(100, stored_license, 32);
                    
                    /* 如果存储区为空 (全0xFF 或 首字符为\0)，则直接放行，视为通过 */
                    if ((unsigned char)stored_license[0] != 0xFF && stored_license[0] != '\0')
                    {
                        if (strcmp(extracted_license, stored_license) == 0)
                        {
                            license_passed = 1;
                        }
                    }
                    else
                    {
                        license_passed = 1; /* 空存储，直接放行 */
                    }
                }
                
                if (license_passed == 1)
                {
                    /* 如果验证通过，查看 calibration 标识符 */
                    char *calib_ptr = strstr((char *)rx_buf, "\"calibration\":");
                    if (calib_ptr != NULL)
                    {
                        int calib_val = 0;
                        calib_ptr += 14; // 跳过 "\"calibration\":"
                        sscanf(calib_ptr, "%d", &calib_val);
                        
                        if (calib_val == 1)
                        {
                            /* 返回当前的状态：计算真实的开口百分比 */
                            int16_t raw; float cur_ang;
                            mt6701_read_angle(&raw, &cur_ang);
                            float base_ang = (float)eeprom_read_word(0x00);
                            
                            /* 计算顺时针到达全开限位 (base_ang) 的距离 */
                            float cw_dist_to_base = get_cw_dist(base_ang, cur_ang);
                            float current_open_percent = 0.0f;
                            
                            if (cw_dist_to_base > 90.0f && cw_dist_to_base <= 270.0f) {
                                current_open_percent = 0.0f; /* 已经超过全关限位 */
                            } else if (cw_dist_to_base > 270.0f) {
                                current_open_percent = 100.0f; /* 已经超过全开限位 */
                            } else {
                                /* 距离全开越近 (cw_dist 越小)，开度越大 */
                                current_open_percent = (90.0f - cw_dist_to_base) / 90.0f * 100.0f;
                            }
                            
                            uint8_t battery_percent = adc_get_battery_percentage();
                            
                            char post_payload[512]= {0};
                            sprintf(post_payload, "{\"id\":\"123\",\"version\":\"1.0\",\"params\":{\"A_angle\":{\"value\":%.1f},\"Battery\":{\"value\":%d},\"calibration\":{\"value\":1}}}", current_open_percent, battery_percent);
                            char post_topic[512]= {0};
                            sprintf(post_topic, "$sys/%s/%s/thing/property/post", ONENET_PRODUCT_ID, ONENET_DEVICE_NAME);
                            dx_ct511_mqtt_publish(post_topic, post_payload);
                        }
                        else if (calib_val == 66)
                        {
                            /* 把当前的角度作为全开 (存入 0x00) */
                            int16_t raw; float cur_ang;
                            mt6701_read_angle(&raw, &cur_ang);
                            eeprom_write_word(0x00, (uint32_t)cur_ang);
                            
                            /* 并返回状态，此时开度定义为 100.0% */
                            float current_open_percent = 100.0f;
                            uint8_t battery_percent = adc_get_battery_percentage();
                            
                            char post_payload[512]= {0};
                            sprintf(post_payload, "{\"id\":\"123\",\"version\":\"1.0\",\"params\":{\"A_angle\":{\"value\":%.1f},\"Battery\":{\"value\":%d},\"calibration\":{\"value\":66}}}", current_open_percent, battery_percent);
                            char post_topic[512]= {0};
                            sprintf(post_topic, "$sys/%s/%s/thing/property/post", ONENET_PRODUCT_ID, ONENET_DEVICE_NAME);
                            dx_ct511_mqtt_publish(post_topic, post_payload);
                        }
                    }
                    else
                    {
                        /* 没有 calibration 标识符，查看 A_angle 控制阀门 */
                        char *angle_ptr = strstr((char *)rx_buf, "\"A_angle\":");
                        if (angle_ptr != NULL)
                        {
                            float angle_val = 0.0f;
                            angle_ptr += 10; // 跳过 "\"A_angle\":"
                            sscanf(angle_ptr, "%f", &angle_val);
                            
                            /* 安全钳位百分比 */
                            if (angle_val < 0.0f) angle_val = 0.0f;
                            if (angle_val > 100.0f) angle_val = 100.0f;
                            
                            float base_ang = (float)eeprom_read_word(0x00);
                            
                            /* 100% 对应 base_ang，0% 对应 base_ang - 90 */
                            float degree_to_subtract = ((100.0f - angle_val) / 100.0f) * 90.0f;
                            float target_angle = base_ang - degree_to_subtract;
                            
                            /* 处理 360 度圆周环绕 (越界修正) */
                            if (target_angle < 0.0f) target_angle += 360.0f;
                            if (target_angle >= 360.0f) target_angle -= 360.0f;
                            
                            /* 执行电机运转 (使用模式 3：自动选择最优路径) */
                            motor_angle(3, target_angle);
                        }
                    }
                }
            }
            ; /* C语言中label后面必须有语句 */
        }
        
        /* 无论是否解析成功，只要处理完了，必须重启接收状态，准备迎接下一帧 */
        dx_ct511_uart_rx_restart();
    }
}

int main(void)
{
    HAL_Init();                                 /* 初始化HAL库 */
    sys_stm32_clock_init();                     /* 设置时钟,16M */
    delay_init(16);                             /* 初始化延时函数 */
	  usart_init(115200); 
		motor_init();                                           /* 初始化电机引脚 */
		mt6701_init();                                          /* 初始化MT6701 */
		adc_init();
		adc_val = adc_get_result_average(BAT_ADC_CHANNEL, 10);
		dx_ct511_init();
//		my_number = eeprom_read_word(0x00);
//		delay_ms(500);
//		eeprom_write_word(0x00, 178);
//		delay_ms(500);
//		my_number = eeprom_read_word(0x00);
		 while(!dx_ct511_mqtt_connect_onenet(ONENET_DEVICE_NAME, ONENET_PRODUCT_ID, ONENET_TOKEN)) delay_ms(500);
		
        
        delay_ms(1000); /* 延时1秒，确保 MQTT 底层握手彻底完成，防止订阅指令丢失 */
        
        /* 订阅云端下发数据的专属 Topic (OneNET 的 $sys 主题不支持 /# 通配符，必须精确订阅) */
        char sub_topic[256]={0};
        sprintf(sub_topic, "$sys/%s/%s/thing/property/set", ONENET_PRODUCT_ID, ONENET_DEVICE_NAME);
        res =dx_ct511_mqtt_subscribe(sub_topic);
        
    while(1)
    {
			process_mqtt_message();  /* 及时检查并处理串口数据，无需死等，瞬间完成 */


/* ================= 角度计算辅助函数 ================= */

			
			delay_ms(50);
    }	
}









/* ================= 角度计算辅助函数 ================= */


static float get_cw_dist(float target, float current)
{
    float dist = target - current;
    if (dist < 0.0f) dist += 360.0f;
    return dist;
}


static float get_ccw_dist(float target, float current)
{
    float dist = current - target;
    if (dist < 0.0f) dist += 360.0f;
    return dist;
}

/*
 * 
 * angle_mode: 1 - 顺时针旋转 (CW)
 *             2 - 逆时针旋转 (CCW)
 *             3 - 自动选择最近路径 (Auto)
 * angle: 目标绝对角度 (0.0 ~ 360.0)
 */
void motor_angle(int angle_mode, float angle)
{
    float start_angle, current_angle;
    float total_to_travel, traveled;
    

    const float lead_angle = 1.0f; 
    

    const float tolerance = 1.0f;
    

    mt6701_read_angle(&angle_raw, &start_angle);
    current_angle = start_angle;
    
 
    if (angle_mode == 3)
    {
        float cw_dist = get_cw_dist(angle, start_angle);
        float ccw_dist = get_ccw_dist(angle, start_angle);
        angle_mode = (cw_dist < ccw_dist) ? 1 : 2;
    }
    

    if (angle_mode == 1)
    {
        
        float total_dist = get_cw_dist(angle, start_angle);
        if (total_dist <= tolerance) return; 
        
        total_to_travel = total_dist - lead_angle;
        if (total_to_travel < 0.0f) total_to_travel = 0.0f;
        
        motor_clockwise(); 
        
        while (1)
        {
            mt6701_read_angle(&angle_raw, &current_angle);
           
            traveled = get_cw_dist(current_angle, start_angle);
            
            
            if (traveled >= total_to_travel)
            {
                motor_stop();
                break;
            }
            delay_ms(2); 
        }
    }

    else if (angle_mode == 2)
    {
        float total_dist = get_ccw_dist(angle, start_angle);
        if (total_dist <= tolerance) return; 
        

        total_to_travel = total_dist - lead_angle;
        if (total_to_travel < 0.0f) total_to_travel = 0.0f;
        
        motor_counterclockwise(); 
        
        while (1)
        {
            mt6701_read_angle(&angle_raw, &current_angle);

            traveled = get_ccw_dist(current_angle, start_angle);
            

            if (traveled >= total_to_travel)
            {
                motor_stop();
                break;
            }
            delay_ms(2);
        }
    }
}


