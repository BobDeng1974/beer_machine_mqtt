/*****************************************************************************
*  SOCKET库                                                          
*  Copyright (C) 2019 wkxboot 1131204425@qq.com.                             
*                                                                            
*                                                                            
*  This program is free software; you can redistribute it and/or modify      
*  it under the terms of the GNU General Public License version 3 as         
*  published by the Free Software Foundation.                                
*                                                                            
*  @file     socket.c                                                   
*  @brief    SOCKET库                                                                                                                                                                                             
*  @author   wkxboot                                                      
*  @email    1131204425@qq.com                                              
*  @version  v1.0.0                                                  
*  @date     2019/1/11                                            
*  @license  GNU General Public License (GPL)                                
*                                                                            
*                                                                            
*****************************************************************************/

#include "cmsis_os.h"
#include "net.h"
#include "socket.h"
#include "utils.h"
#include "log.h"


#define  BUFFER_CNT                           1
#define  BUFFER_SIZE                          2048 /*必须是2的x次方*/
#define  SOCKET_GSM_HANDLE_BASE               10
#define  SOCKET_GSM_HANDLE_MAX                5


static void socket_gsm_handle_init();
static int socket_buffer_init();

typedef struct
{
 int value;
 bool alive;
}socket_gsm_handle_t;

typedef struct
{
bool    valid;
uint8_t socket;
char buffer[BUFFER_SIZE];
uint32_t read;
uint32_t write;
}socket_buffer_t;

static  socket_buffer_t socket_buffer[BUFFER_CNT];
static  char hal_socket_recv_buffer[BUFFER_SIZE];
static  socket_gsm_handle_t   gsm_handle[SOCKET_GSM_HANDLE_MAX];

osMutexId mutex;


/* 函数：socket_init
*  功能：网络连接初始化
*  参数：无
*  返回：0：成功 其他：失败
*/
int socket_init()
{
  osMutexDef(socket_mutex);
  mutex = osMutexCreate(osMutex(socket_mutex));
  log_assert(mutex);

  osMutexRelease(mutex);
  log_debug("create socket mutex ok.\r\n");
   
  socket_gsm_handle_init();
  socket_buffer_init();
 
  log_debug("socket init ok.\r\n");
  return 0;
}

