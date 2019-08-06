#ifndef  __HTTP_CLIENT_H__
#define  __HTTP_CLIENT_H__
#include "stdbool.h"

#define  HTTP_CLIENT_MALLOC(x)           pvPortMalloc((x))
#define  HTTP_CLIENT_FREE(x)             vPortFree((x))
#define  HTTP_BUFFER_SIZE                1600

#define  HTTP_CLIENT_HOST_STR_LEN        50
#define  HTTP_CLIENT_PATH_STR_LEN        200

 /** form数据结构*/
typedef struct
{
    char *name;
    char *value;
}form_data_t;

 /** http client上下文*/
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

/**
* @brief http get方法
* @details
* @param context http上下文
* @return 0：成功 -1：失败
* @attention
* @note
*/
int http_client_get(http_client_context_t *context);

/**
* @brief http post方法
* @details
* @param context http上下文
* @return 0：成功 -1：失败
* @attention
* @note
*/
int http_client_post(http_client_context_t *context);

/**
* @brief http download方法
* @details
* @param context http上下文
* @return 0：成功 -1：失败
* @attention
* @note
*/
int http_client_download(http_client_context_t *context);



#endif
