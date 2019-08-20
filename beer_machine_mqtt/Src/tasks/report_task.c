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
#include "mqtt_task.h"
#include "flash_if.h"
#include "stdlib.h"
#include "cJSON.h"
#include "log.h"


osThreadId  report_task_hdl;
osMessageQId report_task_msg_q_id;

osTimerId utc_timer_id;
osTimerId active_timer_id;
osTimerId log_timer_id;
osTimerId fault_timer_id;
osTimerId upgrade_timer_id;
osTimerId download_timer_id;
osTimerId loop_config_timer_id;

typedef struct
{
    char *url;
    char sn[SN_LEN + 1];
    char *key;
    char *source;
    float temperature_cold;
    float temperature_freeze;
    float temperature_env;
    uint32_t compressor_run_time;
    uint8_t compressor_is_pwr_on;
    base_information_t base_info;
}device_log_t;

typedef struct
{
    http_client_context_t http_client_context;
    char *url;
    char *key;
    char sn[SN_LEN + 1];
    char *source;    
}device_loop_config_t;

typedef struct
{
    http_client_context_t http_client_context;
    char *url;
    char *key;
    char sn[SN_LEN + 1];
    char *source;
    char *device_model;
    char *device_type;
    char *opt_code;
    char *vendor;
    char sim_id[M6312_SIM_ID_STR_LEN + 1];
    char *fw_version;
    uint32_t fw_code;
    base_information_t base_info;
}device_active_t;

typedef struct
{  
    http_client_context_t http_client_context;
    char *url;
    char sn[SN_LEN + 1];
    char *key;
    char *source;
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
    uint32_t log_interval;
    uint32_t loop_interval;
    float temperature_cold_min;
    float temperature_cold_max;
    float temperature_cold_offset;
    float temperature_freeze_min;
    float temperature_freeze_max;
    float temperature_freeze_offset;
    int is_lock;
    int is_new;
}device_config_t;

typedef enum
{
    HAL_FAULT_STATUS_FAULT = 0,
    HAL_FAULT_STATUS_FAULT_CLEAR,
}fault_status_t;
  
typedef struct
{
    int code;
    fault_status_t status;
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
    char sn[SN_LEN + 1];
    char *source;
    char *key;
    device_fault_queue_t queue;
}device_fault_t;
    
typedef struct
{   
    uint8_t is_start_sync_utc;
    uint8_t is_temp_sensor_err;
    uint8_t is_device_active;
    uint32_t utc_time_offset;
    uint32_t fw_version_code;

    device_config_t config;
    device_loop_config_t loop_config;
    device_active_t active;
    device_upgrade_t upgrade;
    device_log_t log;
    device_fault_t fault;
    http_client_context_t http_client_context;
}report_task_context_t;


static report_task_context_t report_task_context;


static void report_task_active_timer_expired(void const *argument);
static void report_task_log_timer_expired(void const *argument);
static void report_task_fault_timer_expired(void const *argument);
static void report_task_utc_timer_expired(void const *argument);
static void report_task_upgrade_timer_expired(void const *argument);
static void report_task_loop_config_timer_expired(void const *argument);
static void report_task_download_timer_expired(void const *argument);

/*同步utc定时器*/
static void report_task_utc_timer_init()
{
    osTimerDef(active_timer,report_task_utc_timer_expired);
    utc_timer_id = osTimerCreate(osTimer(active_timer),osTimerOnce,NULL);
    log_assert_null_ptr(utc_timer_id);
}

static void report_task_start_utc_timer(uint32_t timeout)
{
    osTimerStart(utc_timer_id,timeout);  
}

static void report_task_utc_timer_expired(void const *argument)
{
    report_task_message_t msg;

    msg.head.id = REPORT_TASK_MSG_SYNC_UTC;
    log_assert_bool_false(xQueueSend(report_task_msg_q_id,&msg,REPORT_TASK_PUT_MSG_TIMEOUT) == pdPASS);
}

/*激活设备的定时器*/
static void report_task_active_timer_init()
{
    osTimerDef(active_timer,report_task_active_timer_expired);
    active_timer_id = osTimerCreate(osTimer(active_timer),osTimerOnce,NULL);
    log_assert_null_ptr(active_timer_id);
}

static void report_task_start_active_timer(uint32_t timeout)
{
    osTimerStart(active_timer_id,timeout);  
}

static void report_task_active_timer_expired(void const *argument)
{
    report_task_message_t msg;

    msg.head.id = REPORT_TASK_MSG_ACTIVE_DEVICE;
    log_assert_bool_false(xQueueSend(report_task_msg_q_id,&msg,REPORT_TASK_PUT_MSG_TIMEOUT) == pdPASS);
}

/*轮询配置信息的定时器*/
static void report_task_loop_config_timer_init()
{
    osTimerDef(loop_config_timer,report_task_loop_config_timer_expired);
    loop_config_timer_id = osTimerCreate(osTimer(loop_config_timer),osTimerOnce,NULL);
    log_assert_null_ptr(loop_config_timer_id);
}

static void report_task_start_loop_config_timer(uint32_t timeout)
{
    osTimerStart(loop_config_timer_id,timeout);  
}

static void report_task_loop_config_timer_expired(void const *argument)
{
    report_task_message_t msg;

    msg.head.id = REPORT_TASK_MSG_LOOP_CONFIG;
    log_assert_bool_false(xQueueSend(report_task_msg_q_id,&msg,REPORT_TASK_PUT_MSG_TIMEOUT) == pdPASS);
}


/*日志上报定时器*/
static void report_task_log_timer_init()
{
    osTimerDef(log_timer,report_task_log_timer_expired);
    log_timer_id = osTimerCreate(osTimer(log_timer),osTimerOnce,0);
    log_assert_null_ptr(log_timer_id);
}
static void report_task_start_log_timer(uint32_t timeout)
{
    osTimerStart(log_timer_id,timeout);  
}

static void report_task_log_timer_expired(void const *argument)
{
    report_task_message_t msg;

    msg.head.id = REPORT_TASK_MSG_REPORT_LOG;
    log_assert_bool_false(xQueueSend(report_task_msg_q_id,&msg,REPORT_TASK_PUT_MSG_TIMEOUT) == pdPASS);
}

/*故障上报定时器*/
static void report_task_fault_timer_init()
{
    osTimerDef(fault_timer,report_task_fault_timer_expired);
    fault_timer_id = osTimerCreate(osTimer(fault_timer),osTimerOnce,0);
    log_assert_null_ptr(fault_timer_id);
}
static void report_task_start_fault_timer(uint32_t timeout)
{
    osTimerStart(fault_timer_id,timeout);  
}

static void report_task_fault_timer_expired(void const *argument)
{
    report_task_message_t msg;

    msg.head.id = REPORT_TASK_MSG_REPORT_FAULT;
    log_assert_bool_false(xQueueSend(report_task_msg_q_id,&msg,REPORT_TASK_PUT_MSG_TIMEOUT) == pdPASS);
}

/*更新信息定时器*/
static void report_task_upgrade_timer_init()
{
    osTimerDef(upgrade_timer,report_task_upgrade_timer_expired);
    upgrade_timer_id = osTimerCreate(osTimer(upgrade_timer),osTimerOnce,0);
    log_assert_null_ptr(upgrade_timer_id);
}

static void report_task_start_upgrade_timer(uint32_t timeout)
{
    osTimerStart(upgrade_timer_id,timeout);  
}

static void report_task_upgrade_timer_expired(void const *argument)
{
    report_task_message_t msg;

    msg.head.id = REPORT_TASK_MSG_GET_UPGRADE_INFO;
    log_assert_bool_false(xQueueSend(report_task_msg_q_id,&msg,REPORT_TASK_PUT_MSG_TIMEOUT) == pdPASS);
}

/*下载文件定时器*/
static void report_task_download_timer_init()
{
    osTimerDef(download_timer,report_task_download_timer_expired);
    download_timer_id = osTimerCreate(osTimer(download_timer),osTimerOnce,0);
    log_assert_null_ptr(download_timer_id);
}

