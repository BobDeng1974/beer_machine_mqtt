#include "cmsis_os.h"
#include "device_config.h"
#include "md5.h"
#include "ntp.h"
#include "http_client.h"
#include "report_task.h"
#include "firmware_version.h"
#include "device_env.h"
#include "net_task.h"
#include "tasks_init.h"
#include "temperature_task.h"
#include "compressor_task.h"
#include "cJSON.h"
#include "log.h"


osThreadId  report_task_handle;
osMessageQId report_task_msg_q_id;

osTimerId    active_timer_id;
osTimerId    log_timer_id;
osTimerId    fault_timer_id;




#define  BASE_CNT_MAX  5
typedef struct
{
    char lac[8];
    char cid[8];
    int rssi; 
}base_t;

typedef struct
{
    base_t base[BASE_CNT_MAX];
    int cnt;
}base_information_t;


typedef struct
{
    char *url;
    float temperature_cold;
    float temperature_freeze;
    base_information_t base_info;
}device_log_t;

typedef struct
{
    http_client_context_t *http_ctx;
    char *url;
    char *device_model;
    char *device_type;
    char *opt_code;
    char *vendor;
    char sn[SN_LEN + 1];
    char sim_id[SIM_ID_LEN + 1];
    char *fw_version;
    uint32_t fw_code;
    base_information_t base_info;
}device_active_t;

typedef struct
{  
    uint32_t bin_size;
    uint32_t version_code;
    uint32_t download_size;
    char     version_str[16];
    char     download_url[100];
    char     md5[33];
    bool     update;
}report_upgrade_t;


/*设备运行配置信息表*/
typedef struct
{
    http_client_context_t *http_ctx;
    char *url;
    uint32_t report_log_interval;
    int8_t temperature_cold;
    int8_t temperature_freeze;
    int lock;
    int is_new;
}device_config_t;

typedef enum
{
    HAL_FAULT_STATUS_FAULT = 0,
    HAL_FAULT_STATUS_FAULT_CLEAR,
}fault_status_t;
  
typedef struct
{
    char code[5];
    char msg[6];
    char time[14];
    char status[2];
}device_fault_information_t;

typedef struct
{
  uint32_t read;
  uint32_t write;
  device_fault_information_t imformation[REPORT_TASK_FAULT_QUEUE_SIZE];
}device_fault_queue_t;

typedef struct
{
    http_client_context_t *http_ctx;
    char *url;
    char *sn;
    device_fault_queue_t queue;
}device_fault_t;
    
typedef struct
{   
    device_config_t config;
    device_active_t active;
    device_log_t log;
    device_fault_t fault;
}report_task_context_t;


static report_task_context_t task_context;


static uint32_t report_task_get_retry_timeout(int retry)
{
    return 0;
}

static int report_task_download_upgrade_file(device_upgrade_t *upgrade)
{
    size = upgrade->bin_size - upgrade->download_size > DEVICE_MIN_ERASE_SIZE ? DEVICE_MIN_ERASE_SIZE : upgrade->bin_size - upgrade->download_size;
}
static int report_task_process_upgrade(device_upgrade_t *upgrade)
{
                    /*下载完成 计算md5*/
                    char md5_hex[16];
                    char md5_str[33];
                    md5((const char *)(BOOTLOADER_FLASH_BASE_ADDR + BOOTLOADER_FLASH_UPDATE_APPLICATION_ADDR_OFFSET),upgrade->bin_size,md5_hex);
                    /*转换成hex字符串*/
                    bytes_to_hex_str(md5_hex,md5_str,16);
                    /*校验成功，启动升级*/
                    if (strcmp(md5_str,upgrade->md5) == 0) {             
                    } else {
                    /*中止本次升级*/
                    log_error("md5 err.calculate:%s recv:%s.stop upgrade.\r\n",md5_str,upgrade->md5);
                    /*开启定时作为同步时间定时器*/
                    report_task_start_active_timer(REPORT_TASK_SYNC_UTC_DELAY,REPORT_TASK_MSG_SYNC_UTC);                
                    }           
                } else {
           
                }   


}

