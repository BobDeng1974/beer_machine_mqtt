#include "main.h"
#include "stdbool.h"
#include "flash_utils.h"
#include "log.h"
#define  LOG_MODULE_LEVEL    LOG_LEVEL_DEBUG
#define  LOG_MODULE_NAME     "[flash_utils]"

/*名称：flash_utils_init
* 功能：flash工具初始化
* 参数：无
* 返回：无
*/
void flash_utils_init(void)
{
  /* Unlock the Program memory */
  HAL_FLASH_Unlock();

  /* Clear all FLASH flags */
  __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPERR);
  /* Unlock the Program memory */
  HAL_FLASH_Lock();
}

/*名称：flash_utils_erase
* 功能：擦出指定范围flash数据
* 参数：start_addr 开始地址
* 参数：size       数据大小
* 返回：0：成功 其他：失败
*/
uint32_t flash_utils_erase(uint32_t start_addr,uint32_t size)
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

  if (status != HAL_OK)
  {
    /* Error occurred while page erase */
    return -1;
  }

  return 0;
}

/*名称：flash_utils_write
* 功能：在指定位置写入flash数据
* 参数：destination 目的地址
* 参数：source      源地址
* 参数：size        源大小
* 返回：0：成功 其他：失败
*/
uint32_t flash_utils_write(uint32_t destination, uint32_t *source, uint32_t size)
{
  uint32_t i = 0;

  /* Unlock the Flash to enable the flash control register access *************/
  HAL_FLASH_Unlock();

  for (i = 0; (i < size) && (destination <= (USER_FLASH_END_ADDRESS - 4)); i++)
  {
    /* Device voltage range supposed to be [2.7V to 3.6V], the operation will
       be done by word */ 
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, destination, *(uint32_t*)(source+i)) == HAL_OK)      
    {
     /* Check the written value */
      if (*(uint32_t*)destination != *(uint32_t*)(source+i))
      {
        /* Flash content doesn't match SRAM content */
        return -1;
      }
      /* Increment FLASH destination address */
      destination += 4;
    }
    else
    {
      /* Error occurred while writing data in Flash memory */
      return -1;
    }
  }

  /* Lock the Flash to disable the flash control register access (recommended
     to protect the FLASH memory against possible unwanted operation) *********/
  HAL_FLASH_Lock();

  return 0;
}


/*名称：flash_utils_get_write_protection_status
* 功能：获取整个flash是否被写保护
* 参数：无
* 返回：写保护状态
*/
flash_utils_wr_protection_t flash_utils_get_write_protection_status()
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
  if(ProtectedPAGE != 0)
  {
    /* Some sectors inside the user flash area are write protected */
    return FLASH_UTILS_WR_PROTECTION_ENABLED;
  }
  else
  { 
    /* No write protected sectors inside the user flash area */
    return FLASH_UTILS_WR_PROTECTION_NONE;
  }
}

/*名称：flash_utils_write_protection_config
* 功能：配置整个flash写保护状态
* 参数：protection    保护状态
* 返回：0：成功 其他：失败
*/
int flash_utils_write_protection_config(flash_utils_wr_protection_t protection)
{
  uint32_t ProtectedPAGE = 0x0;
  FLASH_OBProgramInitTypeDef config_new, config_old;
  HAL_StatusTypeDef result = HAL_OK;
  

  /* Get pages write protection status ****************************************/
  HAL_FLASHEx_OBGetConfig(&config_old);

  /* The parameter says whether we turn the protection on or off */
  config_new.WRPState = (protection == FLASH_UTILS_WR_PROTECTION_ENABLED ? OB_WRPSTATE_ENABLE : OB_WRPSTATE_DISABLE);

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
    
  if (result == HAL_OK)
  {
    config_new.WRPPage    = ProtectedPAGE;
    result = HAL_FLASHEx_OBProgram(&config_new);
  }
  
  return (result == HAL_OK ? 0: -1);
}

/*名称：flash_utils_read
* 功能：读取指定位置flash数据
* 参数：dest_buffer 目的缓存地址
* 参数：src_addr    flash源地址
* 参数：size        数据大小
* 返回：0：成功 其他：失败
*/
int flash_utils_read(uint32_t *dst_addr,const uint32_t src_addr,const uint32_t size)
{
uint32_t *addr;
uint32_t i;

if(src_addr + size * 4 > USER_FLASH_END_ADDRESS){
   log_error("flash addr:%d is large than end addr:%d.\r\n",src_addr + size * 4,USER_FLASH_END_ADDRESS);
   return -1;
}

addr = (uint32_t *)src_addr;

for(i = 0; i < size; i ++){
   dst_addr[i] = *(addr + i);
}

return 0;
}
