#include "comm_utils.h"
#include "at.h"
#include "wifi_8710bx.h"
#include "serial.h"
#include "cmsis_os.h"
#include "utils.h"
#include "log.h"

int wifi_8710bx_serial_handle;

static serial_hal_driver_t *wifi_8710bx_serial_uart_driver = &st_serial_uart_hal_driver;

static osMutexId wifi_mutex;

#define  WIFI_8710BX_MALLOC(x)      pvPortMalloc((x))
#define  WIFI_8710BX_FREE(x)        vPortFree((x))

static at_t wifi_at;

/* 函数名：wifi_8710bx_serial_hal_init
*  功能：  串口驱动初始化
*  参数：  无
*  返回：  WIFI_ERR_OK：成功 其他：失败
*/
int wifi_8710bx_serial_hal_init()
{
  int rc;
  rc = serial_create(&wifi_8710bx_serial_handle,WIFI_8710BX_BUFFER_SIZE,WIFI_8710BX_BUFFER_SIZE * 4);
  if(rc != 0){
     log_error("wifi create serial hal err.\r\n");
     return -1;
  }
  log_debug("wifi create serial hal ok.\r\n");
  rc = serial_register_hal_driver(wifi_8710bx_serial_handle,wifi_8710bx_serial_uart_driver);
  if(rc != 0){
    log_error("wifi register serial hal driver err.\r\n");
    return -1;
  }
  log_debug("wifi register serial hal driver ok.\r\n");

  rc = serial_open(wifi_8710bx_serial_handle,2,115200,8,1);
  if(rc != 0){
     log_error("wifi open serial hal err.\r\n");
     return -1;
  }
  log_debug("wifi open serial hal ok.\r\n");

  osMutexDef(wifi_mutex);
  wifi_mutex = osMutexCreate(osMutex(wifi_mutex));
  if(wifi_mutex == NULL){
     log_error("create wifi mutex err.\r\n");
     return -1;
  }
  osMutexRelease(wifi_mutex);
  log_debug("create wifi mutex ok.\r\n");

  return WIFI_ERR_OK; 
}

/* 函数名：wifi_8710bx_reset
*  功能：  wifi软件复位 
*  参数：  无
*  返回：  WIFI_ERR_OK：成功 其他：失败
*/
int wifi_8710bx_reset(void)
{
 int rc;
 at_err_code_t ok,err;
 
 osMutexWait(wifi_mutex,osWaitForever);
 
 memset(&wifi_at,0,sizeof(wifi_at));
 strcpy(wifi_at.send,"ATSR\r\n");
 wifi_at.send_size = strlen(wifi_at.send);
 wifi_at.send_timeout = 5;
 wifi_at.recv_timeout = 1000;
 ok.str = "[ATSR] OK";
 ok.code = WIFI_ERR_OK;
 ok.next = NULL;
 err.str = "[ATSR] ERROR";
 err.code = WIFI_ERR_CMD_ERR;
 err.next = NULL;
 
 at_err_code_add(&wifi_at.err_head,&ok);
 at_err_code_add(&wifi_at.err_head,&err);
 
 rc = at_excute(wifi_8710bx_serial_handle,&wifi_at);

 
 osMutexRelease(wifi_mutex);
 return rc; 
}

/* 函数名：wifi_8710bx_set_mode
*  功能：  设置工作模式 
*  参数：  mode 模式 station or ap
*  返回：  WIFI_ERR_OK：成功 其他：失败
*/
int wifi_8710bx_set_mode(const wifi_8710bx_mode_t mode)
{
 int rc;
 at_err_code_t ok,err;
 
 osMutexWait(wifi_mutex,osWaitForever);
 
 memset(&wifi_at,0,sizeof(wifi_at));
 strcpy(wifi_at.send,mode == WIFI_8710BX_STATION_MODE ? "ATPW=1\r\n" : "ATPW=2\r\n");
 wifi_at.send_size = strlen(wifi_at.send);
 wifi_at.send_timeout = 5;
 wifi_at.recv_timeout = 6000;
 ok.str = "[ATPW] OK";
 ok.code = WIFI_ERR_OK;
 ok.next = NULL;
 err.str = "[ATPW] ERROR";
 err.code = WIFI_ERR_CMD_ERR;
 err.next = NULL;
 
 at_err_code_add(&wifi_at.err_head,&ok);
 at_err_code_add(&wifi_at.err_head,&err);
 
 rc = at_excute(wifi_8710bx_serial_handle,&wifi_at);

 
 osMutexRelease(wifi_mutex);
 return rc; 
}

