#include "cmsis_os.h"
#include "comm_utils.h"
#include "tasks_init.h"
#include "alarm_task.h"
#include "compressor_task.h"
#include "display_task.h"
#include "pressure_task.h"
#include "net_task.h"
#include "temperature_task.h"
#include "report_task.h"
#include "log.h"


EventGroupHandle_t tasks_sync_evt_group_hdl;

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
    osThreadDef(net_task, net_task, osPriorityBelowNormal, 0, 400);
    net_task_hdl = osThreadCreate(osThread(net_task), NULL);

    return 0;
}