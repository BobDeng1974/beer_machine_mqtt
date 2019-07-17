#ifndef  __SOCKET_H__
#define  __SOCKET_H__

#include "cmsis_os.h"
#include "stddef.h"
#include "m6312.h"
 
typedef enum
{
    SOCKET_PROTOCOL_TCP = 0,
    SOCKET_PROTOCOL_UDP
}socket_protocol_t;

enum
{
    SOCKET_ERR_OK = 0,
    SOCKET_ERR_INTERNAL = -1,
    SOCKET_ERR_TIMEOUT = -2,
    SOCKET_ERR_NETWORK = -3,
    SOCKET_ERR_UNKNOW = -4
};

#define  SOCKET_ID_MAX  5

/**
* @brief socket环境初始化
* @details
* @param 无
* @return < 0：失败 = 0：成功
* @attention
* @note
*/
int socket_init(void);


/**
* @brief socket建立连接
* @details
* @param host 主机名称
* @param port 连接端口号
* @param protocol 连接协议 @see socket_protocol_t
* @return < 0：失败 >= 0：成功
* @attention
* @note
*/
int socket_open(char *host,const uint16_t port,socket_protocol_t protocol);


/**
* @brief socket断开连接
* @details
* @param handle socket句柄
* @return < 0：失败 = 0：成功
* @attention
* @note
*/
int socket_close(const int handle);


/**
* @brief 通过网络发送数据
* @details
* @param handle socket句柄
* @param buffer 发送缓存
* @param buffer 缓存大小
* @param buffer 超时时间
* @return < 0：错误 > 0：发送的数据量
* @attention
* @note
*/
int socket_write(const int handle,const char *buffer,int size,uint32_t timeout);


/**
* @brief 通过网络接收数据
* @details
* @param handle socket句柄
* @param buffer 接收缓存
* @param buffer 缓存大小
* @param buffer 超时时间
* @return < 0：错误 > 0：接收到的数据量
* @attention
* @note
*/
int socket_read(const int handle,char *buffer,int size,uint32_t timeout);




#endif