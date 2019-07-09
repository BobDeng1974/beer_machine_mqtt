#ifndef  __REPORT_TASK_H__
#define  __REPORT_TASK_H__



extern osThreadId  report_task_handle;
extern osMessageQId report_task_msg_q_id;
extern osMessageQId report_task_location_msg_q_id;
extern osMessageQId report_task_net_hal_info_msg_q_id;

void report_task(void const * argument);


#define  REPORT_TASK_PUT_MSG_TIMEOUT                5

#define  REPORT_TASK_RETRY_DELAY                    (1 * 60 * 1000)/*重试间隔时间ms*/
#define  REPORT_TASK_RETRY1_DELAY                   (1 * 60 * 1000)/*第一次重试间隔时间ms*/
#define  REPORT_TASK_RETRY2_DELAY                   (5 * 60 * 1000)/*第二次重试间隔时间ms*/
#define  REPORT_TASK_RETRY3_DELAY                   (10 * 60 * 1000)/*第三次重试间隔时间ms*/

#define  REPORT_TASK_SYNC_UTC_DELAY                 (2 * 60 * 60 * 1000U)/*UTC同步间隔时间ms*/

#define  REPORT_TASK_FAULT_QUEUE_SIZE               14 /*故障队列大小*/ 


typedef enum
{
  REPORT_TASK_MSG_NET_HAL_INFO,
  REPORT_TASK_MSG_LOCATION,
  REPORT_TASK_MSG_SYNC_UTC,
  REPORT_TASK_MSG_ACTIVE,
  REPORT_TASK_MSG_REPORT_LOG,
  REPORT_TASK_MSG_REPORT_FAULT,
  REPORT_TASK_MSG_CAPACITY_VALUE,
  REPORT_TASK_MSG_PRESSURE_VALUE,
  REPORT_TASK_MSG_TEMPERATURE_VALUE,
  REPORT_TASK_MSG_TEMPERATURE_ERR,
  REPORT_TASK_MSG_PRESSURE_ERR,
  REPORT_TASK_MSG_CAPACITY_ERR,
  REPORT_TASK_MSG_TEMPERATURE_ERR_CLEAR,
  REPORT_TASK_MSG_PRESSURE_ERR_CLEAR,
  REPORT_TASK_MSG_CAPACITY_ERR_CLEAR,
  REPORT_TASK_MSG_GET_UPGRADE,
  REPORT_TASK_MSG_DOWNLOAD_UPGRADE
}report_task_msg_type_t;

typedef struct
{
  uint32_t type:8;
  uint32_t value:16;
  uint32_t reserved:8;
}report_task_msg_t;



















#endif