/* 函数：socket_gsm_handle_init
*  功能：gsm句柄池初始化
*  参数：无 
*  返回：无
*/ 
static void socket_gsm_handle_init()
{
 for(uint8_t i = 0;i < SOCKET_GSM_HANDLE_MAX; i++){
 gsm_handle[i].alive = false;
 gsm_handle[i].value = i ;      
 }
}
/* 函数：socket_malloc_gsm_handle
*  功能：gsm句柄申请
*  参数：无 
*  返回：>=0 成功 其他：失败
*/ 
static int socket_malloc_gsm_handle()
{
 int rc;
 osMutexWait(mutex,osWaitForever);
 for(uint8_t i = 0;i < SOCKET_GSM_HANDLE_MAX; i++){
     if(gsm_handle[i].alive == false){
        gsm_handle[i].alive = true;
        rc = gsm_handle[i].value; 
        goto exit;
     }
 }
 rc = -1; 
 log_error("gsm has no invalid handle.\r\n"); 
exit: 
 osMutexRelease(mutex);
 return rc; 
}
/* 函数：socket_free_gsm_handle
*  功能：gsm句柄释放
*  参数：无 
*  返回：0：成功 其他：失败
*/ 
static int socket_free_gsm_handle(int handle)
{
 int rc;
 osMutexWait(mutex,osWaitForever);
 
 for(uint8_t i = 0;i < SOCKET_GSM_HANDLE_MAX; i++){
    if(gsm_handle[i].value == handle && gsm_handle[i].alive == true){
       gsm_handle[i].alive = false;      
       rc = 0;
       goto exit;
    }
 }
 rc = -1;
 log_error("gsm free handle:%d invalid.\r\n",handle); 
exit:
 osMutexRelease(mutex); 
 return rc;
}
/* 函数：socket_buffer_init
*  功能：socket接收缓存初始化
*  参数：无 
*  返回：0：成功 其他：失败
*/ 
static int socket_buffer_init()
{
 for(uint8_t i = 0; i< BUFFER_CNT; i++){
     memset(&socket_buffer[i],0,sizeof(socket_buffer_t));  
 }
 return 0;
}
/* 函数：socket_malloc_buffer
*  功能：申请socket接收缓存
*  参数：handle： socket句柄 
*  返回：0：成功 其他：失败
*/ 
static int socket_malloc_buffer(int handle)
{
  int rc;
  osMutexWait(mutex,osWaitForever);
 
  for(uint8_t i = 0; i< BUFFER_CNT; i++){
      if(socket_buffer[i].valid == false){
         socket_buffer[i].socket = handle;
         socket_buffer[i].valid = true;
         rc = 0; 
         goto exit;
    }
  }
  rc = -1;
  log_error("gsm malloc buffer handle:%d fail no more valid buffer.\r\n",handle);  
exit:
 osMutexRelease(mutex); 
 return rc;   
}
/* 函数：socket_free_buffer
*  功能：释放socket接收缓存
*  参数：handle： socket句柄 
*  返回：0：成功 其他：失败
*/ 
static int socket_free_buffer(int handle)
{
  int rc;
  osMutexWait(mutex,osWaitForever);
  
  for(uint8_t i = 0; i< BUFFER_CNT; i++){
    if(socket_buffer[i].socket == handle && socket_buffer[i].valid == true){
    socket_buffer[i].valid = false; 
    socket_buffer[i].socket = 0;
    socket_buffer[i].read = socket_buffer[i].write;
    rc = 0; 
    goto exit;
    }
  }
  rc = -1;
  log_error("gsm free buffer handle:%d invalid.\r\n",handle);
exit:
 osMutexRelease(mutex); 
 return rc;   
}


/* 函数：socket_seek_buffer_idx
*  功能：根据句柄查找接收缓存idx
*  参数：handle： socket句柄 
*  返回：0：成功 其他：失败
*/ 
static int socket_seek_buffer_idx(int handle)
{

  for(uint8_t i = 0; i< BUFFER_CNT; i++){
     if(socket_buffer[i].socket == handle && socket_buffer[i].valid == true){
       return i; 
     }
  }
  log_error("can seek free buffer idx.handle:%d \r\n",handle);
  return -1;  
}

/* 函数：socket_get_used_size
*  功能：根据句柄获取接收缓存已经接收的数据量
*  参数：handle： socket句柄 
*  返回：0：成功 其他：失败
*/ 
static int socket_get_used_size(int handle)
{
  int rc;
  rc = socket_seek_buffer_idx(handle);
  if(rc < 0){
     return -1; 
  }
  return socket_buffer[rc].write - socket_buffer[rc].read;    
}

/* 函数：socket_get_free_size
*  功能：根据句柄获取接收缓存可使用的空间
*  参数：handle： socket句柄 
*  返回：0：成功 其他：失败
*/ 
static int socket_get_free_size(int handle)
{ 
  int rc;
  rc = socket_seek_buffer_idx(handle);
  if(rc < 0){
  return -1; 
  }
  
 return BUFFER_SIZE - (socket_buffer[rc].write - socket_buffer[rc].read);   
}

/* 函数：socket_write_buffer
*  功能：根据句柄向对应接收缓存写入数据
*  参数：handle： socket句柄 
*  参数：src： 数据位置 
*  参数：size：   数据大小 
*  返回：0：成功 其他：失败
*/
static int socket_write_buffer(int handle,const char *src,const int size)
{
  int rc ;
  rc = socket_seek_buffer_idx(handle);
  if(rc < 0){
  return -1;
  }
  
  for(int i= 0; i< size ; i ++){
  socket_buffer[rc].buffer[socket_buffer[rc].write & (BUFFER_SIZE - 1)] = src[i];
  socket_buffer[rc].write ++;
  }
  return 0;
 }

