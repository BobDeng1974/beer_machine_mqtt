#include "main.h"
#include "stdbool.h"
#include "flash_utils.h"
#include "bootloader_if.h"
#include "log.h"
#define  LOG_MODULE_LEVEL    LOG_LEVEL_DEBUG
#define  LOG_MODULE_NAME     "[bootloader_if]"

#define  BANK_FULL_ENV_OFFSET         0x11111111

typedef void (*application_func_t)(void);

static bootloader_env_t default_env = {
.boot_flag = BOOTLOADER_FLAG_BOOT_NORMAL,
.status = BOOTLOADER_ENV_STATUS_VALID,
.swap_ctrl.step = SWAP_STEP_INIT,
.fw_origin.size = BOOTLOADER_FLASH_USER_APPLICATION_SIZE,
};

static application_func_t application_func;

/*名称：bootloader_boot_user_application
* 功能：启动用户区APP
* 参数：无
* 返回：无
*/
void bootloader_boot_user_application()
{
  uint32_t user_application_msp;
  uint32_t user_app_addr;
  
  /*初始化栈指针*/
  user_application_msp = *(uint32_t*)(BOOTLOADER_FLASH_BASE_ADDR + BOOTLOADER_FLASH_USER_APPLICATION_ADDR_OFFSET);
  
  /*获取用户APP地址和栈指针*/
  user_app_addr = *(uint32_t*)(BOOTLOADER_FLASH_BASE_ADDR + BOOTLOADER_FLASH_USER_APPLICATION_ADDR_OFFSET + 4);
  application_func = (application_func_t)user_app_addr;
  log_warning("boot user app --> addr:0x%X stack:0x%X....\r\n",application_func,user_application_msp);
  /*等待日志输出完毕*/
  HAL_Delay(500);
  /*跳转*/
  __disable_irq();
  __set_MSP(user_application_msp);
  application_func();  
  
  while(1);
}

/*名称：bootloader_reset
* 功能：应用程序复位
* 参数：无
* 返回：无
*/
void bootloader_reset()
{
  uint32_t reset_later = BOOTLOADER_RESET_LATER_TIME;
  while(reset_later > 0){
    log_warning("system will reset %dS later.\r\n",reset_later); 
    reset_later --;
    HAL_Delay(1000);
  }  
  NVIC_SystemReset();
}

/*名称：bootloader_disable_wr_protection
* 功能：去除flash写保护
* 参数：无
* 返回：0：成功 其他：失败
*/
int bootloader_disable_wr_protection()
{
  /*解除写保护*/
  log_debug("disable wr protecion.\r\n");
  if(flash_utils_write_protection_config(FLASH_UTILS_WR_PROTECTION_NONE) != 0){  
     log_error("err.\r\n");  
     return -1;
  }
  
 return 0;
}

/*名称：bootloader_enable_wr_protection
* 功能：去除flash写保护
* 参数：无
* 返回：0：成功 其他：失败
*/
int bootloader_enable_wr_protection()
{
  /*解除写保护*/
  log_debug("enable wr protecion.\r\n");
  if(flash_utils_write_protection_config(FLASH_UTILS_WR_PROTECTION_ENABLED) != 0){  
     log_error("err.\r\n");  
     return -1;
  }
  
 return 0;
}
/*名称：bootloader_erase_bank1
* 功能：擦除bank1空间
* 参数：无
* 返回：0：成功 其他：失败
*/
static int bootloader_erase_bank1()
{
  int rc;
  /*擦除bank1参数区域*/
  log_warning("erase bank1...\r\n");
  rc = flash_utils_erase(BOOTLOADER_FLASH_BASE_ADDR + BOOTLOADER_FLASH_ENV_BANK1_ADDR_OFFSET,BOOTLOADER_FLASH_ENV_BANK1_SIZE);
  if(rc != 0){
     log_error("erase bank1 addr:0x%X err.\r\n",BOOTLOADER_FLASH_BASE_ADDR + BOOTLOADER_FLASH_ENV_BANK1_ADDR_OFFSET);  
     return -1;
   }    
  log_warning("done.\r\n"); 
   
 return 0; 
}

