#ifndef  __BOOTLOADER_IF_H__
#define  __BOOTLOADER_IF_H__



#define  BOOTLOADER_FLASH_BASE_ADDR                      (0x08000000)
#define  BOOTLOADER_FLASH_BOOTLOADER_ADDR_OFFSET         (0x00000000)
#define  BOOTLOADER_FLASH_PAGE_SIZE                      (0x800)

#define  BOOTLOADER_FLASH_ENV_BANK1_ADDR_OFFSET          (0x6000)
#define  BOOTLOADER_FLASH_ENV_BANK1_SIZE                 (0x800) /*环境参数区1 2k*/

#define  BOOTLOADER_FLASH_ENV_BANK2_ADDR_OFFSET          (0x6800)
#define  BOOTLOADER_FLASH_ENV_BANK2_SIZE                 (0x800) /*环境参数区2 2k*/

#define  BOOTLOADER_FLASH_USER_APPLICATION_ADDR_OFFSET   (0x7000)
#define  BOOTLOADER_FLASH_USER_APPLICATION_SIZE          (0x19000)/*用户的应用程序区 100k*/

#define  BOOTLOADER_FLASH_UPDATE_APPLICATION_ADDR_OFFSET (0x20000)/*更新的应用程序区 100k*/
#define  BOOTLOADER_FLASH_UPDATE_APPLICATION_SIZE        (0x19000)

#define  BOOTLOADER_FLASH_SWAP_BLOCK_ADDR_OFFSET         (0x39000)/*数据交换区 20k*/
#define  BOOTLOADER_FLASH_SWAP_BLOCK_SIZE                (0x5000)


#define  BOOTLOADER_RESET_LATER_TIME                     1        /*复位延时 单位：秒*/

typedef enum
{
BOOTLOADER_ENV_STATUS_VALID  = 0x11223344U,
BOOTLOADER_ENV_STATUS_INVALID
}bootloader_env_status_t;

typedef enum
{
SWAP_STEP_INIT = 0x00000000U,
SWAP_STEP_COPY_USER_TO_SWAP = 0x22334455U,
SWAP_STEP_COPY_UPDATE_TO_USER,
SWAP_STEP_COPY_SWAP_TO_UPDATE
}swap_step_t;


/*更新完成 需要新的应用程序改回BOOTLOADER_FLAG_BOOT_UPDATE_OK代表升级成功*/
/*如果第二次bootloader启动时是BOOTLOADER_FLAG_BOOT_UPDATE_OK 代表升级成功 设置为 BOOTLOADER_FLAG_BOOT_NORMAL*/
/*如果第二次bootloader启动时仍然是BOOTLOADER_FLAG_BOOT_UPDATE_COMPLETE 代表升级失败 进行回滚操作 回滚完成设置BOOTLOADER_FLAG_BOOT_NORMAL*/
typedef enum
{
BOOTLOADER_FLAG_BOOT_NORMAL = 0x12340000U, /*正常启动*/
BOOTLOADER_FLAG_BOOT_UPDATE,              /*需要更新*/
BOOTLOADER_FLAG_BOOT_UPDATE_COMPLETE,     /*更新完成,待验证是否成功*/
BOOTLOADER_FLAG_BOOT_UPDATE_OK,           /*更新成功*/
}bootloader_flag_t;

typedef struct
{
char value[36];
}bootloader_fw_md5_t;

typedef struct
{
uint32_t  code;
}bootloader_fw_version_t;



typedef struct
{
uint32_t    update_offset;/*更新区的偏移*/
uint32_t    origin_offset;/*用户区的偏移*/
uint32_t    size;         /*交换区的数据大小*/
swap_step_t step;         /*当前已完成的步骤*/
}bootloader_swap_ctrl_t;

typedef struct
{
bootloader_fw_version_t version;
bootloader_fw_md5_t     md5;
uint32_t                size;
}bootloader_fw_t;

typedef struct
{
bootloader_flag_t       boot_flag;    /*启动标志*/
uint32_t                reserved[32]; /*保留32个参数，供应用程序使用*/
bootloader_fw_t         fw_update;    /*更新的固件*/
bootloader_fw_t         fw_origin;    /*原有的固件*/
bootloader_swap_ctrl_t  swap_ctrl;    /*数据交换控制*/
bootloader_env_status_t status;       /*环境参数状态*/
}bootloader_env_t;


/******************************************************************************/
/*             bootloader 接口                                                */
/******************************************************************************/
/*名称：bootloader_init
* 功能：初始化
* 返回：0：成功 其他：失败
*/
int bootloader_init();

/*名称：bootloader_boot_user_application
* 功能：启动用户区APP
* 参数：无
* 返回：无
*/
void bootloader_boot_user_application();

/*名称：bootloader_boot_bootloader
* 功能：应用程序启动bootloader
* 参数：无
* 返回：无
*/
void bootloader_boot_bootloader();

/*名称：bootloader_reset
* 功能：应用程序复位
* 参数：无
* 返回：无
*/
void bootloader_reset();

/*名称：bootloader_enable_wr_protection
* 功能：去除flash写保护
* 参数：无
* 返回：0：成功 其他：失败
*/
int bootloader_enable_wr_protection();

/*名称：bootloader_disable_wr_protection
* 功能：去除flash写保护
* 参数：无
* 返回：0：成功 其他：失败
*/
int bootloader_disable_wr_protection();

/*名称：bootloader_get_env
* 功能：获取ENV参数
* 参数：env 环境参数指针
* 返回：0：成功 1：成功 但是ENV参数是无效的 其他：失败
*/
int bootloader_get_env(bootloader_env_t *env);

/*名称：bootloader_save_env
* 功能：保存ENV参数
* 参数：env 环境参数指针
* 返回：0：成功 其他：失败
*/
int bootloader_save_env(bootloader_env_t *env);

/*名称：bootloader_write_fw
* 功能：flash固件写入参数
* 参数：fw_dest_addr 固件目标地址
* 参数：fw_src_addr  固件源地址
* 参数：size         固件大小
* 返回：0：成功 其他：失败
*/
int bootloader_write_fw(uint32_t fw_dest_addr,uint32_t fw_src_addr,uint32_t size);


/*名称：bootloader_update_user_app
* 功能：更新用户APP
* 参数：env  环境参数指针
* 返回：0：成功 其他：失败
*/
int bootloader_update_user_app(bootloader_env_t *env);

/*名称：bootloader_recovery_user_app
* 功能：恢复用户APP
* 参数：env  环境参数指针
* 返回：无
*/
int bootloader_recovery_user_app(bootloader_env_t *env);


#endif