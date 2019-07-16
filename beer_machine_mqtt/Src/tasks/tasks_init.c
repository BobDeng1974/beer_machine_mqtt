#include "cmsis_os.h"
#include "tasks_init.h"
#include "mqtt_task.h"
#include "net_task.h"
#include "log.h"


/**
* @brief 任务初始化
* @details
* @param 无
* @return 任务初始化是否成功
* @retval 0 成功
* @retval -1 失败
* @attention
* @note
*/
int tasks_init(void)
{
    mqtt_task_msg_hdl = xQueueCreate(6,sizeof(mqtt_task_msg_t));
    log_assert_null_ptr(mqtt_task_msg_hdl);

    osThreadDef(mqtt_task, mqtt_task, osPriorityNormal,0,800);
    mqtt_task_hdl = osThreadCreate(osThread(mqtt_task), NULL);
    log_assert_null_ptr(mqtt_task_hdl);

    osThreadDef(net_task, net_task, osPriorityNormal,0,400);
    net_task_hdl = osThreadCreate(osThread(net_task), NULL);
    log_assert_null_ptr(net_task_hdl);


    return 0;
}