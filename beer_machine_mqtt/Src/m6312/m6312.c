/**
******************************************************************************                                                                                                                                                       
*                                                                            
*  This program is free software; you can redistribute it and/or modify      
*  it under the terms of the GNU General Public License version 3 as         
*  published by the Free Software Foundation.                                
*                                                                            
*  @file       m6312.c
*  @brief      中移动M6312模块
*  @author     wkxboot
*  @version    v1.0.0
*  @date       2019/7/3
*  @copyright  <h4><left>&copy; copyright(c) 2019 wkxboot 1131204425@qq.com</center></h4>  
*                                                                            
*                                                                            
*****************************************************************************/
#include "beer_machine.h"
#include "debug_assert.h"
#include "tiny_timer.h"
#include "xuart.h"
#include "st_cm3_uart_hal_driver.h"
#include "at.h"
#include "string.h"
#include "stdlib.h"
#include "m6312.h"
#include "cmsis_os.h"
#include "log.h"

 /** m6312串口句柄*/
xuart_handle_t m6312_uart_handle;

 /** m6312串口句柄*/
#define  M6312_RECV_BUFFER_SIZE                           1600 /**< m6312串口接收缓存大小*/
#define  M6312_SEND_BUFFER_SIZE                           1600 /**< m6312串口发送缓存大小*/
static uint8_t recv_buffer[M6312_RECV_BUFFER_SIZE];/**< m6312串口接收缓存*/
static uint8_t send_buffer[M6312_SEND_BUFFER_SIZE];/**< m6312串口发送缓存*/

#define  M6312_PWR_ON_TIMEOUT                              5000  /**< m6312模块开机超时时间*/
#define  M6312_PWR_OFF_TIMEOUT                             15000 /**< m6312模块关机超时时间*/

#define  M6312_RESPONSE_TIMEOUT                            10000 /**< m6312模块回应超时时间*/
#define  M6312_RESPONSE_BUFFER_SIZE                        100   /**< m6312模块回应缓存大小*/
#define  M6312_REQUEST_BUFFER_SIZE                         100   /**< m6312模块请求缓存大小*/
#define  M6312_RESPONSE_LINE_CNT_MAX                       10    /**< m6312模块回应行的最大数量*/
#define  M6312_RESPONSE_VALUE_CNT_MAX_PER_LINE             10    /**< m6312模块回应值每行最大数量*/
/**
* @brief M6312中断
* @param 无
* @return 无
*/
void m6312_isr(void)
{
    if (m6312_uart_handle.is_port_open) {
        st_uart_hal_isr(&m6312_uart_handle);
    }
}

/**
* @brief M6312模块开机
* @param 无
* @return 开机是否成功
* @retval 0 成功
* @retval -1 失败
* @attention 无
* @note 无
*/
int m6312_pwr_on(void)
{
    tiny_timer_t timer;
  
    tiny_timer_init(&timer,0,M6312_PWR_ON_TIMEOUT);
  
    if (bsp_get_gsm_pwr_status() == BSP_GSM_STATUS_PWR_ON) {
        return 0;
    }
  
    bsp_gsm_pwr_key_press();
    while (tiny_timer_value(&timer) && bsp_get_gsm_pwr_status() != BSP_GSM_STATUS_PWR_ON) {
        osDelay(50);
    }
    bsp_gsm_pwr_key_release();  
  
    return bsp_get_gsm_pwr_status() == BSP_GSM_STATUS_PWR_ON ? 0 : -1;
}

/**
* @brief M6312模块关机
* @param 无
* @return 关机是否成功
* @retval 0 成功
* @retval -1 失败
* @attention 无
* @note 无
*/
int m6312_pwr_off(void)
{
    tiny_timer_t timer;
  
    tiny_timer_init(&timer,0,M6312_PWR_OFF_TIMEOUT);
  
    if (bsp_get_gsm_pwr_status() == BSP_GSM_STATUS_PWR_OFF) {
        return 0;
    }
  
    bsp_gsm_pwr_key_press();
    while (tiny_timer_value(&timer) && bsp_get_gsm_pwr_status() != BSP_GSM_STATUS_PWR_OFF) {
        osDelay(50);
    }
    bsp_gsm_pwr_key_release();  
  
    return bsp_get_gsm_pwr_status() == BSP_GSM_STATUS_PWR_OFF ? 0 : -1;
}


/**
* @brief M6312模块串口初始化关机
* @param 无
* @return 初始化是否成功
* @retval 0 成功
* @retval -1 失败
* @attention 无
* @note 无
*/
int m6312_uart_init(void)
{
    int rc;

	rc = xuart_init(&xuart_hal_driver);
    if (rc != 0) {
        log_error("m6312 uart init err.\r\n");
        return -1;
    }

    rc = xuart_open(&m6312_uart_handle,M6312_UART_PORT,M6312_UART_BAUD_RATE,
                    M6312_UART_DATA_BIT,M6312_UART_STOP_BIT,recv_buffer,M6312_RECV_BUFFER_SIZE,
                    send_buffer,M6312_SEND_BUFFER_SIZE);
    if (rc != 0) {
        log_error("m6312 uart open err.\r\n");
        return -1;
    }

	return 0; 
}




