/**
******************************************************************************************                                                                                                                                                       
*                                                                            
*  This program is free software; you can redistribute it and/or modify      
*  it under the terms of the GNU General Public License version 3 as         
*  published by the Free Software Foundation.                                
*                                                                            
*  @file       http_client.c
*  @brief      http客户端
*  @author     wkxboot
*  @version    v1.0.0
*  @date       2019/7/17
*  @copyright  <h4><left>&copy; copyright(c) 2019 wkxboot 1131204425@qq.com</center></h4>  
*                                                                            
*                                                                            
*****************************************************************************************/
#include "stdbool.h"
#include "stddef.h"
#include "printf.h"
#include "stdlib.h"
#include "string.h"
#include "stdint.h"
#include "socket.h"
#include "http_client.h"
#include "tiny_timer.h"
#include "cmsis_os.h"
#include "log.h"


#define  HEAD_START0_STR                    "HTTP/1.0 "
#define  HEAD_START1_STR                    "HTTP/1.1 "
#define  HEAD_END_STR                       "\r\n\r\n"


static int http_client_parse_url(const char *url,char *host,uint16_t *port,char *path)
{
    char *host_str;
    char *port_str;
    char *path_str;
    uint8_t host_str_len;
    uint8_t path_str_len;
  
    log_assert_null_ptr(url);
    log_assert_null_ptr(host);
    log_assert_null_ptr(port);
    log_assert_null_ptr(path);

    /*找到host*/
    if (strncmp(url,"http://",7) != 0) {
        log_error("url not start with http://\r\n");
        return -1;
    }
    host_str = (char *)url + 7;
  
    /*找到path*/
    path_str = strstr(host_str,"/");
    if (path_str == NULL) {
        log_error("url path has no /.\r\n");
        return -1;
    }
    /*找到port*/
    port_str = strstr(host_str,":");
    if (port_str == NULL) {
        /*默认端口*/
        *port = 80;
        port_str = path_str;
    } else {
        *port = strtol(port_str + 1,NULL,10);
    }
  
    /*复制host*/
    host_str_len = port_str - host_str;
    if (host_str_len > HTTP_CLIENT_HOST_STR_LEN) {
        log_error("host str len:%d is too large.\r\n",host_str_len);
        return -1;
    }
    memcpy(host,host_str,host_str_len);
    host[host_str_len]= '\0';
    /*找到path*/
    path_str_len = strlen(path_str);
    if (path_str_len > HTTP_CLIENT_PATH_STR_LEN - 1) {
        log_error("path str len:%d is too large.\r\n",path_str_len);
        return -1;
    } 
    strcpy(path,path_str);
  
    return 0; 
}
 
static int http_client_build_head(char *buffer,const char *method,http_client_context_t *context,int size)
{
    int head_size;
    const char *http_version = "HTTP/1.1";
    snprintf(buffer,size,
            /*method----path----http_version*/
            "%s "     "%s "  "%s\r\n"
            /*host----port*/
            "Host: %s:%d\r\n"
            "Connection: keep-alive\r\n"
            "User-Agent: wkxboot-gsm-wifi.\r\n"
            /*content type*/
            "Content-Type: %s%s\r\n"
            "Content-Length: %d\r\n"
            "Range: bytes=%d-%d\r\n"
            "\r\n",
            method,context->path,http_version,
            context->host,context->port,
            context->content_type,
            context->is_form_data ? context->boundary : "",
            context->user_data_size,
            context->range_start,context->range_start + context->range_size -1);
            head_size = strlen(buffer);
            //log_debug("head:%s\r\n",buffer);        
  
    return head_size;     
}

static int http_client_recv_head(http_client_context_t *context,char *buffer,uint16_t buffer_size,const uint32_t timeout)
{
    int rc;
    int recv_total = 0;
    char *tmp_ptr,*encode_type;
    int encode_type_size;
    tiny_timer_t timer;
 
    tiny_timer_init(&timer,0,timeout);
 
    /*获取http head*/
    do {
    rc = socket_read(context->handle,buffer + recv_total,1,tiny_timer_value(&timer));
    if (rc < 0) {
        log_error("http header recv err.code:%d.\r\n",rc);
        return -1;
    }
 
    recv_total += 1;
    buffer[recv_total] = '\0';
    /*尝试解析head*/
    if ((strstr(buffer,HEAD_START0_STR) || strstr(buffer,HEAD_START1_STR)) &&  strstr(buffer,HEAD_END_STR)) {
        log_debug("find header.\r\n"); 
        context->head_size = strstr(buffer,HEAD_END_STR) + strlen(HEAD_END_STR) - buffer;
        /*找到content size*/
        if (NULL != (tmp_ptr = strstr(buffer, "Content-Length"))) {
        context->content_size = atoi(tmp_ptr + strlen("Content-Length: "));
        context->is_chunked = false;
        log_debug("content not chunk. size is %d.\r\n",context->content_size);  
        return 0;
        } else if (NULL != (tmp_ptr = strstr(buffer, "Transfer-Encoding: "))) {
            encode_type_size = strlen("Chunked");
            encode_type =(char *) tmp_ptr + strlen("Transfer-Encoding: ");
            if ((! memcmp(encode_type, "Chunked", encode_type_size)) ||\
                (! memcmp(encode_type, "chunked", encode_type_size))) {
                context->content_size = 0;/*this mean data is chunked*/   
                context->is_chunked = true;
                log_debug("content is chunked.\r\n"); 
                return 0;
            }
        } else {
            log_error("header is invalid.\r\n");
            return -1;/*will stop parse*/
        }
    } 
    } while (recv_total < buffer_size - 1);

    log_error("recv head buffer is overflow.\r\n");
    return -1;
 }
 
