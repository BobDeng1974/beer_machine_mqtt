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

uint32_t time_offset;


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
    float temperature_env;
    uint32_t compressor_run_time;
    uint8_t compressor_status;
    base_information_t base_info;
}device_log_t;

typedef struct
{
    http_client_context_t http_client_context;
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
    http_client_context_t http_client_context;
    char *url;
    uint32_t bin_size;
    uint32_t version_code;
    uint32_t download_size;
    char     version_str[16];
    char     download_url[100];
    char     md5[33];
    uint8_t  update;
    uint8_t  is_download_completion;
}device_upgrade_t;


/** 设备运行配置信息表*/
typedef struct
{
    http_client_context_t http_client_context;
    char *url;
    uint32_t log_interval;
    float temperature_cold;
    float temperature_freeze;
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
    http_client_context_t http_client_context;
    char *url_spawn;
    char *url_clear;
    char *sn;
    char *src;
    char *key;
    device_fault_queue_t queue;
}device_fault_t;
    
typedef struct
{   
    uint8_t is_device_active;
    uint32_t utc_time_offset;
    uint32_t fw_version_code;

    device_config_t config;
    device_active_t active;
    device_upgrade_t upgrade;
    device_log_t log;
    device_fault_t fault;
    http_client_context_t http_client_context;
}report_task_context_t;


static report_task_context_t report_task_context;


static uint32_t report_task_get_retry_timeout(int retry)
{
    return 0;
}
/*同步UTC时间*/
static int report_task_sync_utc(uint32_t *offset)
{
    int rc;
    uint32_t cur_time;
  
    rc = ntp_sync_time(&cur_time);
    if (rc != 0) {
        return -1;
    }
  
    *offset = cur_time - osKernelSysTick() / 1000;
  
    return 0; 
}

/** 获取同步后的本地UTC*/
static uint32_t report_task_get_utc()
{
    return osKernelSysTick() / 1000 + time_offset;
}

/** 字节转换成HEX字符串*/
static void dump_hex_str(const char *src,char *dest,uint16_t src_len)
 {
    char temp;
    char hex_digital[] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};
    for (uint16_t i = 0; i < src_len; i++){  
        temp = src[i];  
        dest[2 * i] = hex_digital[(temp >> 4) & 0x0f ];  
        dest[2 * i + 1] = hex_digital[temp & 0x0f];  
    }
    dest[src_len * 2] = '\0';
}

/** 构建签名信息*/
int report_task_build_sign(char *sign,const int cnt,...)
{ 
#define  SIGN_SRC_BUFFER_SIZE_MAX               300
    int size = 0;
    char sign_src[SIGN_SRC_BUFFER_SIZE_MAX] = { 0 };
    char md5_hex[16];
    char md5_str[33];
    va_list ap;
    char *temp;
 
    va_start(ap, cnt);
    /*组合MD5的源数据,根据输入数据*/  
    for (uint8_t i = 0; i < cnt; i++){
        temp = va_arg(ap,char*);
        /*保证数据不溢出*/
        if (size + strlen(temp) >= SIGN_SRC_BUFFER_SIZE_MAX) {
            return -1;
        }
        size += strlen(temp);
        strcat(sign_src,temp);
        if (i < cnt - 1) {
            strcat(sign_src,"&");
        }
    }
 
    /*第1次MD5*/
    md5(sign_src,strlen(sign_src),md5_hex);
    /*把字节装换成HEX字符串*/
    dump_hex_str(md5_hex,md5_str,16);
    /*第2次MD5*/
    md5(md5_str,32,md5_hex);
    dump_hex_str(md5_hex,md5_str,16);
    strcpy(sign,md5_str);
 
    return 0;
}

static int report_task_build_url(char *url,const int size,const char *origin_url,const char *sn,const char *sign,const char *source,const char *timestamp)
{
    snprintf(url,size,"%s?sn=%s&sign=%s&source=%s&timestamp=%s",origin_url,sn,sign,source,timestamp);
    if (strlen(url) >= size - 1) {
        log_error("url size:%d too large.\r\n",size - 1); 
        return -1;
    }
    return 0;
}

static int report_task_do_download(http_client_context_t *http_client_ctx,char *url,char *buffer,uint32_t start,uint16_t size)
{
    int rc;

    http_client_context_t context;

    http_client_ctx->range_size = size;
    http_client_ctx->range_start = start;
    http_client_ctx->rsp_buffer = buffer;
    http_client_ctx->rsp_buffer_size = size;
    http_client_ctx->url = url;
    http_client_ctx->timeout = 10000;
    http_client_ctx->user_data = NULL;
    http_client_ctx->user_data_size = 0;
    http_client_ctx->boundary = BOUNDARY;
    http_client_ctx->is_form_data = false;
    http_client_ctx->content_type = "application/Json"; 
  
    rc = http_client_download(&context);
  
    if(rc != 0 ){
        log_error("download bin err.\r\n");
        return -1; 
    }
    if(context.content_size != size){
        log_error("download bin size err.\r\n");
        return -1; 
    }
    log_debug("download bin ok. size:%d.\r\n",size);
    return 0;
}