/* 函数名：wifi_8710bx_set_echo
*  功能：  设置回显模式 
*  参数：  echo 回显值 打开或者关闭
*  返回：  WIFI_ERR_OK：成功 其他：失败
*/
int wifi_8710bx_set_echo(const wifi_8710bx_echo_t echo)
{
 int rc;
 at_err_code_t ok,err;
 
 osMutexWait(wifi_mutex,osWaitForever);
 
 memset(&wifi_at,0,sizeof(wifi_at));
 strcpy(wifi_at.send,echo == WIFI_8710BX_ECHO_ON ? "ATSE=1\r\n" : "ATSE=0\r\n");
 wifi_at.send_size = strlen(wifi_at.send);
 wifi_at.send_timeout = 5;
 wifi_at.recv_timeout = 1000;
 ok.str = "[ATSE] OK";
 ok.code = WIFI_ERR_OK;
 ok.next = NULL;
 err.str = "[ATSE] ERROR";
 err.code = WIFI_ERR_CMD_ERR;
 err.next = NULL;
 
 at_err_code_add(&wifi_at.err_head,&ok);
 at_err_code_add(&wifi_at.err_head,&err);
 
 rc = at_excute(wifi_8710bx_serial_handle,&wifi_at);

 
 osMutexRelease(wifi_mutex);
 return rc; 
}


/* 函数名：wifi_8710bx_config_ap
*  功能：  配置AP参数 
*  参数：  ap AP参数结构指针
*  返回：  WIFI_ERR_OK：成功 其他：失败
*/
int wifi_8710bx_config_ap(const wifi_8710bx_ap_config_t *ap)
{
 int rc;
 at_err_code_t ok,err;
 
 osMutexWait(wifi_mutex,osWaitForever);
 
 memset(&wifi_at,0,sizeof(wifi_at));
 snprintf(wifi_at.send,AT_SEND_BUFFER_SIZE,"ATPA=%s,%s,%d,%d\r\n",ap->ssid,ap->passwd,ap->chn,(int)ap->hidden);
 wifi_at.send_size = strlen(wifi_at.send);
 wifi_at.send_timeout = 5;
 wifi_at.recv_timeout = 5000;
 ok.str = "[ATPA] OK";
 ok.code = WIFI_ERR_OK;
 ok.next = NULL;
 err.str = "[ATPA] ERROR";
 err.code = WIFI_ERR_CMD_ERR;
 err.next = NULL;
 
 at_err_code_add(&wifi_at.err_head,&ok);
 at_err_code_add(&wifi_at.err_head,&err);
 
 rc = at_excute(wifi_8710bx_serial_handle,&wifi_at);

 
 osMutexRelease(wifi_mutex);
 return rc; 
}

/* 函数名：wifi_8710bx_get_ap_rssi
*  功能：  扫描出指定ap的rssi值 
*  参数：  ssid ap的ssid名称
*  参数：  rssi ap的rssi的指针
*  返回：  WIFI_ERR_OK：成功 其他：失败
*/
int wifi_8710bx_get_ap_rssi(const char *ssid,int *rssi)
{
 int rc;
 int str_rc;
 char *ssid_str,*tolon_str;
 at_err_code_t ok,err;
 
 osMutexWait(wifi_mutex,osWaitForever);
 
 memset(&wifi_at,0,sizeof(wifi_at));
 strcpy(wifi_at.send,"ATWS\r\n");
 wifi_at.send_size = strlen(wifi_at.send);
 wifi_at.send_timeout = 5;
 wifi_at.recv_timeout = 10000;
 ok.str = "[ATWS] OK";
 ok.code = WIFI_ERR_OK;
 ok.next = NULL;
 err.str = "[ATWS] ERROR";
 err.code = WIFI_ERR_CMD_ERR;
 err.next = NULL;
 
 at_err_code_add(&wifi_at.err_head,&ok);
 at_err_code_add(&wifi_at.err_head,&err);
 
 rc = at_excute(wifi_8710bx_serial_handle,&wifi_at);
 
 if(rc == WIFI_ERR_OK){
 ssid_str = strstr(wifi_at.recv,ssid);
 if(ssid_str == NULL){
 /*没有发现指定ssid rssi默认为0*/
 *rssi = 0;
 goto err_exit;
 }
 /*ssid后第3个tolon是rssi值*/
 str_rc = utils_get_str_addr_by_num((char*)ssid_str,",",3,&tolon_str);
 if(str_rc == 0){
    *rssi = atoi(tolon_str + 1);
 }else{
    rc = WIFI_ERR_UNKNOW;
 }
 }
 
err_exit:
 
 osMutexRelease(wifi_mutex);
 return rc;
}

