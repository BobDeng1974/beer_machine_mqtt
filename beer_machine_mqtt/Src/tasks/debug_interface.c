#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"
#include "string.h"
#include "connection.h"
#include "http_client.h"
#include "cJSON.h"
#include "log.h"
#define  LOG_MODULE_LEVEL    LOG_LEVEL_DEBUG
#define  LOG_MODULE_NAME     "[ut]"


 
#define  DEBUG_GET_AP_RSSI_STR             "get rssi\r\n"
#define  DEBUG_CONNECT_AP_STR              "connect ap\r\n"
#define  DEBUG_DISCONNECT_AP_STR           "disconnect ap\r\n"
#define  DEBUG_SET_WIFI_STATION_MODE_STR   "set station\r\n"
#define  DEBUG_SET_WIFI_AP_MODE_STR        "set ap\r\n"
#define  DEBUG_SET_AP_MODE_STR             "set ap mode\r\n"
#define  DEBUG_SET_SERVER_STR              "set server\r\n"
#define  DEBUG_WAIT_CONNECT_STR            "wait connect\r\n"
#define  DEBUG_WIFI_INFORMATION_STR        "wifi info\r\n"
#define  DEBUG_CONN_INFORMATION_STR        "conn info\r\n"
#define  DEBUG_TCP_CONNECT_STR             "connect tcp\r\n"
#define  DEBUG_TCP_DISCONNECT_STR          "disconnect tcp\r\n"
#define  DEBUG_TCP_SEND_STR                "tcp send\r\n"
#define  DEBUG_TCP_RECV_STR                "tcp recv\r\n"
#define  DEBUG_SERVER_SEND_STR             "server send\r\n"
#define  DEBUG_SERVER_RECV_STR             "server recv\r\n"
#define  DEBUG_HTTP_POST_STR               "http post\r\n"
#define  DEBUG_HTTP_DOWNLOAD_STR           "http download\r\n"

#define  DEBUG_AP_CONNECT_SSID_STR             "wkxboot"
#define  DEBUG_AP_CONNECT_PASSWD_STR           "wkxboot6666"

#define  DEBUG_TCP_HOST_NAME_STR               "47.74.35.0"//"v2236p1176.imwork.net"
#define  DEBUG_TCP_PATH_STR                    "/hellowkxboot.txt"//"/common/service.execute.json"
#define  DEBUG_TCP_PORT                        8888//22132
#define  DEBUG_TCP_LOCAL_PORT                  1234

#define  DEBUG_TCP_SEND_DATA_STR               "{\"_call\":\"shop.shopService.wulong\",\"_params\":{\"name\":\"zhangsan\",\"loginToken\":\"{{token}}\"}}"



int rssi;
wifi_8710bx_ap_t ap_connect;

int tcp_conn_id;
int server_conn_id;
int recv_buffer_len;

osThreadId  wifi_8710bx_serial_task_handle;

uint32_t log_time()
{
return osKernelSysTick();  
}


