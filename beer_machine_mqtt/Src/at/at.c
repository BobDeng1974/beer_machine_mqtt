/**
****************************************************************************************                                                                                                                                                       
*                                                                            
*  This program is free software; you can redistribute it and/or modify      
*  it under the terms of the GNU General Public License version 3 as         
*  published by the Free Software Foundation.                                
*                                                                            
*  @file       at.c
*  @brief      at指令
*  @author     wkxboot
*  @version    v1.0.0
*  @date       2019/7/3
*  @copyright  <h4><left>&copy; copyright(c) 2019 wkxboot 1131204425@qq.com</center></h4>  
*                                                                            
*                                                                            
****************************************************************************************/
#include "string.h"
#include "stdarg.h"
#include "cmsis_os.h"
#include "debug_assert.h"
#include "xuart.h"
#include "tiny_timer.h"
#include "cmsis_os.h"
#include "at.h"
#include "log.h"


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
void at_command_init(at_command_t *command,xuart_handle_t *handle,char *request,uint16_t request_size,char *response,uint16_t response_limit,uint16_t timeout)
{
    log_assert_null_ptr(command);

    memset(command,0,sizeof(at_command_t));

    command->handle = handle;
    command->request = request;
    command->request_size = request_size;
    command->response = response;
    command->response_limit = response_limit;
    command->timeout = timeout;  
}


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
void at_command_add_success_code(at_command_t *command,int code,uint8_t cnt,...)
{
    va_list ap;
    
    log_assert_null_ptr(command);
    log_assert_bool_false(cnt <= AT_CMD_SUCCESS_CNT_MAX);

    va_start(ap,cnt);

    for (uint8_t i = 0;i < cnt;i ++) {
        command->code_parse.success[i] = va_arg(ap,char *);
    }
    command->code_parse.success_code = code;
    command->code_parse.success_cnt = cnt;
    
}

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
void at_command_add_fail_code(at_command_t *command,int code,uint8_t cnt,...)
{
    va_list ap;
    
    log_assert_null_ptr(command);
    log_assert_bool_false(cnt <= AT_CMD_SUCCESS_CNT_MAX);

    va_start(ap,cnt);

    for (uint8_t i = 0;i < cnt;i ++) {
        command->code_parse.fail[i] = va_arg(ap,char *);
    }
    command->code_parse.fail_code = code;
    command->code_parse.fail_cnt = cnt;

}

/**
* @brief AT指令设置回应值前缀
* @details
* @param command AT指令指针
* @param prefix 值的前缀
* @return 无
* @attention
* @note
*/
void at_command_set_value_prefix(at_command_t *command,char *prefix)
{
    log_assert_null_ptr(command);
    log_assert_null_ptr(prefix);

    command->value_parse.prefix = prefix;
}
/**
* @brief AT指令设置回应中数据量的大小，主要用在数据接收
* @details
* @param command AT指令指针
* @param size 数据量的大小
* @return 无
* @attention
* @note
*/
void at_command_set_response_data_size(at_command_t *command,uint16_t size)
{
    log_assert_null_ptr(command);

    command->parse_offset = size;
}


/**
* @brief AT指令解析回应，并得到错误码
* @details
* @param command AT指令指针
* @param token 比对的字符串
* @param code 解析得到的错误码
* @return 是否解析到期望的值 0 是 -1 否
* @attention
* @note
*/
static int at_command_parse_code(at_command_t *command,char *token,int *code)
{
    log_assert_null_ptr(command);
    log_assert_null_ptr(token);
    log_assert_null_ptr(code);

    for (uint8_t i = 0;i < command->code_parse.success_cnt;i ++) {
        if (strncmp(command->code_parse.success[i],token,strlen(command->code_parse.success[i])) == 0) {
            *code = command->code_parse.success_code;
            return 0;
        }
    }
    for (uint8_t i = 0;i < command->code_parse.fail_cnt;i ++) {
        if (strncmp(command->code_parse.fail[i],token,strlen(command->code_parse.fail[i])) == 0) {
            *code = command->code_parse.fail_code;
            return 0;
        }
    }
    return -1;
}