static void report_task_start_download_timer(uint32_t timeout)
{
    osTimerStart(download_timer_id,timeout);  
}

static void report_task_download_timer_expired(void const *argument)
{
    report_task_message_t msg;

    msg.head.id = REPORT_TASK_MSG_DOWNLOAD_UPGRADE_FILE;
    log_assert_bool_false(xQueueSend(report_task_msg_q_id,&msg,REPORT_TASK_PUT_MSG_TIMEOUT) == pdPASS);
}



static uint32_t report_task_get_retry_timeout(int retry)
{
    uint32_t timeout;

    switch (retry) {
    case 0:
        timeout = REPORT_TASK_0_RETRY_TIMEOUT;
        break;
    case 1:
        timeout = REPORT_TASK_1_RETRY_TIMEOUT;
        break;
    case 2:
        timeout = REPORT_TASK_2_RETRY_TIMEOUT;
        break;
    case 3:
        timeout = REPORT_TASK_3_RETRY_TIMEOUT;
        break;
    default:
        timeout = REPORT_TASK_DEFAULT_RETRY_TIMEOUT;
        break;
    }

    return timeout;
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
    return osKernelSysTick() / 1000 + report_task_context.utc_time_offset;
}

/** 字节转换成HEX字符串*/
static void dump_hex_string(const char *src,char *dest,uint16_t src_len)
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
    dump_hex_string(md5_hex,md5_str,16);
    /*第2次MD5*/
    md5(md5_str,32,md5_hex);
    dump_hex_string(md5_hex,md5_str,16);
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

/*构造日志数据json*/
static char *report_task_build_log_json_str(device_log_t *log)
{
    char *json_str;
    cJSON *log_json;
    cJSON *base_info_array_json,*base_info_json;

    log_json = cJSON_CreateObject();
    cJSON_AddStringToObject(log_json,"sn",log->sn);
    cJSON_AddNumberToObject(log_json,"ambTemp",(int)log->temperature_env);

#if CONST_DEVICE_TYPE == CONST_DEVICE_TYPE_C
    cJSON_AddNumberToObject(log_json,"refTemp",(int)log->temperature_cold);
#else
    cJSON_AddNumberToObject(log_json,"frzTemp",(int)log->temperature_freeze);
#endif

    cJSON_AddNumberToObject(log_json,"comState",log->compressor_is_pwr_on);
    cJSON_AddNumberToObject(log_json,"runTime",log->compressor_run_time / (1000 * 60));/*单位分钟*/

    /*base info*/
    cJSON_AddItemToObject(log_json,"baseInfo",base_info_array_json = cJSON_CreateArray());

    for (uint8_t i = 0;i < log->base_info.cnt;i ++) {
        cJSON_AddItemToArray(base_info_array_json,base_info_json = cJSON_CreateObject());
        cJSON_AddStringToObject(base_info_json,"lac",log->base_info.base[i].lac);
        cJSON_AddStringToObject(base_info_json,"cid",log->base_info.base[i].cid);
        cJSON_AddStringToObject(base_info_json,"rssi",log->base_info.base[i].rssi);
    }

    json_str = cJSON_PrintUnformatted(log_json);
    cJSON_Delete(log_json);
    return json_str;
 }
 
/*解析日志上报返回的信息数据json*/
static int report_task_parse_log_rsp_json(char *json_str)
{
    int rc = -1;
    cJSON *log_rsp_json;
    cJSON *temp;
  
    log_debug("parse log rsp.\r\n");
    log_rsp_json = cJSON_Parse(json_str);
    if (log_rsp_json == NULL) {
        log_error("rsp is not json.\r\n");
        return -1;  
    }
    /*检查code值 200成功*/
    temp = cJSON_GetObjectItem(log_rsp_json,"code");
    if (!cJSON_IsNumber(temp)) {
        log_error("code is not num.\r\n");
        goto err_exit;  
    }
               
    log_debug("code:%d\r\n", temp->valueint);
    if (temp->valueint != 200) {
        log_error("log rsp err code:%d.\r\n",temp->valueint); 
        goto err_exit;  
    } 
    rc = 0;
  
err_exit:
    cJSON_Delete(log_rsp_json);
    return rc;
}

/*执行日志数据上报*/
 static int report_task_report_log(http_client_context_t *http_client_ctx,device_log_t *log)
 {
    int rc;
    uint32_t timestamp;
    char timestamp_str[14] = { 0 };
    char sign_str[33] = { 0 };
    char *req;
    char rsp[400] = { 0 };
    char url[200] = { 0 };
    /*计算时间戳字符串*/
    timestamp = report_task_get_utc();
    snprintf(timestamp_str,14,"%d",timestamp);
    /*计算sign*/
    report_task_build_sign(sign_str,4,log->key,log->sn,log->source,timestamp_str);
    /*构建新的url*/
    report_task_build_url(url,200,log->url,log->sn,sign_str,log->source,timestamp_str);  
   
    req = report_task_build_log_json_str(log);
 
    http_client_ctx->range_size = 200;
    http_client_ctx->range_start = 0;
    http_client_ctx->rsp_buffer = rsp;
    http_client_ctx->rsp_buffer_size = 400;
    http_client_ctx->url = url;
    http_client_ctx->timeout = 10000;
    http_client_ctx->user_data = (char *)req;
    http_client_ctx->user_data_size = strlen(req);
    http_client_ctx->boundary = BOUNDARY;
    http_client_ctx->is_form_data = false;
    http_client_ctx->content_type = "application/Json";
 
    rc = http_client_post(http_client_ctx);
    cJSON_free(req);
 
    if (rc != 0) {
        log_error("report log err.\r\n");  
        return -1;
    }
    log_debug("log rsp:%s\r\n",http_client_ctx->rsp_buffer);
    rc = report_task_parse_log_rsp_json(http_client_ctx->rsp_buffer);
    if (rc != 0) {
        log_error("json parse log rsp error.\r\n");  
        return -1;
    }
 
    return 0;
 }

/*执行下载*/
static int report_task_do_download(http_client_context_t *http_client_ctx,char *url,uint8_t *buffer,uint32_t start,uint16_t size)
{
    int rc;
    http_client_ctx->range_size = size;
    http_client_ctx->range_start = start;
    http_client_ctx->rsp_buffer = (char *)buffer;
    http_client_ctx->rsp_buffer_size = size;
    http_client_ctx->url = url;
    http_client_ctx->timeout = 20000;
    http_client_ctx->user_data = NULL;
    http_client_ctx->user_data_size = 0;
    http_client_ctx->boundary = BOUNDARY;
    http_client_ctx->is_form_data = false;
    http_client_ctx->content_type = "application/Json"; 
  
    rc = http_client_download(http_client_ctx);
  
    if(rc != 0 ){
        log_error("download bin err.\r\n");
        return -1; 
    }
    if(http_client_ctx->content_size != size){
        log_error("download bin size err.\r\n");
        return -1; 
    }
    log_debug("download bin ok. size:%d.\r\n",size);
    return 0;
}

static uint8_t download_buffer[2050];

/*执行文件下载*/
static int report_task_download_upgrade_file(http_client_context_t *http_client_ctx,device_upgrade_t *upgrade)
{
    int rc,size;

    size = upgrade->bin_size - upgrade->download_size > DEVICE_MIN_ERASE_SIZE ? DEVICE_MIN_ERASE_SIZE : upgrade->bin_size - upgrade->download_size;
    rc = report_task_do_download(http_client_ctx,upgrade->url,download_buffer,upgrade->download_size,size);
    if (rc != 0) {
        log_error("download fail.\r\n");
        return -1;
    }
    rc = flash_if_write(APPLICATION_UPDATE_BASE_ADDR + upgrade->download_size,download_buffer,size);
    if (rc != 0) {
        return -1;
    }
    if (upgrade->bin_size == upgrade->download_size) {
        upgrade->is_download_completion = 1;
    }
    return 0;
}