/* 函数名：wifi_8710bx_connect_ap
*  功能：  连接指定的ap
*  参数：  ssid ap的ssid名称
*  参数：  passwd ap的passwd
*  返回：  WIFI_ERR_OK：成功 其他：失败
*/
int wifi_8710bx_connect_ap(const char *ssid,const char *passwd)
{
 int rc;
 at_err_code_t ok,err;
 
 osMutexWait(wifi_mutex,osWaitForever);
 
 memset(&wifi_at,0,sizeof(wifi_at));
 snprintf(wifi_at.send,AT_SEND_BUFFER_SIZE,"ATPN=%s,%s\r\n",ssid,passwd);
 wifi_at.send_size = strlen(wifi_at.send);
 wifi_at.send_timeout = 5;
 wifi_at.recv_timeout = 10000;
 ok.str = "[ATPN] OK";
 ok.code = WIFI_ERR_OK;
 ok.next = NULL;
 err.str = "[ATPN] ERROR";
 err.code = WIFI_ERR_CMD_ERR;
 err.next = NULL;
 
 at_err_code_add(&wifi_at.err_head,&ok);
 at_err_code_add(&wifi_at.err_head,&err);
 
 rc = at_excute(wifi_8710bx_serial_handle,&wifi_at);
 
 
 osMutexRelease(wifi_mutex);
 return rc;
}

/* 函数名：wifi_8710bx_disconnect_ap
*  功能：  断开指定的ap连接
*  参数：  无
*  返回：  WIFI_ERR_OK：成功 其他：失败
*/
int wifi_8710bx_disconnect_ap(void)
{
 int rc;
 at_err_code_t ok,err;
 
 osMutexWait(wifi_mutex,osWaitForever);
 
 memset(&wifi_at,0,sizeof(wifi_at));
 snprintf(wifi_at.send,AT_SEND_BUFFER_SIZE,"ATWD\r\n");
 wifi_at.send_size = strlen(wifi_at.send);
 wifi_at.send_timeout = 5;
 wifi_at.recv_timeout = 2000;
 ok.str = "[ATWD] OK";
 ok.code = WIFI_ERR_OK;
 ok.next = NULL;
 err.str = "[ATWD] ERROR";
 err.code = WIFI_ERR_CMD_ERR;
 err.next = NULL;
 
 at_err_code_add(&wifi_at.err_head,&ok);
 at_err_code_add(&wifi_at.err_head,&err);
 
 rc = at_excute(wifi_8710bx_serial_handle,&wifi_at);
 
 
 osMutexRelease(wifi_mutex);
 return rc;
}
 
