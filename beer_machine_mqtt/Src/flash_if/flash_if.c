/**
******************************************************************************************                                                                                                                                                       
*                                                                            
*  This program is free software; you can redistribute it and/or modify      
*  it under the terms of the GNU General Public License version 3 as         
*  published by the Free Software Foundation.                                
*                                                                            
*  @file       flash_if.c
*  @brief      st flash接口
*  @author     wkxboot
*  @version    v1.0.0
*  @date       2019/7/17
*  @copyright  <h4><left>&copy; copyright(c) 2019 wkxboot 1131204425@qq.com</center></h4>  
*                                                                            
*                                                                            
*****************************************************************************************/
#include "flash_if.h"
#include "log.h"

/**
* @brief flash_if_init 数据区域初始化
* @param 无
* @return 0：成功 -1：失败
* @note
*/
int flash_if_init(void)
{
    /* Unlock the Program memory */
    HAL_FLASH_Unlock();

    /* Clear all FLASH flags */
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPERR);
    /* Unlock the Program memory */
    HAL_FLASH_Lock();

    return 0;
}

/**
* @brief 擦除指定地址数据
* @param start_addr 擦除开始地址
* @param size 擦除数据量
* @return 0：成功 -1：失败
* @note
*/
uint32_t flash_if_erase(uint32_t start_addr,uint32_t size)
{
    uint32_t NbrOfPages = 0;
    uint32_t PageError = 0;
    FLASH_EraseInitTypeDef pEraseInit;
    HAL_StatusTypeDef status = HAL_OK;

    /* Unlock the Flash to enable the flash control register access *************/ 
    HAL_FLASH_Unlock();

    /* Get the sector where start the user flash area */
    NbrOfPages = size % FLASH_PAGE_SIZE == 0 ? size / FLASH_PAGE_SIZE : (size / FLASH_PAGE_SIZE) + 1;

    pEraseInit.TypeErase = FLASH_TYPEERASE_PAGES;
    pEraseInit.PageAddress = start_addr;
    pEraseInit.Banks = FLASH_BANK_1;
    pEraseInit.NbPages = NbrOfPages;
    status = HAL_FLASHEx_Erase(&pEraseInit, &PageError);

    /* Lock the Flash to disable the flash control register access (recommended
     to protect the FLASH memory against possible unwanted operation) *********/
    HAL_FLASH_Lock();

    if (status != HAL_OK) {
        /* Error occurred while page erase */
        return -1;
    }

    return 0;
}


/**
* @brief flash_if_read 数据读取
* @param addr 地址
* @param dst 数据目的地址
* @param size 数据量
* @return 0 成功 -1 失败
* @note
*/
int flash_if_read(const uint32_t src_addr,uint8_t *dst_addr,const uint32_t size)
{
    uint8_t *src_temp;
    uint8_t *dst_temp;
    uint32_t i;

    if (src_addr + size > USER_FLASH_END_ADDRESS) {
        log_error("flash addr:%d is large than end addr:%d.\r\n",src_addr + size * 4,USER_FLASH_END_ADDRESS);
        return -1;
    }

    src_temp = (uint8_t *)src_addr;
    dst_temp = dst_addr;

    for (i = 0; i < size; i ++) {
        dst_temp[i] = *(src_temp + i);
    }

    return 0;
}


/**
* @brief flash_if_write 数据编程写入
* @param destination 目的地址
* @param source 数据源
* @param size 写入的数据量
* @return 0 成功 -1 失败
* @note
*/
uint32_t flash_if_write(uint32_t destination, uint8_t *source, uint32_t size)
{
    uint32_t i = 0;
    uint32_t *source32 = (uint32_t *)source;
    uint32_t source_size = size / 4;
    /* Unlock the Flash to enable the flash control register access *************/
    HAL_FLASH_Unlock();

    for (i = 0; (i < source_size) && (destination <= (USER_FLASH_END_ADDRESS - 4)); i++) {
        /* Device voltage range supposed to be [2.7V to 3.6V], the operation will
       be done by word */ 
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, destination, *(uint32_t*)(source32 + i)) == HAL_OK) {
            /* Check the written value */
            if (*(uint32_t*)destination != *(uint32_t*)(source32 + i)) {
                /* Flash content doesn't match SRAM content */
                return -1;
            }
            /* Increment FLASH destination address */
            destination += 4;
        } else {
            /* Error occurred while writing data in Flash memory */
            return -1;
        }
    }

    /* Lock the Flash to disable the flash control register access (recommended
     to protect the FLASH memory against possible unwanted operation) *********/
    HAL_FLASH_Lock();

    return 0;
}


/**
* @brief 获取flash状态
* @param 无
* @return @see flash_if_wr_protection_t
* @note
*/
flash_if_wr_protection_t flash_if_get_write_protection_status()
{
    uint32_t ProtectedPAGE;
    FLASH_OBProgramInitTypeDef OptionsBytesStruct;

    /* Unlock the Flash to enable the flash control register access *************/
    HAL_FLASH_Unlock();

    /* Check if there are write protected sectors inside the user flash area ****/
    HAL_FLASHEx_OBGetConfig(&OptionsBytesStruct);

    /* Lock the Flash to disable the flash control register access (recommended
     to protect the FLASH memory against possible unwanted operation) *********/
    HAL_FLASH_Lock();

    /* Get pages already write protected ****************************************/
    ProtectedPAGE = ~(OptionsBytesStruct.WRPPage) & OB_WRP_ALLPAGES;

    /* Check if desired pages are already write protected ***********************/
    if(ProtectedPAGE != 0) {
        /* Some sectors inside the user flash area are write protected */
        return FLASH_IF_WR_PROTECTION_ENABLED;
    } 

    /* No write protected sectors inside the user flash area */
    return FLASH_IF_WR_PROTECTION_NONE;
}

/**
* @brief 配置flash状态
* @param 无
* @return @see flash_if_wr_protection_t
* @note
*/
int flash_if_write_protection_config(flash_if_wr_protection_t protection)
{
    uint32_t ProtectedPAGE = 0x0;
    FLASH_OBProgramInitTypeDef config_new, config_old;
    HAL_StatusTypeDef result = HAL_OK;
  

    /* Get pages write protection status ****************************************/
    HAL_FLASHEx_OBGetConfig(&config_old);

    /* The parameter says whether we turn the protection on or off */
    config_new.WRPState = (protection == FLASH_IF_WR_PROTECTION_ENABLED ? OB_WRPSTATE_ENABLE : OB_WRPSTATE_DISABLE);

    /* We want to modify only the Write protection */
    config_new.OptionType = OPTIONBYTE_WRP;
  
    /* No read protection, keep BOR and reset settings */
    config_new.RDPLevel = OB_RDP_LEVEL_0;
    config_new.USERConfig = config_old.USERConfig;  
    /* Get pages already write protected ****************************************/
    ProtectedPAGE = config_old.WRPPage | OB_WRP_ALLPAGES;

    /* Unlock the Flash to enable the flash control register access *************/ 
    HAL_FLASH_Unlock();

    /* Unlock the Options Bytes *************************************************/
    HAL_FLASH_OB_Unlock();
  
    /* Erase all the option Bytes ***********************************************/
    result = HAL_FLASHEx_OBErase();
    
    if (result == HAL_OK) {
        config_new.WRPPage = ProtectedPAGE;
        result = HAL_FLASHEx_OBProgram(&config_new);
    }
  
    return (result == HAL_OK ? 0: -1);
}