/**
* @brief M6312模块执行请求
* @param request 请求的字符串
* @param request_size 命令字符串长度
* @param response 回应字符串值保存的地址
* @param response_limit 回应的字符串数量最大限制
* @param timeout 超时时间
* @return 执行是否成功,成功时返回接收的数量
* @retval >=0 执行成功,
* @retval -1 执行失败
* @attention 无
* @note 无
*/
static int m6312_request(char *request,uint16_t request_size,char *response,uint16_t response_limit,uint16_t timeout)
{
    int rc;
    at_command_t command;

    /*构建at命令*/
    rc = at_command_build(&command,&m6312_uart_handle,request,request_size,response,response_limit,timeout);
    if (rc != 0) {
        log_error("m6312命令构建失败.\r\n");
        return -1;
    }
    /*执行at命令，并等待回应完成*/
    rc = at_command_execute(&command);
    if (rc != 0) {
        log_error("m6312命令执行失败.\r\n");
        return -1;
    }

    log_debug("m6312命令执行成功\r\n");
    return command.response_size;
}

/**
* @brief M6312模块解析回应
* @param response 回应的字符串指针
* @param line_separator 行分隔符
* @param expect_success 期望的成功状态字符串
* @param value_prefix 回应值的前缀
* @param value_separator 回应值的分隔符
* @param rsp_value_array 回应值的指针数组
* @param value_cnt_per_line 回应值在每行的数量
* @return 是否成功
* @retval >= 0 执行成功，得到的值的数量
* @retval -1 执行失败
* @attention 无
* @note 无
*/
static int m6312_parse_response(char *response,char *line_separator,char **rsp_status,char *value_prefix,char *value_separator,char **rsp_value_array,uint16_t value_cnt_per_line)
{
    int rc;
    uint8_t index_max,index = 0;
    uint16_t value_cnt = 0;
    char *line[M6312_RESPONSE_LINE_CNT_MAX];

    /*找出所有回应的行*/
    rc = at_command_seek_line(response,line_separator,line,M6312_RESPONSE_LINE_CNT_MAX);
    if (rc <= 0) {
        log_error("m6312回应行错误.\r\n");
        return -1;
    }
    index_max = rc - 1;
    /*如果有状态值，对比状态值*/
    if (rsp_status) {  
        *rsp_status = line[index_max];
        if (index_max > 1) {
            index_max -= 1;
        }
    }
    /*如果回应行里没有回应值*/
    if (value_cnt_per_line == 0 && index_max != 0) {
        log_error("m6312回应行错误.\r\n");
        return -1;
    }
    
    /*如果有回应值，解析回应的值*/
    if (value_cnt_per_line > 0) {
        while (index < index_max) {
            /*找出回应的值*/
            rc = at_command_seek_value(line[index],value_prefix,value_separator,rsp_value_array,value_cnt_per_line);
            if (rc != 0) {
                log_error("m6312回应值错误.\r\n");
                return -1;
            }
            value_cnt += value_cnt_per_line;
            index ++;
        }  
    }
    
    log_debug("m6312请求成功\r\n");
    return value_cnt;
}

/**
* @brief M6312模块回显控制开关
* @param on_off 开关值 =0 关闭 >0 打开
* @return M6312模块回显控制是否成功
* @retval 0 模块回显控制成功
* @retval -1 模块回显控制失败
* @attention 无
* @note 无
*/
int m6312_echo_turn_on_off(uint8_t on_off)
{
    int rc;
    char *rsp_status;
    char response[M6312_RESPONSE_BUFFER_SIZE];
    char *request = on_off == 0 ? "ATE0\r\n" : "ATE1\r\n";

    /*m6312执行请求*/
    rc = m6312_request(request,strlen(request),response,M6312_RESPONSE_BUFFER_SIZE,M6312_RESPONSE_TIMEOUT);
    if (rc < 0) {
        goto err_exit;
    }
    /*m6312解析回应*/
    rc = m6312_parse_response(response,CRLF,&rsp_status,NULL,NULL,NULL,0);
    if (rc != 0) {
        goto err_exit;
    }
    /*对比状态值*/
    if (strcmp(rsp_status,"OK") != 0) {
        goto err_exit;
    }
    log_debug("控制回显成功.\r\n");
    return 0;

err_exit:
    log_error("m6312控制回显失败.\r\n");
    return -1;
}

