/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Martin d'Allens <martin.dallens@gmail.com> wrote this file. As long as you retain
 * this notice you can do whatever you want with this stuff. If we meet some day,
 * and you think this stuff is worth it, you can buy me a beer in return.
 * ----------------------------------------------------------------------------
 */

#ifndef  __HTTP_CLIENT_H__
#define  __HTTP_CLIENT_H__
#include "stdbool.h"

#define  HTTP_CLIENT_MALLOC(x)           pvPortMalloc((x))
#define  HTTP_CLIENT_FREE(x)             vPortFree((x))
#define  HTTP_BUFFER_SIZE                1600

#define  HTTP_CLIENT_HOST_STR_LEN        50
#define  HTTP_CLIENT_PATH_STR_LEN        200

typedef struct
{
int handle;
char *url;
char *content_type;
char *boundary;
char host[HTTP_CLIENT_HOST_STR_LEN];
uint16_t port;
char path[HTTP_CLIENT_PATH_STR_LEN];
bool connected;
bool is_chunked;
bool is_keep_alive;
bool is_form_data;
uint16_t head_size;
uint32_t content_size;/*总共数据量*/
uint16_t chunk_size;  /*chunk数据量*/
uint16_t rsp_buffer_size;
uint16_t user_data_size;
char *user_data;
char *rsp_buffer;
uint32_t range_start;
uint16_t range_size;
uint32_t timeout;
uint16_t status_code;
}http_client_context_t;


/* 函数名：http_client_get
*  功能：  http GET
*  参数：  context 资源上下文
*  返回：  0：成功 其他：失败
*/
int http_client_get(http_client_context_t *context);

/* 函数名：http_client_post
*  功能：  http GET
*  参数：  context 资源上下文
*  返回：  0：成功 其他：失败
*/
int http_client_post(http_client_context_t *context);
/* 函数名：http_client_download
*  功能：  http GET
*  参数：  context 资源上下文
*  返回：  0：成功 其他：失败
*/
int http_client_download(http_client_context_t *context);


#endif
