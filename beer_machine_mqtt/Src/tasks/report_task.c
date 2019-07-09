#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"
#include "stdarg.h"
#include "flash_utils.h"
#include "md5.h"
#include "ntp.h"
#include "http_client.h"
#include "report_task.h"
#include "bootloader_if.h"
#include "firmware_version.h"
#include "net_task.h"
#include "tasks_init.h"
#include "temperature_task.h"
#include "pressure_task.h"
#include "alarm_task.h"
#include "compressor_task.h"
#include "utils.h"
#include "cJSON.h"
#include "device_config.h"
#include "log.h"


osThreadId  report_task_handle;
osMessageQId report_task_msg_q_id;
osMessageQId report_task_location_msg_q_id;
osMessageQId report_task_net_hal_info_msg_q_id;


osTimerId    active_timer_id;
osTimerId    log_timer_id;
osTimerId    fault_timer_id;


typedef enum
{
  HAL_FAULT_STATUS_FAULT = 0,
  HAL_FAULT_STATUS_FAULT_CLEAR,
}hal_fault_status_t;
  
typedef struct
{
  char code[5];
  char msg[6];
  char time[14];
  char status[2];
}report_fault_t;
 
typedef struct
{
  int16_t temperature;
  uint8_t capacity;
  float   pressure;
  net_location_t location;
}report_log_t;

typedef struct
{
  char sn[SN_LEN + 1];
  char sim_id[24];
  char wifi_mac[20];
  char *fw_version;
  bool is_active;
}report_active_t;

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


typedef enum
{
  DEVICE_CONFIG_STATUS_VALID = 0xAA,
  DEVICE_CONFIG_STATUS_INVALID
}device_config_status_t;

typedef enum
{
  COMPRESSOR_LOCK = 0xBB,
  COMPRESSOR_UNLOCK
}compressor_lock_t;

/*设备运行配置信息表*/
typedef struct
{
  uint32_t report_log_interval;
  int8_t temperature_high;
  int8_t temperature_low;
  uint8_t pressure_high;
  uint8_t pressure_low;
  uint8_t capacity_high;
  uint8_t capacity_low;
  compressor_lock_t lock;
  device_config_status_t status;
}device_config_t;

typedef struct
{
  uint32_t read;
  uint32_t write;
  report_fault_t fault[REPORT_TASK_FAULT_QUEUE_SIZE];
}hal_fault_queue_t;


static hal_fault_queue_t fault_queue;

static uint32_t time_offset;

static report_active_t   report_active;
static report_log_t      report_log;
static report_upgrade_t  report_upgrade;

static uint8_t active_event;
const static uint32_t fw_version_code = FIRMWARE_VERSION_HEX;
static device_config_t device_default_config = {
  
.temperature_low = DEFAULT_COMPRESSOR_LOW_TEMPERATURE,
.temperature_high = DEFAULT_COMPRESSOR_HIGH_TEMPERATURE,
.pressure_low = DEFAULT_LOW_PRESSURE,
.pressure_high = DEFAULT_HIGH_PRESSURE,
.capacity_low = DEFAULT_LOW_CAPACITY,
.capacity_high = DEFAULT_HIGH_CAPACITY,
.report_log_interval = DEFAULT_REPORT_LOG_INTERVAL,
.lock = COMPRESSOR_UNLOCK,
.status = DEVICE_CONFIG_STATUS_VALID

};


static void report_task_active_timer_expired(void const *argument);
static void report_task_log_timer_expired(void const *argument);
static void report_task_fault_timer_expired(void const *argument);


/*激活定时器 复用为同步时间定时器*/
static void report_task_active_timer_init(uint8_t *event)
{
  osTimerDef(active_timer,report_task_active_timer_expired);
  active_timer_id = osTimerCreate(osTimer(active_timer),osTimerOnce,event);
  log_assert(active_timer_id);
}
/*启动激活定时器*/
static void report_task_start_active_timer(uint32_t timeout,uint8_t event)
{
  active_event = event;
  osTimerStart(active_timer_id,timeout);  
}
/*激活定时器到达处理*/
static void report_task_active_timer_expired(void const *argument)
{
  osStatus status; 
  report_task_msg_t msg;
  
  msg.type = *( uint8_t *) pvTimerGetTimerID( (void *)argument );
   
  status = osMessagePut(report_task_msg_q_id,*(uint32_t *)&msg,REPORT_TASK_PUT_MSG_TIMEOUT);
  if(status != osOK){
     log_error("put active msg error:%d\r\n",status);
  }
}
/*日志定时器*/
static void report_task_log_timer_init()
{
  osTimerDef(log_timer,report_task_log_timer_expired);
  log_timer_id = osTimerCreate(osTimer(log_timer),osTimerOnce,0);
  log_assert(log_timer_id);
}
/*启动日志定时器*/
static void report_task_start_log_timer(uint32_t timeout)
{
  osTimerStart(log_timer_id,timeout);  
}
/*日志定时器到达处理*/
static void report_task_log_timer_expired(void const *argument)
{
  osStatus status; 
  report_task_msg_t msg;
  
  msg.type = REPORT_TASK_MSG_REPORT_LOG;
  
  status = osMessagePut(report_task_msg_q_id,*(uint32_t *)&msg,REPORT_TASK_PUT_MSG_TIMEOUT);
  if(status != osOK){
     log_error("put log msg error:%d\r\n",status);
  }
}
/*故障定时器*/
static void report_task_fault_timer_init()
{
  osTimerDef(fault_timer,report_task_fault_timer_expired);
  fault_timer_id = osTimerCreate(osTimer(fault_timer),osTimerOnce,0);
  log_assert(fault_timer_id);
}
/*启动故障定时器*/
static void report_task_start_fault_timer(uint32_t timeout)
{
  osTimerStart(fault_timer_id,timeout);  
}

/*故障定时器到达处理*/
static void report_task_fault_timer_expired(void const *argument)
{
  osStatus status; 
  report_task_msg_t msg;
  
  msg.type = REPORT_TASK_MSG_REPORT_FAULT;
  
  status = osMessagePut(report_task_msg_q_id,*(uint32_t *)&msg,REPORT_TASK_PUT_MSG_TIMEOUT);
  if(status != osOK){
     log_error("put fault msg error:%d\r\n",status);
  }
}

/*同步UTC时间*/
static int report_task_sync_utc(uint32_t *offset)
{
  int rc;
  uint32_t cur_time;
  
  rc = ntp_sync_time(&cur_time);
  if(rc != 0){
     return -1;
  }
  
  *offset = cur_time - osKernelSysTick() / 1000;
  
  return 0; 
}