/**
* @brief M6312模块获取sim卡状态
* @param status  @see m6312_sim_card_status_t sim卡状态
* @return 获取状态是否成功
* @attention 无
* @note 无
*/
int m6312_get_sim_card_status(m6312_sim_card_status_t *status)
{
    int rc;
    char *rsp_status;
    char *rsp_value;
    char response[M6312_RESPONSE_BUFFER_SIZE];
    char *request = "AT+CPIN?\r\n";

    /*m6312执行请求*/
    rc = m6312_request(request,strlen(request),response,M6312_RESPONSE_BUFFER_SIZE,M6312_RESPONSE_TIMEOUT);
    if (rc < 0) {
        goto err_exit;
    }
    /*m6312解析回应*/
    rc = m6312_parse_response(response,CRLF,&rsp_status,"+CPIN: ",",",&rsp_value,1);
    if (rc != 1) {
        goto err_exit;
    }
    /*对比状态值*/
    if (strcmp(rsp_status,"OK") != 0) {
        goto err_exit;
    }
    /*比对回应值*/
    if (strcmp(rsp_value,"READY") == 0) {
        *status = M6312_SIM_CARD_EXIST;
    } else if (strcmp(rsp_value,"NO SIM") == 0) {
        *status = M6312_SIM_CARD_NO_EXIST;
    } else if (strcmp(rsp_value,"BLOCK") == 0) {
        *status = M6312_SIM_CARD_BLOCK;
    } else {
        *status = M6312_SIM_CARD_UNKNOW;
    }  
    log_debug("m6312获取sim卡状态:%s成功.\r\n",rsp_value);
    return 0;

err_exit:
    log_error("m6312获取sim卡状态失败.\r\n");
    return -1;
}


/**
* @brief M6312模块获取运营商
* @param *sim_operator  运营商指针 @see m6312_sim_operator_t
* @return 获取是否成功
* @retval 0 获取成功
* @return -1 获取失败
* @attention 无
* @note 无
*/
int m6312_get_operator(m6312_sim_operator_t *sim_operator)
{
    int rc;
    char *rsp_status;
    char *rsp_value[3];
    char response[M6312_RESPONSE_BUFFER_SIZE];
    char *request = "AT+COPS?\r\n";

    /*m6312执行请求*/
    rc = m6312_request(request,strlen(request),response,M6312_RESPONSE_BUFFER_SIZE,M6312_RESPONSE_TIMEOUT);
    if (rc < 0) {
        goto err_exit;
    }
    /*m6312解析回应*/
    rc = m6312_parse_response(response,CRLF,&rsp_status,"+COPS: ",",",rsp_value,3);
    if (rc != 3) {
        goto err_exit;
    }
    /*对比状态值*/
    if (strcmp(rsp_status,"OK") != 0) {
        goto err_exit;
    }
    /*比对回应值*/
    if (strcmp(rsp_value[2],"46000") == 0 || strcmp(rsp_value[2],"CMCC") == 0 || strcmp(rsp_value[2],"ChinaMobile") == 0) {
        *sim_operator = SIM_OPERATOR_CHINA_MOBILE;
    } else if (strcmp(rsp_value[2],"46001") == 0 || strcmp(rsp_value[2],"CU") == 0 || strcmp(rsp_value[2],"ChinaUnicom") == 0) {
        *sim_operator = SIM_OPERATOR_CHINA_UNICOM;
    } else  {
        goto err_exit;
    }  
    log_debug("m6312获取sim卡运营商:%s成功.\r\n",rsp_value[2]);
    return 0;

err_exit:
    log_error("m6312获取sim卡运营商失败.\r\n");
    return -1;
}

/**
* @brief M6312模块获取sim卡id
* @param sim_id sim卡id保存的指针
* @return 获取状态是否成功
* @attention 无
* @note 无
*/
int m6312_get_sim_card_id(char *sim_id)
{
    int rc;
    char *rsp_status;
    char *rsp_value;
    char response[M6312_RESPONSE_BUFFER_SIZE];
    char *request = "AT+CCID?\r\n";

    /*m6312执行请求*/
    rc = m6312_request(request,strlen(request),response,M6312_RESPONSE_BUFFER_SIZE,M6312_RESPONSE_TIMEOUT);
    if (rc < 0) {
        goto err_exit;
    }
    /*m6312解析回应*/
    rc = m6312_parse_response(response,CRLF,&rsp_status,"+CCID: ",",",&rsp_value,1);
    if (rc != 1) {
        goto err_exit;
    }
    /*对比状态值*/
    if (strcmp(rsp_status,"OK") != 0) {
        goto err_exit;
    }
    log_debug("m6312获取sim id:%s成功.\r\n",rsp_value);
    return 0;

err_exit:
    log_error("m6312获取sim卡id失败.\r\n");
    return -1;
}

/**
* @brief M6312模块获取GPRS附着状态
* @param status gprs状态指针
* @return 获取状态是否成功
* @attention 无
* @note 无
*/
int m6312_get_gprs_attach_status(m6312_gprs_attach_status_t *status)
{
    int rc;
    char *rsp_status;
    char *rsp_value;
    char response[M6312_RESPONSE_BUFFER_SIZE];
    char *request = "AT+CGATT?\r\n";

    /*m6312执行请求*/
    rc = m6312_request(request,strlen(request),response,M6312_RESPONSE_BUFFER_SIZE,M6312_RESPONSE_TIMEOUT);
    if (rc < 0) {
        goto err_exit;
    }
    /*m6312解析回应*/
    rc = m6312_parse_response(response,CRLF,&rsp_status,"+CGATT: ",",",&rsp_value,1);
    if (rc != 1) {
        goto err_exit;
    }
    /*对比状态值*/
    if (strcmp(rsp_status,"OK") != 0) {
        goto err_exit;
    }
    /*比对回应值*/
    if (strcmp(rsp_value,"0") == 0) {
        *status = M6312_GPRS_DETACH;
    } else {
        *status = M6312_GPRS_ATTACH;
    } 

    log_debug("m6312查询gprs附着状态:%s成功.\r\n",rsp_value);
    return 0;

err_exit:
    log_error("m6312查询gprs附着状态失败.\r\n");
    return -1;
}