void wifi_8710bx_unit_test(void const * argument)
{
  int rc;
  char cmd[20];
  uint8_t cmd_size;
  
  char res_data[301];
  http_client_request_t req;
  http_client_response_t res;
  
  req.body = "wkxboot";
  req.body_size = 7;
  req.range.start = 0,
  req.range.size = 100;

  res.body = res_data;
  res.body_buffer_size = 300;
  res.timeout = 5000;
  connection_init();
  
  for(;;)
  {
    
  cmd_size = SEGGER_RTT_Read(0,cmd,20);
  if(cmd_size > 0){
  cmd[cmd_size] = '\0';
  log_debug("++++cmd start.\r\n");
  
  if(strcmp((char *)cmd,DEBUG_GET_AP_RSSI_STR)== 0){
  rc = wifi_8710bx_get_ap_rssi(DEBUG_AP_CONNECT_SSID_STR,&rssi);  
  if(rc == 0){
  log_debug("debug get rssi:%d. ok\r\n",rssi);
  } else{
  log_debug("debug get rssi err.\r\n");
  }
  }
    
  if(strcmp((char *)cmd,DEBUG_CONNECT_AP_STR) == 0){
  strcpy(ap_connect.ssid, DEBUG_AP_CONNECT_SSID_STR);
  strcpy(ap_connect.passwd,DEBUG_AP_CONNECT_PASSWD_STR);
  
  rc = wifi_8710bx_connect_ap(DEBUG_AP_CONNECT_SSID_STR,DEBUG_AP_CONNECT_PASSWD_STR);
  if(rc == 0){
  log_debug("debug connect ap ok.\r\n");
  }else{
  log_error("debug connect ap error.\r\n");
  }
    
  }
  if(strcmp((char *)cmd,DEBUG_DISCONNECT_AP_STR) == 0){

  rc = wifi_8710bx_disconnect_ap();
  if(rc == 0){
  log_debug("disconnect ap ok.\r\n");
  }else{
  log_error("disconnect ap error.\r\n");
  }
    
  }
  
  if(strcmp((char *)cmd,DEBUG_SET_WIFI_STATION_MODE_STR)== 0){
  wifi_8710bx_mode_t wifi_mode = WIFI_8710BX_STATION_MODE;
  
  rc = wifi_8710bx_set_mode(wifi_mode);
  if(rc == 0){
  log_debug("debug set wifi station mode ok.\r\n");
  }else{
  log_error("debug  wifi station mode error.\r\n");
  }
    
  }

  if(strcmp((char *)cmd,DEBUG_SET_WIFI_AP_MODE_STR) == 0){

  wifi_8710bx_mode_t wifi_mode = WIFI_8710BX_AP_MODE;
  rc = wifi_8710bx_set_mode(wifi_mode);
  if(rc == 0){
  log_debug("set wifi ap mode ok.\r\n");
  }else{
  log_error("set wifi ap mode error.\r\n");
  }
    
  }

  if(strcmp((char *)cmd,DEBUG_TCP_CONNECT_STR) == 0){
  
  tcp_conn_id = wifi_8710bx_open_client(DEBUG_TCP_HOST_NAME_STR,DEBUG_TCP_PORT,DEBUG_TCP_LOCAL_PORT,WIFI_8710BX_NET_PROTOCOL_TCP);
  if(tcp_conn_id >= 0){
  log_debug("connect tcp ok conn_id:%d.\r\n",tcp_conn_id);
  }else{
  log_error("connect tcp error.\r\n");
  }
    
  }
  
  if(strcmp((char *)cmd,DEBUG_TCP_DISCONNECT_STR) == 0){

  rc = wifi_8710bx_close(tcp_conn_id);
  if(rc == 0){
  log_debug("disconnect tcp ok.\r\n");
  }else{
  log_error("disconnect tcp error.\r\n");
  }
    
  }
  
  if(strcmp((char *)cmd,DEBUG_TCP_SEND_STR) == 0){

  rc = wifi_8710bx_send(tcp_conn_id,DEBUG_TCP_SEND_DATA_STR,strlen(DEBUG_TCP_SEND_DATA_STR));
  if(rc == 0){
  log_debug("send ok.\r\n");
  }else{
  log_error("send error.\r\n");
  }
    
  }
  
  if(strcmp((char *)cmd,DEBUG_TCP_RECV_STR) == 0){

  char recv[30];
  rc = wifi_8710bx_recv(tcp_conn_id,recv,28);
  if(rc >= 0){
  log_debug("recv data ok:%s.\r\n",recv);
  }else{
  log_error("recv data error.\r\n");
  }
    
  }
  
  if(strcmp((char *)cmd,DEBUG_SET_AP_MODE_STR) == 0){
  wifi_8710bx_ap_t ap;
  strcpy(ap.ssid,"hello wkxboot");
  strcpy(ap.passwd,"1122334455");
  ap.chn =5;
  ap.hidden = false;
  
  rc = wifi_8710bx_config_ap(&ap);
  if(rc >= 0){
  log_debug("set ap mode ok.\r\n");
  }else{
  log_error("set ap mode error.\r\n");
  }
    
  }
  
  if(strcmp((char *)cmd,DEBUG_SET_SERVER_STR) == 0){
  
  server_conn_id = wifi_8710bx_open_server(DEBUG_TCP_LOCAL_PORT,WIFI_8710BX_NET_PROTOCOL_TCP);
  if(server_conn_id >= 0){
  log_debug("server  ok\r\n");
  }else{
  log_error("server err.\r\n");
  }
    
  }
  
  if(strcmp((char *)cmd,DEBUG_SERVER_SEND_STR) == 0){
  
  rc = wifi_8710bx_send(server_conn_id ,DEBUG_TCP_SEND_DATA_STR,strlen(DEBUG_TCP_SEND_DATA_STR));
  if(rc == 0){
  log_debug("server  ok\r\n");
  }else{
  log_error("server err.\r\n");
  }
    
  }
  
  if(strcmp((char *)cmd,DEBUG_SERVER_RECV_STR) == 0){
  
  char recv[30];
  rc = wifi_8710bx_recv(server_conn_id,recv,28);
  if(rc >= 0){
  log_debug("server  ok\r\n");
  }else{
  log_error("server err.\r\n");
  }
    
  }
  
  
  if(strcmp((char *)cmd,DEBUG_CONN_INFORMATION_STR) == 0){
  wifi_8710bx_connection_list_t connection_list;

  rc = wifi_8710bx_get_connection(&connection_list);
  if(rc == 0){
  log_debug("clinet connect ok.\r\n");
  for(uint8_t i= 0;i<connection_list.cnt;i++){
    if(connection_list.connection[i].type == WIFI_8710BX_CONNECTION_TYPE_SEED){
    server_conn_id = connection_list.connection[i].conn_id;
    }
  log_debug("type:%d.\r\nconn_id:%d.\r\nprotocol:%d.\r\nip:%s.\r\nport:%d.\r\n"
   ,connection_list.connection[i].type
   ,connection_list.connection[i].conn_id
   ,connection_list.connection[i].protocol
   ,connection_list.connection[i].ip
   ,connection_list.connection[i].port);
  }
  }else{
  log_error("no client connect.\r\n");
  }
    
  }
  
  if(strcmp((char *)cmd,DEBUG_WIFI_INFORMATION_STR) == 0){
  wifi_8710bx_device_t wifi_device;
  rc = wifi_8710bx_get_wifi_device(&wifi_device);
  if(rc == 0){
   log_debug("wifi information ok.\r\nmode:%d\r\nssid:%s\r\npasswd:%s\r\nchn:%d\r\nsec:%s\r\nip:%s\r\nmac:%s\r\ngateway:%s\r\n",
                                wifi_device.mode,
                                wifi_device.ap.ssid,
                                wifi_device.ap.passwd,
                                wifi_device.ap.chn,
                                wifi_device.ap.sec,
                                wifi_device.device.ip,
                                wifi_device.device.mac,
                                wifi_device.device.gateway);
  }else{
  log_error("wifi information error.\r\n");
  }
    
  }

  
  if(strcmp((char *)cmd,DEBUG_HTTP_DOWNLOAD_STR) == 0){
  const char * url = "http://47.74.35.0:8888/hellowkxboot.txt"; 
  rc = http_client_download(url,&req,&res);
  if(rc == 0){
  log_debug("download data ok.\r\n");
  for(uint16_t i=0;i<res.body_size;i++){
  log_array("[%d]:<%d>\r\n",i,res_data[i]);
  osDelay(1);
  }
  }else{
  log_error("download data error.\r\n");
  }   
  }
  
  if(strcmp((char *)cmd,DEBUG_HTTP_POST_STR ) == 0){
  const char * url = "http://syll-test.mymlsoft.com:8083/common/service.execute.json";
  req.body = DEBUG_TCP_SEND_DATA_STR;
  req.body_size = strlen(DEBUG_TCP_SEND_DATA_STR);
  rc = http_client_post(url,&req,&res);
  if(rc == 0){

  cJSON *json;
  json = cJSON_Parse(res.body);
  if(json){
  //log_debug("%s",cJSON_Print(json)); 
  log_debug("post data ok.\r\n");
  }
  
  }else{
  log_error("post data error.\r\n");
  }   
  }  
    
    
 
#define  DEBUG_GSM_IS_READY                 "gsm is rdy\r\n"
#define  DEBUG_GSM_IS_SIM_CARD_READY        "gsm is sim rdy\r\n"
#define  DEBUG_GSM_SET_APN                  "gsm set apn\r\n"
#define  DEBUG_GSM_GET_APN                  "gsm get apn\r\n"
#define  DEBUG_GSM_SET_ACTIVE               "gsm set act\r\n"  
#define  DEBUG_GSM_GET_ACTIVE               "gsm get act\r\n"  
#define  DEBUG_GSM_SET_ATTACH               "gsm set att\r\n"  
#define  DEBUG_GSM_GET_ATTACH               "gsm get att\r\n"  
#define  DEBUG_GSM_SET_OPERATOR             "gsm set optor\r\n"  
#define  DEBUG_GSM_GET_OPERATOR             "gsm get optor\r\n"  
#define  DEBUG_GSM_GET_IMEI                 "gsm get imei\r\n"  
#define  DEBUG_GSM_GET_SN                   "gsm get sn\r\n"  
  
#define  DEBUG_GSM_SET_CONNECT_MODE         "gsm conn mode\r\n"
#define  DEBUG_GSM_SET_TRANSPARENT_MODE     "gsm trans mode\r\n"
#define  DEBUG_GSM_SET_RECV_BUFFER          "gsm recv buffer\r\n"
#define  DEBUG_GSM_GET_RECV_SIZE            "gsm recv size\r\n"
#define  DEBUG_GSM_TCP_CONNECT              "gsm tcp conn\r\n"
#define  DEBUG_GSM_TCP_SEND                 "gsm tcp send\r\n"
#define  DEBUG_GSM_TCP_RECV                 "gsm tcp recv\r\n"
#define  DEBUG_GSM_GET_RECV_SIZE            "gsm recv size\r\n"
#define  DEBUG_GSM_TCP_CONNECT              "gsm tcp conn\r\n"
#define  DEBUG_GSM_TCP_CLOSE                "gsm tcp close\r\n"
#define  DEBUG_GSM_TCP_STATUS               "gsm tcp status\r\n"

  if(strcmp((char *)cmd,DEBUG_GSM_IS_READY) == 0){
  if( gsm_m6312_is_ready() == 0){
    log_debug("gsm m6312 is ready.\r\n");  
    }else{
    log_error("gsm m6312 is not ready.\r\n");   
    }   
   }
   
    if(strcmp((char *)cmd,DEBUG_GSM_IS_SIM_CARD_READY) == 0){
     sim_card_status_t sim_status;
     
    if( gsm_m6312_is_sim_card_ready(&sim_status) == 0){
    log_debug("get sim status ok.\r\n");  
    }else{
    log_error("get sim status err.\r\n");   
    }
     
   }
    
  
    if(strcmp((char *)cmd,DEBUG_GSM_SET_APN) == 0){   
    if(gsm_m6312_gprs_set_apn(1,GSM_GPRS_APN_CMNET) == 0){
    log_debug("gsm set apn ok.\r\n");  
    }else{
    log_error("gsm set apn err.\r\n");   
    }    
   }
   
    if(strcmp((char *)cmd,DEBUG_GSM_GET_APN) == 0){  
    gsm_gprs_apn_t apn;
   
    if(gsm_m6312_gprs_get_apn(1,&apn) == 0){
    log_debug("gsm get apn ok.\r\n");  
    }else{
    log_error("gsm get apn err.\r\n");   
    }    
   }
  
    if(strcmp((char *)cmd,DEBUG_GSM_SET_ACTIVE) == 0){   
    if(gsm_m6312_gprs_set_active_status(1,GSM_GPRS_ACTIVE) == 0){
    log_debug("gsm set active ok.\r\n");  
    }else{
    log_error("gsm set active err.\r\n");   
    }    
   }
   
    if(strcmp((char *)cmd,DEBUG_GSM_GET_ACTIVE) == 0){  
    gsm_gprs_active_status_t active;  
    if(gsm_m6312_gprs_get_active_status(1,&active) == 0){
    log_debug("gsm get active ok.\r\n");  
    }else{
    log_error("gsm get active err.\r\n");   
    }    
   }
  
    if(strcmp((char *)cmd,DEBUG_GSM_SET_ATTACH) == 0){  
    gsm_gprs_attach_status_t attach = GSM_GPRS_ATTACH;  
    if(gsm_m6312_gprs_set_attach_status(attach) == 0){
    log_debug("gsm set attach ok.\r\n");  
    }else{
    log_error("gsm set attach err.\r\n");   
    }    
   }
  
    if(strcmp((char *)cmd,DEBUG_GSM_GET_ATTACH) == 0){  
    gsm_gprs_attach_status_t attach = GSM_GPRS_ATTACH;  
    if(gsm_m6312_gprs_get_attach_status(&attach) == 0){
    log_debug("gsm get attach ok.\r\n");  
    }else{
    log_error("gsm get attach err.\r\n");   
    }    
   }
  
    if(strcmp((char *)cmd,DEBUG_GSM_SET_OPERATOR) == 0){  
    gsm_m6312_operator_format_t format = GSM_M6312_OPERATOR_FORMAT_NUMERIC_NAME;  
    if(gsm_m6312_set_auto_operator_format(format) == 0){
    log_debug("gsm set operator ok.\r\n");  
    }else{
    log_error("gsm set operator err.\r\n");   
    }    
   }
  
    if(strcmp((char *)cmd,DEBUG_GSM_GET_OPERATOR) == 0){  
    operator_name_t gsm_operator;  
    if(gsm_m6312_get_operator(&gsm_operator) == 0){
    log_debug("gsm get operator ok.\r\n");  
    }else{
    log_error("gsm get operator err.\r\n");   
    }    
   }
  
    if(strcmp((char *)cmd,DEBUG_GSM_GET_IMEI) == 0){  
    char imei[16];  
    if(gsm_m6312_get_imei(imei) == 0){
    log_debug("gsm get imei ok.\r\n");  
    }else{
    log_error("gsm get imei err.\r\n");   
    }    
   }
  
    if(strcmp((char *)cmd,DEBUG_GSM_GET_SN) == 0){  
    char sn[21];  
    if(gsm_m6312_get_sn(sn) == 0){
    log_debug("gsm get sn ok.\r\n");  
    }else{
    log_error("gsm get sn err.\r\n");   
    }    
   }
  

    if(strcmp((char *)cmd,DEBUG_GSM_SET_CONNECT_MODE) == 0){
    if(gsm_m6312_gprs_set_connect_mode(GSM_M6312_GPRS_CONNECT_MODE_MULTIPLE)== 0){
    log_debug("gsm set tcp conn mode ok.\r\n");  
    }else{
    log_error("gsm set tcp conn mode err.\r\n");   
    }    
   }

    if(strcmp((char *)cmd,DEBUG_GSM_SET_TRANSPARENT_MODE) == 0){
    if(gsm_m6312_set_transparent(GSM_M6312_NO_TRANPARENT)== 0){
    log_debug("gsm set tcp tranparent mode ok.\r\n");  
    }else{
    log_error("gsm set tcp tranparent mode err.\r\n");   
    }    
   }

    if(strcmp((char *)cmd,DEBUG_GSM_SET_RECV_BUFFER) == 0){
    if(gsm_m6312_config_recv_buffer(GSM_M6312_RECV_BUFFERE)== 0){
    log_debug("gsm set tcp buffer ok.\r\n");  
    }else{
    log_error("gsm set tcp buffer err.\r\n");   
    }    
   }

  
    if(strcmp((char *)cmd,DEBUG_GSM_GET_RECV_SIZE) == 0){
    recv_buffer_len =gsm_m6312_get_recv_buffer_size(1);
    if(recv_buffer_len >= 0){
    log_debug("gsm get tcp buffer size ok.\r\n");  
    }else{
    log_error("gsm get tcp buffer size err.\r\n");   
    }    
   }
  
  
    if(strcmp((char *)cmd,DEBUG_GSM_TCP_CLOSE) == 0){
    if(gsm_m6312_close_client(1)== 0){
    log_debug("gsm close tcp  ok.\r\n");  
    }else{
    log_error("gsm close tcp err err.\r\n");   
    }    
   }

  
    if(strcmp((char *)cmd,DEBUG_GSM_TCP_CONNECT) == 0){  
    if(gsm_m6312_open_client(1,GSM_M6312_NET_PROTOCOL_TCP ,"\"47.74.35.0\""/*"\"syll-test.mymlsoft.com\""*/,8989)== 0){
    log_debug("gsm tcp connect ok.\r\n");  
    }else{
    log_error("gsm tcp connect err.\r\n");   
    }    
   }

  
    if(strcmp((char *)cmd,DEBUG_GSM_TCP_SEND) == 0){  
    if(gsm_m6312_send(1,"hello wkxboot",13)== 0){
    log_debug("gsm tcp send ok.\r\n");  
    }else{
    log_error("gsm tcp send err.\r\n");   
    }    
   }
  
    if(strcmp((char *)cmd,DEBUG_GSM_TCP_RECV) == 0){
    char recv[100];
    if(recv_buffer_len <= 0){
    continue;
    }
    if(gsm_m6312_recv(1,recv,recv_buffer_len)== 0){
    log_debug("gsm tcp recv ok.\r\n");  
    }else{
    log_error("gsm tcp recv err.\r\n");   
    }    
   }
  
    if(strcmp((char *)cmd,DEBUG_GSM_TCP_STATUS) == 0){
    tcp_connect_status_t status;
    
    if(gsm_m6312_get_connect_status(1,&status)== 0){
    log_debug("gsm get tcp status ok.\r\n");  
    }else{
    log_error("gsm get tcp status err.\r\n");   
    }    
   }
#define  DEBUG_SET_WIFI_DEVICE         "set wifi device\r\n"
#define  DEBUG_SET_GSM_DEVICE          "set gsm device\r\n"
#define  DEBUG_GSM_SET_ECHO_ON         "gsm echo on\r\n"
#define  DEBUG_GSM_SET_ECHO_OFF        "gsm echo off\r\n"
  
    if(strcmp((char *)cmd,DEBUG_SET_WIFI_DEVICE) == 0){     
     connection_set_device(CONNECTION_DEVICE_WIFI_BX8710);
     log_debug("set wifi device ok.\r\n");
     } 
  
    if(strcmp((char *)cmd,DEBUG_SET_GSM_DEVICE) == 0){     
     connection_set_device(CONNECTION_DEVICE_GSM_M6312);
     log_debug("set gsm device ok.\r\n");
     } 
  
     if(strcmp((char *)cmd,DEBUG_GSM_SET_ECHO_ON) == 0){     
     if(gsm_m6312_set_echo(GSM_M6312_ECHO_ON) == 0){
     log_debug("set gsm echo on ok.\r\n");
     }else{
     log_error("set gsm echo on err.\r\n");
     }
     }
  
     if(strcmp((char *)cmd,DEBUG_GSM_SET_ECHO_OFF) == 0){     
     if(gsm_m6312_set_echo(GSM_M6312_ECHO_OFF) == 0){
     log_debug("set gsm echo off ok.\r\n");
     }else{
     log_error("set gsm echo off err.\r\n");
     }
     }
  osDelay(50);
  }
}
}