#define  CHUNK_CODE_SIZE_MAX    6

static int http_client_recv_chunk_size(http_client_context_t *context,uint32_t timeout)
{
    int rc;
    int recv_total = 0;
    char chunk_code[CHUNK_CODE_SIZE_MAX];
    tiny_timer_t timer;
  
    tiny_timer_init(&timer,0,timeout);
    context->chunk_size = 0;
    /*获取chunk code*/
    do{
    rc = socket_read(context->handle,chunk_code + recv_total,1,tiny_timer_value(&timer));
    if (rc < 0) {
        log_error("http chunk code recv err.code:%d.\r\n",rc);
        return -1;
    }
 
    recv_total += 1;
    if (recv_total >= 3) {
        chunk_code[recv_total] = '\0';
        /*尝试解析chunk code*/
        if (chunk_code[recv_total - 2] == '\r' && chunk_code[recv_total - 1] == '\n') {
            /*编码是16进制hex*/
            context->chunk_size = strtol(chunk_code,NULL,16);
            log_debug("find chunk code(HEX):%s",chunk_code); 
            log_debug("chunk size:%d.\r\n",context->chunk_size); 
            return 0;
        }
    }
    }while (recv_total < CHUNK_CODE_SIZE_MAX - 1);
                                                          
    log_error("recv chunk code not find .\r\n");                                                        
    return -1;                                                         
}                                                 

static int http_client_recv_chunk_tail(http_client_context_t *context,uint32_t timeout)
{
    int rc;
    tiny_timer_t timer;
    char tail[2];
  
    tiny_timer_init(&timer,0,timeout);
  
    rc = socket_read(context->handle,tail ,2,tiny_timer_value(&timer));
    if (rc < 0) {
        log_error("http chunk code recv err.\r\n");
        return -1;
    }

    if (tail[0] != '\r' || tail[1] != '\n') {
        /*超时返回*/
        log_error("chunk format err.\r\n");
        return -1;  
    } 
    return 0;  
 }
  
 
static int http_client_recv_chunk(http_client_context_t *context,uint32_t timeout)
{
    int rc;
    tiny_timer_t timer;
  
    tiny_timer_init(&timer,0,timeout);
  
    context->content_size = 0;
    do {
    rc = http_client_recv_chunk_size(context,tiny_timer_value(&timer));
    if (rc != 0) {
        return -1;   
    }
    if (context->chunk_size == 0) {
        /*接收chunk tail*/
        rc = http_client_recv_chunk_tail(context,tiny_timer_value(&timer));
        if (rc != 0) {
            return -1;   
        }
  
        context->rsp_buffer[context->content_size] = '\0';
        log_debug("last chunk.\r\n");
        return 0; 
    }
    if (context->chunk_size > context->rsp_buffer_size - context->content_size - 1) {
        log_error("chunk size:%d large than free buffer size:%d.\r\n",context->chunk_size,context->rsp_buffer_size - context->content_size - 1);
        return -1;   
    }
  
    rc = socket_read(context->handle,context->rsp_buffer + context->content_size,context->chunk_size,tiny_timer_value(&timer));
    if (rc < 0) {
        log_error("http chunk code recv err.\r\n");
        return -1;
    }

    /*接收chunk tail*/
    rc = http_client_recv_chunk_tail(context,tiny_timer_value(&timer));
    if (rc != 0) {
        return -1;   
    }
    /*更新content size*/
    context->content_size +=context->chunk_size;
  
    } while (context->content_size);/*消除编译警告*/
    /*不可到达*/
    return -1;
}