/**
* @brief M6312模块设置GPRS附着状态
* @param attach gprs附着状态 @see m6312_gprs_attach_status_t
* @return 设置是否成功
* @retval 0 成功
* @retval -1 失败
* @attention 无
* @note 无
*/
int m6312_set_gprs_attach(m6312_gprs_attach_status_t attach)
{
    int rc;
    char *rsp_status;
    char response[M6312_RESPONSE_BUFFER_SIZE];
    char *request = attach == M6312_GPRS_ATTACH ? "AT+CGATT=1\r\n" : "AT+CGATT=0\r\n";

    /*m6312执行请求*/
    rc = m6312_request(request,strlen(request),response,M6312_RESPONSE_BUFFER_SIZE,M6312_RESPONSE_TIMEOUT);
    if (rc < 0) {
        goto err_exit;
    }
    /*m6312解析回应*/
    rc = m6312_parse_response(response,CRLF,&rsp_status,NULL,NULL,NULL,0);
    if (rc != 0) {
        goto err_exit;
    }
    /*对比状态值*/
    if (strcmp(rsp_status,"OK") != 0) {
        goto err_exit;
    }
   
    log_debug("设置gprs附着状态:%d成功.\r\n",attach);
    return 0;

err_exit:
    log_error("设置gprs附着状态:%d失败.\r\n",attach);
    return -1;
}

/**
* @brief M6312模块设置apn
* @param apn 运营商apn
* @return M6312模块设置apn是否成功
* @retval 0 设置apn成功
* @retval -1 设置apn失败
* @attention 无
* @note 无
*/
int m6312_set_gprs_apn(char *apn)
{
    int rc;
    char *rsp_status;
    char response[M6312_RESPONSE_BUFFER_SIZE];
    char request[M6312_REQUEST_BUFFER_SIZE];

    snprintf(request,M6312_REQUEST_BUFFER_SIZE,"AT+CGDCONT=1,IP,%s\r\n",apn);
    /*m6312执行请求*/
    rc = m6312_request(request,strlen(request),response,M6312_RESPONSE_BUFFER_SIZE,M6312_RESPONSE_TIMEOUT);
    if (rc < 0) {
        goto err_exit;
    }
    /*m6312解析回应*/
    rc = m6312_parse_response(response,CRLF,&rsp_status,NULL,NULL,NULL,0);
    if (rc != 0) {
        goto err_exit;
    }
      /*对比状态值*/
    if (strcmp(rsp_status,"OK") != 0) {
        goto err_exit;
    }

    log_debug("m6312设置apn:%s成功.\r\n",apn);
    return 0;

err_exit:
    log_error("m6312设置apn:%s失败.\r\n",apn);
    return -1;
}


/**
* @brief M6312模块获取gprs网络状态
* @param status m6312模块gprs网络状态指针
* @return M6312模块获取状态是否成功
* @retval 0 获取状态成功
* @retval -1 获取状态失败
* @attention 无
* @note 无
*/
int m6312_get_gprs_net_status(m6312_gprs_net_status_t *status)
{
    int rc;
    char *rsp_status;
    char *rsp_value[2];
    char response[M6312_RESPONSE_BUFFER_SIZE];
    char *request = "AT+CGACT?\r\n";

    /*m6312执行请求*/
    rc = m6312_request(request,strlen(request),response,M6312_RESPONSE_BUFFER_SIZE,M6312_RESPONSE_TIMEOUT);
    if (rc < 0) {
        goto err_exit;
    }
    /*m6312解析回应*/
    rc = m6312_parse_response(response,CRLF,&rsp_status,"+CGACT: ",",",rsp_value,2);
    if (rc != 2) {
        goto err_exit;
    }
     /*对比状态值*/
    if (strcmp(rsp_status,"OK") != 0) {
        goto err_exit;
    } 
    /*比对值*/
    if (strcmp(rsp_value[1],"0") == 0) {
        *status = M6312_GPRS_NET_INACTIVE;
    } else if (strcmp(rsp_value[1],"1") == 0) {
        *status = M6312_GPRS_NET_ACTIVE;
    } else {
        goto err_exit;
    }

    log_debug("m6312获取gprs网络状态:%s成功.\r\n",rsp_value[1]);
    return 0;

err_exit:
    log_error("m6312获取gprs网络状态失败.\r\n");
    return -1;

}

