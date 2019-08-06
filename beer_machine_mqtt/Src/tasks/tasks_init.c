#include "cmsis_os.h"
#include "tasks_init.h"
#include "mqtt_task.h"
#include "temperature_task.h"
#include "compressor_task.h"
#include "report_task.h"
#include "net_task.h"
#include "adc_task.h"
#include "firmware_version.h"
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
    log_info("\r\nfirmware version: %s\r\n\r\n",FIRMWARE_VERSION_STR);
    
    mqtt_task_msg_q_id = xQueueCreate(6,sizeof(mqtt_task_msg_t));
    log_assert_null_ptr(mqtt_task_msg_q_id);

    temperature_task_msg_q_id = xQueueCreate(6,sizeof(temperature_task_message_t));
    log_assert_null_ptr(temperature_task_msg_q_id);

    report_task_msg_q_id = xQueueCreate(6,sizeof(report_task_message_t));
    log_assert_null_ptr(report_task_msg_q_id);

    compressor_task_msg_q_id = xQueueCreate(6,sizeof(compressor_task_message_t));
    log_assert_null_ptr(compressor_task_msg_q_id);

    osThreadDef(report_task, report_task, osPriorityNormal,0,1000);
    report_task_hdl = osThreadCreate(osThread(report_task), NULL);
    log_assert_null_ptr(report_task_hdl);
    
    osThreadDef(mqtt_task, mqtt_task, osPriorityNormal,0,800);
    mqtt_task_hdl = osThreadCreate(osThread(mqtt_task), NULL);
    log_assert_null_ptr(mqtt_task_hdl);
    
    osThreadDef(net_task, net_task, osPriorityNormal,0,600);
    net_task_hdl = osThreadCreate(osThread(net_task), NULL);
    log_assert_null_ptr(net_task_hdl);

    osThreadDef(adc_task, adc_task, osPriorityNormal,0,600);
    adc_task_hdl = osThreadCreate(osThread(adc_task), NULL);
    log_assert_null_ptr(adc_task_hdl);

    osThreadDef(temperature_task, temperature_task, osPriorityNormal,0,600);
    temperature_task_hdl = osThreadCreate(osThread(temperature_task), NULL);
    log_assert_null_ptr(temperature_task_hdl);

    osThreadDef(compressor_task, compressor_task, osPriorityNormal,0,600);
    compressor_task_hdl = osThreadCreate(osThread(compressor_task), NULL);
    log_assert_null_ptr(compressor_task_hdl);

    return 0;
}