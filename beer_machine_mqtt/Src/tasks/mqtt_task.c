/**
******************************************************************************************                                                                                                                                                       
*                                                                            
*  This program is free software; you can redistribute it and/or modify      
*  it under the terms of the GNU General Public License version 3 as         
*  published by the Free Software Foundation.                                
*                                                                            
*  @file       mqtt_task.h
*  @brief      
*  @author     wkxboot
*  @version    v1.0.0
*  @date       2019/7/11
*  @copyright  <h4><left>&copy; copyright(c) 2019 wkxboot 1131204425@qq.com</center></h4>  
*                                                                            
*                                                                            
*****************************************************************************************/
#include "cmsis_os.h"
#include "net_task.h"
#include "printf.h"
#include "tasks_init.h"
#include "MQTTClient.h"
#include "mqtt_task.h"
#include "log.h"

 /** 消息队列handle*/
QueueHandle_t  mqtt_task_msg_hdl;
/**< 任务句柄*/
osThreadId  mqtt_task_hdl;       


#define  DEVICE_LOG_TOPIC         "/device/log"
#define  DEVICE_LOG_CONTENT       "hello world.\r\n"

#define  HOST_SOCKET              0
#define  HOST_NAME                "47.92.229.28"
#define  HOST_PORT                1883

#define  MQTT_SEND_BUFFER_SIZE    100
#define  MQTT_RECV_BUFFER_SIZE    100
#define  MQTT_TIMEOUT             20000/**< 单位豪秒*/
#define  MQTT_INTERVAL            30/**< 单位秒，如果这段时间内有数据传输，则顺延*/

/** mqtt接收和发送缓冲*/
uint8_t send_buffer[MQTT_SEND_BUFFER_SIZE], recv_buffer[MQTT_RECV_BUFFER_SIZE];
/** mqtt上下文*/
static MQTTClient client;
/** mqtt网络接口*/
static Network net_work;


/** 描述*/
osTimerId mqtt_timer_id;

/**
* @brief m6312定时器回调
* @details 
* @param 无
* @return 无
* @attention
* @note
*/
static void mqtt_task_timer_callback(void const *argument);


/**
* @brief m6312定时器
* @details 
* @param 无
* @return 无
* @attention
* @note
*/
void mqtt_task_timer_init(void)
{
    osTimerDef(m6312_timer,mqtt_task_timer_callback);

    mqtt_timer_id = osTimerCreate(osTimer(m6312_timer),osTimerOnce,NULL);
    log_assert_null_ptr(mqtt_timer_id);
}
/**
* @brief m6312定时器开始
* @details 
* @param 无
* @return 无
* @attention
* @note
*/
static void mqtt_task_timer_start(uint32_t timeout)
{
    osTimerStart(mqtt_timer_id,timeout);
}
/**
* @brief m6312定时器停止
* @details 
* @param 无
* @return 无
* @attention
* @note
*/
static void mqtt_task_timer_stop(void)
{
    osTimerStop(mqtt_timer_id);
}
/**
* @brief m6312定时器回调
* @details 
* @param 无
* @return 无
* @attention
* @note
*/
static void mqtt_task_timer_callback(void const *argument)
{
    osSignalSet(net_task_hdl,NET_TASK_M6312_SEND_MESSAGE);
}

static void message_handle(MessageData* data)
{
  log_info("topic:%s message ==>> arrived: %s\r\n",(char *)data->topicName,(char*)data->message->payload);
}

/**
* @brief
* @details
* @param
* @param
* @return
* @attention
* @note
*/
static int mqtt_task_client_init(MQTTClient *client,Network *net_work,uint8_t socket,uint8_t *send_buffer,uint32_t send_size,uint8_t *recv_buffer,uint32_t recv_size,uint32_t interval,uint32_t timeout)
{
    int rc;

    MQTTPacket_connectData connectData = MQTTPacket_connectData_initializer;
    NetworkInit(net_work);
    net_work->my_socket = socket;
    MQTTClientInit(client,net_work,timeout,send_buffer,send_size,recv_buffer,recv_size);

    if ((rc = NetworkConnect(net_work,HOST_NAME,HOST_PORT)) != 0) {
        log_error("mqtt network connect fail:code %d.\r\n", rc);
        NetworkDisconnect(net_work);
        return -1;
    }

    connectData.MQTTVersion = 3;
    connectData.username.cstring = "a24a642b4d1d473b";
    connectData.password.cstring = "pwd";
    connectData.clientID.cstring = "D3A8002184DATET811260001";
    connectData.keepAliveInterval = interval;

    if ((rc = MQTTConnect(client,&connectData)) != 0) {
        log_error("mqtt send connect data fail:code %d.\r\n", rc);
        NetworkDisconnect(net_work);
        return -1;
    } 

    log_info("mqtt connected\r\n");
    return 0;
}