/**
* @brief M6312模块gprs网络激活
* @param status 是否激活 @see m6312_gprs_net_status_t
* @return M6312模块gprs激活是否成功
* @retval 0 激活成功
* @retval -1 激活失败
* @attention 无
* @note 无
*/
int m6312_set_gprs_net(m6312_gprs_net_status_t status)
{
    int rc;
    char *rsp_status;
    char response[M6312_RESPONSE_BUFFER_SIZE];
    char *request = status == M6312_GPRS_NET_ACTIVE ? "AT+CGACT=1,1\r\n" : "AT+CGACT=1,0\r\n";

    /*m6312执行请求*/
    rc = m6312_request(request,strlen(request),response,M6312_RESPONSE_BUFFER_SIZE,M6312_RESPONSE_TIMEOUT);
    if (rc < 0) {
        goto err_exit;
    }
    /*m6312解析回应*/
    rc = m6312_parse_response(response,CRLF,&rsp_status,NULL,NULL,NULL,0);
    if (rc != 0) {
        goto err_exit;
    }
     /*对比状态值*/
    if (strcmp(rsp_status,"OK") != 0) {
        goto err_exit;
    } 
    log_debug("m6312设置gprs网络:%d成功.\r\n",status);
    return 0;

err_exit:
    log_error("m6312设置gprs网络:%d失败.\r\n",status);
    return -1;

}


/**
* @brief M6312模块获取sim卡注册状态
* @param status sim卡注册状态
* @return 获取状态是否成功
* @attention 无
* @note 无
*/
int m6312_get_sim_register_status(m6312_sim_register_status_t *status)
{
    int rc;
    char *rsp_status;
    char *rsp_value[2];
    char response[M6312_RESPONSE_BUFFER_SIZE];
    char *request = "AT+CGREG?\r\n";

    /*m6312执行请求*/
    rc = m6312_request(request,strlen(request),response,M6312_RESPONSE_BUFFER_SIZE,M6312_RESPONSE_TIMEOUT);
    if (rc < 0) {
        goto err_exit;
    }
    /*m6312解析回应*/
    rc = m6312_parse_response(response,CRLF,&rsp_status,"+CGREG: ",",",rsp_value,2);
    if (rc != 2) {
        goto err_exit;
    }
    /*对比状态值*/
    if (strcmp(rsp_status,"OK") != 0) {
        goto err_exit;
    }
    /*m6312对比值*/
    if (strcmp(rsp_value[1],"0") == 0) {
        *status = M6312_SIM_REGISTER_NO;
    } else if (strcmp(rsp_value[1],"1") == 0) {
        *status = M6312_SIM_REGISTER_YES;
    } else if (strcmp(rsp_value[1],"2") == 0) {
        *status = M6312_SIM_REGISTER_NO;
    } else if (strcmp(rsp_value[1],"3") == 0) {
        *status = M6312_SIM_REGISTER_YES;
    } else if (strcmp(rsp_value[1],"4") == 0) {
        *status = M6312_SIM_REGISTER_NO;
    } else if (strcmp(rsp_value[1],"5") == 0) {
        *status = M6312_SIM_REGISTER_YES;
    } else {
        *status = M6312_SIM_REGISTER_UNKNOW;
    }

    log_debug("m6312查询sim卡注册状态:%s成功.\r\n",rsp_value[1]);
    return 0;

err_exit:
    log_error("m6312查询sim卡注册状态失败.\r\n");
    return -1;
}

/**
* @brief M6312模块获取连接模式
* @param mode 连接模式指针@see m6312_connection_mode_t
* @return M6312模块获取连接模式是否成功
* @retval 0 M6312模块获取连接模式成功
* @retval -1 M6312模块获取连接模式失败
* @attention 无
* @note 无
*/
int m6312_get_connection_mode(m6312_connection_mode_t *mode)
{
    int rc;
    char *rsp_status;
    char *rsp_value;
    char response[M6312_RESPONSE_BUFFER_SIZE];
    char *request = "AT+CMMUX?\r\n";

    /*m6312执行请求*/
    rc = m6312_request(request,strlen(request),response,M6312_RESPONSE_BUFFER_SIZE,M6312_RESPONSE_TIMEOUT);
    if (rc < 0) {
        goto err_exit;
    }
    /*m6312解析回应*/
    rc = m6312_parse_response(response,CRLF,&rsp_status,"+CMMUX: ",",",&rsp_value,1);
    if (rc != 1) {
        goto err_exit;
    }
      /*对比状态值*/
    if (strcmp(rsp_status,"OK") != 0) {
        goto err_exit;
    }
    /*对比回应值*/
    if (strcmp(rsp_value,"1") == 0) {
        *mode = M6312_CONNECTION_MODE_MULTI;
    } else {
        *mode = M6312_CONNECTION_MODE_SINGLE;
    }

    log_debug("m6312获取连接模式:%s成功.\r\n",rsp_value);
    return 0;

err_exit:
    log_error("m6312设置连接模式失败.\r\n");
    return -1;
}

/**
* @brief M6312模块配置连接模式
* @param mode 连接模式 @see m6312_connection_mode_t
* @return M6312模块配置连接模式是否成功
* @retval 0 M6312模块配置连接模式成功
* @retval -1 M6312模块配置连接模式失败
* @attention 无
* @note 无
*/
int m6312_set_connection_mode(m6312_connection_mode_t mode)
{
    int rc;
    char *rsp_status;
    char response[M6312_RESPONSE_BUFFER_SIZE];
    char *request = mode == M6312_CONNECTION_MODE_MULTI ? "AT+CMMUX=1\r\n" : "AT+CMMUX=0\r\n";

    /*m6312执行请求*/
    rc = m6312_request(request,strlen(request),response,M6312_RESPONSE_BUFFER_SIZE,M6312_RESPONSE_TIMEOUT);
    if (rc < 0) {
        goto err_exit;
    }
    /*m6312解析回应*/
    rc = m6312_parse_response(response,CRLF,&rsp_status,NULL,NULL,NULL,0);
    if (rc != 0) {
        goto err_exit;
    }
      /*对比状态值*/
    if (strcmp(rsp_status,"OK") != 0) {
        goto err_exit;
    }

    log_debug("m6312设置连接模式:%d成功.\r\n",mode);
    return 0;

err_exit:
    log_error("m6312设置连接模式:%d失败.\r\n",mode);
    return -1;
}

