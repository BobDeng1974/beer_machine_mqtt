#ifndef  __COMPRESSOR_TASK_H__
#define  __COMPRESSOR_TASK_H__
#include "stdint.h"


#ifdef  __cplusplus
#define COMPRESSOR_TASK_BEGIN  extern "C" {
#define COMPRESSOR_TASK_END    }
#else
#define COMPRESSOR_TASK_BEGIN  
#define COMPRESSOR_TASK_END   
#endif


COMPRESSOR_TASK_BEGIN

extern osThreadId   compressor_task_hdl;
extern osMessageQId compressor_task_msg_q_id;

void compressor_task(void const *argument);


#define  COMPRESSOR_TASK_WORK_TIMEOUT                 (120*60*1000) /*连续工作时间单位:ms*/
#define  COMPRESSOR_TASK_REST_TIMEOUT                 (5*60*1000)   /*连续工作时间后的休息时间单位:ms*/
#define  COMPRESSOR_TASK_WAIT_TIMEOUT                 (5*60*1000)   /*2次开机的等待时间 单位:ms*/

#define  COMPRESSOR_TASK_PUT_MSG_TIMEOUT               5            /*发送消息超时时间 单位:ms*/

#define  COMPRESSOR_TASK_PWR_ON_WAIT_TIMEOUT          (2*60*1000)   /*压缩机上电后等待就绪的时间 单位:ms*/
#define  COMPRESSOR_TASK_RUN_TIME_UPDATE_TIMEOUT      (1*60*1000)   /*压缩机运行时间统计间隔*/


#define  COMPRESSOR_TASK_SUCCESS                       0
#define  COMPRESSOR_TASK_FAIL                          1

#define  COMPRESSOR_TASK_TEMPERATURE_START_ENV_NAME    "t_stop"
#define  COMPRESSOR_TASK_TEMPERATURE_STOP_ENV_NAME     "t_stop"

enum
{
  COMPRESSOR_TASK_MSG_TYPE_TEMPERATURE_UPDATE,
  COMPRESSOR_TASK_MSG_TYPE_TEMPERATURE_ERR,
  COMPRESSOR_TASK_MSG_TYPE_UPDATE_STATUS,
  COMPRESSOR_TASK_MSG_TYPE_WORK_TIMEOUT,
  COMPRESSOR_TASK_MSG_TYPE_WAIT_TIMEOUT,
  COMPRESSOR_TASK_MSG_TYPE_REST_TIMEOUT,
  COMPRESSOR_TASK_MSG_TYPE_TIMER_TIMEOUT,
  COMPRESSOR_TASK_MSG_TYPE_TEMPERATURE_SETTING,
  COMPRESSOR_TASK_MSG_TYPE_RSP_TEMPERATURE_SETTING,
  COMPRESSOR_TASK_MSG_TYPE_QUERY_TEMPERATURE_SETTING,
  COMPRESSOR_TASK_MSG_TYPE_RSP_QUERY_TEMPERATURE_SETTING,
  COMPRESSOR_TASK_MSG_RUN_TIME_UPDATE,
  COMPRESSOR_TASK_MSG_PWR_ON_DISABLE,
  COMPRESSOR_TASK_MSG_PWR_ON_ENABLE
};

typedef struct
{
    struct { 
        uint8_t id;
        uint8_t type;
    }head;
    struct {
        float temperature_setting_min;/*设置的温度最小值*/
        float temperature_setting_max;/*设置的温度最大值*/
        float temperature_float;/*浮点温度*/
    }content;
}compressor_task_message_t;/*压缩机任务消息体*/


COMPRESSOR_TASK_END

#endif