static int http_client_request(const char *method,http_client_context_t *context)
{
    int rc;
    int head_size;
    char *http_buffer;
    tiny_timer_t timer;
 
    /*整个过程的时间计算超时时间*/
    tiny_timer_init(&timer,0,context->timeout);
 
    /*申请http缓存*/
    http_buffer = HTTP_CLIENT_MALLOC(HTTP_BUFFER_SIZE);
    if (http_buffer == NULL) {
        log_error("http malloc buffer err.\r\n");
        return -1;
    }
    /*解析url*/
    rc = http_client_parse_url(context->url,context->host,&context->port,context->path);
    if (rc != 0) {
        log_error("http client parse url error.\r\n");
        goto err_handler;
    }
    /*构建http head缓存*/
    head_size = http_client_build_head(http_buffer,method,context,HTTP_BUFFER_SIZE);
    if (head_size < 0) {
        log_error("http build head err.\r\n");
        goto err_handler;
    }
    /*http建立连接*/
    rc = socket_open(context->host,context->port,SOCKET_PROTOCOL_TCP);
    if(rc < 0){
        goto err_handler;
    }
    context->handle = rc;
    context->connected = true;
 
    /*http head发送*/
    rc = socket_write(context->handle,http_buffer,head_size,tiny_timer_value(&timer));
    if (rc != head_size) {
        log_error("http head send err.\r\n");
        goto err_handler;
    }

    /*http user data发送*/
    if (context->user_data_size > 0) {
        rc = socket_write(context->handle,context->user_data,context->user_data_size,tiny_timer_value(&timer));
        if (rc != context->user_data_size) {
            log_error("http user data send err.\r\n");
            goto err_handler;
        }
    }

    /*清空http buffer 等待接收数据*/
    memset(http_buffer,0,HTTP_BUFFER_SIZE);
 
    rc = http_client_recv_head(context,http_buffer,HTTP_BUFFER_SIZE,tiny_timer_value(&timer));
    /*接收head失败 返回*/
    if (rc != 0) {
        goto err_handler;
    }

    /*编码是chunk处理*/
    if (context->is_chunked == true) {
        rc = http_client_recv_chunk(context,tiny_timer_value(&timer));
        if (rc != 0){
            goto err_handler;  
        }
    } else {/*不是chunk处理*/
        if (context->content_size > context->rsp_buffer_size) {
            log_error("content size:%d large than free buffer size:%d.\r\n",context->content_size,context->rsp_buffer_size);   
            return -1;
        }
        rc = socket_read(context->handle,context->rsp_buffer,context->content_size,tiny_timer_value(&timer));
        if (rc < 0) {
            log_error("content recv err.\r\n");
            goto err_handler;
        }
        if (rc != context->content_size) {
            log_error("content recv timeout.\r\n");
            goto err_handler; 
        }
    }
    rc = 0;
    /*错误处理*/ 
err_handler:
    /*释放http 缓存*/
    HTTP_CLIENT_FREE(http_buffer);
    if (context->connected == true) {
        socket_close(context->handle);
        context->connected = false;
    }
 
    return rc;
}

/**
* @brief
* @details
* @param
* @param
* @return
* @attention
* @note
*/
int http_client_build_form_data(char *form_data,const int size,const char *boundary,const int cnt,...)
{
    int put_size;
    int temp_size;  
    va_list ap;
    form_data_t *temp;

    put_size = 0;
    va_start(ap,cnt);
 
    /*组合输入数据*/  
    for (uint8_t i = 0; i < cnt; i++) {
        temp = va_arg(ap,form_data_t*);
        /*添加boundary 和 name 和 value*/
        snprintf(form_data + put_size ,size - put_size,"--%s\r\nContent-Disposition: form-data; name=%s\r\n\r\n%s\r\n",boundary,temp->name,temp->value);
        temp_size = strlen(form_data);
        /*保证数据完整*/
        if (temp_size >= size - 1) {
            log_error("form data size :%d is too large.\r\n",temp_size);
            return -1;
        } 
        put_size = strlen(form_data);
    }
    /*添加结束标志*/
    snprintf(form_data + put_size,size - put_size,"--%s--\r\n",boundary);
    put_size = strlen(form_data);
    if (put_size >= size - 1) {
        log_error("form data size :%d is too large.\r\n",put_size);
        return -1;  
    }
 
    return 0; 
}


/**
* @brief http get方法
* @details
* @param context http上下文
* @return 0：成功 -1：失败
* @attention
* @note
*/
int http_client_get(http_client_context_t *context)
{
    if (context == NULL) {
        log_error("null pointer.\r\n");
        return -1;
    } 
    return  http_client_request("GET",context);
}

/**
* @brief http post方法
* @details
* @param context http上下文
* @return 0：成功 -1：失败
* @attention
* @note
*/
int http_client_post(http_client_context_t *context)
{
    if (context == NULL) {
        log_error("null pointer.\r\n");
        return -1;
    } 

    return  http_client_request("POST",context);
}

/**
* @brief http download方法
* @details
* @param context http上下文
* @return 0：成功 -1：失败
* @attention
* @note
*/
int http_client_download(http_client_context_t *context)
{
    if (context == NULL) {
        log_error("null pointer.\r\n");
        return -1;
    }  
    return  http_client_request("GET",context);
}