/*获取同步后的本地UTC*/
static uint32_t report_task_get_utc()
{
 return osKernelSysTick() / 1000 + time_offset;
}


/* 函数名：report_task_build_sign
*  功能：  签名算法
*  参数：  sign 签名结果缓存 
*  返回：  0：成功 其他：失败 
*/ 
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
 for(uint8_t i=0; i < cnt; i++){
 temp = va_arg(ap,char*);
 /*保证数据不溢出*/
 if(size + strlen(temp) >= SIGN_SRC_BUFFER_SIZE_MAX){
 return -1;
 }
 size += strlen(temp);
 strcat(sign_src,temp);
 if(i < cnt - 1){
 strcat(sign_src,"&");
 }
 }
 
 /*第1次MD5*/
 md5(sign_src,strlen(sign_src),md5_hex);
 /*把字节装换成HEX字符串*/
 bytes_to_hex_str(md5_hex,md5_str,16);
 /*第2次MD5*/
 md5(md5_str,32,md5_hex);
 bytes_to_hex_str(md5_hex,md5_str,16);
 strcpy(sign,md5_str);
 
 return 0;
}

/* 函数名：report_task_build_url
*  功能：  构造新的啤酒机需要的url格式
*  参数：  url 新url缓存 
*  参数：  size 缓存大小 
*  参数：  origin_url 原始url 
*  参数：  sn 串号 
*  参数：  sign 签名 
*  参数：  source 源 
*  参数：  timestamp 时间戳 
*  返回：  0：成功 其他：失败 
*/ 
int report_task_build_url(char *url,const int size,const char *origin_url,const char *sn,const char *sign,const char *source,const char *timestamp)
{
snprintf(url,size,"%s?sn=%s&sign=%s&source=%s&timestamp=%s",origin_url,sn,sign,source,timestamp);
if(strlen(url) == size - 1){
log_error("url size:%d too large.\r\n",size - 1); 
return -1;
}
return 0;
}
 

/* 函数：report_task_build_form_data
*  功能：构造form-data格式数据
*  参数：form_data form-data缓存 
*  参数：size 缓存大小 
*  参数：boundary 分界标志 
*  参数：cnt form-data数据个数 
*  参数：... form-data参数列表 
*  参数：source 源 
*  参数：timestamp 时间戳 
*  返回：0：成功 其他：失败 
*/ 
int report_task_build_form_data(char *form_data,const int size,const char *boundary,const int cnt,...)
{
 int put_size;
 int temp_size;  
 va_list ap;
 form_data_t *temp;

 put_size = 0;
 va_start(ap, cnt);
 
 /*组合输入数据*/  
 for(uint8_t i=0; i < cnt; i++){
 
 temp = va_arg(ap,form_data_t*);
 /*添加boundary 和 name 和 value*/
 snprintf(form_data + put_size ,size - put_size,"--%s\r\nContent-Disposition: form-data; name=%s\r\n\r\n%s\r\n",boundary,temp->name,temp->value);
 temp_size = strlen(form_data);
 /*保证数据完整*/
 if(temp_size >= size - 1){
 log_error("form data size :%d is too large.\r\n",temp_size);
 return -1;
 } 
 put_size = strlen(form_data);
 }
 /*添加结束标志*/
 snprintf(form_data + put_size,size - put_size,"--%s--\r\n",boundary);
 put_size = strlen(form_data);
 if(put_size >= size - 1){
 log_error("form data size :%d is too large.\r\n",put_size);
 return -1;  
 }
 
 return 0; 
}
  
/*构造日志数据json*/
static char *report_task_build_log_json_str(report_log_t *log)
{
  char *json_str;
  cJSON *log_json;
  cJSON *location_array,*location1,*location2,*location3,*location4;
  log_json = cJSON_CreateObject();
  cJSON_AddStringToObject(log_json,"source",SOURCE);
  cJSON_AddNumberToObject(log_json,"pressure",log->pressure);
  cJSON_AddNumberToObject(log_json,"capacity",log->capacity);
  cJSON_AddNumberToObject(log_json,"temp",log->temperature);
  /*location数组*/
  cJSON_AddItemToObject(log_json,"location",location_array = cJSON_CreateArray());
  
  cJSON_AddItemToArray(location_array,location1 = cJSON_CreateObject());
  cJSON_AddStringToObject(location1,"lac",log->location.base_main.lac);
  cJSON_AddStringToObject(location1,"cid",log->location.base_main.ci);
  cJSON_AddStringToObject(location1,"rssi",log->location.base_main.rssi);
  
  cJSON_AddItemToArray(location_array,location2 = cJSON_CreateObject());
  cJSON_AddStringToObject(location2,"lac",log->location.base_assist1.lac);
  cJSON_AddStringToObject(location2,"cid",log->location.base_assist1.ci);
  cJSON_AddStringToObject(location2,"rssi",log->location.base_assist1.rssi);
  
  cJSON_AddItemToArray(location_array,location3 = cJSON_CreateObject());
  cJSON_AddStringToObject(location3,"lac",log->location.base_assist2.lac);
  cJSON_AddStringToObject(location3,"cid",log->location.base_assist2.ci);
  cJSON_AddStringToObject(location3,"rssi",log->location.base_assist2.rssi);
  
  cJSON_AddItemToArray(location_array,location4 = cJSON_CreateObject());
  cJSON_AddStringToObject(location4,"lac",log->location.base_assist3.lac);
  cJSON_AddStringToObject(location4,"cid",log->location.base_assist3.ci);
  cJSON_AddStringToObject(location4,"rssi",log->location.base_assist3.rssi);
  
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
  
  log_debug("parse fault rsp.\r\n");
  log_rsp_json = cJSON_Parse(json_str);
  if(log_rsp_json == NULL){
     log_error("rsp is not json.\r\n");
     return -1;  
  }
  /*检查code值 200成功*/
  temp = cJSON_GetObjectItem(log_rsp_json,"code");
  if(!cJSON_IsNumber(temp)){
     log_error("code is not num.\r\n");
     goto err_exit;  
  }
               
  log_debug("code:%d\r\n", temp->valueint);
  if(temp->valueint != 200){
     log_error("log rsp err code:%d.\r\n",temp->valueint); 
     goto err_exit;  
  } 
  rc = 0;
  
err_exit:
  cJSON_Delete(log_rsp_json);
  return rc;
}