/*执行故障信息数据上报*/
static int report_task_do_report_fault(http_client_context_t *http_ctx,device_fault_queue_t *queue)
{
    //report_task_build_sign(sign,5,/*errorCOde*/"2010",/*errorMsg*/"",sn,source,timestamp);
    int rc;
    uint32_t timestamp;
    http_client_context_t context;
 
    char timestamp_str[14] = { 0 };
    char sign_str[33] = { 0 };
    char req[320];
    char rsp[400] = { 0 };
    char url[200] = { 0 };

    /*计算时间戳字符串*/
    timestamp = report_task_get_utc();
    snprintf(timestamp_str,14,"%d",timestamp);
    /*计算sign*/
    report_task_build_sign(sign_str,8,fault->code,fault->msg,fault->time,KEY,sn,SOURCE,fault->status,timestamp_str);
    /*构建新的url*/
    report_task_build_url(url,200,url_origin,sn,sign_str,SOURCE,timestamp_str);  
   
    report_task_build_fault_form_data_str(req,320,fault);

 
    context.range_size = 200;
    context.range_start = 0;
    context.rsp_buffer = rsp;
    context.rsp_buffer_size = 400;
    context.url = url;
    context.timeout = 10000;
    context.user_data = (char *)req;
    context.user_data_size = strlen(req);
    context.boundary = BOUNDARY;
    context.is_form_data = true;
    context.content_type = "multipart/form-data; boundary=";
 
    rc = http_client_post(&context);
 
    if (rc != 0) {
        log_error("report fault err.\r\n");  
        return -1;
    }
    rc = report_task_parse_fault_rsp_json(context.rsp_buffer);
    if (rc != 0) {
        log_error("json parse fault rsp error.\r\n");  
        return -1;
    }
    log_debug("report fault ok.\r\n");  
    return 0;
}