/**
* @brief M6312模块获取接收缓存的模式
* @param status m6312接收缓存状态指针@see m6312_recv_cache_mode_t
* @return M6312模块获取连接模式是否成功
* @retval 0 M6312模块获取连接模式成功
* @retval -1 M6312模块获取连接模式失败
* @attention 无
* @note 无
*/
int m6312_get_recv_cache_mode(m6312_recv_cache_mode_t *mode)
{
    int rc;
    char *rsp_status;
    char *rsp_value[2];
    char response[M6312_RESPONSE_BUFFER_SIZE];
    char *request = "AT+CMNDI?\r\n";

    /*m6312执行请求*/
    rc = m6312_request(request,strlen(request),response,M6312_RESPONSE_BUFFER_SIZE,M6312_RESPONSE_TIMEOUT);
    if (rc < 0) {
        goto err_exit;
    }
    /*m6312解析回应*/
    rc = m6312_parse_response(response,CRLF,&rsp_status,"+CMNDI: ",",",rsp_value,2);
    if (rc != 2) {
        goto err_exit;
    }
      /*对比状态值*/
    if (strcmp(rsp_status,"OK") != 0) {
        goto err_exit;
    }
    /*对比回应值*/
    if (strcmp(rsp_value[0],"1") == 0) {
        *mode = M6312_RECV_CACHE_MODE_CACHED;
    } else {
        *mode = M6312_RECV_CACHE_MODE_NO_CACHED;
    }

    log_debug("m6312获取接收缓存模式:%s成功.\r\n",rsp_value[0]);
    return 0;

err_exit:
    log_error("m6312获取接收缓存模式失败.\r\n");
    return -1;
}


/**
* @brief M6312模块设置接收缓存的模式
* @param mode 接收缓存模式 @see m6312_recv_cache_mode_t
* @return M6312模块设置接收缓存模式是否成功
* @retval 0 M6312模块设置接收缓存模式成功
* @retval -1 M6312模块设置接收缓存模式失败
* @attention 无
* @note 无
*/
int m6312_set_recv_cache_mode(m6312_recv_cache_mode_t mode)
{
    int rc;
    char *rsp_status;
    char response[M6312_RESPONSE_BUFFER_SIZE];
    char *request = mode == M6312_RECV_CACHE_MODE_CACHED ? "AT+CMNDI=1\r\n" : "AT+CMNDI=0\r\n";

    /*m6312执行请求*/
    rc = m6312_request(request,strlen(request),response,M6312_RESPONSE_BUFFER_SIZE,M6312_RESPONSE_TIMEOUT);
    if (rc < 0) {
        goto err_exit;
    }
    /*m6312解析回应*/
    rc = m6312_parse_response(response,CRLF,&rsp_status,NULL,NULL,NULL,0);
    if (rc != 0) {
        goto err_exit;
    }
      /*对比状态值*/
    if (strcmp(rsp_status,"OK") != 0) {
        goto err_exit;
    }

    log_debug("m6312设置接收缓存:%d成功.\r\n",mode);
    return 0;

err_exit:
    log_error("m6312设置接收缓存:%d失败.\r\n",mode);
    return -1;
}

/**
* @brief M6312模块获取传输模式
* @param mode 传输模式指针 @see m6312_transport_mode_t
* @return M6312模块获取传输模式是否成功
* @retval 0 M6312模块获取传输模式成功
* @retval -1 M6312模块获取传输模式失败
* @attention 无
* @note 无
*/
int m6312_get_transport_mode(m6312_transport_mode_t *mode)
{
    int rc;
    char *rsp_status;
    char *rsp_value;
    char response[M6312_RESPONSE_BUFFER_SIZE];
    char *request = "AT+CMMODE?\r\n";

    /*m6312执行请求*/
    rc = m6312_request(request,strlen(request),response,M6312_RESPONSE_BUFFER_SIZE,M6312_RESPONSE_TIMEOUT);
    if (rc < 0) {
        goto err_exit;
    }
    /*m6312解析回应*/
    rc = m6312_parse_response(response,CRLF,&rsp_status,"+CMMODE: ",",",&rsp_value,1);
    if (rc != 1) {
        goto err_exit;
    }
      /*对比状态值*/
    if (strcmp(rsp_status,"OK") != 0) {
        goto err_exit;
    }
    /*对比回应值*/
    if (strcmp(rsp_value,"1") == 0) {
        *mode = M6312_TRANSPORT_MODE_TRANSPARENT;
    } else {
        *mode = M6312_TRANSPORT_MODE_NO_TRANSPARENT;
    }

    log_debug("m6312获取传输模式:%s成功.\r\n",rsp_value);
    return 0;

err_exit:
    log_error("m6312获取传输模式失败.\r\n");
    return -1;
}