/*系统复位*/
static void report_task_system_reboot(void)
{

}

/*对比文件并执行升级*/
static int report_task_process_upgrade(device_upgrade_t *upgrade)
{
    /*下载完成 计算md5*/
    char md5_hex[16];
    char md5_str[33];
    
    md5((const char *)(APPLICATION_UPDATE_BASE_ADDR),upgrade->bin_size,md5_hex);
    /*转换成hex字符串*/
    dump_hex_string(md5_hex,md5_str,16);
    /*校验成功，启动升级*/
    if (strcmp(md5_str,upgrade->md5) == 0) {          
        report_task_system_reboot();
    } 
    /*中止本次升级*/
    log_error("md5 err.calculate:%s recv:%s.stop upgrade.\r\n",md5_str,upgrade->md5); 
    return -1;
}

/*解析升级信息回应*/
static int report_task_parse_upgrade_rsp_json_str(char *json_str,device_upgrade_t *device_upgrade)
{
    int rc = -1;
    cJSON *upgrade_rsp_json;
    cJSON *temp,*data,*upgrade;
  
    log_debug("parse upgrade rsp.\r\n");
    upgrade_rsp_json = cJSON_Parse(json_str);
    if (upgrade_rsp_json == NULL) {
        log_error("rsp is not json.\r\n");
        return -1;  
    }
    /*检查code值 200 获取的升级信息*/
    temp = cJSON_GetObjectItem(upgrade_rsp_json,"code");
    if (!cJSON_IsNumber(temp)) {
        log_error("code is not num.\r\n");
        goto err_exit;  
    }
 
    log_debug("code:%d\r\n", temp->valueint);
    /*获取到升级信息*/    
    if (temp->valueint == 200 ) {
        device_upgrade->update = true;
    } else if (temp->valueint == 404){
        device_upgrade->update = false;
        rc = 0;
        goto err_exit; 
    } else { 
        log_error("update rsp err code:%d.\r\n",temp->valueint); 
        goto err_exit; 
    }

    /*检查success值 true or false*/
    temp = cJSON_GetObjectItem(upgrade_rsp_json,"success");
    if (!cJSON_IsBool(temp) || !cJSON_IsTrue(temp)) {
        log_error("success is not bool or is not true.\r\n");
        goto err_exit;  
    }
    log_debug("success:%s\r\n",temp->valueint ? "true" : "false"); 
  
    /*检查data */
    data = cJSON_GetObjectItem(upgrade_rsp_json,"data");
    if (!cJSON_IsObject(data)) {
        log_error("data is not obj.\r\n");
        goto err_exit;  
    }
    /*检查upgrade */ 
    upgrade = cJSON_GetObjectItem(data,"upgrade");
    if (!cJSON_IsObject(upgrade)) {
        log_error("upgrade is not obj .\r\n");
        goto err_exit;  
    }
    /*检查url*/
    temp = cJSON_GetObjectItem(upgrade,"upgradeUrl");
    if (!cJSON_IsString(temp) || temp->valuestring == NULL) {
        log_error("upgradeUrl is not str or is null.\r\n");
        goto err_exit;  
    }
    strcpy(device_upgrade->download_url,temp->valuestring);

    /*检查version code*/
    temp = cJSON_GetObjectItem(upgrade,"majorVersion");
    if (!cJSON_IsNumber(temp)) {
        log_error("majorVersion is not num.\r\n");
        goto err_exit;  
    }
    device_upgrade->version_code = temp->valueint;
  
    /*检查version str*/
    temp = cJSON_GetObjectItem(upgrade,"version");
    if (!cJSON_IsString(temp) || temp->valuestring == NULL) {
        log_error("version is not str or is null.\r\n");
        goto err_exit;  
    }
    strcpy(device_upgrade->version_str,temp->valuestring);
  
    /*检查size*/
    temp = cJSON_GetObjectItem(upgrade,"size");
    if (!cJSON_IsNumber(temp)) {
        log_error("size is not num.\r\n");
        goto err_exit;  
    }
    device_upgrade->bin_size = temp->valueint;
  
    /*检查md5*/
    temp = cJSON_GetObjectItem(upgrade,"md5");
    if (!cJSON_IsString(temp) || temp->valuestring == NULL) {
        log_error("md5 is not str or is null.\r\n");
        goto err_exit;  
    }
    strcpy(device_upgrade->md5,temp->valuestring);
  
    log_debug("upgrade rsp:\r\ndwn_url:%s\r\nver_code:%d.\r\nver_str:%s\r\nmd5:%s.\r\nsize:%d.\r\n",
                device_upgrade->download_url,
                device_upgrade->version_code,
                device_upgrade->version_str,
                device_upgrade->md5,
                device_upgrade->bin_size);
    rc = 0;
  
err_exit:
    cJSON_Delete(upgrade_rsp_json);
    return rc; 
}

/*执行获取升级信息*/  
static int report_task_get_upgrade(http_client_context_t *http_client_ctx,device_upgrade_t *upgrade)
{
    int rc;
    uint32_t timestamp;
    char timestamp_str[14] = { 0 };
    char sign_str[33] = { 0 };
    char rsp[400] = { 0 };
    char url[200] = { 0 };
    /*计算时间戳字符串*/
    timestamp = report_task_get_utc();
    snprintf(timestamp_str,14,"%d",timestamp);
    /*计算sign*/
    report_task_build_sign(sign_str,4,upgrade->key,upgrade->sn,upgrade->source,timestamp_str);
    /*构建新的url*/
    report_task_build_url(url,200,upgrade->url,upgrade->sn,sign_str,upgrade->source,timestamp_str);  
 
    http_client_ctx->range_size = 200;
    http_client_ctx->range_start = 0;
    http_client_ctx->rsp_buffer = rsp;
    http_client_ctx->rsp_buffer_size = 400;
    http_client_ctx->url = url;
    http_client_ctx->timeout = 10000;
    http_client_ctx->user_data = NULL;
    http_client_ctx->user_data_size = 0;
    http_client_ctx->boundary = BOUNDARY;
    http_client_ctx->is_form_data = false;
    http_client_ctx->content_type = "application/Json";
 
    rc = http_client_get(http_client_ctx);
 
    if (rc != 0) {
        log_error("get upgrade err.\r\n");  
        return -1;
    }
    log_debug("upgrade rsp:%s\r\n",http_client_ctx->rsp_buffer);
    rc = report_task_parse_upgrade_rsp_json_str(http_client_ctx->rsp_buffer,upgrade);
    if (rc != 0) {
        log_error("json parse upgrade rsp error.\r\n");  
        return -1;
    }
    log_debug("get upgrade ok.\r\n");  
    return 0;
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
    log_info("insert fault.code:%d.\r\n",fault->code);
    return 0;    
}

/*从故障参数队列取出一个故障参数*/         
static int report_task_peek_fault(device_fault_queue_t *queue,device_fault_information_t *fault)
{     
    if (queue->read >= queue->write) {
        log_warning("fault queue is null.\r\n");
        return 0;
    }
       
    *fault = queue->imformation[queue->read & (REPORT_TASK_FAULT_QUEUE_SIZE - 1)];   
    return 1;    
}
/*从故障参数队列删除一个故障参数*/         
static int report_task_delete_fault(device_fault_queue_t *queue)
{
    if (queue->read < queue->write) {
        queue->read++; 
    }
    log_info("delete one fault.\r\n");
    return 0;
}
 
#if 0
/*构造故障信息*/
static void report_task_build_fault(device_fault_information_t *fault,char *code,char *msg,const uint32_t time,uint8_t status)
{
    char time_str[14];
      
    snprintf(time_str,14,"%d",time);
    strcpy(fault->code,code); 
    strcpy(fault->msg,msg); 
    strcpy(fault->time,time_str);
    fault->status = status;
}