/*执行日志数据上报*/
 static int report_task_report_log(const char *url_origin,report_log_t *log,const char *sn)
 {
  //report_task_build_sign(sign,5,/*errorCOde*/"2010",/*errorMsg*/"",sn,source,timestamp);
 int rc;
 uint32_t timestamp;
 http_client_context_t context;
 char timestamp_str[14] = { 0 };
 char sign_str[33] = { 0 };
 char *req;
 char rsp[400] = { 0 };
 char url[200] = { 0 };
 /*计算时间戳字符串*/
 timestamp = report_task_get_utc();
 snprintf(timestamp_str,14,"%d",timestamp);
 /*计算sign*/
 report_task_build_sign(sign_str,4,KEY,sn,SOURCE,timestamp_str);
 /*构建新的url*/
 report_task_build_url(url,200,url_origin,sn,sign_str,SOURCE,timestamp_str);  
   
 req = report_task_build_log_json_str(log);
 
 context.range_size = 200;
 context.range_start = 0;
 context.rsp_buffer = rsp;
 context.rsp_buffer_size = 400;
 context.url = url;
 context.timeout = 10000;
 context.user_data = (char *)req;
 context.user_data_size = strlen(req);
 context.boundary = BOUNDARY;
 context.is_form_data = false;
 context.content_type = "application/Json";//"multipart/form-data; boundary=";
 
 rc = http_client_post(&context);
 cJSON_free(req);
 
 if(rc != 0){
    log_error("report log err.\r\n");  
    return -1;
 }
 rc = report_task_parse_log_rsp_json(context.rsp_buffer);
 if(rc != 0){
    log_error("json parse log rsp error.\r\n");  
    return -1;
 }
 
 return 0;
 }
 
 
 
 /*构造激活信息数据json*/
 static char *report_task_build_active_json_str(report_active_t *active)
 {
  char *json_str;
  cJSON *active_json;
  active_json = cJSON_CreateObject();
  cJSON_AddStringToObject(active_json,"model",MODEL);
  cJSON_AddStringToObject(active_json,"sn",active->sn);
  cJSON_AddStringToObject(active_json,"firmware",active->fw_version);
  cJSON_AddStringToObject(active_json,"simId",active->sim_id);
  cJSON_AddStringToObject(active_json,"wifiMac",active->wifi_mac);
  json_str = cJSON_PrintUnformatted(active_json);
  cJSON_Delete(active_json);
  return json_str;
 }



/*解析激活返回的信息数据json*/
static int report_task_parse_active_rsp_json(char *json_str ,device_config_t *config)
{
  int rc = -1;
  cJSON *active_rsp_json;
  cJSON *temp,*data,*run_config,*temperature,*pressure;
  
  log_debug("parse active rsp.\r\n");
  active_rsp_json = cJSON_Parse(json_str);
  if(active_rsp_json == NULL){
     log_error("rsp is not json.\r\n");
     return -1;  
  }
  /*检查code值 200成功*/
  temp = cJSON_GetObjectItem(active_rsp_json,"code");
  if(!cJSON_IsNumber(temp)){
     log_error("code is not num.\r\n");
     goto err_exit;  
  }
               
  log_debug("code:%d\r\n", temp->valueint);
  if(temp->valueint != 200 ){
     log_error("active rsp err code:%d.\r\n",temp->valueint); 
     goto err_exit;  
  }  
 
  /*检查success值 true or false*/
  temp = cJSON_GetObjectItem(active_rsp_json,"success");
  if(!cJSON_IsBool(temp) || !cJSON_IsTrue(temp)){
     log_error("success is not bool or is not true.\r\n");
     goto err_exit;  
  }
  log_debug("success:%s\r\n",temp->valueint ? "true" : "false"); 
  
  /*检查data */
  data = cJSON_GetObjectItem(active_rsp_json,"data");
  if(!cJSON_IsObject(data)){
     log_error("data is not obj.\r\n");
     goto err_exit;  
  }
  /*检查runConfig */ 
  run_config = cJSON_GetObjectItem(data,"runConfig");
  if(!cJSON_IsObject(run_config)){
     log_error("run config is not obj.\r\n");
     goto err_exit;  
  }
  /*检查log submit interval*/
  temp = cJSON_GetObjectItem(run_config,"logSubmitDur");
  if(!cJSON_IsNumber(temp)){
     log_error("logSubmitDur is not num.\r\n");
     goto err_exit;  
  }
  config->report_log_interval = temp->valueint * 60 * 1000;/*配置的单位是分钟，定时器使用毫秒*/
  
  /*检查lock*/
  temp = cJSON_GetObjectItem(run_config,"lock");
  if(!cJSON_IsBool(temp)){ 
     log_error("lock is not bool.\r\n");
     goto err_exit;  
  }
  
  if(cJSON_IsTrue(temp)){
  config->lock = COMPRESSOR_LOCK;
  }else{
  config->lock = COMPRESSOR_UNLOCK;
  }

  /*检查temperature */ 
  temperature = cJSON_GetObjectItem(run_config,"temp");
  if(!cJSON_IsObject(temperature)){
     log_error("temperature is not obj.\r\n");
     goto err_exit;  
  }
  /*检查temperature min */ 
  temp = cJSON_GetObjectItem(temperature,"min");
  if(!cJSON_IsNumber(temp)){
     log_error("t min is not num.\r\n");
     goto err_exit;  
  }
  config->temperature_low = temp->valueint;
 /*检查temperature max */ 
  temp = cJSON_GetObjectItem(temperature,"max");
  if(!cJSON_IsNumber(temp)){
     log_error("t max is not num.\r\n");
     goto err_exit;  
  }
  config->temperature_high = temp->valueint;
  
  /*检查pressure*/ 
  pressure = cJSON_GetObjectItem(run_config,"pressure");
  if(!cJSON_IsObject(pressure)  ){
     log_error("pressure is not obj.\r\n");
     goto err_exit;  
  }
  /*检查pressure min */ 
  temp = cJSON_GetObjectItem(pressure,"min");
  if(!cJSON_IsNumber(temp)){
     log_error("p min is not num.\r\n");
     goto err_exit;  
  }
 /*把浮点数*10转换成uint8_t*/
  config->pressure_low = (uint8_t)temp->valuedouble * 10;
 /*检查pressure max */ 
  temp = cJSON_GetObjectItem(pressure,"max");
  if(!cJSON_IsNumber(temp)){
     log_error("p max is not num.\r\n");
     goto err_exit;  
  }
  /*把浮点数*10转换成uint8_t*/
  config->pressure_high = (uint8_t)temp->valuedouble * 10;
  
  /*液位暂不可配,填写默认值*/
  config->capacity_low = DEFAULT_LOW_CAPACITY;
  config->capacity_high = DEFAULT_HIGH_CAPACITY;
  
  rc = 0;
  log_info("active rsp [lock:%d log interval:%dmS. t_low:%d t_high:%d p_low:%d p_high:%d.\r\n",
            config->lock,config->report_log_interval,config->temperature_low,config->temperature_high,config->pressure_low,config->pressure_high);
  

err_exit:
  cJSON_Delete(active_rsp_json);
  return rc;
}



