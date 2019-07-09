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
#include "debug_assert.h"
#include "xuart.h"
#include "cmsis_os.h"
#include "at.h"
#include "log.h"

/**
* @brief AT指令构建
* @details
* @param command AT指令指针
* @param handle 串口句柄
* @param request 请求的数据指针
* @param response 指向缓存回应数据的地址
* @param response_limit 缓存回应数据的最大长度
* @param timeout 等待超时时间
* @return 初始化是否成功
* @retval 0 成功
* @retval -1 失败
* @attention
* @note
*/
int at_command_build(at_command_t *command,xuart_handle_t *handle,char *request,uint16_t request_size,char *response,uint16_t response_limit,uint16_t timeout)
{
    if (command == NULL) {
        log_error("at command is null.\r\n");
        return -1;
    }
    command->handle = handle;
    command->request = request;
    command->request_size = request_size;
    command->response = response;
    command->response_limit = response_limit;
    command->timeout = timeout;

    return 0;   
}

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
int at_command_execute(at_command_t *command)
{
    uint8_t complete = 0;
    uint32_t select_size,read,read_total = 0;
    uint32_t write;

    if (command == NULL || command->handle == NULL || command->response == NULL ||\
        (command->request_size > 0 && command->request == NULL)) {
        log_error("at command param is null.\r\n");
        return -1;
    }
    log_debug("at request:%s\r\n",command->request);
    /*清除上次结果*/
    xuart_clear(command->handle);
    if (command->request_size > 0) {
        /*写入输出*/
        write = xuart_write(command->handle,(uint8_t *)command->request,command->request_size);
        if (write != command->request_size) {
            log_error("at commad write size error.expect:%d write:%d.\r\n",command->request_size,write);
            return -1;
        }
    }
    /*等待数据*/
    select_size = xuart_select(command->handle,command->timeout);
    if (select_size == 0) {
        log_error("at command:%s wait timeout.\r\n",command->request != NULL ? command->request : "");
        return -1;
    }
    /*轮询等待接收完毕*/
    while (complete == 0) {  
        /*判断回应的数据是否超过限制*/
        if (read_total + select_size >= command->response_limit) {
            log_error("at response size:%d too large than limit:%d.\r\n",read_total + select_size,command->response_limit);
            return -1;
        }
        /*读取数据*/
        read = xuart_read(command->handle,(uint8_t *)command->response + read_total,select_size);
        read_total += read; 
        /*再次等待数据*/
        select_size = xuart_select(command->handle,AT_COMMAND_FRAME_TIMEOUT); 
        /*没有数据了*/
        if (select_size == 0) {
            command->response_size = read_total;
            command->response[read_total] = 0;/*补全为字符串格式*/
            complete = 1;
        }          
    }

    log_debug("at response:%s.\r\n",command->response);
    return 0;   
}

/**
* @brief AT找出回应的一行字符串
* @details
* @param response AT指令的回应
* @param line_separator 行的分隔符
* @param line 找到的每个行的指针
* @param line_limit 允许找出的行的最大的数量
* @return 成功找到的行数量
* @attention
* @note
*/
int at_command_seek_line(char *response,char *line_separator,char **line,uint8_t line_limit)
{
    uint8_t cnt = 0;
    char *token;

    log_assert_null_ptr(response);
    log_assert_null_ptr(line_separator);
    log_assert_null_ptr(line);

    /*查找回应的行*/
    token = strtok(response,line_separator);
    
    while (token && line_limit --) {
        *line ++ = token;
        cnt ++;
        token = strtok(NULL,line_separator);
    }

    return cnt;
}
  
/**
* @brief AT找出回应的一行字符串里多个值
* @details
* @param line AT指令回应的一行字符串
* @param prefix 值前缀符
* @param separator 值的分隔符
* @param value 每个值的指针
* @param value_limit 值的最大的数量
* @return 执行是否成功
* @retval >= 0 成功找到的值数量
* @retval -1 失败
* @attention
* @note
*/
int at_command_seek_value(char *line,char *value_prefix,char *value_separator,char **value,uint8_t value_limit)
{
    char *token,*rsp_prefix,*value_start;

    log_assert_null_ptr(line);
    log_assert_null_ptr(value_prefix);
    log_assert_null_ptr(value_separator);
    log_assert_null_ptr(value);

    if (value_prefix) {
        /*查找前缀符号*/
        rsp_prefix = strstr(line,value_prefix);
        if (rsp_prefix == NULL) {
            log_error("回应值的前缀不匹配.\r\n")
            return -1;
        }
        value_start = line + strlen(value_prefix);
    } else {
        value_start = line;
    }

    /*查找回应的行*/
    token = strtok(value_start,value_separator);
    while (token && value_limit --) {
        *value ++ = token;
        token = strtok(NULL,value_separator);
    }
    if (token || value_limit) {
        log_error("回应值数量 != %d.\r\n",value_limit);
        return -1;
    }

    return 0;
}