/*构建form-data*/
static void report_task_build_form_data(char *form_data,const int size,const char *boundary,const int cnt,...)
{
    int put_size;
    int temp_size;  
    va_list ap;
    form_data_t *temp;

    put_size = 0;
    va_start(ap, cnt);
 
    /*组合输入数据*/  
    for (uint8_t i=0; i < cnt; i++) {
        temp = va_arg(ap,form_data_t*);
        /*添加boundary 和 name 和 value*/
        snprintf(form_data + put_size ,size - put_size,"--%s\r\nContent-Disposition: form-data; name=%s\r\n\r\n%s\r\n",boundary,temp->name,temp->value);
        temp_size = strlen(form_data);
        /*保证数据完整*/
        log_assert_bool_false(temp_size < size - 1);
        put_size = strlen(form_data);
    }
    /*添加结束标志*/
    snprintf(form_data + put_size,size - put_size,"--%s--\r\n",boundary);
    put_size = strlen(form_data);
    log_assert_bool_false(put_size < size - 1);
}

/*构造故障上报参数*/
static void report_task_build_fault_form_data_str(char *form_data,const uint16_t size,device_fault_information_t *fault,char *boundary)
{
    form_data_t err_code;
    form_data_t err_msg;
    form_data_t err_time;

    err_code.name = "errorCode";
    err_code.value = fault->code;
 
    err_msg.name = "errorMsg";
    err_msg.value = fault->msg;
 
    err_time.name = "errorTime";
    err_time.value = fault->time;
 
    report_task_build_form_data(form_data,size,BOUNDARY,3,&err_code,&err_msg,&err_time);
 }
#endif


static char *report_task_build_fault_json_str(char *sn,device_fault_information_t *fault)
{
 /*构造故障信息数据json*/
    char *json_str;
    cJSON *fault_json;

    fault_json = cJSON_CreateObject();
    cJSON_AddStringToObject(fault_json,"sn",sn);
    cJSON_AddNumberToObject(fault_json,"faultCode",fault->code);

    json_str = cJSON_PrintUnformatted(fault_json);
    cJSON_Delete(fault_json);
    return json_str;
}

/*解析故障上报返回的信息数据json*/
static int report_task_parse_fault_rsp_json(char *json_str)
{
    int rc = -1;
    cJSON *fault_rsp_json;
    cJSON *temp;
  
    log_debug("parse fault rsp.\r\n");
    fault_rsp_json = cJSON_Parse(json_str);
    if (fault_rsp_json == NULL) {
        log_error("rsp is not json.\r\n");
        return -1;  
    }
    /*检查code值 200成功*/
    temp = cJSON_GetObjectItem(fault_rsp_json,"code");
    if (!cJSON_IsNumber(temp)) {
        log_error("code is not num.\r\n");
        goto err_exit;  
    }
               
    log_debug("code:%d\r\n", temp->valueint);
    if (temp->valueint != 200 && temp->valueint != 30010) {
        log_error("fault rsp err code:%d.\r\n",temp->valueint); 
        goto err_exit;  
    } 
    rc = 0;
  
err_exit:
    cJSON_Delete(fault_rsp_json);
    return rc;
}

/*执行故障信息数据上报*/
static int report_task_do_report_fault(http_client_context_t *http_client_ctx,char *url_origin,char *key,char *sn,char *src,device_fault_information_t *fault)
{
    int rc;
    uint32_t timestamp;
    char timestamp_str[14] = { 0 };
    char sign_str[33] = { 0 };
    char *req;
    char rsp[400] = { 0 };
    char url[200] = { 0 };

    /*计算时间戳字符串*/
    timestamp = report_task_get_utc();
    snprintf(timestamp_str,14,"%d",timestamp);

    /*计算时间戳字符串*/
    timestamp = report_task_get_utc();
    snprintf(timestamp_str,14,"%d",timestamp);
    /*计算sign*/
    report_task_build_sign(sign_str,4,key,sn,src,timestamp_str);
    /*构建新的url*/
    report_task_build_url(url,200,url_origin,sn,sign_str,src,timestamp_str);  
    req = report_task_build_fault_json_str(sn,fault);

 
    http_client_ctx->range_size = 200;
    http_client_ctx->range_start = 0;
    http_client_ctx->rsp_buffer = rsp;
    http_client_ctx->rsp_buffer_size = 400;
    http_client_ctx->url = url;
    http_client_ctx->timeout = 20000;
    http_client_ctx->user_data = (char *)req;
    http_client_ctx->user_data_size = strlen(req);
    http_client_ctx->content_type = "application/Json";
 
    rc = http_client_post(http_client_ctx);
 
    if (rc != 0) {
        log_error("report fault err.\r\n");  
        return -1;
    }
    log_debug("fault rsp:%s\r\n",http_client_ctx->rsp_buffer);
    rc = report_task_parse_fault_rsp_json(http_client_ctx->rsp_buffer);
    if (rc != 0) {
        log_error("json parse fault rsp error.\r\n");  
        return -1;
    }
    log_debug("report fault ok.\r\n");  
    return 0;
}


/*上报故障或者上报故障解除*/
static int report_task_report_fault(http_client_context_t *http_client_ctx,device_fault_t *fault)
{
    int rc;
    device_fault_information_t fault_info;

    /*取出故障信息*/
    rc = report_task_peek_fault(&fault->queue,&fault_info);
    /*存在未上报的故障*/
    if (rc == 1) {
        if (fault_info.status == HAL_FAULT_STATUS_FAULT) {
            rc = report_task_do_report_fault(http_client_ctx,fault->url_spawn,fault->key,fault->sn,fault->source,&fault_info);
        } else {
            rc = report_task_do_report_fault(http_client_ctx,fault->url_clear,fault->key,fault->sn,fault->source,&fault_info);
        }
        if (rc != 0) {
            log_error("report task report fault fail.\r\n");
            return -1;/*存在故障，并且上报失败*/
        } else {
            report_task_delete_fault(&fault->queue);
            log_info("report task report fault ok.\r\n");
            return 1;/*存在故障，并且上报成功*/
        }
    }

    return 0;/*不存在故障*/
}

 /*构造轮询配置信息数据json*/
 static char *report_task_build_loop_config_json_str(device_loop_config_t *loop_config)
 {
    char *json_str;
    cJSON *loop_config_json;

    loop_config_json = cJSON_CreateObject();
    cJSON_AddStringToObject(loop_config_json,"sn",loop_config->sn);
 
    json_str = cJSON_PrintUnformatted(loop_config_json);
    cJSON_Delete(loop_config_json);
    return json_str;
 }