/* 函数：socket_read_buffer
*  功能：根据句柄从对应接收缓存读出数据
*  参数：handle： socket句柄 
*  参数：dst： 数据位置 
*  参数：size：   数据大小 
*  返回：0：成功 其他：失败
*/     
 static int socket_read_buffer(int handle,char *dst,const int size)
{
  int rc ;
  rc = socket_seek_buffer_idx(handle);
  if(rc < 0){
  return -1;
  }
  
  for(int i = 0 ; i < size ; i++){
  dst[i] = socket_buffer[rc].buffer[socket_buffer[rc].read & (BUFFER_SIZE - 1)];
  socket_buffer[rc].read ++; 
  }
  return 0;  
}
  
/* 函数：get_wifi_net_status
*  功能：获取wifi网络状态
*  参数：无 
*  返回：0：成功 其他：失败
*/  
__weak net_status_t get_wifi_net_status() 
{
  
  return NET_STATUS_INIT;
}

/* 函数：get_gsm_net_status
*  功能：获取wifi网络状态
*  参数：无 
*  返回：0：成功 其他：失败
*/  
__weak net_status_t get_gsm_net_status() 
{
  
  return NET_STATUS_INIT;
}


/* 函数名：socket_connect
*  功能：  建立网络连接
*  参数：  host 主机域名
*  参数：  remote_port 主机端口
*  参数：  local_port 本地端口
*  参数：  protocol   连接协议
*  返回：  >=0 成功连接句柄 其他：失败
*/ 
int socket_connect(const char *host,uint16_t remote_port,socket_protocol_t protocol)
{
 int rc ;
 int handle;
 net_status_t status;
 
 wifi_8710bx_net_protocol_t wifi_net_protocol;
 gsm_m6312_net_protocol_t  gsm_net_protocol;
 
 if(protocol == SOCKET_PROTOCOL_TCP){
 wifi_net_protocol = WIFI_8710BX_NET_PROTOCOL_TCP;
 gsm_net_protocol = GSM_M6312_NET_PROTOCOL_TCP;
 }else{
 wifi_net_protocol = WIFI_8710BX_NET_PROTOCOL_UDP;
 gsm_net_protocol = GSM_M6312_NET_PROTOCOL_UDP;
 }

 /*只要wifi连接上了，始终优先*/
 status = get_wifi_net_status();
 if(status == NET_STATUS_ONLINE ){
 rc = wifi_8710bx_open_client(host,remote_port,wifi_net_protocol);
 }else{
 rc = -1;
 log_warning("wifi is not ready.retry gsm.\r\n");      
 }  

 /*wifi连接失败，选择gsm*/
 if(rc < 0){ 
 status = get_gsm_net_status();
 if(status == NET_STATUS_ONLINE){
 handle = socket_malloc_gsm_handle();
 if(handle < 0){
 return -1;    
 }
 rc = gsm_m6312_open_client(handle,gsm_net_protocol,host,remote_port);
 if(rc == handle){
 rc =  handle + SOCKET_GSM_HANDLE_BASE;
 log_debug("gsm connect ok handle:%d.\r\n",handle);    
 }else{
 socket_free_gsm_handle(handle);
 log_error("gsm handle :%d connect fail.\r\n",handle);    
 }
 }else{
 log_warning("gsm is not ready.\r\n");    
 }
 }else{
 log_debug("wifi connect ok handle:%d.\r\n",rc);    
 }
 
 /*申请buffer*/
 if(rc > 0 ){
 socket_malloc_buffer(rc);
 }
 return rc;  
}

