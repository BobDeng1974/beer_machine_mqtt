#ifndef  __ALARM_TASK_H__
#define  __ALARM_TASK_H__
#include "stdint.h"
#include "stdbool.h"

#ifdef  __cplusplus
#define ALARM_TASK_BEGIN  extern "C" {
#define ALARM_TASK_END    }
#else
#define ALARM_TASK_BEGIN  
#define ALARM_TASK_END   
#endif


ALARM_TASK_BEGIN


extern osThreadId   alarm_task_handle;
extern osMessageQId alarm_task_msg_q_id;

void alarm_task(void const *argument);

#define  ALARM_TASK_PRESSURE_ERR_VALUE         (0x0e * 10 + 0)
#define  ALARM_TASK_TEMPERATURE_ERR_VALUE      (0x0e * 10 + 1)
#define  ALARM_TASK_CAPACITY_ERR_VALUE         (0x0e * 10 + 2)

#define  ALARM_TASK_COMPRESSOR_LOCK_VALUE      (0x0e * 10 + 3)
#define  ALARM_TASK_COMPRESSOR_UNLOCK_VALUE    (0x0e * 10 + 4)


typedef enum
{
  ALARM_TASK_MSG_TIMER_TIMEOUT,
  ALARM_TASK_MSG_TEMPERATURE,
  ALARM_TASK_MSG_PRESSURE,
  ALARM_TASK_MSG_CAPACITY,
  ALARM_TASK_MSG_TEMPERATURE_CONFIG,
  ALARM_TASK_MSG_PRESSURE_CONFIG,
  ALARM_TASK_MSG_CAPACITY_CONFIG,
  ALARM_TASK_MSG_COMPRESSOR_LOCK_CONFIG,
}alarm_task_msg_type_t;


typedef struct
{
  uint32_t type:8;
  uint32_t value:8;
  uint32_t reserved:16;
}alarm_task_msg_t;




ALARM_TASK_END

#endif