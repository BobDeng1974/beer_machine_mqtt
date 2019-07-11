#ifndef  __MQTT_TASK_H__
#define  __MQTT_TASK_H__
 
#ifdef  __cplusplus
#define  MQTT_TASK_H_BEGIN extern "C" {
#define  MQTT_TASK_H_END   }
#else
#define  MQTT_TASK_BEGIN
#define  MQTT_TASK_END
#endif
 
MQTT_TASK_BEGIN

 /** 消息队列handle*/
extern QueueHandle_t  mqtt_task_msg_hdl;
/**< 任务handle*/
extern osThreadId  mqtt_task_hdl;       
/**
* @brief
* @details
* @param
* @return
* @attention
* @note
*/
void mqtt_task(void const * argument);


#define  MQTT_TASK_CONTENT_SIZE_MAX  128
typedef struct
{
    struct {
        uint32_t id;
        uint32_t type;
    }head;
    struct {
        char value[MQTT_TASK_CONTENT_SIZE_MAX];
        uint32_t size;
    }content;
}mqtt_task_msg_t;

#define  MQTT_TASK_NET_INIT    0x100
#define  MQTT_TASK_SUBSCRIBE   0x101
#define  MQTT_TASK_REPORT_LOG  0x102



MQTT_TASK_END
#endif