/* 函数名：socket_disconnect
*  功能：  断开网络连接
*  参数：  socket_info 连接参数
*  返回：  0：成功 其他：失败
*/ 
int socket_disconnect(const int socket_handle)
{
 int rc ;

 /*此连接handle是GSM网络*/
 if(socket_handle >= SOCKET_GSM_HANDLE_BASE){
 rc = gsm_m6312_close_client(socket_handle - SOCKET_GSM_HANDLE_BASE);
 if(rc != 0){
 log_error("gsm handle:%d disconnect err.\r\n",socket_handle - SOCKET_GSM_HANDLE_BASE);  
 }
 socket_free_gsm_handle(socket_handle - SOCKET_GSM_HANDLE_BASE);
 }else { 
 /*此连接handle是WIFI网络*/
 rc = wifi_8710bx_close(socket_handle);
 if(rc != 0){
 log_error("wifi handle:%d disconnect err.\r\n",socket_handle);  
 }
 }
 /*释放buffer*/
 socket_free_buffer(socket_handle);
 return rc;
}


/* 函数名：socket_send
*  功能：  网络发送
*  参数：  socket_handle 连接句柄
*  参数：  buffer 发送缓存
*  参数：  size 长度
*  参数    timeout 发送超时
*  返回：  >=0 成功发送的数据 其他：失败
*/ 
int socket_send(const int socket_handle,const char *buffer,int size,uint32_t timeout)
{
 int send_total = 0,send_size = 0,send_left = size;
 utils_timer_t timer;
 
 utils_timer_init(&timer,timeout,false);
 
 /*此连接handle是GSM网络*/
 while(utils_timer_value(&timer) > 0 && send_left > 0){
   
 if(socket_handle >= SOCKET_GSM_HANDLE_BASE ){

 send_size = gsm_m6312_send(socket_handle - SOCKET_GSM_HANDLE_BASE,buffer + send_total,send_left); 
 
 }else{ 
 /*此连接handle是WIFI网络*/

 send_size = wifi_8710bx_send(socket_handle,buffer + send_total,send_left);

 }
 
 if(send_size >= 0){
 send_total +=send_size;
 send_left -=send_size;
 }else{
 return -1;  
 }

 }
 log_debug("send data size:%d .\r\n",send_total);
 return send_total;
}

/* 函数名：socket_recv
*  功能：  网络接收
*  参数：  socket_handle 连接句柄
*  参数：  buffer 接收缓存
*  参数：  size 长度
*  参数：  timeout 接收超时
*  返回：  >=0:成功接收的长度 其他：失败
*/ 
int socket_recv(const int socket_handle,char *buffer,int size,uint32_t timeout)
{
 int buffer_size,free_size,read_size,recv_size = 0;;
 utils_timer_t timer;
 
 if(size == 0){
    return 0;
 }
 utils_timer_init(&timer,timeout,false);
 
 read_size = 0;
 
 while(read_size < size && utils_timer_value(&timer) > 0){
    buffer_size = socket_get_used_size(socket_handle);
    if(buffer_size < 0){
       return -1;
    }
    /*如果缓存数据足够，直接从缓存读出*/
    if(buffer_size + read_size >= size){
       socket_read_buffer(socket_handle,buffer + read_size, size - read_size);
       return size;
    }

    /*如果本地缓存不够 先读出所有本地缓存*/
    socket_read_buffer(socket_handle,buffer + read_size,buffer_size);
    read_size += buffer_size;
 
    free_size = socket_get_free_size(socket_handle);

    /*再读出硬件所有缓存*/
    /*此连接handle是GSM网络*/
    if(socket_handle >= SOCKET_GSM_HANDLE_BASE){
       recv_size = gsm_m6312_recv(socket_handle - SOCKET_GSM_HANDLE_BASE,hal_socket_recv_buffer,free_size);
    }else{
      /*此连接handle是WIFI网络*/
       recv_size = wifi_8710bx_recv(socket_handle,hal_socket_recv_buffer,free_size);
    }
 
    if(recv_size >= 0){
       socket_write_buffer(socket_handle,hal_socket_recv_buffer,recv_size);
    }else{
       return -1;  
    }
  }
 
  log_debug("read data size:%d .\r\n",read_size);
  return read_size;
}