/*名称：bootloader_erase_bank2
* 功能：擦除bank2空间
* 参数：无
* 返回：0：成功 其他：失败
*/
static int bootloader_erase_bank2()
{
  int rc;
  /*擦除bank2参数区域*/
  log_warning("erase bank2...\r\n");
  rc = flash_utils_erase(BOOTLOADER_FLASH_BASE_ADDR + BOOTLOADER_FLASH_ENV_BANK2_ADDR_OFFSET,BOOTLOADER_FLASH_ENV_BANK2_SIZE);
  if(rc != 0){
   log_error("erase bank2 addr:0x%X err.\r\n",BOOTLOADER_FLASH_BASE_ADDR + BOOTLOADER_FLASH_ENV_BANK2_ADDR_OFFSET);  
   return -1;
   }    
  log_warning("done.\r\n"); 
   
 return 0; 
}

/*名称：bootloader_read_bank1_env
* 功能：读取bank1空间的env
* 参数：offset env在bank1的偏移量
* 参数：env env指针
* 返回：0：成功 其他：失败
*/
static int bootloader_read_bank1_env(uint32_t offset,bootloader_env_t *env)
{
  int rc;
  log_debug("read bank1 env addr:0x%X...\r\n",BOOTLOADER_FLASH_BASE_ADDR + BOOTLOADER_FLASH_ENV_BANK1_ADDR_OFFSET + offset * sizeof(bootloader_env_t));
  rc = flash_utils_read((uint32_t *)env,BOOTLOADER_FLASH_BASE_ADDR + BOOTLOADER_FLASH_ENV_BANK1_ADDR_OFFSET + offset * sizeof(bootloader_env_t),sizeof(bootloader_env_t) / 4);
  if(rc != 0 ){
    log_error("read bank2 env addr:0x%X err.\r\n",BOOTLOADER_FLASH_BASE_ADDR + BOOTLOADER_FLASH_ENV_BANK1_ADDR_OFFSET + offset * sizeof(bootloader_env_t));  
    return -1;
  }
  log_debug("done.\r\n");   
  
  return 0;
}

/*名称：bootloader_write_bank1_env
* 功能：向bank1空间写入env
* 参数：offset env在bank1的偏移量
* 参数：env env指针
* 返回：0：成功 其他：失败
*/
static int bootloader_write_bank1_env(uint32_t offset,bootloader_env_t *env)
{
  int rc;
  log_debug("write bank1 env addr:0x%X...\r\n",BOOTLOADER_FLASH_BASE_ADDR + BOOTLOADER_FLASH_ENV_BANK1_ADDR_OFFSET + offset * sizeof(bootloader_env_t));
  rc = flash_utils_write(BOOTLOADER_FLASH_BASE_ADDR + BOOTLOADER_FLASH_ENV_BANK1_ADDR_OFFSET + offset * sizeof(bootloader_env_t),(uint32_t *)env,sizeof(bootloader_env_t) / 4);
  if( rc != 0 ){
    log_error("write bank2 env addr:0x%X err.\r\n",BOOTLOADER_FLASH_BASE_ADDR + BOOTLOADER_FLASH_ENV_BANK1_ADDR_OFFSET + offset * sizeof(bootloader_env_t));  
    return -1;
  }
  log_debug("done.\r\n");   
  
  return 0;
}

/*名称：bootloader_get_bank1_env_cnt
* 功能：获取bank1空间可存储的env数量
* 参数：无 
* 返回：0：成功 其他：失败
*/
static int bootloader_get_bank1_env_cnt()
{
  return  BOOTLOADER_FLASH_ENV_BANK1_SIZE / sizeof(bootloader_env_t);
}