/* 函数名：wifi_8710bx_dump_wifi_device_info
*  功能：  dump wifi设备信息
*  参数：  buffer  源buffer
*  参数：  wifi_device wifi设备指针
*  返回：  WIFI_ERR_OK：成功 其他：失败
*/
static int wifi_8710bx_dump_wifi_device_info(const char *buffer,wifi_8710bx_device_t *wifi_device)
{
 char *pos;
 int  str_rc;
 
 if(strstr(buffer,"AP")){
    wifi_device->mode = WIFI_8710BX_AP_MODE; 
 }else if(strstr(buffer,"STA")){
    wifi_device->mode = WIFI_8710BX_STATION_MODE;  
 }else{
    return WIFI_ERR_UNKNOW;
 }

 /*找到第1个break位置后的ssid*/
 str_rc = utils_get_str_value_by_num((char *)buffer,wifi_device->ap.ssid,",",1);
 if(str_rc != 0){
    return WIFI_ERR_UNKNOW;
 }
/*找到第2个break位置后chn位置*/
 str_rc = utils_get_str_addr_by_num((char *)buffer,",",2,&pos);
 if(str_rc != 0){
    return WIFI_ERR_UNKNOW;
 }
 wifi_device->ap.chn = atoi(pos + 1);
 
 /*找到第3个break位置后的sec*/
 str_rc = utils_get_str_value_by_num((char *)buffer,wifi_device->ap.sec,",",3);
 if(str_rc != 0){
    return WIFI_ERR_UNKNOW;
 }
 /*找到第4个break位置后的passwd*/
 str_rc = utils_get_str_value_by_num((char *)buffer,wifi_device->ap.passwd,",",4);
 if(str_rc != 0){
    return WIFI_ERR_UNKNOW;
 }
 /*找到第5个break位置后的mac*/
 str_rc = utils_get_str_value_by_num((char *)buffer,wifi_device->device.mac,",",5);
 if(str_rc != 0){
    return WIFI_ERR_UNKNOW;
 }
 /*找到第6个break位置后的ip*/
 str_rc = utils_get_str_value_by_num((char *)buffer,wifi_device->device.ip,",",6);
 if(str_rc != 0){
    return WIFI_ERR_UNKNOW;
 }

 /*找到gateway位置*/
 str_rc = utils_get_str_value_by_num((char *)buffer,wifi_device->device.gateway,",",7);
 if(str_rc != 0){
    return WIFI_ERR_UNKNOW;
 }

 log_debug("dump wifi info ok.\r\n");
 return WIFI_ERR_OK;    
}

/* 函数名：wifi_8710bx_get_wifi_device
*  功能：  获取wifi设备信息
*  参数：  wifi_device 设备指针
*  返回：  WIFI_ERR_OK：成功 其他：失败
*/
int wifi_8710bx_get_wifi_device(wifi_8710bx_device_t *wifi_device)
{
 int rc;
 at_err_code_t ok,err;
 
 osMutexWait(wifi_mutex,osWaitForever);
 
 memset(&wifi_at,0,sizeof(wifi_at));
 snprintf(wifi_at.send,AT_SEND_BUFFER_SIZE,"ATW?\r\n");
 wifi_at.send_size = strlen(wifi_at.send);
 wifi_at.send_timeout = 5;
 wifi_at.recv_timeout = 5000;
 ok.str = "[ATW?] OK";
 ok.code = WIFI_ERR_OK;
 ok.next = NULL;
 err.str = "[ATW?] ERROR";
 err.code = WIFI_ERR_CMD_ERR;
 err.next = NULL;
 
 at_err_code_add(&wifi_at.err_head,&ok);
 at_err_code_add(&wifi_at.err_head,&err);
 
 rc = at_excute(wifi_8710bx_serial_handle,&wifi_at);
 if(rc == WIFI_ERR_OK){
 rc = wifi_8710bx_dump_wifi_device_info(wifi_at.recv,wifi_device);
 }
 
 
 osMutexRelease(wifi_mutex);
 return rc;
}