/*解析轮询配置返回的信息数据json*/
static int report_task_parse_loop_config_rsp_json_str(char *json_str ,device_config_t *config)
{
    int rc = -1;
    cJSON *loop_cfg_rsp_json;
    cJSON *loop_cfg_data_json;
    cJSON *loop_cfg_run_cfg_json;
    cJSON *temp;
  
    log_debug("parse loop config rsp.\r\n");
    loop_cfg_rsp_json = cJSON_Parse(json_str);
    if (loop_cfg_rsp_json == NULL) {
        log_error("rsp is not json.\r\n");
        return -1;  
    }
    /*检查code值 200成功*/
    temp = cJSON_GetObjectItem(loop_cfg_rsp_json,"code");
    if (!cJSON_IsNumber(temp)) {
        log_error("code is not num.\r\n");
        goto err_exit;  
    }
               
    log_debug("code:%d\r\n", temp->valueint);
    if (temp->valueint != 200 ) {
        log_error("active rsp err code:%d.\r\n",temp->valueint); 
        goto err_exit;  
    }  
    /*检查data*/
    loop_cfg_data_json = cJSON_GetObjectItem(loop_cfg_rsp_json,"data");
    if (!cJSON_IsObject(loop_cfg_data_json)) {
        log_error("data is null.\r\n");
        rc = 0;
        goto err_exit;  
    }
     /*检查runConfig*/
    loop_cfg_run_cfg_json = cJSON_GetObjectItem(loop_cfg_data_json,"runConfig");
    if (!cJSON_IsObject(loop_cfg_run_cfg_json)) {
        log_error("runConfig is null.\r\n");
        rc = 0;
        goto err_exit;  
    }
    /*检查safeTempMin*/
    temp = cJSON_GetObjectItem(loop_cfg_run_cfg_json,"safeTempMin");
    if (!cJSON_IsNumber(temp)) {
        log_error("safeTempMin is not num.\r\n");
        goto err_exit;  
    }

    if (temp->valuedouble > DEFAULT_COMPRESSOR_LOW_TEMPERATURE_LIMIT && temp->valuedouble < DEFAULT_COMPRESSOR_HIGH_TEMPERATURE_LIMIT) {
        config->temperature_cold_min = temp->valuedouble;
    }
    /*检查safeTempMax*/
    temp = cJSON_GetObjectItem(loop_cfg_run_cfg_json,"safeTempMax");
    if (!cJSON_IsNumber(temp)) {
        log_error("safeTempMax is not num.\r\n");
        goto err_exit;  
    }
    if (temp->valuedouble > DEFAULT_COMPRESSOR_LOW_TEMPERATURE_LIMIT && temp->valuedouble < DEFAULT_COMPRESSOR_HIGH_TEMPERATURE_LIMIT) {
        config->temperature_cold_max = temp->valuedouble;
    }
    /*检查freezingTempMin*/
    temp = cJSON_GetObjectItem(loop_cfg_run_cfg_json,"freezingTempMin");
    if (!cJSON_IsNumber(temp)) {
        log_error("freezingTempMin is not num.\r\n");
        goto err_exit;  
    }
    if (temp->valuedouble > DEFAULT_COMPRESSOR_LOW_TEMPERATURE_LIMIT && temp->valuedouble < DEFAULT_COMPRESSOR_HIGH_TEMPERATURE_LIMIT) {
        config->temperature_freeze_min = temp->valuedouble;
    }

    /*检查freezingTempMin*/
    temp = cJSON_GetObjectItem(loop_cfg_run_cfg_json,"freezingTempMax");
    if (!cJSON_IsNumber(temp)) {
        log_error("freezingTempMax is not num.\r\n");
        goto err_exit;  
    }
    if (temp->valuedouble > DEFAULT_COMPRESSOR_LOW_TEMPERATURE_LIMIT && temp->valuedouble < DEFAULT_COMPRESSOR_HIGH_TEMPERATURE_LIMIT) {
        config->temperature_freeze_max = temp->valuedouble;
    }
  
    /*检查powerState*/
    temp = cJSON_GetObjectItem(loop_cfg_run_cfg_json,"powerState");
    if (!cJSON_IsNumber(temp)) {
        log_error("powerState is not num.\r\n");
        goto err_exit;  
    }
    config->is_lock = temp->valueint == 0 ? 1 : 0;
    /*检查日志间隔*/
    temp = cJSON_GetObjectItem(loop_cfg_run_cfg_json,"logInterval");
    if (!cJSON_IsNumber(temp)) {
        log_error("logInterval is not num.\r\n");
        goto err_exit;  
    }
    config->log_interval = temp->valueint * 60 * 1000;/*转换成ms*/
    /*检查轮询配置间隔*/
    temp = cJSON_GetObjectItem(loop_cfg_run_cfg_json,"loopConfInterval");
    if (!cJSON_IsNumber(temp)) {
        log_error("logInterval is not num.\r\n");
        goto err_exit;  
    }
    config->loop_interval = temp->valueint * 60 * 1000;/*转换成ms*/
    rc = 0;
    log_info("loop config rsp [lock:%d t_cold:%.2f ~ %.2f t_freeze:%.2f ~ %.2f log:%d loop:%d\r\n",
             config->is_lock,config->temperature_cold_min,config->temperature_cold_max,
             config->temperature_freeze_min,config->temperature_freeze_max,config->log_interval,
             config->loop_interval);
err_exit:
    cJSON_Delete(loop_cfg_rsp_json);
    return rc;
}

/*执行轮询配置信息*/  
static int report_task_loop_config(http_client_context_t *http_client_ctx,device_loop_config_t *loop_config,device_config_t *config)
{
    int rc;
  
    char rsp[400] = { 0 };
    char url[200] = { 0 };

    /*构建新的url*/
    snprintf(url,200,"%s%s%s",loop_config->url,"/",loop_config->sn);  
   
    http_client_ctx->range_size = 200;
    http_client_ctx->range_start = 0;
    http_client_ctx->rsp_buffer = rsp;
    http_client_ctx->rsp_buffer_size = 400;
    http_client_ctx->url = url;
    http_client_ctx->timeout = 10000;
    http_client_ctx->user_data = NULL;
    http_client_ctx->user_data_size = 0;
    http_client_ctx->boundary = BOUNDARY;
    http_client_ctx->is_form_data = false;
    http_client_ctx->content_type = "application/Json";
 
    rc = http_client_get(http_client_ctx);
 
    if (rc != 0) {
        log_error("loop config err.\r\n");  
        return -1;
    }
    log_debug("loop config rsp:%s\r\n",http_client_ctx->rsp_buffer);
    rc = report_task_parse_loop_config_rsp_json_str(http_client_ctx->rsp_buffer,config);
    if (rc != 0) {
        log_error("json parse loop config rsp error.\r\n");  
        return -1;
    }
    log_debug("report loop config ok.\r\n");  
    return 0;
 }



 /*构造激活信息数据json*/
 static char *report_task_build_active_json_str(device_active_t *active)
 {
    char *json_str;
    cJSON *active_json;
    //cJSON *base_info_array_json;
    //cJSON *base_info_json;

    active_json = cJSON_CreateObject();
    cJSON_AddStringToObject(active_json,"sn",active->sn);
    cJSON_AddStringToObject(active_json,"deviceModel",active->device_model);
    cJSON_AddStringToObject(active_json,"softVersion",active->fw_version);
    cJSON_AddNumberToObject(active_json,"softCode",active->fw_code);
    cJSON_AddStringToObject(active_json,"deviceType",active->device_type);
    cJSON_AddStringToObject(active_json,"optCode",active->opt_code);
    cJSON_AddStringToObject(active_json,"simId",active->sim_id);
    cJSON_AddStringToObject(active_json,"vendor",active->vendor);

    /*base info数组*/
    /*
    cJSON_AddItemToObject(active_json,"baseInfo",base_info_array_json = cJSON_CreateArray());

    for (uint8_t i = 0;i < active->base_info.cnt;i ++) {
        cJSON_AddItemToArray(base_info_array_json,base_info_json = cJSON_CreateObject());
        cJSON_AddStringToObject(base_info_json,"lac",active->base_info.base[i].lac);
        cJSON_AddStringToObject(base_info_json,"cid",active->base_info.base[i].cid);
        cJSON_AddStringToObject(base_info_json,"rssi",active->base_info.base[i].rssi);
    }
    */
    json_str = cJSON_PrintUnformatted(active_json);
    cJSON_Delete(active_json);
    return json_str;
 }