static char download_buffer[2050];

static int report_task_download_upgrade_file(http_client_context_t *http_client_ctx,device_upgrade_t *upgrade)
{
    int rc,size;

    size = upgrade->bin_size - upgrade->download_size > DEVICE_MIN_ERASE_SIZE ? DEVICE_MIN_ERASE_SIZE : upgrade->bin_size - upgrade->download_size;
    rc = report_task_do_download(&upgrade->http_client_context,upgrade->url,download_buffer,upgrade->download_size,size);
    if (rc != 0) {
        log_error("download fail.\r\n");
        return -1;
    }
    rc = flash_if_write(APPLICATION_UPDATE_BASE_ADDR + upgrade->download_size,download_buffer,size);
    if (rc != 0) {
        return -1;
    }
    if (upgrade->bin_size == upgrade->download_size) {
        upgrade->completion = 1;
    }
    return 0;
}


static int report_task_process_upgrade(device_upgrade_t *upgrade)
{
    /*下载完成 计算md5*/
    char md5_hex[16];
    char md5_str[33];
    
    md5((const char *)(APPLICATION_UPDATE_BASE_ADDR),upgrade->bin_size,md5_hex);
    /*转换成hex字符串*/
    dump_hex_to_string(md5_hex,md5_str,16);
    /*校验成功，启动升级*/
    if (strcmp(md5_str,upgrade->md5) == 0) {          
        system_reboot();
    } 
    /*中止本次升级*/
    log_error("md5 err.calculate:%s recv:%s.stop upgrade.\r\n",md5_str,upgrade->md5); 
    return -1;
}


/*把一个故障参数放进故障参数队列*/      
static int report_task_insert_fault(device_fault_queue_t *queue,device_fault_information_t *fault)
{      
    if (queue->write - queue->read >= REPORT_TASK_FAULT_QUEUE_SIZE){
        log_warning("fault queue is full.\r\n");
        return -1;
    }
    queue->imformation[queue->write & (REPORT_TASK_FAULT_QUEUE_SIZE - 1)] = *fault;
    queue->write++;    
    return 0;    
}

/*从故障参数队列取出一个故障参数*/         
static int report_task_peek_fault(device_fault_queue_t *queue,device_fault_information_t *fault)
{     
    if (queue->read >= queue->write) {
        log_warning("fault queue is null.\r\n");
        return -1;
    }
       
    *fault = queue->imformation[queue->read & (REPORT_TASK_FAULT_QUEUE_SIZE - 1)];   
    return 0;    
}
/*从故障参数队列删除一个故障参数*/         
static int report_task_delete_fault(device_fault_queue_t *queue)
{
    if (queue->read < queue->write) {
        queue->read++; 
    }
    return 0;
}
 


/*执行故障信息数据上报*/
static int report_task_do_report_fault(http_client_context_t *http_client_ctx,char *url_origin,char *key,char *sn,char *src,device_fault_information_t *fault)
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

    /*判断错误时间*/
    timestamp = atoi(fault->time);
    if (timestamp < 1546099200) {/*时间晚于2018.12.30没有同步，重新构造时间*/
        timestamp = report_task_get_utc() - timestamp;
        snprintf(fault->time,14,"%d",timestamp);
    }

    /*计算时间戳字符串*/
    timestamp = report_task_get_utc();
    snprintf(timestamp_str,14,"%d",timestamp);
    /*计算sign*/
    report_task_build_sign(sign_str,8,fault->code,fault->msg,fault->time,key,sn,src,fault->status,timestamp_str);
    /*构建新的url*/
    report_task_build_url(url,200,url_origin,sn,sign_str,src,timestamp_str);  
   
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

static int report_task_report_fault(http_client_context_t *http_client_ctx,device_fault_t *fault)
{
    int rc;
    device_fault_information_t fault_info;

    /*取出故障信息*/
    rc = report_task_peek_fault(&fault->queue,&fault_info);
    /*存在未上报的故障*/
    if (rc == 1) {
        if (fault_info.status == HAL_FAULT_STATUS_FAULT) {
            rc = report_task_do_report_fault(http_client_ctx,fault->url_spawn,fault->key,fault->sn,fault->src,&fault_info);
        } else {
            rc = report_task_do_report_fault(http_client_ctx,fault->url_clear,fault->key,fault->sn,fault->src,&fault_info);
        }
        if (rc != 0) {
            log_error("report task report fault fail.\r\n");
            return -1;
        } else {
            report_task_delete_fault(&fault->queue);
        }
    }

    return 0;
}