/**
* @brief AT指令解析回应，并得到错误码
* @details
* @param command AT指令指针
* @param token 比对的字符串
* @param code 解析得到的错误码
* @return 是否解析到期望的值 0 是 -1 否
* @attention
* @note
*/
static void at_command_parse_value(at_command_t *command,char *token)
{
    char *prefix;
    char *value;
    log_assert_null_ptr(command);
    log_assert_null_ptr(token);

    /*需要解析值*/
    if (command->value_parse.prefix) {
        prefix = strtok(token,VALUE_PREFIX_SEPARATOR);
        if (prefix && strcmp(prefix,command->value_parse.prefix) == 0) {
            value = strtok(NULL,VALUE_SEPARATOR);
            while (value && command->value_parse.cnt < AT_VALUE_CNT_MAX) {
                command->value_parse.value[command->value_parse.cnt] = value;
                command->value_parse.cnt ++;
                value = strtok(NULL,VALUE_SEPARATOR);
            }
        }
    } else {
        /*没有前缀，整体作为值*/
        command->value_parse.value[command->value_parse.cnt] = token;
        command->value_parse.cnt ++;
    }
}


/**
* @brief AT指令解析每一帧的回应
* @details
* @param command AT指令指针
* @param frame 回应的帧字符串
* @return 是否解析到期望的值 0 是 -1 否
* @attention
* @note
*/
static int at_command_parse_frame(at_command_t *command,char *frame,int *code)
{
    int rc;
    char *line;
    char *start,*next;

    log_assert_null_ptr(command);
    log_assert_null_ptr(frame);

    /*找出每一行*/
    start = frame;
    while (start) {
        line = strtok_r(start,CRLF,&next);
        if (line == NULL) {
            break;
        }
        /*先解析code码*/
        rc = at_command_parse_code(command,line,code);
        /*code码找到，直接退出*/
        if (rc == 0) {
            return 0;
        }
        /*如果没解析出code，尝试解析回应值*/
        at_command_parse_value(command,line);
        /*循环*/
        start = next;
    }

    return -1;
}


/**
* @brief AT接收一帧数据
* @details
* @param handle 串口句柄
* @param buffer 缓存数据的指针
* @param size 缓存大小
* @param timeout 超时时间
* @return 发送的数量
* @attention
* @note
*/
static int at_wait_frame_data(xuart_handle_t *handle,char *buffer,uint16_t size,uint32_t timeout)
{
    uint8_t frame_complete = 0;
    uint32_t select_size;
    uint32_t read,frame_size = 0;

    /*等待数据*/
    select_size = xuart_select(handle,timeout);
    if (select_size == 0) {
        return 0;
    }
    /*轮询等待接收完毕*/
    while (frame_complete == 0) {  
        /*判断回应的数据是否超过限制*/
        if (frame_size + select_size >= size) {
            log_error("at frame size:%d too large than free:%d.\r\n",frame_size + select_size,size);
            return -1;
        }

        /*读取数据*/
        read = xuart_read(handle,(uint8_t *)buffer + frame_size,select_size);
        frame_size += read;
        /*再次等待数据*/
        select_size = xuart_select(handle,AT_COMMAND_FRAME_TIMEOUT); 
        /*接收完一帧数据*/
        if (select_size == 0) {
            buffer[frame_size] = '\0';
            frame_complete = 1;
        }
    }
    return frame_size;
}
/**
* @brief AT发送一帧数据
* @details
* @param handle 串口句柄
* @param buffer 数据指针
* @param size 数据数量
* @param timeout 超时时间
* @return 发送的数量
* @attention
* @note
*/
static uint32_t at_send_frame_data(xuart_handle_t *handle,char *buffer,uint16_t size,uint32_t timeout)
{
    uint32_t write = 0,remain_size;

    if (size > 0) {
        /*写入输出*/
        write = xuart_write(handle,(uint8_t *)buffer,size);
        remain_size = xuart_complete(handle,timeout);
    }

    return write - remain_size;
}

 /**< at指令的信号量*/
