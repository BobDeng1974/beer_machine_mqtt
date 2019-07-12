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
/** 是否使用RTOS*/
#define  AT_COMMAND_RTOS                 1
/** 特定字符串*/
#define  CRLF                            "\r\n"
#define  VALUE_PREFIX_SEPARATOR          " "
#define  VALUE_SEPARATOR                 ","
/** 每一帧的超时时间*/
#define  AT_COMMAND_FRAME_TIMEOUT        5
/** 状态码域成功和失败字符串数量限制*/
#define  AT_CMD_SUCCESS_CNT_MAX          4 
#define  AT_CMD_FAIL_CNT_MAX             4 
/** 值域值的数量限制*/
#define  AT_VALUE_CNT_MAX                20

 /** at指令回应的状态码域解析*/
typedef struct
{
    char *success[AT_CMD_SUCCESS_CNT_MAX];/**< 回应代表成功的状态码字符串指针*/
    uint8_t success_cnt;/**< 回应代表成功的状态码字符串数量*/
    int success_code;/**< 回应代表成功的状态码返回值*/
    char *fail[AT_CMD_FAIL_CNT_MAX];/**< 回应代表失败的状态码字符串指针*/
    uint8_t fail_cnt;/**< 回应代表失败的状态码字符串数量*/
    int fail_code;/**< 回应代表失败的状态码返回值*/
}at_code_parse_t;

 /** at指令回应的值域解析*/
typedef struct
{
    char *prefix; /**< 值域的前缀*/
    char *value[AT_VALUE_CNT_MAX];/**< 值域的值指针*/
    uint8_t cnt;/**< 值域的值的数量*/
}at_value_parse_t;

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
    at_code_parse_t code_parse;/**< 状态码域解析*/
    at_value_parse_t value_parse;/**< 值域解析*/
    uint16_t parse_offset;/** <解析开始的偏移地址，同时有数据和状态的时候用到*/
}at_command_t;


/**
* @brief AT指令初始化
* @details
* @param command AT指令指针
* @param handle 串口句柄
* @param request 请求的数据指针
* @param response 指向缓存回应数据的地址
* @param response_limit 缓存回应数据的最大长度
* @param timeout 等待超时时间
* @attention
* @note
*/
void at_command_init(at_command_t *command,xuart_handle_t *handle,char *request,uint16_t request_size,char *response,uint16_t response_limit,uint16_t timeout);

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
* @brief AT指令添加期望的解析成功码
* @details
* @param command AT指令指针
* @param code 解析成功的返回值
* @param cnt 成功的字符串数量
* @param ... 成功的字符串
* @return 无
* @attention
* @note
*/
void at_command_add_success_code(at_command_t *command,int code,uint8_t cnt,...);

/**
* @brief AT指令添加期望的解析失败错误码
* @details
* @param command AT指令指针
* @param code 解析失败的返回值
* @param cnt 失败的字符串数量
* @param ... 失败的字符串
* @return 无
* @attention
* @note
*/
void at_command_add_fail_code(at_command_t *command,int code,uint8_t cnt,...);

/**
* @brief AT指令设置回应值前缀
* @details
* @param command AT指令指针
* @param prefix 值的前缀
* @return 无
* @attention
* @note
*/
void at_command_set_value_prefix(at_command_t *command,char *prefix);


/**
* @brief AT指令设置回应中数据量的大小，主要用在数据接收
* @details
* @param command AT指令指针
* @param size 数据量的大小
* @return 无
* @attention
* @note
*/
void at_command_set_response_data_size(at_command_t *command,uint16_t size);

AT_COMMAND_END
#endif