/* 函数名：wifi_8710bx_dump_connection_info
*  功能：  服务端的连接信息
*  参数：  connect 连接指针
*  参数：  buffer  源buffer
*  返回：  WIFI_ERR_OK：成功 其他：失败
*/
static int wifi_8710bx_dump_connection_info(const char *buffer,wifi_8710bx_connection_list_t *connection_list)
{

 char *pos,*start,*end;
 int  str_rc,size;
 char temp[8];
 
 connection_list->cnt = 0;
 for(int8_t i = 0;i< WIFI_8710BX_CONNECTION_CNT_MAX;i++){
    /*找到conn id标志*/
     pos = strstr(buffer,"con_id:"); 
     if(pos == NULL){
        log_debug("dump conn info completed.\r\n");
        return WIFI_ERR_OK;
      }
    buffer = pos + strlen("con_id:");
    connection_list->connection[i].conn_id = atoi(buffer);
    /*获取第1个break位置之后的字符串*/
    str_rc = utils_get_str_value_by_num((char *)buffer,temp,",",1);
    if(str_rc != 0){
       return WIFI_ERR_UNKNOW;
    }
    if(strcmp(temp,"server") == 0){
       connection_list->connection[i].type = WIFI_8710BX_CONNECTION_TYPE_SERVER;
    }else if(strcmp(temp,"seed") == 0){
       connection_list->connection[i].type = WIFI_8710BX_CONNECTION_TYPE_SEED;
    }else {
       connection_list->connection[i].type = WIFI_8710BX_CONNECTION_TYPE_CLIENT;
    }
    /*获取第2个break位置之后的字符串*/
    str_rc = utils_get_str_value_by_num((char *)buffer,temp,",",2);
    if(str_rc != 0){
       return WIFI_ERR_UNKNOW;
    }
    if(strcmp(temp,"tcp") == 0){
       connection_list->connection[i].protocol = WIFI_8710BX_NET_PROTOCOL_TCP;
    }else {
       connection_list->connection[i].protocol = WIFI_8710BX_NET_PROTOCOL_UDP;
    }
    /*找到address的字符串*/
    pos = strstr(buffer,"address:");
    if(pos == NULL){
       return WIFI_ERR_UNKNOW;
    }
    end = strstr(pos,",");
    if(end == NULL){
       return WIFI_ERR_UNKNOW;
    }
    start = pos + strlen("address:");
    size = end - start > WIFI_8710BX_IP_STR_LEN -1 ? WIFI_8710BX_IP_STR_LEN -1 : end - start;
    memcpy(connection_list->connection[i].ip,start,size);
    connection_list->connection[i].ip[size] = '\0';   

   /*找到port的值*/
    pos = strstr(buffer,"port:");
    if(pos == NULL){
       return WIFI_ERR_UNKNOW;
    }
    start = pos + strlen("port:");
    connection_list->connection[i].port = atoi(start);
    /*找到socket的值*/
    pos = strstr(buffer,"socket:");
    if(pos == NULL){
       return WIFI_ERR_UNKNOW;
    }
    start = pos + strlen("socket:");
    connection_list->connection[i].socket_id = atoi(start);
    connection_list->cnt = i + 1;
   }
 
   return WIFI_ERR_OK; 
}

/* 函数名：wifi_8710bx_get_connection
*  功能：  获取连接信息
*  参数：  connection_list 连接列表指针
*  返回：  WIFI_ERR_OK：成功 其他：失败
*/
int wifi_8710bx_get_connection(wifi_8710bx_connection_list_t *connection_list)
{
 int rc;
 at_err_code_t ok,err;
 
 osMutexWait(wifi_mutex,osWaitForever);
 
 memset(&wifi_at,0,sizeof(wifi_at));
 strcpy(wifi_at.send,"ATPI\r\n");
 wifi_at.send_size = strlen(wifi_at.send);
 wifi_at.send_timeout = 5;
 wifi_at.recv_timeout = 2000;
 ok.str = "[ATPI] OK";
 ok.code = WIFI_ERR_OK;
 ok.next = NULL;
 err.str = "[ATPI] ERROR";
 err.code = WIFI_ERR_CMD_ERR;
 err.next = NULL;
 
 at_err_code_add(&wifi_at.err_head,&ok);
 at_err_code_add(&wifi_at.err_head,&err);
 
 rc = at_excute(wifi_8710bx_serial_handle,&wifi_at);
 if(rc == WIFI_ERR_OK){
 rc = wifi_8710bx_dump_connection_info(wifi_at.recv,connection_list);
 }
 
 
 osMutexRelease(wifi_mutex);
 return rc;
}

/* 函数名：wifi_8710bx_dump_conn_id
*  功能：  获取连接id
*  参数：  buffer 源数据buffer
*  返回：  WIFI_ERR_OK：成功 其他：失败
*/
static int wifi_8710bx_dump_conn_id(const char *buffer)
{
char *pos;
int  conn_id;

pos = strstr(buffer,"con_id=") + strlen("con_id=");
if(pos == NULL){
return WIFI_ERR_UNKNOW;
}
conn_id = atoi(pos);
  
return conn_id;
}