/*解析激活返回的信息数据json*/
static int report_task_parse_active_rsp_json_str(char *json_str ,device_config_t *config)
{
    int rc = -1;
    cJSON *active_rsp_json;
    cJSON *active_data_json;
    cJSON *active_run_config_json;
    cJSON *temp;
  
    log_debug("parse active rsp.\r\n");
    active_rsp_json = cJSON_Parse(json_str);
    if (active_rsp_json == NULL) {
        log_error("rsp is not json.\r\n");
        return -1;  
    }
    /*检查code值 200成功*/
    temp = cJSON_GetObjectItem(active_rsp_json,"code");
    if (!cJSON_IsNumber(temp)) {
        log_error("code is not num.\r\n");
        goto err_exit;  
    }
               
    log_debug("code:%d\r\n", temp->valueint);
    if (temp->valueint != 200 ) {
        log_error("active rsp err code:%d.\r\n",temp->valueint); 
        goto err_exit;  
    }  
     /*检查data*/
    active_data_json = cJSON_GetObjectItem(active_rsp_json,"data");
    if (!cJSON_IsObject(active_data_json)) {
        log_error("data is null.\r\n");
        rc = 0;
        goto err_exit;  
    }
     /*检查runConfig*/
    active_run_config_json = cJSON_GetObjectItem(active_data_json,"runConfig");
    if (!cJSON_IsObject(active_run_config_json)) {
        log_error("runConfig is null.\r\n");
        rc = 0;
        goto err_exit;  
    }
    /*检查safeTempMin*/
    temp = cJSON_GetObjectItem(active_run_config_json,"safeTempMin");
    if (!cJSON_IsNumber(temp)) {
        log_error("safeTempMin is not num.\r\n");
        goto err_exit;  
    }

    if (temp->valuedouble > DEFAULT_COMPRESSOR_LOW_TEMPERATURE_LIMIT && temp->valuedouble < DEFAULT_COMPRESSOR_HIGH_TEMPERATURE_LIMIT) {
        config->temperature_cold_min = temp->valuedouble;
    }
    /*检查safeTempMax*/
    temp = cJSON_GetObjectItem(active_run_config_json,"safeTempMax");
    if (!cJSON_IsNumber(temp)) {
        log_error("safeTempMax is not num.\r\n");
        goto err_exit;  
    }
    if (temp->valuedouble > DEFAULT_COMPRESSOR_LOW_TEMPERATURE_LIMIT && temp->valuedouble < DEFAULT_COMPRESSOR_HIGH_TEMPERATURE_LIMIT) {
        config->temperature_cold_max = temp->valuedouble;
    }
    /*检查freezingTempMin*/
    temp = cJSON_GetObjectItem(active_run_config_json,"freezingTempMin");
    if (!cJSON_IsNumber(temp)) {
        log_error("freezingTempMin is not num.\r\n");
        goto err_exit;  
    }
    if (temp->valuedouble > DEFAULT_COMPRESSOR_LOW_TEMPERATURE_LIMIT && temp->valuedouble < DEFAULT_COMPRESSOR_HIGH_TEMPERATURE_LIMIT) {
        config->temperature_freeze_min = temp->valuedouble;
    }

    /*检查freezingTempMin*/
    temp = cJSON_GetObjectItem(active_run_config_json,"freezingTempMax");
    if (!cJSON_IsNumber(temp)) {
        log_error("freezingTempMax is not num.\r\n");
        goto err_exit;  
    }
    if (temp->valuedouble > DEFAULT_COMPRESSOR_LOW_TEMPERATURE_LIMIT && temp->valuedouble < DEFAULT_COMPRESSOR_HIGH_TEMPERATURE_LIMIT) {
        config->temperature_freeze_max = temp->valuedouble;
    }
  
    /*检查powerState*/
    temp = cJSON_GetObjectItem(active_run_config_json,"powerState");
    if (!cJSON_IsNumber(temp)) {
        log_error("powerState is not num.\r\n");
        goto err_exit;  
    }
    config->is_lock = temp->valueint == 0 ? 1 : 0;
    /*检查日志间隔*/
    temp = cJSON_GetObjectItem(active_run_config_json,"logInterval");
    if (!cJSON_IsNumber(temp)) {
        log_error("logInterval is not num.\r\n");
        goto err_exit;  
    }
    config->log_interval = temp->valueint * 60 * 1000;/*转换成ms*/
    /*检查轮询配置间隔*/
    temp = cJSON_GetObjectItem(active_run_config_json,"loopConfInterval");
    if (!cJSON_IsNumber(temp)) {
        log_error("logInterval is not num.\r\n");
        goto err_exit;  
    }
    config->loop_interval = temp->valueint * 60 * 1000;/*转换成ms*/
    rc = 0;
    log_info("active rsp lock:%d t_cold:%.2f ~ %.2f t_freeze:%.2f ~ %.2f log:%d loop:%d\r\n",
             config->is_lock,config->temperature_cold_min,config->temperature_cold_max,
             config->temperature_freeze_min,config->temperature_freeze_max,config->log_interval,
             config->loop_interval);
  
err_exit:
    cJSON_Delete(active_rsp_json);
    return rc;
}



/*执行激活信息数据上报*/  
static int report_task_active_device(http_client_context_t *http_client_ctx,device_active_t *active,device_config_t *config)
{
    int rc;
    uint32_t timestamp;
    char timestamp_str[14] = { 0 };
    char sign_str[33] = { 0 };
    char *req;
    char rsp[400] = { 0 };
    char url[200] = { 0 };

    /*计算时间戳字符串*/
    timestamp = report_task_get_utc();
    snprintf(timestamp_str,14,"%d",timestamp);
    /*计算sign*/
    report_task_build_sign(sign_str,4,active->key,active->sn,active->source,timestamp_str);
    /*构建新的url*/
    report_task_build_url(url,200,active->url,active->sn,sign_str,active->source,timestamp_str);  
   
    req = report_task_build_active_json_str(active);
 
    http_client_ctx->range_size = 200;
    http_client_ctx->range_start = 0;
    http_client_ctx->rsp_buffer = rsp;
    http_client_ctx->rsp_buffer_size = 400;
    http_client_ctx->url = url;
    http_client_ctx->timeout = 10000;
    http_client_ctx->user_data = (char *)req;
    http_client_ctx->user_data_size = strlen(req);
    http_client_ctx->boundary = BOUNDARY;
    http_client_ctx->is_form_data = false;
    http_client_ctx->content_type = "application/Json";
 
    rc = http_client_post(http_client_ctx);
    cJSON_free(req);
 
    if (rc != 0) {
        log_error("report active err.\r\n");  
        return -1;
    }
    log_debug("active rsp:%s\r\n",http_client_ctx->rsp_buffer);
    rc = report_task_parse_active_rsp_json_str(http_client_ctx->rsp_buffer,config);
    if (rc != 0) {
        log_error("json parse active rsp error.\r\n");  
        return -1;
    }
    log_debug("report active ok.\r\n");  
    return 0;
 }


/*默认参数读取*/
static void report_task_device_config_init(device_config_t *config)
{
    char *temp;

    /*默认配置*/
    config->is_lock = 0;
    config->log_interval = DEFAULT_REPORT_LOG_INTERVAL;
    config->loop_interval = DEFAULT_REPORT_LOOP_CONFIG_INTERVAL;
    config->temperature_cold_min = DEFAULT_COMPRESSOR_TEMPERATURE_STOP;
    config->temperature_cold_max = DEFAULT_COMPRESSOR_TEMPERATURE_START;
    config->temperature_freeze_min = DEFAULT_COMPRESSOR_TEMPERATURE_STOP;
    config->temperature_freeze_max = DEFAULT_COMPRESSOR_TEMPERATURE_START;

    temp = device_env_get(ENV_NAME_TEMPERATURE_COLD_MIN);
    if (temp) {
        config->temperature_cold_min = atoi(temp);
    }

    temp = device_env_get(ENV_NAME_TEMPERATURE_COLD_MAX);
    if (temp) {
        config->temperature_cold_max = atoi(temp);
    }

    temp = device_env_get(ENV_NAME_TEMPERATURE_FREEZE_MIN);
    if (temp) {
        config->temperature_freeze_min = atoi(temp);
    }

    temp = device_env_get(ENV_NAME_TEMPERATURE_FREEZE_MAX);
    if (temp) {
        config->temperature_freeze_max = atoi(temp);
    }

    temp = device_env_get(ENV_NAME_COMPRESSOR_CTRL);
    if (temp) {
        config->is_lock = atoi(temp);
    }
    temp = device_env_get(ENV_NAME_LOG_INTERVAL);
    if (temp) {
        config->log_interval = atoi(temp);
    }

}