/*名称：bootloader_read_bank2_env
* 功能：读取bank2空间的env
* 参数：offset env在bank2的偏移量
* 参数：env env指针
* 返回：0：成功 其他：失败
*/
static int bootloader_read_bank2_env(uint32_t offset,bootloader_env_t *env)
{
  int rc;
  log_debug("read bank2 env addr:0x%X...\r\n",BOOTLOADER_FLASH_BASE_ADDR + BOOTLOADER_FLASH_ENV_BANK2_ADDR_OFFSET + offset * sizeof(bootloader_env_t));
  rc = flash_utils_read((uint32_t *)env,BOOTLOADER_FLASH_BASE_ADDR + BOOTLOADER_FLASH_ENV_BANK2_ADDR_OFFSET + offset * sizeof(bootloader_env_t),sizeof(bootloader_env_t) / 4);
  if( rc != 0 ){
    log_error("read bank2 env addr:0x%X err.\r\n",BOOTLOADER_FLASH_BASE_ADDR + BOOTLOADER_FLASH_ENV_BANK2_ADDR_OFFSET + offset * sizeof(bootloader_env_t));  
    return -1;
  }
  log_debug("done.\r\n");   
  
  return 0;
}
/*名称：bootloader_write_bank2_env
* 功能：向bank2空间写入env
* 参数：offset env在bank2的偏移量
* 参数：env env指针
* 返回：0：成功 其他：失败
*/
static int bootloader_write_bank2_env(uint32_t offset,bootloader_env_t *env)
{
  int rc;
  log_debug("write bank2 env addr:0x%X...\r\n",BOOTLOADER_FLASH_BASE_ADDR + BOOTLOADER_FLASH_ENV_BANK2_ADDR_OFFSET + offset * sizeof(bootloader_env_t));
  rc = flash_utils_write(BOOTLOADER_FLASH_BASE_ADDR + BOOTLOADER_FLASH_ENV_BANK2_ADDR_OFFSET + offset * sizeof(bootloader_env_t),(uint32_t *)env,sizeof(bootloader_env_t) / 4);
  if( rc != 0 ){
    log_error("write bank2 env addr:0x%X err.\r\n",BOOTLOADER_FLASH_BASE_ADDR + BOOTLOADER_FLASH_ENV_BANK2_ADDR_OFFSET + offset * sizeof(bootloader_env_t));  
    return -1;
  }
  log_debug("done.\r\n");   
  
  return 0;
}

/*名称：bootloader_flush_env
* 功能：重置bank1的存储空间
* 参数：env 当前env指针
* 返回：0 成功 其他：失败
*/ 
static int bootloader_flush_env(bootloader_env_t *env)
{ 
   int rc;
   /*步骤1.擦除bank2*/
   log_debug("step 1.erase bank2...\r\n");
   rc = bootloader_erase_bank2();
   if(rc != 0){
      return -1;
    }
   log_debug("done.\r\n");
    /*步骤2.把当前env写入到bank2 offset0*/
   log_debug("step 2.write bank1 env to bank2...\r\n");
   rc = bootloader_write_bank2_env(0,env);
   if(rc != 0){
      log_error("write bank1 offset addr:0x%X err.\r\n",0);  
      return -1;
    }         
   /*步骤3.擦除bank1 */
   log_debug("step 3.erase bank1...\r\n");
   rc = bootloader_erase_bank1();
   if(rc != 0){
      return -1;
   }
   /*步骤4.把env回写到bank1*/
   log_debug("step 4.write env to bank1...\r\n");
   rc = bootloader_write_bank1_env(0,env);
   if(rc != 0){
      log_error("write bank1 offset addr:0x%X err.\r\n",0);  
      return -1;
    }    
   /* 步骤5.擦除bank2*/
   log_debug("step 5.erase bank2...\r\n");
   rc = bootloader_erase_bank2();
   if(rc != 0){
      return -1;
    } 
   
   return 0;
}

/*名称：bootloader_if_init
* 功能：flash接口初始化
* 返回：0：成功 其他：失败
*/
static void bootloader_if_init()
{
  flash_utils_init();
}
/*名称：bootloader_init
* 功能：bootloader初始化
* 返回：0：成功 其他：失败
*/
int bootloader_init()
{
  int rc;
  bootloader_env_t env_bank1,env_bank2;
  
  bootloader_if_init();

  log_debug("check env.\r\n");
  rc = bootloader_read_bank1_env(0,&env_bank1);
  if(rc != 0){
    return -1;
  }
  
  rc = bootloader_read_bank2_env(0,&env_bank2);
  if(rc != 0){
    return -1;
  }
  
  if(env_bank1.status != BOOTLOADER_ENV_STATUS_VALID && env_bank2.status != BOOTLOADER_ENV_STATUS_VALID){  
    
     /*把默认env写入bank1区域第一个位置*/
     log_debug("firt boot.\r\n");
      /*第一次启动，擦除bank1*/
     rc = bootloader_erase_bank1();
     if(rc != 0){
        return -1;
     }
     rc = bootloader_write_bank1_env(0,&default_env);
     if(rc != 0){
        log_error("write bank1 offset addr:0x%X err.\r\n",0);  
        return -1;
     } 
     log_debug("done.\r\n")   
  }else if(env_bank2.status == BOOTLOADER_ENV_STATUS_VALID){
     /*有数据没有回写到bank1，回写到bank1*/ 
     log_debug("need recovery bank1 env.\r\n");
    /*擦除bank1参数区域*/
    rc = bootloader_erase_bank1();  
    if(rc != 0){
      return -1;
    }    
    log_debug("done.\r\n");   
    /*把bank2的env写入bank1区域第一个位置*/
    rc = bootloader_write_bank1_env(0,&env_bank2);
    if(rc != 0){
      log_error("write bank1 offset addr:0x%X err.\r\n",0);  
      return -1;
    } 
   /*擦除bank1参数区域*/
   rc = bootloader_erase_bank1();   
   if(rc != 0){
      return -1;
    }  
  }
  return 0;
}
 