/*执行激活信息数据上报*/  
static int report_task_report_active(const char *url_origin,report_active_t *active,device_config_t *config)
{
 int rc;
 uint32_t timestamp;
 http_client_context_t context;
 char timestamp_str[14] = { 0 };
 char sign_str[33] = { 0 };
 char *req;
 char rsp[400] = { 0 };
 char url[200] = { 0 };
 /*计算时间戳字符串*/
 timestamp = report_task_get_utc();
 snprintf(timestamp_str,14,"%d",timestamp);
 /*计算sign*/
 report_task_build_sign(sign_str,4,KEY,active->sn,SOURCE,timestamp_str);
 /*构建新的url*/
 report_task_build_url(url,200,url_origin,active->sn,sign_str,SOURCE,timestamp_str);  
   
 req = report_task_build_active_json_str(active);

 
 context.range_size = 200;
 context.range_start = 0;
 context.rsp_buffer = rsp;
 context.rsp_buffer_size = 400;
 context.url = url;
 context.timeout = 10000;
 context.user_data = (char *)req;
 context.user_data_size = strlen(req);
 context.boundary = BOUNDARY;
 context.is_form_data = false;
 context.content_type = "application/Json";
 
 rc = http_client_post(&context);
 cJSON_free(req);
 
 if(rc != 0){
    log_error("report active err.\r\n");  
    return -1;
 }
 
 rc = report_task_parse_active_rsp_json(context.rsp_buffer,config);
 if(rc != 0){
    log_error("json parse active rsp error.\r\n");  
    return -1;
 }
 log_debug("report active ok.\r\n");  
 return 0;
 }
  

/*构造故障信息json*/
static void report_task_build_fault_form_data_str(char *form_data,const uint16_t size,const report_fault_t *fault)
{
 form_data_t err_code;
 form_data_t err_msg;
 form_data_t err_time;
 form_data_t err_status;
 
 err_code.name = "errorCode";
 err_code.value = fault->code;
 
 err_msg.name = "errorMsg";
 err_msg.value = fault->msg;
 
 err_time.name = "errorTime";
 err_time.value = fault->time;
 
 err_status.name = "state";
 err_status.value = fault->status;
 
 report_task_build_form_data(form_data,size,BOUNDARY,4,&err_code,&err_msg,&err_time,&err_status);
 }
  

/*解析故障上报返回的信息数据json*/
static int report_task_parse_fault_rsp_json(char *json_str)
{
  int rc = -1;
  cJSON *fault_rsp_json;
  cJSON *temp;
  
  log_debug("parse fault rsp.\r\n");
  fault_rsp_json = cJSON_Parse(json_str);
  if(fault_rsp_json == NULL){
     log_error("rsp is not json.\r\n");
     return -1;  
  }
  /*检查code值 200成功*/
  temp = cJSON_GetObjectItem(fault_rsp_json,"code");
  if(!cJSON_IsNumber(temp)){
     log_error("code is not num.\r\n");
     goto err_exit;  
  }
               
  log_debug("code:%d\r\n", temp->valueint);
  if(temp->valueint != 200 && temp->valueint != 30010){
     log_error("fault rsp err code:%d.\r\n",temp->valueint); 
     goto err_exit;  
  } 
  rc = 0;
  
err_exit:
  cJSON_Delete(fault_rsp_json);
  return rc;
}

 /*执行故障信息数据上报*/
static int report_task_report_fault(const char *url_origin,report_fault_t *fault,const char *sn)
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
 
 if(rc != 0){
    log_error("report fault err.\r\n");  
    return -1;
 }
 rc = report_task_parse_fault_rsp_json(context.rsp_buffer);
 if(rc != 0){
    log_error("json parse fault rsp error.\r\n");  
    return -1;
 }
 log_debug("report fault ok.\r\n");  
 return 0;
}

/*根据重试次数获取重试延时时间*/
static int report_task_retry_delay(uint8_t retry)
{
  if(retry == 0){
     return REPORT_TASK_RETRY1_DELAY;
  }else if(retry == 1){
     return REPORT_TASK_RETRY2_DELAY;
  }
  
  return REPORT_TASK_RETRY3_DELAY; 
}

/*读取保存在ENV中的设备配置参数*/
static int report_task_get_env_device_config(device_config_t *config)
{
  int rc;
  bootloader_env_t env;
  
  rc = bootloader_get_env(&env);
  /*必须保证env读取成功*/
  log_assert(rc == 0 && env.status == BOOTLOADER_ENV_STATUS_VALID);
  
  *config = *(device_config_t *)&env.reserved[ENV_DEVICE_CONFIG_OFFSET];

  return 0;
}

/*保存新的设备配置参数在ENV中*/
static int report_task_save_env_device_config(device_config_t *config)
{
  int rc;
  bootloader_env_t env;
  device_config_t  config_env;
  
  rc = bootloader_get_env(&env);
  /*读取env失败*/
  if(rc != 0 ){
     return -1;
   }
  
  /*读取env成功*/
  config_env = *(device_config_t *)&env.reserved[0];
  if(memcmp(&config_env,config,sizeof(device_config_t)) != 0 ){
    *(device_config_t*)&env.reserved[0] = *config;
    rc = bootloader_save_env(&env);
    /*保存env失败*/
    if(rc != 0 ){
       return -1;
    }  
  }  
  
  return 0;
}
  
  
 

