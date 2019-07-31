/**
******************************************************************************************                                                                                                                                                       
*                                                                            
*  This program is free software; you can redistribute it and/or modify      
*  it under the terms of the GNU General Public License version 3 as         
*  published by the Free Software Foundation.                                
*                                                                            
*  @file       socket.c
*  @brief      socket库
*  @author     wkxboot
*  @version    v1.0.0
*  @date       2019/7/17
*  @copyright  <h4><left>&copy; copyright(c) 2019 wkxboot 1131204425@qq.com</center></h4>  
*                                                                            
*                                                                            
*****************************************************************************************/
#include "socket.h"
#include "tiny_timer.h"
#include "circle_buffer.h"
#include "log.h"

#define  SOCKET_RECV_BUFFER_SIZE  1024
#define  SOCKET_RECV_BUFFER_CNT   2
#define  SOCKET_ID_CNT            2


 /** socket互斥量结构*/
typedef struct
{
    osMutexId mutex;
}socket_mutex_t;

typedef struct
{
    int value;
    uint8_t buffer[SOCKET_RECV_BUFFER_SIZE];
    circle_buffer_t circle_buffer;
    uint8_t is_free;
}socket_id_t;

typedef struct
{
    socket_id_t id[SOCKET_ID_CNT];
    uint8_t free;
}socket_id_pool_t;

 /** socket管理器结构*/
typedef struct
{
    /** socket互斥量*/
    socket_mutex_t socket_mutex;
    socket_id_pool_t id_pool;
}socket_manage_t;

 /** socket管理器*/
static socket_manage_t socket_manage;


static void socket_mutex_init(socket_mutex_t *socket_mutex)
{
    osMutexDef(socket_mutex);
    socket_mutex->mutex = osMutexCreate(osMutex(socket_mutex));
    log_assert_null_ptr(socket_mutex->mutex);

    osMutexRelease(socket_mutex->mutex);

    log_debug("create socket mutex ok.\r\n");
}

static void socket_mutex_lock(socket_mutex_t *socket_mutex)
{
    log_assert_null_ptr(socket_mutex);
    log_assert_null_ptr(socket_mutex->mutex);
    osMutexWait(socket_mutex->mutex,osWaitForever);
}

static void socket_mutex_unlock(socket_mutex_t *socket_mutex)
{
    log_assert_null_ptr(socket_mutex);
    log_assert_null_ptr(socket_mutex->mutex);
    osMutexRelease(socket_mutex->mutex);
}

static circle_buffer_t *socket_get_circle_buffer(int handle)
{
    for (uint8_t i = 0;i < SOCKET_RECV_BUFFER_CNT;i ++) {
        if (socket_manage.id_pool.id[i].value == handle && \
            socket_manage.id_pool.id[i].is_free == 0) {
            return &socket_manage.id_pool.id[i].circle_buffer;
        }
    }

    log_assert_bool_false(0);
    return NULL;
}


static int socket_alloc_id(void)
{
    int id = -1;

    socket_mutex_lock(&socket_manage.socket_mutex);

    for (uint8_t i = 0;i < SOCKET_ID_MAX;i ++) {
        if (socket_manage.id_pool.id[i].is_free == 1) {
            socket_manage.id_pool.id[i].is_free = 0;
            socket_manage.id_pool.free -= 1;
            id = socket_manage.id_pool.id[i].value;
            circle_buffer_init(&socket_manage.id_pool.id[i].circle_buffer,socket_manage.id_pool.id[i].buffer,SOCKET_RECV_BUFFER_SIZE);
            goto exit;
        }
    }

exit:
    socket_mutex_unlock(&socket_manage.socket_mutex);
    return id;
}

static int socket_free_id(int id)
{
    int rc = -1;
    int index;

    socket_mutex_lock(&socket_manage.socket_mutex);

    for (index = 0;index < SOCKET_ID_MAX;index ++) {
        if (socket_manage.id_pool.id[index].value == id && socket_manage.id_pool.id[index].is_free == 0) {
            socket_manage.id_pool.id[index].is_free = 1;   
            socket_manage.id_pool.free ++;
            rc = 0;
            goto exit;
        }
    }
exit:
    socket_mutex_unlock(&socket_manage.socket_mutex); 
    return rc;
}