/*配置参数保存*/
void report_task_save_device_config(device_config_t *new_config,device_config_t *default_config)
{
    char temp[14];

    if (new_config->temperature_cold_min != default_config->temperature_cold_min) {
        snprintf(temp,14,"%.1f",new_config->temperature_cold_min);
        device_env_set(ENV_NAME_TEMPERATURE_COLD_MIN,temp);
        new_config->is_new = 1;
    }
    if (new_config->temperature_cold_max != default_config->temperature_cold_max) {
        snprintf(temp,14,"%.1f",new_config->temperature_cold_max);
        device_env_set(ENV_NAME_TEMPERATURE_COLD_MAX,temp);
        new_config->is_new = 1;
    }
    if (new_config->temperature_freeze_min != new_config->temperature_freeze_min) {
        snprintf(temp,14,"%.1f",new_config->temperature_freeze_min);
        device_env_set(ENV_NAME_TEMPERATURE_FREEZE_MIN,temp);
        new_config->is_new = 1;
    }
    if (new_config->temperature_freeze_max != new_config->temperature_freeze_max) {
        snprintf(temp,14,"%.1f",new_config->temperature_freeze_max);
        device_env_set(ENV_NAME_TEMPERATURE_FREEZE_MAX,temp);
        new_config->is_new = 1;
    }
    if (new_config->is_lock != default_config->is_lock) {
        snprintf(temp,14,"%d",new_config->is_lock);
        device_env_set(ENV_NAME_COMPRESSOR_CTRL,temp);
        new_config->is_new = 1;
    }
    
}

/*向对应任务分发工作参数*/
static int report_task_dispatch_device_config(device_config_t *config)
{
   
    compressor_task_message_t compressor_msg;;

    if (config->is_lock == 1) {
        compressor_msg.head.id = COMPRESSOR_TASK_MSG_PWR_ON_DISABLE;
    } else {
        compressor_msg.head.id = COMPRESSOR_TASK_MSG_PWR_ON_ENABLE;
    }
    log_assert_bool_false(xQueueSend(compressor_task_msg_q_id,&compressor_msg,REPORT_TASK_PUT_MSG_TIMEOUT) == pdPASS);

#if CONST_DEVICE_TYPE == CONST_DEVICE_TYPE_C
    /*分发冷藏温度*/
    compressor_msg.head.id = COMPRESSOR_TASK_MSG_TYPE_TEMPERATURE_SETTING;
    compressor_msg.content.temperature_setting_min = config->temperature_cold_min;
    compressor_msg.content.temperature_setting_max = config->temperature_cold_max;
#else 
    /*分发冷冻温度*/
    compressor_msg.head.id = COMPRESSOR_TASK_MSG_TYPE_TEMPERATURE_SETTING;
    compressor_msg.content.temperature_setting_min = config->temperature_freeze_min;
    compressor_msg.content.temperature_setting_max = config->temperature_freeze_max;
#endif
    log_assert_bool_false(xQueueSend(compressor_task_msg_q_id,&compressor_msg,REPORT_TASK_PUT_MSG_TIMEOUT) == pdPASS);
    return 0;
}

static void report_task_get_sn(char *sn)
{
    flash_if_read(SN_ADDR,(uint8_t *)sn,SN_LEN);
    sn[SN_LEN] = 0;
}


static void report_task_init()
{
    /*激活*/
    report_task_context.active.device_model = MODEL;
    report_task_context.active.device_type = TYPE;
    report_task_get_sn(report_task_context.active.sn);
    report_task_context.active.key = KEY;
    report_task_context.active.source = SOURCE;
    report_task_context.active.url = URL_ACTIVE;
    report_task_context.active.vendor = VENDOR;
    report_task_context.active.opt_code = OPT_CODE_CHINA_MOBILE;
    report_task_context.active.fw_version = FIRMWARE_VERSION_STR;
    report_task_context.active.fw_code = FIRMWARE_VERSION_HEX;
    /*轮询配置*/
    report_task_context.loop_config.url = URL_LOOP_CONFIG;
    report_task_context.loop_config.key = KEY;
    report_task_context.loop_config.source = SOURCE;
    report_task_get_sn(report_task_context.loop_config.sn);
    /*日志*/
    report_task_context.log.key = KEY;
    report_task_get_sn(report_task_context.log.sn);
    report_task_context.log.source = SOURCE;
    report_task_context.log.url = URL_LOG;
    /*升级*/
    report_task_context.upgrade.key = KEY;
    report_task_get_sn(report_task_context.upgrade.sn);
    report_task_context.upgrade.source = SOURCE;
    report_task_context.upgrade.url = URL_UPGRADE;
    /*故障*/
    report_task_context.fault.key = KEY;
    report_task_get_sn(report_task_context.fault.sn);
    report_task_context.fault.source = SOURCE;
    report_task_context.fault.url_spawn = URL_FAULT;
    report_task_context.fault.url_clear = URL_FAULT_DELETE;

}