/**
* @brief M6312模块配置传送模式
* @param mode 传输模式 @see m6312_transport_mode_t
* @return M6312模块配置透明传送是否成功
* @retval 0 M6312模块配置透明传送成功
* @retval -1 M6312模块配置透明传送失败
* @attention 无
* @note 无
*/
int m6312_set_transport_mode(m6312_transport_mode_t mode)
{
    int rc;
    char *rsp_status;
    char response[M6312_RESPONSE_BUFFER_SIZE];
    char *request = mode == M6312_TRANSPORT_MODE_TRANSPARENT ? "AT+CMMODE=1\r\n" : "AT+CMMODE=0\r\n";

    /*m6312执行请求*/
    rc = m6312_request(request,strlen(request),response,M6312_RESPONSE_BUFFER_SIZE,M6312_RESPONSE_TIMEOUT);
    if (rc < 0) {
        goto err_exit;
    }
    /*m6312解析回应*/
    rc = m6312_parse_response(response,CRLF,&rsp_status,NULL,NULL,NULL,0);
    if (rc != 0) {
        goto err_exit;
    }
      /*对比状态值*/
    if (strcmp(rsp_status,"OK") != 0) {
        goto err_exit;
    }

    log_debug("m6312设置透明传输:%d成功.\r\n",mode);
    return 0;

err_exit:
    log_error("m6312设置透明传输:%d失败.\r\n",mode);
    return -1;
}


/**
* @brief M6312模块建立TCP或者UDP连接
* @param index 建立连接的通道号
* @param host 要建立连接的主机名
* @param type 建立连接的类型
* @return 建立连接是否成功
* @retval 0 建立连接成功
* @retval -1 建立连接失败
* @attention 无
* @note 无
*/
int m6312_connect(uint8_t index,char *host,char *port,m6312_connect_type_t type)
{
    int rc;
    char *rsp_status;
    char *connect_type;
    char request[M6312_REQUEST_BUFFER_SIZE];
    char response[M6312_RESPONSE_BUFFER_SIZE];

    if (type == M6312_CONNECT_TCP) {
        connect_type = "TCP";
    } else {
        connect_type = "UDP";
    }
    /*构建请求*/
    snprintf(request,M6312_REQUEST_BUFFER_SIZE,"AT+IPSTART=%d,%s,%s,%s\r\n",index,connect_type,host,port);
    /*m6312执行请求*/
    rc = m6312_request(request,strlen(request),response,M6312_RESPONSE_BUFFER_SIZE,M6312_RESPONSE_TIMEOUT);
    if (rc < 0) {
        goto err_exit;
    }
    /*m6312解析回应*/
    rc = m6312_parse_response(response,CRLF,&rsp_status,NULL,NULL,NULL,0);
    if (rc != 0) {
        goto err_exit;
    }

    /*对比状态值*/
    if (strcmp(rsp_status,"OK") == 0) {
        log_debug("m6312等待连接状态...\r\n");
        /*m6312执行请求*/
        rc = m6312_request(NULL,0,response,M6312_RESPONSE_BUFFER_SIZE,M6312_RESPONSE_TIMEOUT);
        if (rc < 0) {
            goto err_exit;
        }
        /*m6312解析回应*/
        rc = m6312_parse_response(response,CRLF,&rsp_status,NULL,NULL,NULL,0);
        if (rc != 0) {
            goto err_exit;
        }
        if (type == M6312_CONNECT_TCP) {
            /*对比状态值*/
            if (strcmp(rsp_status,"CONNECT OK") != 0) {
                goto err_exit;
            }
        } else {
            if (strcmp(rsp_status,"BIND OK") != 0) {
                goto err_exit;
            }
        }
    } else if (strcmp(rsp_status,"ALREADY CONNECT") != 0) {
            goto err_exit;
    }

    log_debug("m6312 %s连接成功.\r\n",connect_type);
    return 0;

err_exit:
    log_error("m6312 %s连接失败.\r\n",connect_type);
    return -1;
}

/**
* @brief M6312模块关闭TCP或者UDP连接
* @param index 建立连接的通道号
* @return 关闭连接是否成功
* @retval 0 关闭连接成功
* @retval -1 关闭连接失败
* @attention 无
* @note 无
*/
int m6312_close(uint8_t index)
{
    int rc;
    char *rsp_status;
    char response[M6312_RESPONSE_BUFFER_SIZE];
    char request[M6312_REQUEST_BUFFER_SIZE];

    /*m6312构建请求*/
    snprintf(request,M6312_REQUEST_BUFFER_SIZE,"AT+IPCLOSE=%d\r\n",index);
    /*m6312执行请求*/
    rc = m6312_request(request,strlen(request),response,M6312_RESPONSE_BUFFER_SIZE,M6312_RESPONSE_TIMEOUT);
    if (rc < 0) {
        goto err_exit;
    }
    /*m6312解析回应*/
    rc = m6312_parse_response(response,CRLF,&rsp_status,NULL,NULL,NULL,0);
    if (rc != 0) {
        goto err_exit;
    }
    /*对比状态值*/
    if (strcmp(rsp_status,"OK") != 0) {
        goto err_exit;
    }
    log_debug("m6312 关闭连接:%d成功.\r\n",index);
    return 0;

err_exit:
    log_error("m6312 关闭连接%d失败.\r\n",index);
    return -1;
}