/* 函数名：wifi_8710bx_open_server
*  功能：  创建服务端
*  参数：  port 服务端本地端口
*  参数：  protocol   服务端网络协议类型
*  返回：  连接句柄，大于WIFI_ERR_OK：成功 其他：失败
*/
int wifi_8710bx_open_server(const uint16_t port,const wifi_8710bx_net_protocol_t protocol)
{
 int rc;
 at_err_code_t ok,err;
 
 osMutexWait(wifi_mutex,osWaitForever);
 
 memset(&wifi_at,0,sizeof(wifi_at));
 snprintf(wifi_at.send,AT_SEND_BUFFER_SIZE,"ATPS=%s,%d\r\n",protocol == WIFI_8710BX_NET_PROTOCOL_TCP ? "0" : "1",port);
 wifi_at.send_size = strlen(wifi_at.send);
 wifi_at.send_timeout = 50;
 wifi_at.recv_timeout = 10000;
 ok.str = "[ATPS] OK";
 ok.code = WIFI_ERR_OK;
 ok.next = NULL;
 err.str = "[ATPS] ERROR";
 err.code = WIFI_ERR_CMD_ERR;
 err.next = NULL;
 
 at_err_code_add(&wifi_at.err_head,&ok);
 at_err_code_add(&wifi_at.err_head,&err);
 
 rc = at_excute(wifi_8710bx_serial_handle,&wifi_at);
 if(rc == WIFI_ERR_OK){
 rc = wifi_8710bx_dump_conn_id(wifi_at.recv);  
 }
 
 
 osMutexRelease(wifi_mutex);
 return rc;
}

/* 函数名：wifi_8710bx_open_client
*  功能：  创建客户端
*  参数：  host        服务端域名
*  参数：  remote_port 服务端端口
*  参数：  protocol    网络协议类型
*  返回：  连接句柄，大于WIFI_ERR_OK：成功 其他：失败
*/
int wifi_8710bx_open_client(const char *host,const uint16_t remote_port,const wifi_8710bx_net_protocol_t protocol)
{
 int rc;
 at_err_code_t ok,err;
 
 osMutexWait(wifi_mutex,osWaitForever);
 
 memset(&wifi_at,0,sizeof(wifi_at));
 snprintf(wifi_at.send,AT_SEND_BUFFER_SIZE,"ATPC=%s,%s,%d\r\n",protocol == WIFI_8710BX_NET_PROTOCOL_TCP ? "0" : "1",host,remote_port);
 wifi_at.send_size = strlen(wifi_at.send);
 wifi_at.send_timeout = 50;
 wifi_at.recv_timeout = 10000;
 ok.str = "[ATPC] OK";
 ok.code = WIFI_ERR_OK;
 ok.next = NULL;
 err.str = "[ATPC] ERROR";
 err.code = WIFI_ERR_CMD_ERR;
 err.next = NULL;
 
 at_err_code_add(&wifi_at.err_head,&ok);
 at_err_code_add(&wifi_at.err_head,&err);
 
 rc = at_excute(wifi_8710bx_serial_handle,&wifi_at);
 if(rc == WIFI_ERR_OK){
 rc = wifi_8710bx_dump_conn_id(wifi_at.recv);  
 }
 
 
 osMutexRelease(wifi_mutex);
 return rc;
}

/* 函数名：wifi_8710bx_close
*  功能：  关闭客户或者服务端域      
*  参数：  conn_id     连接句柄
*  返回：  WIFI_ERR_OK：成功 其他：失败
*/
int wifi_8710bx_close(const int conn_id)
{
 int rc;
 at_err_code_t ok,err;
 
 osMutexWait(wifi_mutex,osWaitForever);
 
 memset(&wifi_at,0,sizeof(wifi_at));
 snprintf(wifi_at.send,AT_SEND_BUFFER_SIZE,"ATPD=%d\r\n",conn_id);
 wifi_at.send_size = strlen(wifi_at.send);
 wifi_at.send_timeout = 5;
 wifi_at.recv_timeout = 2000;
 ok.str = "[ATPD] OK";
 ok.code = WIFI_ERR_OK;
 ok.next = NULL;
 err.str = "[ATPD] ERROR";
 err.code = WIFI_ERR_CMD_ERR;
 err.next = NULL;
 
 at_err_code_add(&wifi_at.err_head,&ok);
 at_err_code_add(&wifi_at.err_head,&err);
 
 rc = at_excute(wifi_8710bx_serial_handle,&wifi_at);

 
 osMutexRelease(wifi_mutex);
 return rc;
}