/*上报任务*/
void report_task(void const *argument)
{
    int rc;
    int retry;
    device_config_t default_config;

    report_task_message_t msg_recv;
    report_task_message_t msg_temp;

    /*定时器初始化*/
    report_task_utc_timer_init();
    report_task_active_timer_init();
    report_task_log_timer_init();
    report_task_fault_timer_init();
    report_task_upgrade_timer_init();
    report_task_download_timer_init();
    report_task_loop_config_timer_init();

    report_task_init();
    device_env_init();
    report_task_device_config_init(&default_config);

    /*分发默认的开机配置参数*/
    report_task_dispatch_device_config(&default_config);
    /*复制默认配置*/
    report_task_context.config = default_config;

    while (1)
    {
    if (xQueueReceive(report_task_msg_q_id, &msg_recv,0xFFFFFFFF) == pdTRUE) {
        /*处理SIM ID消息*/
        if (msg_recv.head.id == REPORT_TASK_MSG_SIM_ID) {
            strcpy(report_task_context.active.sim_id,msg_recv.content.sim_id);
            /*开始同步时间*/
            if (report_task_context.is_start_sync_utc == 0) {
                report_task_start_utc_timer(0); 
                report_task_context.is_start_sync_utc = 1;
            }
        }
    
        /*处理位置消息*/
        if (msg_recv.head.id == REPORT_TASK_MSG_BASE_INFO) {
            report_task_context.log.base_info = msg_recv.content.base_info;
        }

        /*处理同步UTC消息*/
        if (msg_recv.head.id == REPORT_TASK_MSG_SYNC_UTC) { 
            rc = report_task_sync_utc(&report_task_context.utc_time_offset);
            if (rc != 0) {
                retry ++;
                log_error("report task sync utc fail.\r\n");
                report_task_start_utc_timer(report_task_get_retry_timeout(retry)); 
            } else {
                retry = 0;
                log_info("report task sync utc ok.\r\n");
                report_task_start_utc_timer(REPORT_TASK_SYNC_UTC_INTERVAL); 
                /*开始设备激活*/
                report_task_context.active.base_info = report_task_context.log.base_info;
                msg_temp.head.id = REPORT_TASK_MSG_ACTIVE_DEVICE;
                log_assert_bool_false(xQueueSend(report_task_msg_q_id,&msg_temp,REPORT_TASK_PUT_MSG_TIMEOUT) == pdPASS);
            }
        }

        /*设备激活消息*/
        if (msg_recv.head.id == REPORT_TASK_MSG_ACTIVE_DEVICE) { 
            rc = report_task_active_device(&report_task_context.http_client_context,&report_task_context.active,&report_task_context.config);
            if (rc != 0) {
                retry ++;
                log_error("report task active fail.\r\n" );
                report_task_start_active_timer(report_task_get_retry_timeout(retry)); 
            } else {
                retry = 0;
                report_task_context.is_device_active = 1;
                log_info("device active ok.\r\n");

                /*保存新激活的参数*/
                report_task_save_device_config(&report_task_context.config,&default_config);
                /*更新默认配置*/
                default_config = report_task_context.config;
                /*更新日志压缩机状态*/
                report_task_context.log.compressor_is_pwr_on = report_task_context.config.is_lock == 1 ? 0 : 1;

                /*分发激活后的配置参数*/
                report_task_dispatch_device_config(&report_task_context.config);

                /*打开启日志上报定时器*/
                report_task_start_log_timer(report_task_context.config.log_interval);
                /*打开故障上报定时器*/
                report_task_start_fault_timer(0);
                /*激活后获取升级信息*/
                //report_task_start_upgrade_timer(0); 
                /*激活后获取准备获取配置信息*/
                report_task_start_loop_config_timer(report_task_context.config.loop_interval);
                /*告知mqtt任务网络就绪*/
                mqtt_task_msg_t mqtt_msg;
                mqtt_msg.head.id = MQTT_TASK_MSG_NET_READY;
                log_assert_bool_false(xQueueSend(mqtt_task_msg_q_id,&mqtt_msg,5) == pdPASS);
            }
        }

        /*设备轮询配置消息*/
        if (msg_recv.head.id == REPORT_TASK_MSG_LOOP_CONFIG) { 
            rc = report_task_loop_config(&report_task_context.http_client_context,&report_task_context.loop_config,&report_task_context.config);
            if (rc != 0) {
                retry ++;
                log_error("report task loop config fail.\r\n" );
                report_task_start_loop_config_timer(report_task_get_retry_timeout(retry)); 
            } else {
                retry = 0;
                log_info("device loop config ok.\r\n");
                /*分发激活后的配置参数*/
                report_task_dispatch_device_config(&report_task_context.config);
                /*保存新激活的参数*/
                report_task_save_device_config(&report_task_context.config,&default_config);
                /*更新默认配置*/
                default_config = report_task_context.config;
                /*更新日志压缩机状态*/
                report_task_context.log.compressor_is_pwr_on = report_task_context.config.is_lock == 1 ? 0 : 1;

                /*再次获取准备获取配置信息*/
                report_task_start_loop_config_timer(report_task_context.config.loop_interval);
            }
        }


        /*获取更新信息消息*/
        if (msg_recv.head.id == REPORT_TASK_MSG_GET_UPGRADE_INFO) { 
            rc = report_task_get_upgrade(&report_task_context.http_client_context,&report_task_context.upgrade);
            if (rc != 0) {
                retry ++;
                log_error("report task get upgrade fail.\r\n");
                report_task_start_active_timer(report_task_get_retry_timeout(retry)); 
            } else {
            /*获取成功处理*/   
                retry = 0;
                /*对比现在的版本号*/
                if (report_task_context.upgrade.update == 1 && report_task_context.upgrade.version_code > report_task_context.fw_version_code) {
                    log_info("firmware need upgrade.ver_code:%d size:%d md5:%s.start download...\r\n",
                    report_task_context.upgrade.version_code,report_task_context.upgrade.bin_size,report_task_context.upgrade.md5);
                    /*下载新文件*/
                    report_task_start_download_timer(0);                 
                } else {
                    log_info("firmware is latest.\r\n");       
                }             
            }
        }
    
    
        /*下载更新文件*/
        if (msg_recv.head.id == REPORT_TASK_MSG_DOWNLOAD_UPGRADE_FILE) { 
            rc = report_task_download_upgrade_file(&report_task_context.http_client_context,&report_task_context.upgrade);
            if(rc != 0){
                retry ++;
                log_error("report task download upgrade fail.\r\n");
                report_task_start_download_timer(report_task_get_retry_timeout(retry)); 
            } else {      
                /*判断是否下载完毕*/
                if (report_task_context.upgrade.is_download_completion) {
                    /*处理升级*/
                    report_task_process_upgrade(&report_task_context.upgrade);
                } else {
                    /*没有下载完毕，继续下载*/
                    report_task_start_download_timer(0);
                }
            }
        }
        /*压缩机工作时间消息*/
        if (msg_recv.head.id == REPORT_TASK_MSG_COMPRESSOR_RUN_TIME) { 
            report_task_context.log.compressor_run_time += msg_recv.content.run_time;
        }
        /*压缩机工作状态*/
        if (msg_recv.head.id == REPORT_TASK_MSG_COMPRESSOR_STATUS) {
            /*更新日志压缩机状态*/
            report_task_context.log.compressor_is_pwr_on = msg_recv.content.is_pwr_on_enable;
        }

        /*温度消息*/
        if (msg_recv.head.id == REPORT_TASK_MSG_TEMPERATURE_UPDATE) { 
#if CONST_DEVICE_TYPE == CONST_DEVICE_TYPE_C
            report_task_context.log.temperature_cold = msg_recv.content.temperature_float[0] - 20;
#else
            report_task_context.log.temperature_freeze = msg_recv.content.temperature_float[0] - 50;
#endif
            report_task_context.log.temperature_env = msg_recv.content.temperature_float[0];
            if (report_task_context.is_temp_sensor_err == 1) {
                msg_temp.head.id = REPORT_TASK_MSG_TEMPERATURE_SENSOR_FAULT_CLEAR;
                log_assert_bool_false(xQueueSend(report_task_msg_q_id,&msg_temp,REPORT_TASK_PUT_MSG_TIMEOUT) == pdPASS);
            }
        }

        /*温度传感器故障消息*/
        if (msg_recv.head.id == REPORT_TASK_MSG_TEMPERATURE_SENSOR_FAULT_SPAWM) {   
            report_task_context.is_temp_sensor_err = 1;
            device_fault_information_t fault;
            report_task_context.log.temperature_cold = 0xFF;
            report_task_context.log.temperature_env = 0xFF;
            report_task_context.log.temperature_freeze = 0xFF;
#if CONST_DEVICE_TYPE == CONST_DEVICE_TYPE_C
            fault.code = 110;
#else
            fault.code = 120;
#endif
            fault.status = HAL_FAULT_STATUS_FAULT;
            report_task_insert_fault(&report_task_context.fault.queue,&fault); 
            fault.code = 110;
            report_task_insert_fault(&report_task_context.fault.queue,&fault); 

            /*立即开启故障上报*/
            report_task_start_fault_timer(0);
        }

        /*温度传感器故障解除消息*/
        if (msg_recv.head.id == REPORT_TASK_MSG_TEMPERATURE_SENSOR_FAULT_CLEAR) { 
            report_task_context.is_temp_sensor_err = 0;   
            device_fault_information_t fault;
#if CONST_DEVICE_TYPE == CONST_DEVICE_TYPE_C
            fault.code = 110;
#else
            fault.code = 120;
#endif
            fault.status = HAL_FAULT_STATUS_FAULT_CLEAR;
            report_task_insert_fault(&report_task_context.fault.queue,&fault); 
            //fault.code = 110;
            //report_task_insert_fault(&report_task_context.fault.queue,&fault); 
            /*立即开启故障上报*/
            report_task_start_fault_timer(0);  
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
                    report_task_context.log.compressor_run_time = 0;/*清零压缩机运行时间，准备下次上报*/
                } else {
                    retry ++;
                    log_error("report log err.\r\n");
                    /*重置日志上报定时器*/
                    report_task_start_log_timer(report_task_get_retry_timeout(retry));
                }
            }   
        }

        /*故障上报消息*/
        if (msg_recv.head.id == REPORT_TASK_MSG_REPORT_FAULT) {
            /*只有设备激活才可以上报故障*/
            if (report_task_context.is_device_active == 1) {
                rc = report_task_report_fault(&report_task_context.http_client_context,&report_task_context.fault);
                if (rc == 0) {
                    log_info("no device fault.\r\n");
                } else if (rc == 1) {
                    retry = 0;
                    log_info("report fault ok.\r\n");
                    /*再次开启*/
                    report_task_start_fault_timer(0);
                } else {
                    retry ++;
                    log_error("report log err.\r\n");
                    /*重置日志上报定时器*/
                    report_task_start_fault_timer(report_task_get_retry_timeout(retry));
                }                

            }
          }
        }
   }

 }