/*向对应任务分发工作参数*/
static int report_task_dispatch_device_config(device_config_t *config)
{
  osStatus status;
   
  alarm_task_msg_t alarm_msg;

  if(config->lock == COMPRESSOR_LOCK){
     alarm_msg.value = ALARM_TASK_COMPRESSOR_LOCK_VALUE;
  }else{
     alarm_msg.value = ALARM_TASK_COMPRESSOR_UNLOCK_VALUE;
  }      
  alarm_msg.type = ALARM_TASK_MSG_COMPRESSOR_LOCK_CONFIG;
  status = osMessagePut(alarm_task_msg_q_id,*(uint32_t *)&alarm_msg,REPORT_TASK_PUT_MSG_TIMEOUT);
  if(status != osOK){
     log_error("report task put lock msg err.\r\n"); 
  }
   
  alarm_msg.type = ALARM_TASK_MSG_TEMPERATURE_CONFIG;
  alarm_msg.reserved = ((uint16_t)config->temperature_high << 8 | config->temperature_low) & 0xFFFF;
  status = osMessagePut(alarm_task_msg_q_id,*(uint32_t *)&alarm_msg,REPORT_TASK_PUT_MSG_TIMEOUT);
  if(status != osOK){
     log_error("report task put t msg err.\r\n"); 
  }
  
  alarm_msg.type = ALARM_TASK_MSG_PRESSURE_CONFIG;
  alarm_msg.reserved = ((uint16_t)config->pressure_high << 8 | config->pressure_low) & 0xFFFF;
  status = osMessagePut(alarm_task_msg_q_id,*(uint32_t *)&alarm_msg,REPORT_TASK_PUT_MSG_TIMEOUT);
  if(status != osOK){
     log_error("report task put pressure msg err.\r\n"); 
  }
  
  alarm_msg.type = ALARM_TASK_MSG_CAPACITY_CONFIG;
  alarm_msg.reserved = ((uint16_t)config->capacity_high << 8 | config->capacity_low) & 0xFFFF;
  status = osMessagePut(alarm_task_msg_q_id,*(uint32_t *)&alarm_msg,REPORT_TASK_PUT_MSG_TIMEOUT);
  if(status != osOK){
     log_error("report task put capacity msg err.\r\n"); 
  }
  
  return 0;
}

/*等待网络硬件消息，用来激活设备*/
static int report_task_get_net_hal_info(char *mac,char *sim_id,uint32_t timeout)
{
  osEvent os_event;
  net_hal_information_t *net_hal_info;
 
  /*处理网络模块硬件消息*/
  os_event = osMessageGet(report_task_net_hal_info_msg_q_id,timeout);
  if(os_event.status == osEventMessage){
     net_hal_info = (net_hal_information_t*)os_event.value.v; 
     strcpy(mac,net_hal_info->mac);
     strcpy(sim_id,net_hal_info->sim_id);
     return 0;
  }  
 
  return -1;
}


/*读取flash中的SN*/
static void report_task_get_sn(char *sn)
{
  int rc;
  
  uint32_t sn_str[SN_LEN / 4 + 1];
  
  rc = flash_utils_read(sn_str,SN_ADDR,SN_LEN / 4 + 1);
  if(rc != 0){
     sn[0] = 0; 
  }else{
     memcpy(sn,(char *)sn_str,SN_LEN);
     sn[SN_LEN] = '\0';  
  }
}
/*读取版本号*/
static void report_task_get_firmware_version(char **fw_version)
{
  *fw_version = FIRMWARE_VERSION_STR;
}

/*构造故障参数*/
static void report_task_build_fault(report_fault_t *fault,const char *code,const char *msg,const uint32_t time,const int status)
{
  char time_str[14];
  char status_str[2];
        
  snprintf(time_str,14,"%d",time);
  snprintf(status_str,2,"%d",status);
        
  strcpy(fault->code,code); 
  strcpy(fault->msg,msg); 
  strcpy(fault->time,time_str); 
  strcpy(fault->status,status_str); 
}

/*把一个故障参数放进故障参数队列*/      
static int report_task_put_fault_to_queue(hal_fault_queue_t *queue,report_fault_t *fault)
{      
  if(queue->write - queue->read >= REPORT_TASK_FAULT_QUEUE_SIZE){
     log_warning("fault queue is full.\r\n");
     return -1;
  }
  queue->fault[queue->write & (REPORT_TASK_FAULT_QUEUE_SIZE - 1)] = *fault;
  queue->write++;    
  
  return 0;    
}

/*从故障参数队列取出一个故障参数*/         
int report_task_get_fault_from_queue(hal_fault_queue_t *queue,report_fault_t *fault)
{     
  if(queue->read >= queue->write){
     log_warning("fault queue is null.\r\n");
     return -1;
   }
       
  *fault = queue->fault[queue->read & (REPORT_TASK_FAULT_QUEUE_SIZE - 1)];   
   return 0;    
}
/*从故障参数队列删除一个故障参数*/         
int report_task_delete_fault_from_queue(hal_fault_queue_t *queue)
{
  if(queue->read < queue->write){
   queue->read++; 
  }
  return 0;
}
 
