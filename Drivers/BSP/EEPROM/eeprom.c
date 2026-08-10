/**
 ****************************************************************************************************
 * @file        eeprom.c
 * @author      正点原子(ALIENTEK)参考风格
 * @version     V1.0
 * @date        2026-06-30
 * @brief       STM32L031 内部 DATA EEPROM 驱动代码
 ****************************************************************************************************
 */

#include "./BSP/EEPROM/eeprom.h"

/**
 * @brief       从内部EEPROM读取一个字节(8位)
 * @param       offset: 偏移地址 (0 ~ 1023)
 * @retval      读取到的字节数据
 */
uint8_t eeprom_read_byte(uint16_t offset)
{
    if (offset >= EEPROM_MAX_SIZE) 
    {
        return 0;
    }
    return *(__IO uint8_t*)(EEPROM_BASE_ADDR + offset);
}

/**
 * @brief       向内部EEPROM写入一个字节(8位)
 * @param       offset: 偏移地址 (0 ~ 1023)
 * @param       data: 要写入的数据
 * @retval      无
 */
void eeprom_write_byte(uint16_t offset, uint8_t data)
{
    if (offset >= EEPROM_MAX_SIZE) 
    {
        return;
    }
    
    HAL_FLASHEx_DATAEEPROM_Unlock();
    
    /* 清除可能残留的错误标志，防止后续操作被阻塞 */
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_WRPERR | FLASH_FLAG_PGAERR | 
                           FLASH_FLAG_SIZERR | FLASH_FLAG_OPTVERR | FLASH_FLAG_RDERR | 
                           FLASH_FLAG_FWWERR | FLASH_FLAG_NOTZEROERR);

    /* 显式擦除该地址，确保旧数据被彻底清除，避免无法覆盖的问题 */
    HAL_FLASHEx_DATAEEPROM_Erase(EEPROM_BASE_ADDR + offset);

    HAL_FLASHEx_DATAEEPROM_Program(FLASH_TYPEPROGRAMDATA_BYTE, EEPROM_BASE_ADDR + offset, data);
    HAL_FLASHEx_DATAEEPROM_Lock();
}

/**
 * @brief       从内部EEPROM读取一个半字(16位)
 * @param       offset: 偏移地址 (0 ~ 1022)，必须为2的倍数
 * @retval      读取到的半字数据
 */
uint16_t eeprom_read_halfword(uint16_t offset)
{
    if (offset >= (EEPROM_MAX_SIZE - 1)) 
    {
        return 0;
    }
    return *(__IO uint16_t*)(EEPROM_BASE_ADDR + offset);
}

/**
 * @brief       向内部EEPROM写入一个半字(16位)
 * @param       offset: 偏移地址 (0 ~ 1022)，必须为2的倍数
 * @param       data: 要写入的数据
 * @retval      无
 */
void eeprom_write_halfword(uint16_t offset, uint16_t data)
{
    if (offset >= (EEPROM_MAX_SIZE - 1)) 
    {
        return;
    }
    
    HAL_FLASHEx_DATAEEPROM_Unlock();
    
    /* 清除可能残留的错误标志，防止后续操作被阻塞 */
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_WRPERR | FLASH_FLAG_PGAERR | 
                           FLASH_FLAG_SIZERR | FLASH_FLAG_OPTVERR | FLASH_FLAG_RDERR | 
                           FLASH_FLAG_FWWERR | FLASH_FLAG_NOTZEROERR);

    /* 显式擦除该地址，确保旧数据被彻底清除，避免无法覆盖的问题 */
    HAL_FLASHEx_DATAEEPROM_Erase(EEPROM_BASE_ADDR + offset);

    HAL_FLASHEx_DATAEEPROM_Program(FLASH_TYPEPROGRAMDATA_HALFWORD, EEPROM_BASE_ADDR + offset, data);
    HAL_FLASHEx_DATAEEPROM_Lock();
}

/**
 * @brief       从内部EEPROM读取一个字(32位)
 * @param       offset: 偏移地址 (0 ~ 1020)，必须为4的倍数
 * @retval      读取到的字数据
 */
uint32_t eeprom_read_word(uint16_t offset)
{
    if (offset >= (EEPROM_MAX_SIZE - 3)) 
    {
        return 0;
    }
    return *(__IO uint32_t*)(EEPROM_BASE_ADDR + offset);
}

/**
 * @brief       向内部EEPROM写入一个字(32位)
 * @param       offset: 偏移地址 (0 ~ 1020)，必须为4的倍数
 * @param       data: 要写入的数据
 * @retval      无
 */
void eeprom_write_word(uint16_t offset, uint32_t data)
{
    if (offset >= (EEPROM_MAX_SIZE - 3)) 
    {
        return;
    }
    
    HAL_FLASHEx_DATAEEPROM_Unlock();
    
    /* 清除可能残留的错误标志，防止后续操作被阻塞 */
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_WRPERR | FLASH_FLAG_PGAERR | 
                           FLASH_FLAG_SIZERR | FLASH_FLAG_OPTVERR | FLASH_FLAG_RDERR | 
                           FLASH_FLAG_FWWERR | FLASH_FLAG_NOTZEROERR);

    /* 显式擦除该地址，确保旧数据被彻底清除，避免无法覆盖的问题 */
    HAL_FLASHEx_DATAEEPROM_Erase(EEPROM_BASE_ADDR + offset);

    HAL_FLASHEx_DATAEEPROM_Program(FLASH_TYPEPROGRAMDATA_WORD, EEPROM_BASE_ADDR + offset, data);
    HAL_FLASHEx_DATAEEPROM_Lock();
}

/**
 * @brief       从内部EEPROM读取字符串
 * @param       offset: 偏移地址 (0 ~ 1023)，建议为4的倍数
 * @param       str: 存放读取字符串的缓冲
 * @param       max_len: 最大读取长度(包括结束符'\0')
 * @retval      无
 */
void eeprom_read_string(uint16_t offset, char *str, uint16_t max_len)
{
    uint32_t word_val = 0;
    for (uint16_t i = 0; i < max_len; i++)
    {
        if (i % 4 == 0)
        {
            word_val = eeprom_read_word(offset + i);
        }
        str[i] = (char)((word_val >> ((i % 4) * 8)) & 0xFF);
        if (str[i] == '\0') 
        {
            break;
        }
    }
    str[max_len - 1] = '\0'; /* 确保字符串安全结束 */
}

/**
 * @brief       向内部EEPROM写入字符串
 * @param       offset: 偏移地址 (0 ~ 1023)，建议为4的倍数
 * @param       str: 要写入的字符串 (最大支持到遇到'\0')
 * @retval      无
 */
void eeprom_write_string(uint16_t offset, const char *str)
{
    uint32_t word_val = 0;
    uint16_t i = 0;
    
    while(str[i] != '\0')
    {
        word_val |= ((uint32_t)str[i]) << ((i % 4) * 8);
        i++;
        if (i % 4 == 0)
        {
            eeprom_write_word(offset + i - 4, word_val);
            word_val = 0;
        }
    }
    /* 将剩余的字节(包括强制的'\0'结束符)写入 */
    eeprom_write_word(offset + i - (i % 4), word_val);
}