/*名称：bootloader_search_cur_env_offset
* 功能：找到当前的env的在bank1的偏移
* 参数：env_offset偏移量
* 返回：0 成功 其他：失败
*/
static int bootloader_search_cur_env_offset(uint32_t *env_offset)
{
    int rc;
    uint32_t offset = 0;
    uint32_t env_cnt;
    bootloader_env_t env;
    
    log_debug("search cur env addr.\r\n");
    log_debug("search bank1...\r\n");
    
    env_cnt = bootloader_get_bank1_env_cnt(); 
    while(offset < env_cnt){
        rc = bootloader_read_bank1_env(offset,&env);
        if(rc != 0){
           return -1;
        }
        if(env.status != BOOTLOADER_ENV_STATUS_VALID){
           break;  
        }
        offset ++;
    }
    if(offset == 0){
     log_error("bank1 is null.\r\n");
     return -1;  
    }

    if(offset >= env_cnt){
      log_debug("bank1 is full.\r\n");
      *env_offset = env_cnt - 1;
    }else{
      *env_offset = offset - 1;
    }
    log_debug("find cur env offset:%d.\r\n",*env_offset );
    
    return 0;
}


/*名称：bootloader_search_next_env_offset
* 功能：搜索下一个可写入env的偏移量
* 参数：env_offset 偏移量指针
* 返回：0 成功 其他 失败
*/
static int bootloader_search_next_env_offset(uint32_t *env_offset)
{
    int rc;
    uint32_t offset = 0;
    uint32_t env_cnt;
    bootloader_env_t env;
    
    log_debug("search next env offset.\r\n");
    log_debug("search bank1...\r\n");
    
    env_cnt = bootloader_get_bank1_env_cnt(); 
    while(offset < env_cnt){
        rc = bootloader_read_bank1_env(offset,&env);
        if(rc != 0){
           return -1;
        }
        if(env.status != BOOTLOADER_ENV_STATUS_VALID){
           break;  
        }
        offset ++;
    } 
    if(offset == 0){
       log_error("bank1 is null. err.\r\n");
       return -1;
    }
    /*bank1全部写入了env*/
    if(offset >= env_cnt){
       log_debug("bank1 is full.\r\n");     
       *env_offset = BANK_FULL_ENV_OFFSET;            
    }else{
       *env_offset = offset; 
    }
    log_debug("find next env offset:%d.\r\n",*env_offset ); 
    
    return 0;
}


/*名称：bootloader_get_env
* 功能：获取ENV参数
* 参数：env 环境参数指针
* 返回：0：成功 其他：失败
*/
int bootloader_get_env(bootloader_env_t *env)
{
  int rc;
  uint32_t env_offset;

  rc = bootloader_search_cur_env_offset(&env_offset);
  if(rc < 0 ){
    return -1;
  }else {
    rc = bootloader_read_bank1_env(env_offset,env);
    if(rc != 0){
       return -1;
   }
  }
  return 0;
}


/*名称：bootloader_save_env
* 功能：保存当前env
* 参数：env 环境参数指针
* 返回：0：成功 其他：失败
*/
int bootloader_save_env(bootloader_env_t *env)
{
  int rc;
  uint32_t env_offset;
  
  rc = bootloader_search_next_env_offset(&env_offset);
  if(rc < 0 ){
     return -1;
  }
  
  if(env_offset == BANK_FULL_ENV_OFFSET){
     rc = bootloader_flush_env(env);
     if(rc != 0){
        return -1;
     }
  }else{     
    rc = bootloader_write_bank1_env( env_offset,env);  
    if(rc != 0 ){
       return -1;
    }
  }
  
  return 0;
}