/* 函数名：wifi_8710bx_send
*  功能：  发送数据
*  参数：  conn_id 连接句柄
*  参数：  data    数据buffer
*  参数：  size    需要发送的数量
*  返回：  >=0：成功发送的数据 其他：失败
*/
int wifi_8710bx_send(int conn_id,const char *data,const int size)
{
 int rc;
 int used_size,space_size,send_size;
 
 at_err_code_t ok,err;
 
 osMutexWait(wifi_mutex,osWaitForever);
 
 memset(&wifi_at,0,sizeof(wifi_at));
 snprintf(wifi_at.send,AT_SEND_BUFFER_SIZE,"ATPT=%d,%d:",size,conn_id);
 used_size = strlen(wifi_at.send);
 space_size = AT_SEND_BUFFER_SIZE - used_size;
 send_size = space_size < size ? space_size :size;
 memcpy(wifi_at.send + used_size,data,send_size);
 wifi_at.send_size = used_size + send_size;
 wifi_at.send_timeout = send_size / 8 + 10;
 wifi_at.recv_timeout = 10000;
 ok.str = "[ATPT] OK";
 ok.code = WIFI_ERR_OK;
 ok.next = NULL;
 err.str = "[ATPT] ERROR";
 err.code = WIFI_ERR_CMD_ERR;
 err.next = NULL;
 
 at_err_code_add(&wifi_at.err_head,&ok);
 at_err_code_add(&wifi_at.err_head,&err);
 
 rc = at_excute(wifi_8710bx_serial_handle,&wifi_at);
 if(rc == WIFI_ERR_OK){
 rc = send_size;
 }

 
 osMutexRelease(wifi_mutex);
 return rc;
}

/* 函数名：wifi_8710bx_recv
*  功能：  接收数据
*  参数：  buffer_dst  目的buffer
*  参数：  buffer_src  源buffer
*  参数：  buffer_size 目的buffer大小
*  返回：  >= 0：接收到的数据量 其他：失败
*/
int wifi_8710bx_recv(const int conn_id,char *buffer,int size)
{
 int rc;
 int data_size;
 char *size_str,*data_start_str; 
 at_err_code_t ok,err;
 
 osMutexWait(wifi_mutex,osWaitForever);
 
 memset(&wifi_at,0,sizeof(wifi_at));
 snprintf(wifi_at.send,AT_SEND_BUFFER_SIZE,"ATPR=%d,%d\r\n",conn_id,size > 1600 ? 1600 : size);
 wifi_at.send_size = strlen(wifi_at.send);
 wifi_at.send_timeout = 5;
 wifi_at.recv_timeout = 2000;
 ok.str = "[ATPR] OK";
 ok.code = WIFI_ERR_OK;
 ok.next = NULL;
 err.str = "[ATPR] ERROR";
 err.code = WIFI_ERR_CMD_ERR;
 err.next = NULL;
 
 at_err_code_add(&wifi_at.err_head,&ok);
 at_err_code_add(&wifi_at.err_head,&err);
 
 rc = at_excute(wifi_8710bx_serial_handle,&wifi_at);
 if(rc == WIFI_ERR_OK){
    size_str = strstr(wifi_at.recv,"[ATPR] OK") + strlen("[ATPR] OK") + 1;
    data_size = atoi(size_str);
    data_size = data_size > size ? size : data_size ;
 if(data_size > 0){
    data_start_str = strstr(size_str,":");
    if(data_start_str){
       memcpy(buffer,data_start_str + 1,data_size);
    }else{
       rc = WIFI_ERR_UNKNOW;
       goto err_exit;
    }
 }
 rc = data_size;  
 }
 
err_exit:
 
 osMutexRelease(wifi_mutex);
 return rc;
}