#define  REPORT_TASK_SYNC_UTC_INTERVAL                   (24 * 60 * 60 * 1000)

#define  REPORT_TASK_MSG_SIM_ID                          0x1000
#define  REPORT_TASK_MSG_BASE_INFO                       0x1001
#define  REPORT_TASK_MSG_SYNC_UTC                        0x1002
#define  REPORT_TASK_MSG_ACTIVE_DEVICE                   0x1003
#define  REPORT_TASK_MSG_GET_UPGRADE_INFO                0x1004
#define  REPORT_TASK_MSG_DOWNLOAD_UPGRADE_FILE           0x1005
#define  REPORT_TASK_MSG_TEMPERATURE_UPDATE              0x1006
#define  REPORT_TASK_MSG_TEMPERATURE_SENSOR_FAULT_SPAWM  0x1007
#define  REPORT_TASK_MSG_TEMPERATURE_SENSOR_FAULT_CLEAR  0x1008
#define  REPORT_TASK_MSG_REPORT_LOG                      0x1009
#define  REPORT_TASK_MSG_REPORT_FAULT                    0x100a


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

    device_config_init(&report_task_context.config);

    temp = device_env_get(ENV_NAME_TEMPERATURE_COLD);
    if (temp) {
        report_task_context.config.temperature_cold = atoi(temp);
    }
    temp = device_env_get(ENV_NAME_TEMPERATURE_FREEZE);
    if (temp) {
        report_task_context.config.temperature_freeze = atoi(temp);
    }
    temp = device_env_get(ENV_NAME_COMPRESSOR_CTRL);
    if (temp) {
        report_task_context.config.lock = atoi(temp);
    }

    /*分发默认的开机配置参数*/
    report_task_dispatch_device_config(&report_task_context.config);

    while (1)
    {
    if (xQueueReceive(report_task_msg_q_id, &msg_recv,0xFFFFFFFF) == pdTRUE) {
        /*处理SIM ID消息*/
        if (msg_recv.head.id == REPORT_TASK_MSG_SIM_ID) {
            strncpy(report_task_context.active.sim_id,msg_recv.content.value,SIM_ID_LEN);
        }
    
        /*处理位置消息*/
        if (msg_recv.head.id == REPORT_TASK_MSG_BASE_INFO) { 
            update_base_information(report_task_context.log.base_info,msg_recv.content.value,msg_recv.content.size);
        }

        /*处理同步UTC消息*/
        if (msg_recv.head.id == REPORT_TASK_MSG_SYNC_UTC) { 
            rc = report_task_sync_utc(&report_task_context.utc_time_offset);
            if (rc != 0) {
                retry ++;
                log_error("report task sync utc fail.\r\n");
                report_task_start_sync_utc_timer(report_task_get_retry_timeout(retry)); 
            } else {
                retry = 0;
                log_info("report task sync utc ok.\r\n");
                report_task_start_sync_utc_timer(REPORT_TASK_SYNC_UTC_INTERVAL); 
                /*开始设备激活*/
                msg_temp.head.id = REPORT_TASK_MSG_ACTIVE_DEVICE;
                log_assert_bool_false(xQueueSend(report_task_msg_q_id,&msg_temp,REPORT_TASK_PUT_MSG_TIMEOUT) == pdPASS);
            }
        }

        /*设备激活消息*/
        if (msg_recv.head.id == REPORT_TASK_MSG_ACTIVE_DEVICE) { 
            rc = report_task_device_active(&report_task_context.http_client_context,&report_task_context.active,&report_task_context.config);
            if (rc != 0) {
                retry ++;
                log_error("report task active fail.\r\n" );
                report_task_start_active_timer(report_task_get_retry_timeout(retry)); 
            } else {
                retry = 0;
                report_task_context.is_device_active = 1;
                log_info("device active ok.\r\n");
                /*分发激活后的配置参数*/
                report_task_dispatch_device_config(&report_task_context.config);
                /*保存新激活的参数*/
                report_task_save_device_config(&report_task_context.config);
                /*打开启日志上报定时器*/
                report_task_start_log_timer(report_task_context.config.log_interval);
         
                /*打开故障上报定时器*/
                report_task_start_fault_timer(0);
                /*激活后获取升级信息*/
                report_task_start_active_timer(0,REPORT_TASK_MSG_GET_UPGRADE_INFO); 
            }
        }

        /*获取更新信息消息*/
        if (msg_recv.head.id == REPORT_TASK_MSG_GET_UPGRADE_INFO) { 
            rc = report_task_get_upgrade(&report_task_context.http_client_context,&report_task_context.upgrade);
            if (rc != 0) {
                retry ++;
                log_error("report task get upgrade fail.\r\n");
                report_task_start_active_timer(report_task_retry_delay(retry),REPORT_TASK_MSG_GET_UPGRADE_INFO); 
            } else {
            /*获取成功处理*/   
                retry = 0;
                /*对比现在的版本号*/
                if (report_task_context.upgrade.update == 1 && report_task_context.upgrade.version_code > report_task_context.fw_version_code) {
                    log_info("firmware need upgrade.ver_code:%d size:%d md5:%s.start download...\r\n",
                    report_task_context.upgrade.version_code,report_task_context.upgrade.bin_size,report_task_context.upgrade.md5);
                    /*下载新文件*/
                    report_task_start_active_timer(0,REPORT_TASK_MSG_DOWNLOAD_UPGRADE_FILE);                 
                } else {
                    log_info("firmware is latest.\r\n");       
                }             
            }
        }
    
    
        /*下载更新文件*/
        if (msg_recv.head.id == REPORT_TASK_MSG_DOWNLOAD_UPGRADE_FILE) { 
            int size;
            rc = report_task_download_upgrade_file(&report_task_context.http_client_context,&report_task_context.upgrade);
            if(rc != 0){
                retry ++;
                log_error("report task download upgrade fail.\r\n");
                report_task_start_download_timer(report_task_retry_delay(retry)); 
            } else {      
                /*判断是否下载完毕*/
                if (report_task_context.upgrade.is_download_completion) {
                    /*处理升级*/
                    report_task_do_upgrade(&report_task_context.upgrade);
                } else {
                    /*没有下载完毕，继续下载*/
                    report_task_start_download_timer(0);
                }
            }
        }
          
        /*温度消息*/
        if (msg_recv.head.id == REPORT_TASK_MSG_TEMPERATURE_UPDATE) { 
            report_task_context.log.temperature_cold = msg_recv.content.value_float[0];
            report_task_context.log.temperature_freeze = msg_recv.content.value_float[1];
        }

        /*温度传感器故障消息*/
        if (msg_recv.head.id == REPORT_TASK_MSG_TEMPERATURE_SENSOR_FAULT_SPAWM) {   
            device_fault_information_t fault;
            report_task_context.log.temperature_cold = 0xFF;
            report_task_context.log.temperature_env = 0xFF;
            report_task_context.log.temperature_freeze = 0xFF;
            report_task_build_fault(&fault,"2010","null",report_task_get_utc(),1);
            report_task_insert_fault(&report_task_context.fault.queue,&fault);  
        }

        /*温度传感器故障解除消息*/
        if (msg_recv.head.id == REPORT_TASK_MSG_TEMPERATURE_SENSOR_FAULT_CLEAR) {    
            device_fault_information_t fault;
            report_task_context.log.temperature_cold = *(float *)msg_recv.content.value;
            report_task_context.log.temperature_env = report_task_context.log.temperature_cold + 10;
            report_task_context.log.temperature_freeze = report_task_context.log.temperature_cold - 10;
            report_task_build_fault(&fault,"2010","null",report_task_get_utc(),0);
            report_task_insert_fault(&report_task_context.fault.queue,&fault);   
        }
 
        /*数据上报消息*/
        if (msg_recv.head.id == REPORT_TASK_MSG_REPORT_LOG) {
            /*只有设备激活才可以上报数据*/
            if (report_task_context.is_device_active == 1) {       
                rc = report_task_report_log(&report_task_context.http_client_context,&report_task_context.log);
                if (rc == 0) {
                    retry = 0;
                    log_info("report log ok.\r\n");
                    /*重置日志上报定时器*/
                    report_task_start_log_timer(report_task_context.config.log_interval); 
                } else {
                    retry ++;
                    log_error("report log err.\r\n");
                    /*重置日志上报定时器*/
                    report_task_start_log_timer(report_task_retry_delay(retry));
                }
            }   
        }

        /*故障上报消息*/
        if (msg_recv.head.id == REPORT_TASK_MSG_REPORT_FAULT) {
            /*只有设备激活才可以上报故障*/
            if (report_task_context.is_device_active == 1) {
                rc = report_task_report_fault(&report_task_context.http_client_context,&report_task_context.fault);
                if (rc == 0) {
                    retry = 0;
                    log_info("report fault ok.\r\n");
                    /*再次开启*/
                    report_task_start_fault_timer(0);
                } else {
                    retry ++;
                    log_error("report log err.\r\n");
                    /*重置日志上报定时器*/
                    report_task_start_fault_timer(report_task_retry_delay(retry));
                }                

            }
          }
       }
   }

 }