static int report_task_report_fault(device_fault_queue_t *queue)
{
    fault_information_t fault;
    uint32_t fault_time;
    /*取出故障信息*/
    rc = report_task_peek_fault_from_queue(queue,&fault);
    /*存在未上报的故障*/
    if (rc == 1) {
       fault_time = atoi(fault.time);
        if (fault_time < 1546099200) {/*时间晚于2018.12.30没有同步，重新构造时间*/
            fault_time = report_task_get_utc() - fault_time;
            snprintf(fault.time,14,"%d",fault_time);
        }

        
       rc = report_task_report_fault(&task_context.fault_queue);
       if (rc != 0) {
        log_error("report task report fault timeout.%d S later retry.\r\n",report_task_retry_delay(fault_retry) / 1000);
        report_task_start_fault_timer(report_task_retry_delay(fault_retry)); 
        } else {
       report_task_delete_fault_from_queue(&fault_queue);
        fault_retry = 0;
    }


}
/*上报任务*/
void report_task(void const *argument)
{
    int rc;
    int retry;
    char *temp;
    report_task_message_t msg_recv;
    report_task_message_t msg_temp;

    /*定时器初始化*/
    report_task_utc_timer_init();
    report_task_active_timer_init();
    report_task_log_timer_init();
    report_task_fault_timer_init();

    device_env_init();

    device_config_init(&task_context.config);

    temp = device_env_get(ENV_NAME_TEMPERATURE_COLD);
    if (temp) {
        task_context.config.temperature_cold = atoi(temp);
    }
    temp = device_env_get(ENV_NAME_TEMPERATURE_FREEZE);
    if (temp) {
        task_context.config.temperature_freeze = atoi(temp);
    }
    temp = device_env_get(ENV_NAME_COMPRESSOR_CTRL);
    if (temp) {
        task_context.config.lock = atoi(temp);
    }

    /*分发默认的开机配置参数*/
    report_task_dispatch_device_config(&device_config);

    while (1)
    {
    if (xQueueReceive(mqtt_task_msg_hdl, &msg_recv,0xFFFFFFFF) == pdTRUE) {
        /*处理SIM ID消息*/
        if (msg_recv.head.id == REPORT_TASK_MSG_SIM_ID) {
            strncpy(task_context.active.sim_id,msg_recv.content.value,SIM_ID_LEN);
        }
    
        /*处理位置消息*/
        if (msg_recv.head.id == REPORT_TASK_MSG_BASE_INFO) { 
            update_base_information(task_context.log.base_info,msg_recv.content.value,msg_recv.content.size);
            /*开始同步UTC时间*/
            msg_temp.head.id = REPORT_TASK_MSG_SYNC_UTC;
            log_assert_bool_false(xQueueSend(report_task_msg_hdl,&msg_temp,REPORT_TASK_PUT_MSG_TIMEOUT) == pdPASS);
        }

        /*处理同步UTC消息*/
        if (msg_recv.head.id == REPORT_TASK_MSG_SYNC_UTC) { 
            rc = report_task_sync_utc(&task_context.time_offset);
            if (rc != 0) {
                retry ++;
                log_error("report task sync utc fail.%d S later retry.\r\n",report_task_get_retry_timeout(retry) / 1000);
                report_task_start_sync_utc_timer(report_task_get_retry_timeout(++retry)); 
            } else {
                retry = 0;
                log_info("report task sync utc ok.\r\n");
                /*为了后面的定时同步时间*/
                if (task_context.is_active == false) {
                    report_task_start_active_timer(0);
                } else {
                    report_task_start_sync_utc_timer(REPORT_TASK_SYNC_UTC_INTERVAL); 
                }
            }
        }

        /*设备激活消息*/
        if (msg_recv.head.id == REPORT_TASK_MSG_DEVICE_ACTIVE) { 
            rc = report_task_device_active(&task_context.active,&task_context.config);
            if (rc != 0) {
                retry ++;
                log_error("report task active fail.%d S later retry.\r\n",report_task_get_retry_timeout(retry)); );
                report_task_start_active_timer(report_task_get_retry_timeout(++retry)); 
            } else {
                retry = 0;
                task_context.is_active = true;
                log_info("device active ok.\r\n");
                /*分发激活后的配置参数*/
                report_task_dispatch_device_config(&task_context.config);
                /*保存新激活的参数*/
                report_task_save_device_config(&device_config);
                /*打开启日志上报定时器*/
                report_task_start_log_timer(device_config.report_log_interval);
         
                /*打开故障上报定时器*/
                report_task_start_fault_timer(0);
                /*激活后获取升级信息*/
                report_task_start_active_timer(0,REPORT_TASK_MSG_GET_UPGRADE); 
            }
        }
        /*获取更新信息消息*/
        if (msg_recv.head.id == REPORT_TASK_MSG_GET_UPGRADE) { 
            rc = report_task_get_upgrade(&context.upgrade);
            if (rc != 0) {
                log_error("report task get upgrade fail.%d S later retry.\r\n",report_task_retry_delay(upgrade_retry) / 1000);
                report_task_start_active_timer(report_task_retry_delay(upgrade_retry),REPORT_TASK_MSG_GET_UPGRADE); 
            } else {
        /*获取成功处理*/   
         upgrade_retry = 0;
         /*对比现在的版本号*/
         if(upgrade->update == true && upgrade->version_code > fw_version_code){
           log_info("firmware need upgrade.ver_code:%d size:%d md5:%s.start download...\r\n",
                    upgrade->version_code,upgrade->bin_size,upgrade->md5);
            report_task_start_active_timer(0,REPORT_TASK_MSG_DOWNLOAD_UPGRADE);                 
         }else{
            log_info("firmware is latest.\r\n");  
            /*开启定时作为同步时间定时器*/
            report_task_start_active_timer(REPORT_TASK_SYNC_UTC_DELAY,REPORT_TASK_MSG_SYNC_UTC);       
         }             
       }
    }
    
    
        /*下载更新文件*/
        if (msg_recv.head.id == REPORT_TASK_MSG_DOWNLOAD_UPGRADE_FILE) { 
            int size;
            rc = report_task_download_upgrade_file(&task_contex.upgrade);
            if(rc != 0){
                retry ++;
                log_error("report task download upgrade fail.%d S later retry.\r\n",report_task_retry_delay(retry) / 1000);
                report_task_start_download_timer(report_task_retry_delay(upgrade_retry)); 
            } else {      
                /*判断是否下载完毕*/
                if (upgrade.is_download_completion) {
                    /*处理升级*/
                    report_task_do_upgrade(&task_context.upgrade);
                } else {
                    /*没有下载完毕，继续下载*/
                    report_task_start_download_timer(0);
                }
            }
        }
          
        /*温度消息*/
        if (msg_recv.head.id == REPORT_TASK_MSG_TEMPERATURE_UPDATE) { 
            task_context.log.temperature_cold = msg_recv.content.value_float[0];
            task_context.log.temperature_freeze = msg_recv.content.value_float[1];
        }

        /*温度传感器故障消息*/
        if (msg_recv.head.id == REPORT_TASK_MSG_TEMPERATURE_SENSOR_FAULT) {   
            fault_information_t fault;
            task_context.log.temperature = 0xFF;
            report_task_build_fault(&fault,"2010","null",report_task_get_utc(),1);
            report_task_put_fault_to_queue(&task_context.fault_queue,&fault);  
        }

        /*温度传感器故障解除消息*/
        if (msg_recv.head.id == REPORT_TASK_MSG_TEMPERATURE_SENSOR_NORMAL) {    
            fault_information_t fault;
            report_log.temperature = msg.value;
            report_task_build_fault(&fault,"2010","null",report_task_get_utc(),0);
            report_task_put_fault_to_queue(&fault_queue,&fault);  
        }
 
        /*数据上报消息*/
        if (msg_recv.head.id == REPORT_TASK_MSG_REPORT_LOG) {
            /*只有设备激活才可以上报数据*/
            if (task_context.is_active == 1) {       
                rc = report_task_report_log(URL_LOG,&report_log,report_active.sn);
                if (rc == 0) {
                    retry = 0;
                    log_info("report log ok.\r\n");
                    /*重置日志上报定时器*/
                    report_task_start_log_timer(task_context.config.log_interval); 
                } else {
                    retry ++;
                    log_error("report log err.\r\n");
                    /*重置日志上报定时器*/
                    report_task_start_log_timer(report_task_retry_delay(upgrade_retry));
                }
            }   
        }

        /*故障上报消息*/
        if (msg_recv.head.id == REPORT_TASK_MSG_REPORT_FAULT) {
            /*只有设备激活才可以上报故障*/
            if (task_context.is_active == 1) {    
    
                report_task_start_fault_timer(0);
            }
          }
       }
   }

   
                  
  }
}