/*名称：bootloader_write_fw
* 功能：flash固件写入参数
* 参数：fw_dest_addr 固件目标地址
* 参数：fw_src_addr  固件源地址
* 参数：size         固件大小
* 返回：0：成功 其他：失败
*/
int bootloader_write_fw(uint32_t fw_dest_addr,uint32_t fw_src_addr,uint32_t size)
{
  int rc;
  /*拷贝fw*/
  /*擦除FW区域*/
  log_warning("program fw form addr:0x%X to 0x%X size:%d...\r\n",fw_src_addr,fw_dest_addr,size);
  log_warning("erase addr:0x%X size:%d...\r\n",fw_dest_addr,size);
  rc = flash_utils_erase(fw_dest_addr,size); 
  if(rc != 0){
  return -1;
  }
  log_warning("done.\r\n");
  /*编程FW区域*/
  log_warning("write addr:0x%X size:%d...\r\n",fw_dest_addr,size);
  rc = flash_utils_write(fw_dest_addr,(uint32_t *)fw_src_addr,size % 4 == 0 ? size / 4 : size / 4 + 1); 
  if(rc != 0){
  return -1;
  }
  log_warning("done.\r\n");
  return 0; 
}

/*名称：bootloader_copy_user_to_swap
* 功能：从用户区复制数据到交换区
* 参数：env 参数指针
* 返回：0：成功 其他：失败
*/
static int bootloader_copy_user_to_swap(bootloader_env_t *env)
{
  int rc;
  uint32_t size;
  
  /*第一次运行或者已经完成上一步骤是 SWAP_STEP_COPY_SWAP_TO_UPDATE 情况下才会执行过程*/
  log_warning("copy user app to swap.\r\n");
  if(env->swap_ctrl.step == SWAP_STEP_INIT || env->swap_ctrl.step == SWAP_STEP_COPY_SWAP_TO_UPDATE){    
     if(env->swap_ctrl.origin_offset < env->fw_origin.size){  
        size = env->fw_origin.size - env->swap_ctrl.origin_offset > BOOTLOADER_FLASH_SWAP_BLOCK_SIZE ? BOOTLOADER_FLASH_SWAP_BLOCK_SIZE : env->fw_origin.size - env->swap_ctrl.origin_offset;
        rc = bootloader_write_fw(BOOTLOADER_FLASH_BASE_ADDR + BOOTLOADER_FLASH_SWAP_BLOCK_ADDR_OFFSET,
                                 BOOTLOADER_FLASH_BASE_ADDR + BOOTLOADER_FLASH_USER_APPLICATION_ADDR_OFFSET + env->swap_ctrl.origin_offset,
                                 size);
        if(rc != 0){
           return -1;
         }           
         env->swap_ctrl.origin_offset += env->swap_ctrl.size;
         env->swap_ctrl.size = size;
     }
  env->swap_ctrl.step = SWAP_STEP_COPY_USER_TO_SWAP;
  /*保存当前env*/ 
  rc = bootloader_save_env(env);
  if(rc != 0){
   return -1;  
  }
  }
  log_warning("done.\r\n");
  return 0;
 } 
  
/*名称：bootloader_copy_update_to_user
* 功能：从更新区复制数据到用户区
* 参数：env 参数指针
* 返回：0：成功 其他：失败
*/
static int bootloader_copy_update_to_user(bootloader_env_t *env)
{
  int rc;
  uint32_t size;
  
  log_warning("copy update to user.\r\n");
  /*上一步骤是SWAP_STEP_COPY_USER_TO_SWAP情况下才会执行复制过程*/
  if(env->swap_ctrl.step == SWAP_STEP_COPY_USER_TO_SWAP){
     if(env->swap_ctrl.update_offset < env->fw_update.size){
        size = env->fw_update.size - env->swap_ctrl.update_offset > BOOTLOADER_FLASH_SWAP_BLOCK_SIZE ? BOOTLOADER_FLASH_SWAP_BLOCK_SIZE : env->fw_update.size - env->swap_ctrl.update_offset;
        rc = bootloader_write_fw(BOOTLOADER_FLASH_BASE_ADDR + BOOTLOADER_FLASH_USER_APPLICATION_ADDR_OFFSET + env->swap_ctrl.update_offset,
                                 BOOTLOADER_FLASH_BASE_ADDR + BOOTLOADER_FLASH_UPDATE_APPLICATION_ADDR_OFFSET + env->swap_ctrl.update_offset,
                                 size);
        if(rc != 0){
           return -1;
        } 
       env->swap_ctrl.update_offset += size;
    }
    env->swap_ctrl.step = SWAP_STEP_COPY_UPDATE_TO_USER;
    /*保存当前env*/ 
    rc = bootloader_save_env(env);
    if(rc != 0){
      return -1;  
    }
  }
  log_warning("done.\r\n");
  return 0;
}

