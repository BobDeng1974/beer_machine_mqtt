#ifndef  __WIFI_8710BX_H__
#define  __WIFI_8710BX_H__

#include "stdbool.h"
#include "stddef.h"
#include "stdint.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "st_serial_uart_hal_driver.h"

extern int wifi_8710bx_serial_handle;

#define  WIFI_8710BX_BUFFER_SIZE           256

#define  WIFI_8710BX_IP_STR_LEN            15
#define  WIFI_8710BX_PORT_STR_LEN          5
#define  WIFI_8710BX_MAC_STR_LEN           17
#define  WIFI_8710BX_SSID_STR_LEN          32
#define  WIFI_8710BX_PASSWD_STR_LEN        16
#define  WIFI_8710BX_CHN_STR_LEN           3
#define  WIFI_8710BX_HIDDEN_STR_LEN        2
#define  WIFI_8710BX_SEC_STR_LEN           16


#define  WIFI_8710BX_CONNECTION_CNT_MAX   5

enum  {
 WIFI_ERR_OK = 0,
 WIFI_ERR_MALLOC_FAIL = -1,
 WIFI_ERR_CMD_ERR = -2,
 WIFI_ERR_CMD_TIMEOUT = -3,
 WIFI_ERR_SOCKET_ALREADY_CONNECT = -4,
 WIFI_ERR_CONNECT_FAIL = -5,
 WIFI_ERR_SOCKET_SEND_FAIL = -6,
 WIFI_ERR_SOCKET_DISCONNECT = -7,
 WIFI_ERR_UNKNOW = -200
};


typedef enum
{
 WIFI_8710BX_STATION_MODE= 0,
 WIFI_8710BX_AP_MODE
}wifi_8710bx_mode_t;

typedef enum
{
WIFI_8710BX_ECHO_ON,
WIFI_8710BX_ECHO_OFF
}wifi_8710bx_echo_t;


typedef struct
{
char ssid[WIFI_8710BX_SSID_STR_LEN + 1];
char passwd[WIFI_8710BX_PASSWD_STR_LEN + 1];
char sec[WIFI_8710BX_SEC_STR_LEN + 1];
uint8_t chn;
bool hidden;
}wifi_8710bx_ap_config_t;


typedef struct
{
char ssid[WIFI_8710BX_SSID_STR_LEN + 1];
char passwd[WIFI_8710BX_PASSWD_STR_LEN + 1];
char sec[WIFI_8710BX_SEC_STR_LEN + 1];
uint8_t chn;
int  rssi;
}wifi_8710bx_information_t;


typedef struct
{
char ssid[WIFI_8710BX_SSID_STR_LEN + 1];
char passwd[WIFI_8710BX_PASSWD_STR_LEN + 1];
bool valid;
}wifi_8710bx_ap_connect_t;


typedef struct
{
char ip[WIFI_8710BX_IP_STR_LEN + 1];
char mac[WIFI_8710BX_MAC_STR_LEN + 1];
char gateway[WIFI_8710BX_IP_STR_LEN + 1];
}net_device_t;


typedef struct
{
wifi_8710bx_mode_t         mode;
wifi_8710bx_information_t  ap;
net_device_t               device;
}wifi_8710bx_device_t;



typedef enum
{
WIFI_8710BX_NET_PROTOCOL_TCP =0,
WIFI_8710BX_NET_PROTOCOL_UDP
}wifi_8710bx_net_protocol_t;

typedef enum
{
 WIFI_8710BX_CONNECTION_TYPE_SERVER = 0,
 WIFI_8710BX_CONNECTION_TYPE_CLIENT,
 WIFI_8710BX_CONNECTION_TYPE_SEED
}wifi_8710bx_connection_type_t;

typedef struct
{
char ip[WIFI_8710BX_IP_STR_LEN];
int  port;
int  conn_id;
int  socket_id;
wifi_8710bx_connection_type_t type;
wifi_8710bx_net_protocol_t    protocol;
}wifi_8710bx_connection_t;

typedef struct
{
wifi_8710bx_connection_t connection[WIFI_8710BX_CONNECTION_CNT_MAX];
int8_t cnt;
}wifi_8710bx_connection_list_t;




/*wifi 8710bx串口外设初始化*/
int wifi_8710bx_serial_hal_init();