typedef struct
{
    uint8_t is_init;
    osMutexId id;
}at_command_mutex_t;
static at_command_mutex_t at_command_mutex;

/**
* @brief at命令信号量初始化
* @details
* @param mutex 信号量指针
* @return 无
* @attention
* @note
*/
void at_command_mutex_init(at_command_mutex_t* mutex)
{
    osMutexDef(at_command_mutex);
    mutex->id = osMutexCreate(osMutex(at_command_mutex));
    log_assert_null_ptr(mutex->id);
}
/**
* @brief at命令信号量获取
* @details
* @param mutex 信号量指针
* @return 获取结果
* @attention
* @note
*/
int at_command_mutex_lock(at_command_mutex_t* mutex)
{
    return osMutexWait(mutex->id, osWaitForever);
}
/**
* @brief at命令信号量释放
* @details
* @param mutex 信号量指针
* @return 释放结果
* @attention
* @note
*/
int at_command_mutex_unlock(at_command_mutex_t* mutex)
{
    return osMutexRelease(mutex->id);
}


/**
* @brief AT指令执行线程安全
* @details
* @param command AT指令指针
* @return 执行结果
* @attention
* @note
*/
int at_command_execute(at_command_t *command)
{
    int rc,code = -1;
    uint8_t complete = 0;
    int frame_size,response_size;
    int parse_start_offset = -1;
    tiny_timer_t timer;

    log_assert_null_ptr(command);
    log_assert_null_ptr(command->handle);
    log_assert_null_ptr(command->response);

    log_assert_bool_false(!(command->request_size > 0 && command->request == NULL));

    log_debug("at request:%s\r\n",command->request);

    response_size = 0;
    tiny_timer_init(&timer,0,command->timeout);

#if  AT_COMMAND_RTOS > 0
    if (at_command_mutex.is_init == 0) {
        at_command_mutex_init(&at_command_mutex);
        at_command_mutex.is_init = 1;
    }
    at_command_mutex_lock(&at_command_mutex);
#endif
    /*发送请求*/
    rc = at_send_frame_data(command->handle,command->request,command->request_size,tiny_timer_value(&timer));
    if (rc != command->request_size) {
        log_error("at commad write size error.expect:%d write:%d.\r\n",command->request_size,rc);
        goto exit;
    }

    /*等待回应*/
    while (complete == 0) {
        frame_size = at_wait_frame_data(command->handle,command->response + response_size,command->response_limit - response_size,tiny_timer_value(&timer));
        if (frame_size < 0) {
            rc = -1;
            goto exit;
        }
        /*等待超时*/
        if (frame_size == 0) {
            log_error("at command:%s wait timeout.\r\n",command->request != NULL ? command->request : "null");
            rc = -1;
            goto exit;
        }
        /*解析这帧数据*/
        /*如果没有开始解析*/
        if (response_size + frame_size >= command->parse_offset) {
            if (parse_start_offset == -1) {
                parse_start_offset = command->parse_offset;
            } else {
                parse_start_offset = response_size;
            }
            log_debug("at response frame:%s\r\n",command->response + parse_start_offset);
            response_size += frame_size;
            command->response_size = response_size;
            rc = at_command_parse_frame(command,command->response + parse_start_offset,&code);
            /*在这一帧中解析成功*/
            if (rc == 0) {
                complete = 1;
                goto exit;
            }
        }
    }

exit:
#if  AT_COMMAND_RTOS > 0
    at_command_mutex_unlock(&at_command_mutex);
#endif
    if (rc == 0) {
        log_debug("at command parse success.code:%d\r\n",code);
        return code;
    }
    return -1;
}
