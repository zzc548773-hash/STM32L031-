/**
 ****************************************************************************************************
 * @file        eeprom.h
 * @author      正点原子(ALIENTEK)参考风格
 * @version     V1.0
 * @date        2026-06-30
 * @brief       STM32L031 内部 DATA EEPROM 驱动代码
 ****************************************************************************************************
 */

#ifndef _EEPROM_H
#define _EEPROM_H
#include "./SYSTEM/sys/sys.h"

/* STM32L031G6U6 内部 DATA EEPROM 基地址和大小 */
#define EEPROM_BASE_ADDR        0x08080000
#define EEPROM_MAX_SIZE         1024        /* 1KB = 1024 Bytes */

/* 函数声明 */
uint8_t eeprom_read_byte(uint16_t offset);
void eeprom_write_byte(uint16_t offset, uint8_t data);

uint16_t eeprom_read_halfword(uint16_t offset);
void eeprom_write_halfword(uint16_t offset, uint16_t data);

uint32_t eeprom_read_word(uint16_t offset);
void eeprom_write_word(uint16_t offset, uint32_t data);

void eeprom_read_string(uint16_t offset, char *str, uint16_t max_len);
void eeprom_write_string(uint16_t offset, const char *str);

#endif
