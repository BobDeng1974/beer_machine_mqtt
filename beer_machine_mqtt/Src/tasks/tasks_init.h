#ifndef  __TASKS_INIT_H__
#define  __TASKS_INIT_H__


extern EventGroupHandle_t tasks_sync_evt_group_hdl;

/* 函数名：tasks_init
*  功能：  所有任务参数初始化 
*  参数：  无
*  返回：  0：成功 其他：失败
*/
int tasks_init(void);



#define  TASKS_SYNC_EVENT_REPORT_TASK_RDY               (1<<0)
#define  TASKS_SYNC_EVENT_NET_TASK_RDY                  (1<<1)
#define  TASKS_SYNC_EVENT_ALARM_TASK_RDY                (1<<2)
#define  TASKS_SYNC_EVENT_TEMPERATURE_TASK_RDY          (1<<3)
#define  TASKS_SYNC_EVENT_ADC_TASK_RDY                  (1<<4)
#define  TASKS_SYNC_EVENT_PRESSURE_TASK_RDY             (1<<5)
#define  TASKS_SYNC_EVENT_DISPLAY_TASK_RDY              (1<<6)
#define  TASKS_SYNC_EVENT_COMPRESSOR_TASK_RDY           (1<<7)
#define  TASKS_SYNC_EVENT_CAPACITY_TASK_RDY             (1<<8)
#define  TASKS_SYNC_EVENT_ALL_TASKS_RDY                 ((1<<2)-1)


#endif