static int socket_hal_read(int handle,uint8_t *buffer,uint32_t size)
{
    int rc;

    rc = m6312_recv(handle,buffer,size);
    return rc;
}

static int socket_recv_buffer_read(int handle,char *buffer,uint32_t size,uint32_t timeout)
{
    int rc;
    circle_buffer_t *circle_buffer;
    uint32_t read_size,read_size_total = 0,read_size_remain = size;
    static uint8_t temp_buffer[SOCKET_RECV_BUFFER_SIZE];
    tiny_timer_t timer;

    tiny_timer_init(&timer,0,timeout);
    circle_buffer = socket_get_circle_buffer(handle);

    while (tiny_timer_value(&timer) > 0 && read_size_remain > 0) {
        read_size = circle_buffer_read(circle_buffer,buffer + read_size_total,read_size_remain);
        read_size_remain -= read_size;
        read_size_total += read_size;
        /*接收缓存是空的，就从模块的缓存读取数据，尝试读满*/
        if (circle_buffer_is_empty(circle_buffer)) {
            rc = socket_hal_read(handle,temp_buffer,SOCKET_RECV_BUFFER_SIZE);
            if (rc < 0) {
                return -1;
            }
            if (rc == 0) {
                osDelay(500);
            } else {
                circle_buffer_write(circle_buffer,(const char *)temp_buffer,rc);
                osDelay(100);
            }
        
        }
    }
    return read_size_total;
}
/**
* @brief socket环境初始化
* @details
* @param 无
* @return < 0：失败 = 0：成功
* @attention
* @note
*/
int socket_init(void)
{
    socket_mutex_init(&socket_manage.socket_mutex);
    for (int i = 0;i < SOCKET_ID_MAX;i ++) {
        socket_manage.id_pool.id[i].value = i;
        socket_manage.id_pool.id[i].is_free = 1;
    }
    socket_manage.id_pool.free =  SOCKET_ID_MAX;
    return 0;
}
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
int socket_open(char *host,const uint16_t port,socket_protocol_t protocol)
{
    int rc ;
    int handle;
    char port_str[6];

    handle = socket_alloc_id();
    if (handle < 0) {
        log_error("alloc socket id err.\r\n");
        return SOCKET_ERR_INTERNAL;
    }
    snprintf(port_str,6,"%d",port);
    if (protocol == SOCKET_PROTOCOL_TCP){
        rc = m6312_connect(handle,host,port_str,M6312_CONNECT_TCP);
    } else {
        rc = m6312_connect(handle,host,port_str,M6312_CONNECT_UDP);
    }
    if (rc == 0) {
        return handle;
    }
    socket_free_id(handle);
    return SOCKET_ERR_NETWORK;
}

/**
* @brief socket断开连接
* @details
* @param handle socket句柄
* @return < 0：失败 = 0：成功
* @attention
* @note
*/
int socket_close(const int handle)
{
    int rc ;
    
    socket_free_id(handle);
    rc = m6312_close(handle);
    if (rc == 0) {
        return SOCKET_ERR_OK;
    }

    return SOCKET_ERR_NETWORK;
}

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
int socket_write(const int handle,const char *buffer,int size,uint32_t timeout)
{
    int write_total = 0,write;

    tiny_timer_t timer;

    tiny_timer_init(&timer,0,timeout);

    while (tiny_timer_value(&timer) > 0 && write_total < size) {
        write = m6312_send(handle,(uint8_t *)buffer + write_total,size - write_total);
        if (write < 0) {
            return SOCKET_ERR_NETWORK;
        }
        write_total += write;
    }

    return write_total;
}

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
int socket_read(const int handle,char *buffer,int size,uint32_t timeout)
{
    int read;
    
    read = socket_recv_buffer_read(handle,buffer,size,timeout);
    
    if (read < 0 || read != size) {
        return SOCKET_ERR_NETWORK;
    }
    if (read == 0) {
        return SOCKET_ERR_TIMEOUT;
    }

    return read;
}