/*解析升级信息回应*/
static int report_task_parse_upgrade_rsp_json(char *json_str,report_upgrade_t *report_upgrade)
{
  int rc = -1;
  cJSON *upgrade_rsp_json;
  cJSON *temp,*data,*upgrade;
  
  log_debug("parse active rsp.\r\n");
  upgrade_rsp_json = cJSON_Parse(json_str);
  if(upgrade_rsp_json == NULL){
     log_error("rsp is not json.\r\n");
     return -1;  
  }
  /*检查code值 200 获取的升级信息*/
  temp = cJSON_GetObjectItem(upgrade_rsp_json,"code");
  if(!cJSON_IsNumber(temp)){
     log_error("code is not num.\r\n");
     goto err_exit;  
  }
 
  log_debug("code:%d\r\n", temp->valueint);
  /*获取到升级信息*/    
  if(temp->valueint == 200 ){
    report_upgrade->update = true;
  }else if (temp->valueint == 404) {
    report_upgrade->update = false;
    rc = 0;
    goto err_exit; 
  }else{
    log_error("update rsp err code:%d.\r\n",temp->valueint); 
    goto err_exit; 
  }

  /*检查success值 true or false*/
  temp = cJSON_GetObjectItem(upgrade_rsp_json,"success");
  if(!cJSON_IsBool(temp) || !cJSON_IsTrue(temp)){
     log_error("success is not bool or is not true.\r\n");
     goto err_exit;  
  }
  log_debug("success:%s\r\n",temp->valueint ? "true" : "false"); 
  
  /*检查data */
  data = cJSON_GetObjectItem(upgrade_rsp_json,"data");
  if(!cJSON_IsObject(data) ){
     log_error("data is not obj.\r\n");
     goto err_exit;  
  }
  /*检查upgrade */ 
  upgrade = cJSON_GetObjectItem(data,"upgrade");
  if(!cJSON_IsObject(upgrade)){
     log_error("upgrade is not obj .\r\n");
     goto err_exit;  
  }
  /*检查url*/
  temp = cJSON_GetObjectItem(upgrade,"upgradeUrl");
  if(!cJSON_IsString(temp) || temp->valuestring == NULL){
     log_error("upgradeUrl is not str or is null.\r\n");
     goto err_exit;  
  }
  strcpy(report_upgrade->download_url,temp->valuestring);

  /*检查version code*/
  temp = cJSON_GetObjectItem(upgrade,"majorVersion");
  if(!cJSON_IsNumber(temp)){
     log_error("majorVersion is not num.\r\n");
     goto err_exit;  
  }
  report_upgrade->version_code = temp->valueint;
  
 /*检查version str*/
  temp = cJSON_GetObjectItem(upgrade,"version");
  if(!cJSON_IsString(temp) || temp->valuestring == NULL){
     log_error("version is not str or is null.\r\n");
     goto err_exit;  
  }
  strcpy(report_upgrade->version_str,temp->valuestring);
  
  /*检查size*/
  temp = cJSON_GetObjectItem(upgrade,"size");
  if(!cJSON_IsNumber(temp)){
     log_error("size is not num.\r\n");
     goto err_exit;  
  }
  report_upgrade->bin_size = temp->valueint;
  
  /*检查md5*/
  temp = cJSON_GetObjectItem(upgrade,"md5");
  if(!cJSON_IsString(temp) || temp->valuestring == NULL){
     log_error("md5 is not str or is null.\r\n");
     goto err_exit;  
  }
  strcpy(report_upgrade->md5,temp->valuestring);
  
  log_debug("upgrade rsp:\r\ndwn_url:%s\r\nver_code:%d.\r\nver_str:%s\r\nmd5:%s.\r\nsize:%d.\r\n",
            report_upgrade->download_url,
            report_upgrade->version_code,
            report_upgrade->version_str,
            report_upgrade->md5,
            report_upgrade->bin_size);
  rc = 0;
  
err_exit:
  cJSON_Delete(upgrade_rsp_json);
  return rc; 
}

/*执行获取升级信息*/  
static int report_task_get_upgrade(const char *url_origin,const char *sn,report_upgrade_t *upgrade)
{
  int rc;
  uint32_t timestamp;
  http_client_context_t context;
  char timestamp_str[14] = { 0 };
  char sign_str[33] = { 0 };
  char rsp[400] = { 0 };
  char url[200] = { 0 };
  /*计算时间戳字符串*/
  timestamp = report_task_get_utc();
  snprintf(timestamp_str,14,"%d",timestamp);
  /*计算sign*/
  report_task_build_sign(sign_str,4,KEY,sn,SOURCE,timestamp_str);
  /*构建新的url*/
  report_task_build_url(url,200,url_origin,sn,sign_str,SOURCE,timestamp_str);  
 
  context.range_size = 200;
  context.range_start = 0;
  context.rsp_buffer = rsp;
  context.rsp_buffer_size = 400;
  context.url = url;
  context.timeout = 10000;
  context.user_data = NULL;
  context.user_data_size = 0;
  context.boundary = BOUNDARY;
  context.is_form_data = false;
  context.content_type = "application/Json";
 
  rc = http_client_get(&context);
 
  if(rc != 0){
     log_error("get upgrade err.\r\n");  
     return -1;
  }
 
  rc = report_task_parse_upgrade_rsp_json(context.rsp_buffer,upgrade);
  if(rc != 0){
     log_error("json parse upgrade rsp error.\r\n");  
     return -1;
  }
  log_debug("get upgrade ok.\r\n");  
  return 0;
 }
  
static char bin_data[BOOTLOADER_FLASH_PAGE_SIZE + 1];

