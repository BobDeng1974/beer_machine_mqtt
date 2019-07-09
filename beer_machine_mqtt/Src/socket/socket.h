#ifndef  __SOCKET_H__
#define  __SOCKET_H__

#include "cmsis_os.h"
#include "wifi_8710bx.h"
#include "gsm_m6312.h"
 
typedef enum
{
 SOCKET_PROTOCOL_TCP =0,
 SOCKET_PROTOCOL_UDP
}socket_protocol_t;


typedef struct
{
 socket_protocol_t protocol;
 char *host;
 uint16_t remote_port;
 uint16_t local_port;
}socket_info_t;


/* 函数名：socket_init
*  功能：  网络连接初始化
*  参数：  无
*  返回：  0：成功 其他：失败
*/
int socket_init();

/* 函数名：socket_connect
*  功能：  建立网络连接
*  参数：  host 主机域名
*  参数：  remote_port 主机端口
*  参数：  protocol   连接协议
*  返回：  >=0 成功连接句柄 其他：失败
*/ 
int socket_connect(const char *host,uint16_t remote_port,socket_protocol_t protocol);

/* 函数名：socket_disconnect
*  功能：  断开网络连接
*  参数：  socket_info 连接参数
*  返回：  0：成功 其他：失败
*/ 
int socket_disconnect(int conenction_handle);
/* 函数名：socket_send
*  功能：  网络发送
*  参数：  socket_handle 连接句柄
*  参数：  buffer 发送缓存
*  参数：  size 长度
*  参数    timeout 发送超时
*  返回：  >=0 成功发送的数据 其他：失败
*/ 
int socket_send(int socket_handle,const char *buffer,int size,uint32_t timeout);

/* 函数名：socket_recv
*  功能：  网络接收
*  参数：  socket_handle 连接句柄
*  参数：  buffer 接收缓存
*  参数：  size 长度
*  参数：  timeout 接收超时
*  返回：  >=0:成功接收的长度 其他：失败
*/ 
int socket_recv(int socket_handle,char *buffer,int size,uint32_t timeout);
#endif