/* 函数名：wifi_8710bx_reset
*  功能：  wifi软件复位 
*  参数：  无
*  返回：  WIFI_ERR_OK：成功 其他：失败
*/
int wifi_8710bx_reset(void);

/* 函数名：wifi_8710bx_set_mode
*  功能：  设置作模式 
*  参数：  mode 模式 station or ap
*  返回：  WIFI_ERR_OK：成功 其他：失败
*/
int wifi_8710bx_set_mode(const wifi_8710bx_mode_t mode);
/* 函数名：wifi_8710bx_set_echo
*  功能：  设置回显模式 
*  参数：  echo 回显值 打开或者关闭
*  返回：  WIFI_ERR_OK：成功 其他：失败
*/
int wifi_8710bx_set_echo(const wifi_8710bx_echo_t echo);
/* 函数名：wifi_8710bx_config_ap
*  功能：  配置AP参数 
*  参数：  ap参数指针
*  返回：  WIFI_ERR_OK：成功 其他：失败
*/
int wifi_8710bx_config_ap(const wifi_8710bx_ap_config_t *ap);
/* 函数名：wifi_8710bx_get_ap_rssi
*  功能：  扫描出指定ap的rssi值 
*  参数：  ssid ap的ssid名称
*  参数：  rssi ap的rssi的指针
*  返回：  WIFI_ERR_OK：成功 其他：失败
*/
int wifi_8710bx_get_ap_rssi(const char *ssid,int *rssi);
/* 函数名：wifi_8710bx_connect_ap
*  功能：  连接指定的ap
*  参数：  ssid ap的ssid名称
*  参数：  passwd ap的passwd
*  返回：  WIFI_ERR_OK：成功 其他：失败
*/
int wifi_8710bx_connect_ap(const char *ssid,const char *passwd);
/* 函数名：wifi_8710bx_disconnect_ap
*  功能：  断开指定的ap连接
*  参数：  无
*  返回：  WIFI_ERR_OK：成功 其他：失败
*/
int wifi_8710bx_disconnect_ap(void);
/* 函数名：wifi_8710bx_get_wifi_device
*  功能：  获取wifi设备信息
*  参数：  wifi_device 设备指针
*  返回：  WIFI_ERR_OK：成功 其他：失败
*/
int wifi_8710bx_get_wifi_device(wifi_8710bx_device_t *wifi_device);

/* 函数名：wifi_8710bx_get_connection
*  功能：  获取连接信息
*  参数：  connection_list 连接列表指针
*  返回：  WIFI_ERR_OK：成功 其他：失败
*/
int wifi_8710bx_get_connection(wifi_8710bx_connection_list_t *connection_list);
/* 函数名：wifi_8710bx_open_server
*  功能：  创建服务端
*  参数：  port 服务端本地端口
*  参数：  protocol   服务端网络协议类型
*  返回：  连接句柄，大于WIFI_ERR_OK：成功 其他：失败
*/
int wifi_8710bx_open_server(const uint16_t port,const wifi_8710bx_net_protocol_t protocol);
/* 函数名：wifi_8710bx_open_client
*  功能：  创建客户端
*  参数：  host        服务端域名
*  参数：  remote_port 服务端端口
*  参数：  protocol    网络协议类型
*  返回：  连接句柄，大于WIFI_ERR_OK：成功 其他：失败
*/
int wifi_8710bx_open_client(const char *host,const uint16_t remote_port,const wifi_8710bx_net_protocol_t protocol);
/* 函数名：wifi_8710bx_close
*  功能：  关闭客户或者服务端域      
*  参数：  conn_id     连接句柄
*  返回：  WIFI_ERR_OK：成功 其他：失败
*/
int wifi_8710bx_close(const int conn_id);
/* 函数名：wifi_8710bx_send
*  功能：  发送数据
*  参数：  conn_id 连接句柄
*  参数：  data    数据buffer
*  参数：  size    需要发送的数量
*  返回：  WIFI_ERR_OK：成功 其他：失败
*/
int wifi_8710bx_send(int conn_id,const char *data,const int size);
/* 函数名：wifi_8710bx_recv
*  功能：  接收数据
*  参数：  buffer_dst  目的buffer
*  参数：  buffer_src  源buffer
*  参数：  buffer_size 目的buffer大小
*  返回：  >= 0：接收到的数据量 其他：失败
*/
int wifi_8710bx_recv(const int conn_id,char *buffer,int size);
#endif