static int report_task_download_upgrade_bin(char *url,char *buffer,uint32_t start,uint16_t size)
{
  int rc;

  http_client_context_t context;

  context.range_size = size;
  context.range_start = start;
  context.rsp_buffer = buffer;
  context.rsp_buffer_size = size;
  context.url = url;
  context.timeout = 10000;
  context.user_data = NULL;
  context.user_data_size = 0;
  context.boundary = BOUNDARY;
  context.is_form_data = false;
  context.content_type = "application/Json"; 
  
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


/*上报任务*/
void report_task(void const *argument)
{
 int rc;
 int fault_retry = 0,active_retry = 0,upgrade_retry = 0;
 osEvent os_event;
 report_task_msg_t msg;
 device_config_t device_config;
 bootloader_env_t env;

 /*定时器初始化*/
 report_task_active_timer_init(&active_event);
 report_task_log_timer_init();
 report_task_fault_timer_init();

 report_task_get_firmware_version(&report_active.fw_version);
 report_task_get_sn(report_active.sn);
 
 log_info("\r\nfirmware version: %s\r\n\r\n",report_active.fw_version);
 log_info("\r\nSN: %s\r\n\r\n",report_active.sn);

 flash_utils_init();
 
 rc = bootloader_get_env(&env);
 /*必须保证env存在且有效*/
 log_assert(rc == 0 && env.status == BOOTLOADER_ENV_STATUS_VALID);
 
 if(env.boot_flag == BOOTLOADER_FLAG_BOOT_UPDATE_COMPLETE){
    env.boot_flag = BOOTLOADER_FLAG_BOOT_UPDATE_OK;
    rc = bootloader_save_env(&env);  
    log_assert(rc == 0);
 }

 /*读取存储在ENV中的设备配置表参数*/
 report_task_get_env_device_config(&device_config);
 if(device_config.status != DEVICE_CONFIG_STATUS_VALID){
    device_config = device_default_config;
 }
 
 /*分发默认的开机配置参数*/
 report_task_dispatch_device_config(&device_config);

 
 /*等待任务同步*/
 /*
 xEventGroupSync(tasks_sync_evt_group_hdl,TASKS_SYNC_EVENT_REPORT_TASK_RDY,TASKS_SYNC_EVENT_ALL_TASKS_RDY,osWaitForever);
 log_debug("report task sync ok.\r\n");
*/
 
 while(1){
 os_event = osMessageGet(report_task_msg_q_id,osWaitForever);
 if(os_event.status == osEventMessage){
    msg = *(report_task_msg_t*)&os_event.value.v; 
    
    /*获取网络硬件消息*/
    if(msg.type == REPORT_TASK_MSG_NET_HAL_INFO){ 
       rc = report_task_get_net_hal_info(report_active.wifi_mac,report_active.sim_id,0);
       /*获取失败 设备离线 无法激活和进行下面的步骤 继续尝试*/
       if(rc != 0){
          log_error("report task get net hal info timeout.%d S later retry.\r\n",REPORT_TASK_RETRY_DELAY / 1000);
          report_task_start_active_timer(REPORT_TASK_RETRY_DELAY,REPORT_TASK_MSG_NET_HAL_INFO); 
       }else{
          report_task_start_active_timer(0,REPORT_TASK_MSG_SYNC_UTC); 
       }
    }
    /*同步UTC消息*/
    if(msg.type == REPORT_TASK_MSG_SYNC_UTC){ 
       rc = report_task_sync_utc(&time_offset);
       if(rc != 0){
          log_error("report task sync utc timeout.%d S later retry.\r\n",REPORT_TASK_RETRY_DELAY / 1000);
          report_task_start_active_timer(REPORT_TASK_RETRY_DELAY,REPORT_TASK_MSG_SYNC_UTC); 
       }else{
          log_info("report task sync utc ok.\r\n");
          /*为了后面的定时同步时间*/
          if(report_active.is_active == false){
             report_task_start_active_timer(0,REPORT_TASK_MSG_ACTIVE);
          }else{
             report_task_start_active_timer(REPORT_TASK_SYNC_UTC_DELAY,REPORT_TASK_MSG_SYNC_UTC); 
          }
       }
    }
     /*设备激活消息*/
    if(msg.type == REPORT_TASK_MSG_ACTIVE){ 
       rc = report_task_report_active(URL_ACTIVE,&report_active,&device_config);
       if(rc != 0){
          log_error("report task active timeout.%d S later retry.\r\n",report_task_retry_delay(active_retry) / 1000);
          report_task_start_active_timer(report_task_retry_delay(active_retry),REPORT_TASK_MSG_ACTIVE); 
          if(++active_retry >= 3){
             active_retry = 0;
          }
       }else{
         active_retry = 0;
         report_active.is_active = true;
         log_info("device active ok.\r\n");
         
         /*分发激活后的配置参数*/
         report_task_dispatch_device_config(&device_config);
         
         /*比较保存新激活的参数*/
         report_task_save_env_device_config(&device_config);
         
         /*激活后获取升级信息*/
         report_task_start_active_timer(0,REPORT_TASK_MSG_GET_UPGRADE); 
             
         /*打开启日志上报定时器*/
         report_task_start_log_timer(device_config.report_log_interval);
         
         /*打开故障上报定时器*/
         report_task_start_fault_timer(0);
       }
    }
    /*获取更新信息消息*/
    if(msg.type == REPORT_TASK_MSG_GET_UPGRADE){ 
       rc = report_task_get_upgrade(URL_UPGRADE,report_active.sn,&report_upgrade);
       if(rc != 0){
          log_error("report task get upgrade timeout.%d S later retry.\r\n",report_task_retry_delay(upgrade_retry) / 1000);
          report_task_start_active_timer(report_task_retry_delay(upgrade_retry),REPORT_TASK_MSG_GET_UPGRADE); 
          if(++upgrade_retry >= 3){
             upgrade_retry = 0;
          }
       }else{
        /*获取成功处理*/   
         upgrade_retry = 0;
         /*对比现在的版本号*/
         if(report_upgrade.update == true && report_upgrade.version_code > fw_version_code){
           log_info("firmware need upgrade.ver_code:%d size:%d md5:%s.start download...\r\n",
                    report_upgrade.version_code,report_upgrade.bin_size,report_upgrade.md5);
            report_task_start_active_timer(0,REPORT_TASK_MSG_DOWNLOAD_UPGRADE);                 
         }else{
            log_info("firmware is latest.\r\n");  
            /*开启定时作为同步时间定时器*/
            report_task_start_active_timer(REPORT_TASK_SYNC_UTC_DELAY,REPORT_TASK_MSG_SYNC_UTC);       
         }             
       }
    }
    
    
    /*下载更新文件*/
    if(msg.type == REPORT_TASK_MSG_DOWNLOAD_UPGRADE){
       int size;
       size = report_upgrade.bin_size - report_upgrade.download_size > BOOTLOADER_FLASH_PAGE_SIZE ? BOOTLOADER_FLASH_PAGE_SIZE : report_upgrade.bin_size - report_upgrade.download_size;
       rc = report_task_download_upgrade_bin(report_upgrade.download_url,bin_data,report_upgrade.download_size,size);
       if(rc != 0){
          log_error("report task download upgrade fail.%d S later retry.\r\n",report_task_retry_delay(upgrade_retry) / 1000);
          report_task_start_active_timer(report_task_retry_delay(upgrade_retry),REPORT_TASK_MSG_DOWNLOAD_UPGRADE); 
          if(++upgrade_retry >= 3){
             upgrade_retry = 0;
          }
       }else{
       /*保存下载的文件*/   
       rc = bootloader_write_fw(BOOTLOADER_FLASH_BASE_ADDR + BOOTLOADER_FLASH_UPDATE_APPLICATION_ADDR_OFFSET + report_upgrade.download_size,
                               (uint32_t)bin_data,
                                size);
       
       if(rc != 0){
          /*中止本次升级*/
          log_error("bin data save err.stop.\r\n")
          /*开启定时作为同步时间定时器*/
          report_task_start_active_timer(REPORT_TASK_SYNC_UTC_DELAY,REPORT_TASK_MSG_SYNC_UTC);  
       }else{
         /*保存成功判断是否下载完毕*/
          report_upgrade.download_size += size;
          if(report_upgrade.download_size == report_upgrade.bin_size){
             /*下载完成 计算md5*/
             char md5_hex[16];
             char md5_str[33];
             md5((const char *)(BOOTLOADER_FLASH_BASE_ADDR + BOOTLOADER_FLASH_UPDATE_APPLICATION_ADDR_OFFSET),report_upgrade.bin_size,md5_hex);
             /*转换成hex字符串*/
             bytes_to_hex_str(md5_hex,md5_str,16);
             /*校验成功，启动升级*/
             if(strcmp(md5_str,report_upgrade.md5) == 0){
                rc = bootloader_get_env(&env);
                /*必须保证env存在且有效*/
                log_assert(rc == 0 && env.status == BOOTLOADER_ENV_STATUS_VALID);
                /*设置更新标志 bootloader使用*/
                env.boot_flag = BOOTLOADER_FLAG_BOOT_UPDATE;
                env.fw_update.size = report_upgrade.bin_size;
                strcpy(env.fw_update.md5.value,report_upgrade.md5);
                env.fw_update.version.code = report_upgrade.version_code;
                
                bootloader_save_env(&env);
                /*启动bootloader，开始更新*/
                bootloader_reset();                 
             }else{
               /*中止本次升级*/
               log_error("md5 err.calculate:%s recv:%s.stop upgrade.\r\n",md5_str,report_upgrade.md5);
               /*开启定时作为同步时间定时器*/
               report_task_start_active_timer(REPORT_TASK_SYNC_UTC_DELAY,REPORT_TASK_MSG_SYNC_UTC);                
             }           
          }else{
          /*没有下载完毕，继续下载*/
          report_task_start_active_timer(0,REPORT_TASK_MSG_DOWNLOAD_UPGRADE);             
          }        
       }
       }   
    }
          
    /*温度消息*/
    if(msg.type == REPORT_TASK_MSG_TEMPERATURE_VALUE){
       report_log.temperature = (int8_t)msg.value;
    }
    /*压力消息*/
    if(msg.type == REPORT_TASK_MSG_PRESSURE_VALUE){
       report_log.pressure = msg.value / 10.0 ;
    }
 
    /*容积消息*/
    if(msg.type == REPORT_TASK_MSG_CAPACITY_VALUE){
       report_log.capacity = msg.value;
    }
 
    /*温度传感器故障消息*/
    if(msg.type == REPORT_TASK_MSG_TEMPERATURE_ERR){    
       report_fault_t fault;
       report_log.temperature = msg.value;
       report_task_build_fault(&fault,"2010","null",report_task_get_utc(), HAL_FAULT_STATUS_FAULT );
       report_task_put_fault_to_queue(&fault_queue,&fault);  
       report_task_start_fault_timer(0);
    }
 
    /*压力传感器故障消息*/
    if(msg.type == REPORT_TASK_MSG_PRESSURE_ERR){
       report_fault_t fault;   
       report_log.pressure = msg.value;
       report_task_build_fault(&fault,"2011","null",report_task_get_utc(), HAL_FAULT_STATUS_FAULT );
       report_task_put_fault_to_queue(&fault_queue,&fault);
       report_task_start_fault_timer(0);
     }
 
    /*液位传感器故障消息*/
    if(msg.type == REPORT_TASK_MSG_CAPACITY_ERR){
       report_fault_t fault;
       report_log.capacity = msg.value;
       report_task_build_fault(&fault,"2012","null",report_task_get_utc(), HAL_FAULT_STATUS_FAULT);
       report_task_put_fault_to_queue(&fault_queue,&fault); 
       report_task_start_fault_timer(0);
     }
     /*温度传感器故障解除消息*/
    if(msg.type == REPORT_TASK_MSG_TEMPERATURE_ERR_CLEAR){    
       report_fault_t fault;
       report_log.temperature = msg.value;
       report_task_build_fault(&fault,"2010","null",report_task_get_utc(), HAL_FAULT_STATUS_FAULT_CLEAR);
       report_task_put_fault_to_queue(&fault_queue,&fault); 
       report_task_start_fault_timer(0);    
    }
 
    /*压力传感器故障解除消息*/
    if(msg.type == REPORT_TASK_MSG_PRESSURE_ERR_CLEAR){
       report_fault_t fault;  
       report_log.pressure = msg.value;
       report_task_build_fault(&fault,"2011","null",report_task_get_utc(), HAL_FAULT_STATUS_FAULT_CLEAR);
       report_task_put_fault_to_queue(&fault_queue,&fault);
       report_task_start_fault_timer(0);
     }
 
    /*液位传感器故障解除消息*/
    if(msg.type == REPORT_TASK_MSG_CAPACITY_ERR_CLEAR){
       report_fault_t fault;
       report_log.capacity = msg.value;
       report_task_build_fault(&fault,"2012","null",report_task_get_utc(), HAL_FAULT_STATUS_FAULT_CLEAR);
       report_task_put_fault_to_queue(&fault_queue,&fault); 
       report_task_start_fault_timer(0);
     }
    
    /*数据上报消息*/
    if(msg.type == REPORT_TASK_MSG_REPORT_LOG){
      /*只有设备激活才可以上报数据*/
      if(report_active.is_active == true){       
         rc = report_task_report_log(URL_LOG,&report_log,report_active.sn);
         if(rc == 0){
           log_info("report log ok.\r\n");
         }else{
           log_error("report log err.\r\n");
         }
         /*重置日志上报定时器*/
         report_task_start_log_timer(device_config.report_log_interval); 
      }   
    }

   /*故障上报消息*/
   if(msg.type == REPORT_TASK_MSG_REPORT_FAULT){
      /*只有设备激活才可以上报故障*/
      if(report_active.is_active == true){    
         report_fault_t report_fault;
         uint32_t fault_time;
         /*取出故障信息*/
         rc = report_task_get_fault_from_queue(&fault_queue,&report_fault);
         if(rc == 0){
            fault_time = atoi(report_fault.time);
            if(fault_time < 1546099200){/*时间晚于2018.12.30没有同步，重新构造时间*/
               fault_time = report_task_get_utc() - fault_time;
               snprintf(report_fault.time,14,"%d",fault_time);
            }
            rc = report_task_report_fault(URL_FAULT,&report_fault,report_active.sn);
            if(rc != 0){
               log_error("report task report fault timeout.%d S later retry.\r\n",report_task_retry_delay(fault_retry) / 1000);
               report_task_start_fault_timer(report_task_retry_delay(fault_retry)); 
               if(++fault_retry >= 3){
                  fault_retry = 0;
               }
            }else{
              report_task_delete_fault_from_queue(&fault_queue);
              fault_retry = 0;
              report_task_start_fault_timer(0);
            }
          }
       }
   }
   /*处理位置信息队列消息*/
   if(msg.type == REPORT_TASK_MSG_LOCATION){ 
      os_event = osMessageGet(report_task_location_msg_q_id,0);
      if(os_event.status == osEventMessage){
         report_log.location  = *(net_location_t*)os_event.value.v; 
      }  
   
   }
                   
   }
  }
}
