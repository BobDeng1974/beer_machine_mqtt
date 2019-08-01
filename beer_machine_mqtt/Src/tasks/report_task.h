#ifndef  __REPORT_TASK_H__
#define  __REPORT_TASK_H__
#include "net_task.h"


extern osThreadId  report_task_hdl;
extern osMessageQId report_task_msg_q_id;
void report_task(void const * argument);


#define  REPORT_TASK_PUT_MSG_TIMEOUT                      5
#define  REPORT_TASK_SYNC_UTC_INTERVAL                   (24 * 60 * 60 * 1000)

#define  REPORT_TASK_MSG_SIM_ID                          0x1000
#define  REPORT_TASK_MSG_BASE_INFO                       0x1001
#define  REPORT_TASK_MSG_SYNC_UTC                        0x1002
#define  REPORT_TASK_MSG_ACTIVE_DEVICE                   0x1003
#define  REPORT_TASK_MSG_GET_UPGRADE_INFO                0x1004
#define  REPORT_TASK_MSG_DOWNLOAD_UPGRADE_FILE           0x1005
#define  REPORT_TASK_MSG_TEMPERATURE_UPDATE              0x1006
#define  REPORT_TASK_MSG_TEMPERATURE_SENSOR_FAULT_SPAWM  0x1007
#define  REPORT_TASK_MSG_TEMPERATURE_SENSOR_FAULT_CLEAR  0x1008
#define  REPORT_TASK_MSG_REPORT_LOG                      0x1009
#define  REPORT_TASK_MSG_REPORT_FAULT                    0x100a
#define  REPORT_TASK_MSG_LOOP_CONFIG                     0x100b
#define  REPORT_TASK_MSG_COMPRESSOR_RUN_TIME             0x100c

#define  REPORT_TASK_0_RETRY_TIMEOUT                    (1 * 60 * 1000)
#define  REPORT_TASK_1_RETRY_TIMEOUT                    (3 * 60 * 1000)
#define  REPORT_TASK_2_RETRY_TIMEOUT                    (5 * 60 * 1000)
#define  REPORT_TASK_3_RETRY_TIMEOUT                    (10 * 60 * 1000)
#define  REPORT_TASK_DEFAULT_RETRY_TIMEOUT              (30 * 60 * 1000)



#define  REPORT_TASK_FAULT_QUEUE_SIZE                    16 /*故障队列大小*/ 

typedef struct
{
    struct {
        uint32_t id;
        uint32_t type;
    }head;
    union {
        base_information_t base_info;
        char sim_id[M6312_SIM_ID_STR_LEN];
        float temperature_float[4];
        uint32_t run_time;/*运行时间，单位ms*/
    }content;
}report_task_message_t;



















#endif