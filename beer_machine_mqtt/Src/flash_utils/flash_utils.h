#ifndef  __FLASH_UTILS_H__
#define  __FLASH_UTILS_H__
#include "stm32f1xx_hal.h"


#define  USER_FLASH_END_ADDRESS          0x8003FFFF


typedef enum
{
FLASH_UTILS_WR_PROTECTION_NONE = 0,
FLASH_UTILS_WR_PROTECTION_ENABLED 
}flash_utils_wr_protection_t;





/*名称：flash_utils_init
* 功能：flash工具初始化
* 参数：无
* 返回：无
*/
void flash_utils_init(void);
/*名称：flash_utils_erase
* 功能：擦出指定范围flash数据
* 参数：start_addr 开始地址
* 参数：size       数据大小
* 返回：0：成功 其他：失败
*/
uint32_t flash_utils_erase(uint32_t start_addr,uint32_t size);
/*名称：flash_utils_write
* 功能：在指定位置写入flash数据
* 参数：destination 目的地址
* 参数：source      源地址
* 参数：size        源大小
* 返回：0：成功 其他：失败
*/
uint32_t flash_utils_write(uint32_t destination, uint32_t *source, uint32_t size);
/*名称：flash_utils_get_write_protection_status
* 功能：获取整个flash是否被写保护
* 参数：无
* 返回：写保护状态
*/
flash_utils_wr_protection_t flash_utils_get_write_protection_status();
/*名称：flash_utils_write_protection_config
* 功能：配置整个flash写保护状态
* 参数：protection    保护状态
* 返回：0：成功 其他：失败
*/
int flash_utils_write_protection_config(flash_utils_wr_protection_t protection);

/*名称：flash_utils_read
* 功能：读取指定位置flash数据
* 参数：dest_buffer 目的缓存地址
* 参数：src_addr    flash源地址
* 参数：size        数据大小
* 返回：0：成功 其他：失败
*/
int flash_utils_read(uint32_t *dst,const uint32_t addr,const uint32_t size);









#endif