/**
* @brief M6312模块发送数据
* @param socket_id 建立连接的通道号
* @param buffer 发送的数据地址
* @param size 发送的数据量
* @param wait_response 是否需要等待回应 0不等待 >0等待
* @return 发送是否成功
* @retval 0 发送成功
* @retval -1 发送失败
* @attention 无
* @note 无
*/
int m6312_send(uint8_t socket_id,uint8_t *buffer,uint16_t size,uint8_t wait_response)
{
    int rc;
    char *rsp_status;
    char response[M6312_RESPONSE_BUFFER_SIZE];
    char request[M6312_REQUEST_BUFFER_SIZE];

    /*第一步 启动发送*/
    /*m6312构建请求*/
    snprintf(request,M6312_REQUEST_BUFFER_SIZE,"AT+IPSEND=%d,%d\r\n",socket_id,size);
    /*m6312执行请求*/
    rc = m6312_request(request,strlen(request),response,M6312_RESPONSE_BUFFER_SIZE,M6312_RESPONSE_TIMEOUT);
    if (rc < 0) {
        goto err_exit;
    }
    /*m6312解析回应*/
    rc = m6312_parse_response(response,CRLF,&rsp_status,NULL,NULL,NULL,0);
    if (rc != 0) {
        goto err_exit;
    }
    /*对比状态值*/
    if (strcmp(rsp_status,"> ") != 0) {
        goto err_exit;
    }
    /*第二步 发送数据*/
    /*m6312执行请求*/
    rc = m6312_request((char *)buffer,size,response,M6312_RESPONSE_BUFFER_SIZE,M6312_RESPONSE_TIMEOUT);
    if (rc < 0) {
        goto err_exit;
    }
    /*m6312解析回应*/
    rc = m6312_parse_response(response,CRLF,&rsp_status,NULL,NULL,NULL,0);
    if (rc != 0) {
        goto err_exit;
    }
    /*对比状态值*/
    if (strcmp(rsp_status,"SEND FAIL") == 0) {
        goto err_exit;
    }

    log_debug("m6312发送成功.\r\n");
    return 0;

err_exit:
    log_error("m6312发送失败\r\n");
    return -1;
}


/**
* @brief M6312模块接收数据
* @param socket_id 建立连接的通道号
* @param buffer 接收的数据地址
* @param size 接收的数据量
* @return 实际接收数据量
* @retval >=0 成功接收的数据量
* @retval -1 接收失败
* @attention 无
* @note 无
*/
int m6312_recv(uint8_t socket_id,uint8_t *recv_buffer,uint16_t size)
{
    int rc;
    uint16_t recv_size;
    uint8_t temp_id;
    char *rsp_status;
    char *rsp_value[3];
    char response[M6312_RESPONSE_BUFFER_SIZE];
    char request[M6312_REQUEST_BUFFER_SIZE] = { 0 };

    /*第一步 读取缓存*/
    log_debug("m6312等待数据...\r\n");
    /*m6312执行请求*/
    rc = m6312_request(request,0,response,M6312_RESPONSE_BUFFER_SIZE,M6312_RESPONSE_TIMEOUT);
    if (rc < 0) {
        goto err_exit;
    }
    if (rc == 0) {
        /*没有数据超时返回0*/
        return 0;
    }
    /*m6312解析回应*/
    rc = m6312_parse_response(response,CRLF,&rsp_status,"+CMRD: ",",",rsp_value,3);
    if (rc != 3) {
        goto err_exit;
    }
    /*对比状态值*/
    if (strcmp(rsp_status,"OK") != 0) {
        goto err_exit;
    }
    /*查找对应的连接号*/
    temp_id = atoi(rsp_value[0]);
    if (temp_id != socket_id) {
        log_error("m6312接收的socket_id:%d != %d错误.\r\n",temp_id,socket_id);
        goto err_exit;
    }
    recv_size = atoi(rsp_value[2]);
    /*第二部读取数据*/
    /*构建请求*/
    snprintf(request,M6312_REQUEST_BUFFER_SIZE,"AT+CMRD=%d,%d\r\n",socket_id,recv_size);
    /*m6312执行请求*/
    rc = m6312_request(request,strlen(request),(char *)recv_buffer,size,M6312_RESPONSE_TIMEOUT);
    if (rc < recv_size) {
        goto err_exit;
    }
    
    /*m6312解析回应*/
    rc = m6312_parse_response((char *)recv_buffer + recv_size,CRLF,&rsp_status,NULL,NULL,NULL,0);
    if (rc != 0) {
        goto err_exit;
    }
    /*对比状态值*/
    if (strcmp(rsp_status,"OK") != 0) {
        goto err_exit;
    }
    log_debug("m6312接收数据size:%d成功.\r\n",recv_size);
    return recv_size;
err_exit:

    log_error("m6312接收数据失败.\r\n");
    return -1;
}