/*名称：bootloader_copy_swap_to_update
* 功能：从交换区复制数据到更新区
* 参数：env env参数指针
* 返回：0：成功 其他：失败
*/
static int bootloader_copy_swap_to_update(bootloader_env_t *env)
{
  int rc;
  
  log_warning("copy swap to update.\r\n");
  /*上一步骤是SWAP_STEP_COPY_UPDATE_TO_USER情况下才会执行复制过程*/
  if(env->swap_ctrl.step == SWAP_STEP_COPY_UPDATE_TO_USER){
     if(env->swap_ctrl.size > 0){  
        rc = bootloader_write_fw(BOOTLOADER_FLASH_BASE_ADDR + BOOTLOADER_FLASH_UPDATE_APPLICATION_ADDR_OFFSET + env->swap_ctrl.origin_offset,
                                 BOOTLOADER_FLASH_BASE_ADDR + BOOTLOADER_FLASH_SWAP_BLOCK_ADDR_OFFSET,
                                 env->swap_ctrl.size);
        if(rc != 0){
           return -1;
        }           
        env->swap_ctrl.origin_offset += env->swap_ctrl.size;
        env->swap_ctrl.size = 0;
    }
    env->swap_ctrl.step = SWAP_STEP_COPY_SWAP_TO_UPDATE;
    /*保存当前env*/ 
    rc = bootloader_save_env(env);
    if(rc != 0){
       return -1;  
    }
  }
  log_warning("done.\r\n");
  return 0;
 } 
  

/*名称：bootloader_update_user_app
* 功能：更新用户APP
* 参数：env  环境参数指针
* 返回：0：成功 其他：失败
*/
int bootloader_update_user_app(bootloader_env_t *env)
{
 int rc;
 bootloader_fw_t fw_temp;
 
 log_warning("update user app...\r\n");
 /*等待所有数据复制完毕*/
 while(env->swap_ctrl.update_offset != env->fw_update.size || env->swap_ctrl.origin_offset != env->fw_origin.size){ 
    /*以下3个步骤顺序，循环执行*/
   if( bootloader_copy_user_to_swap(env) != 0){
      return -1;
   }
   if( bootloader_copy_update_to_user(env)!= 0){
      return -1;
   }
   if( bootloader_copy_swap_to_update(env)!= 0){
      return -1;
   }
 }
 
 env->boot_flag = BOOTLOADER_FLAG_BOOT_UPDATE_COMPLETE;

 fw_temp = env->fw_origin;
 env->fw_origin = env->fw_update;
 env->fw_update = fw_temp;
 env->swap_ctrl.step = SWAP_STEP_INIT;
 env->swap_ctrl.origin_offset = 0;
 env->swap_ctrl.update_offset = 0;
 env->swap_ctrl.size = 0;
 /*保存当前env*/ 
 rc = bootloader_save_env(env);
 if(rc != 0){
   return -1;  
 }
 log_warning("done.\r\n");
 
 return 0;
}

/*名称：bootloader_recovery_user_app
* 功能：恢复用户APP
* 参数：env  环境参数指针
* 返回：无
*/
int bootloader_recovery_user_app(bootloader_env_t *env)
{
 int rc;
 
 log_warning("recovery user app...\r\n");
 bootloader_write_fw(BOOTLOADER_FLASH_BASE_ADDR + BOOTLOADER_FLASH_USER_APPLICATION_ADDR_OFFSET,
                     BOOTLOADER_FLASH_BASE_ADDR + BOOTLOADER_FLASH_UPDATE_APPLICATION_ADDR_OFFSET,
                     env->fw_update.size);
 
 env->fw_origin = env->fw_update;
 env->boot_flag = BOOTLOADER_FLAG_BOOT_NORMAL;
 
 env->swap_ctrl.step = SWAP_STEP_INIT;
 env->swap_ctrl.origin_offset = 0;
 env->swap_ctrl.update_offset = 0;
 env->swap_ctrl.size = 0;
 /*保存当前env*/ 
 rc = bootloader_save_env(env);
 if(rc != 0){
   return -1;  
 }
 log_warning("done.\r\n")
   
 return 0;
}