/**
* @brief
* @details
* @param
* @param
* @return
* @retval
* @retval
* @attention
* @note
*/

static void mqtt_message_thread(MQTTClient *client)
{
#if defined(MQTT_TASK)
    int rc;
    rc = MQTTStartTask(client);
    log_assert_bool_false(rc == pdPASS);

    log_debug("mqtt message task run...\r\n");
#endif
}
/**
* @brief
* @details
* @param
* @param
* @return
* @retval
* @retval
* @attention
* @note
*/

static int mqtt_task_subscribe(MQTTClient *client,char *topic,enum QoS qos,messageHandler message_hdl) 
{
    int rc;

    rc = MQTTSubscribe(client,topic, qos,message_hdl);
    if (rc != 0) {
        log_error("mqtt subscribe to topic:%s fail:%d\r\n",topic,rc);
        return -1;
    } 

    log_debug("mqtt subscribe to topic:%s\r\n",topic);
    return 0;
}
/**
* @brief
* @details
* @param
* @param
* @return
* @retval
* @retval
* @attention
* @note
*/
static int mqtt_task_publish(MQTTClient *client,char *topic,enum QoS qos,char *payload,uint32_t size)
{
    int rc;
    MQTTMessage message;
    
    message.qos = qos;
    message.retained = 0;
    message.payload = payload;
    message.payloadlen = size;
    
    rc = MQTTPublish(client,topic,&message);
    if (rc != 0) {
        log_error("mqtt publish topic:%s fail:code:%d\r\n",topic,rc);
        return -1;
    }

    log_debug("mqtt publish topic:%s success code:%d\r\n",topic,rc);
    return 0;
}

/**
* @brief
* @details
* @param
* @return
* @attention
* @note
*/
void mqtt_task(void const * argument)
{
    int rc;
    mqtt_task_msg_t mqtt_msg; 
    mqtt_task_msg_t mqtt_msg_recv; 
    while(1)
    {
    osDelay(1000);
    if (xQueueReceive(mqtt_task_msg_hdl, &mqtt_msg_recv,0xFFFFFFFF) == pdTRUE) {
        /*处理消息*/
        if (mqtt_msg_recv.head.id == MQTT_TASK_NET_INIT) {
            rc = mqtt_task_client_init(&client,&net_work,HOST_SOCKET,send_buffer,MQTT_SEND_BUFFER_SIZE,recv_buffer,MQTT_SEND_BUFFER_SIZE,MQTT_INTERVAL,MQTT_TIMEOUT);
            if (rc != 0) {
                mqtt_msg.head.id = MQTT_TASK_NET_INIT;
                xQueueSend(mqtt_task_msg_hdl,&mqtt_msg,5);
            } else {
                mqtt_message_thread(&client);
                mqtt_msg.head.id = MQTT_TASK_SUBSCRIBE;
                xQueueSend(mqtt_task_msg_hdl,&mqtt_msg,5);
            }
        }
        /*处理消息*/
        if (mqtt_msg_recv.head.id == MQTT_TASK_SUBSCRIBE) {
            rc = mqtt_task_subscribe(&client,DEVICE_LOG_TOPIC,0,message_handle);
            if (rc != 0) {
                mqtt_msg.head.id = MQTT_TASK_SUBSCRIBE;
                xQueueSend(mqtt_task_msg_hdl,&mqtt_msg,5);    
            } else {
                mqtt_msg.head.id = MQTT_TASK_REPORT_LOG;
                xQueueSend(mqtt_task_msg_hdl,&mqtt_msg,5);
            }
        }

        /*处理消息*/
        if (mqtt_msg_recv.head.id == MQTT_TASK_REPORT_LOG) {
            rc = mqtt_task_publish(&client,DEVICE_LOG_TOPIC,0,DEVICE_LOG_CONTENT,strlen(DEVICE_LOG_CONTENT));
            if (rc != 0) {
                mqtt_msg.head.id = MQTT_TASK_REPORT_LOG;
                xQueueSend(mqtt_task_msg_hdl,&mqtt_msg,5);   
            } else {
                mqtt_msg.head.id = MQTT_TASK_REPORT_LOG;
                xQueueSend(mqtt_task_msg_hdl,&mqtt_msg,5);  
            }
        }

    }
    }

}