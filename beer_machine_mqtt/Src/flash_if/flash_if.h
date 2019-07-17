#ifndef  __FLASH_IF_H__
#define  __FLASH_IF_H__
#include "stm32f1xx_hal.h"


#define  USER_FLASH_END_ADDRESS          0x8003FFFF


typedef enum
{
    FLASH_IF_WR_PROTECTION_NONE = 0,
    FLASH_IF_WR_PROTECTION_ENABLED 
}flash_if_wr_protection_t;

/**
* @brief flash_if_init 数据区域初始化
* @param 无
* @return 0：成功 -1：失败
* @note
*/
int flash_if_init(void);

/**
* @brief flash_if_read 数据读取
* @param addr 地址
* @param dst 数据目的地址
* @param size 数据量
* @return 0 成功 -1 失败
* @note
*/
int flash_if_read(const uint32_t src_addr,uint8_t *dst_addr,const uint32_t size);


/**
* @brief flash_if_write 数据编程写入
* @param destination 目的地址
* @param source 数据源
* @param size 写入的数据量
* @return 0 成功 -1 失败
* @note
*/
uint32_t flash_if_write(uint32_t destination, uint8_t *source, uint32_t size);


/**
* @brief 擦除指定地址数据
* @param start_addr 擦除开始地址
* @param size 擦除数据量
* @return 0：成功 -1：失败
* @note
*/
uint32_t flash_if_erase(uint32_t start_addr,uint32_t size);




#endif