#ifndef  __AT_COMMAND_H__
#define  __AT_COMMAND_H__
 
#ifdef  __cplusplus
#define  AT_COMMAND_BEGIN extern "C" {
#define  AT_COMMAND_END   }
#else
#define  AT_COMMAND_BEGIN
#define  AT_COMMAND_END
#endif
 
AT_COMMAND_BEGIN


#define  AT_COMMAND_FRAME_TIMEOUT        10


 /** @brief AT指令结构*/
typedef struct
{
    xuart_handle_t *handle;/**< uart句柄指针*/
    char *request; /**< 请求的数据指针*/
    char *response;/**< 回应的数据指针*/
    uint16_t request_size;/**< 请求的数据长度*/
    uint16_t response_size;/**< 回应的数据量*/
    uint16_t response_limit;/**< 回应的数据最大值*/
    uint16_t timeout;/**< 超时时间*/
}at_command_t;


/**
* @brief AT指令构建
* @details
* @param command AT指令指针
* @param handle 串口句柄
* @param request 请求的数据指针
* @param request_size 请求的数据大小
* @param response 指向缓存回应数据的地址
* @param response_limit 缓存回应数据的最大长度
* @param timeout 等待超时时间
* @return 初始化是否成功
* @retval 0 成功
* @retval -1 失败
* @attention
* @note
*/
int at_command_build(at_command_t *command,xuart_handle_t *handle,char *request,uint16_t request_size,char *response,uint16_t response_limit,uint16_t timeout);


/**
* @brief AT指令执行
* @details
* @param command AT指令指针
* @return 执行是否成功
* @retval 0 成功
* @retval -1 失败
* @attention
* @note
*/
int at_command_execute(at_command_t *command);

/**
* @brief AT找出回应的一行字符串
* @details
* @param response AT指令的回应
* @param separator 行的分隔符
* @param line 找到的每个行的指针
* @param line_cnt 允许找到的行的最大的数量
* @return 执行是否成功
* @retval >= 0 成功找到的行数量
* @retval -1 失败
* @attention
* @note
*/
int at_command_seek_line(char *response,char *separator,char **line,uint8_t line_cnt);


/**
* @brief AT找出回应的一行字符串里多个值
* @details
* @param line AT指令回应的一行字符串
* @param prefix 值前缀符
* @param separator 值的分隔符
* @param value 每个值的指针
* @param value_cnt 允许找到的值最大的数量
* @return 执行是否成功
* @retval >= 0 成功找到的值数量
* @retval -1 失败
* @attention
* @note
*/
int at_command_seek_value(char *line,char *prefix,char *separator,char **value,uint8_t value_cnt);

AT_COMMAND_END
#endif