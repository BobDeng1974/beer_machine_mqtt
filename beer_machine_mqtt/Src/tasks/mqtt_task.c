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
#include "mqtt_client.h"
#include "mqtt_task.h"
#include "log.h"

 /** 消息队列handle*/
QueueHandle_t  mqtt_task_msg_hdl;
/**< 任务句柄*/
osThreadId  mqtt_task_hdl;       


#define  DEVICE_LOG_TOPIC         "/device/log"
#define  DEVICE_LOG_CONTENT       "hello world.\r\n"

#define  DEVICE_LOG_CONFIG        "/device/config"

#define  HOST_SOCKET              0
#define  HOST_NAME                "47.92.229.28"
#define  HOST_PORT                1883

#define  MQTT_SEND_BUFFER_SIZE    100
#define  MQTT_RECV_BUFFER_SIZE    100
#define  MQTT_TIMEOUT             20000/**< 单位豪秒*/
#define  MQTT_INTERVAL            0/**< 单位秒，如果这段时间内有数据传输，则顺延*/

/** mqtt接收和发送缓冲*/
uint8_t send_buffer[MQTT_SEND_BUFFER_SIZE], recv_buffer[MQTT_RECV_BUFFER_SIZE];

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

#ifdef HAVE_CONFIG_H
    #include "config.h"
#endif

#include "mqtt_client.h"
#include "mqttnet.h"

//#define  WOLFMQTT_DISCONNECT_CB 
/* Locals */
static int mStopRead = 0;

/* Configuration */

/* Maximum size for network read/write callbacks. There is also a v5 define that
   describes the max MQTT control packet size, DEFAULT_MAX_PKT_SZ. */
#define MAX_BUFFER_SIZE 1024

uint8_t recv[ MAX_BUFFER_SIZE];
uint8_t send[ MAX_BUFFER_SIZE];
#define TEST_MESSAGE    "test"


MqttClient client;
MqttNet net;
MqttConnect connect;
MqttSubscribe subscribe;
MqttPublish publish;
MqttTopic sub_topic;
MqttTopic *topic;
MqttDisconnect disconnect;

#ifdef WOLFMQTT_DISCONNECT_CB
/* callback indicates a network error occurred */
static int mqtt_disconnect_cb(MqttClient* client, int error_code, void* ctx)
{
    (void)client;
    (void)ctx;
    log_info("Network Error Callback: %s (error %d)",
        MqttClient_ReturnCodeToString(error_code), error_code);
    return 0;
}
#endif

static int mqtt_message_cb(MqttClient *client, MqttMessage *msg,
    byte msg_new, byte msg_done)
{
    byte buf[PRINT_BUFFER_SIZE+1];
    word32 len;
    MQTTCtx* mqttCtx = (MQTTCtx*)client->ctx;

    (void)mqttCtx;

    if (msg_new) {
        /* Determine min size to dump */
        len = msg->topic_name_len;
        if (len > PRINT_BUFFER_SIZE) {
            len = PRINT_BUFFER_SIZE;
        }
        XMEMCPY(buf, msg->topic_name, len);
        buf[len] = '\0'; /* Make sure its null terminated */

        /* Print incoming message */
        log_info("MQTT Message: Topic %s, Qos %d, Len %u",
            buf, msg->qos, msg->total_len);

    }

    /* Print message payload */
    len = msg->buffer_len;
    if (len > PRINT_BUFFER_SIZE) {
        len = PRINT_BUFFER_SIZE;
    }
    XMEMCPY(buf, msg->buffer, len);
    buf[len] = '\0'; /* Make sure its null terminated */
    log_info("Payload (%d - %d): %s",
        msg->buffer_pos, msg->buffer_pos + len, buf);

    if (msg_done) {
        log_info("MQTT Message: Done");
    }

    /* Return negative to terminate publish processing */
    return MQTT_CODE_SUCCESS;
}

int mqttclient_test(void)
{
    int rc = MQTT_CODE_SUCCESS, i;

    log_info("MQTT Client: QoS %d, Use TLS %d", 0,
            0);

    /* Initialize Network */
    rc = MqttClientNet_Init(&net,NULL);
    log_info("MQTT Net Init: %s (%d)",
        MqttClient_ReturnCodeToString(rc), rc);
    if (rc != MQTT_CODE_SUCCESS) {
        goto exit;
    }

    /* Initialize MqttClient structure */
    rc = MqttClient_Init(&client, &net,
        mqtt_message_cb,
        send, MAX_BUFFER_SIZE,
        recv, MAX_BUFFER_SIZE,
        10000);

    log_info("MQTT Init: %s (%d)",
        MqttClient_ReturnCodeToString(rc), rc);
    if (rc != MQTT_CODE_SUCCESS) {
        goto exit;
    }

#ifdef WOLFMQTT_DISCONNECT_CB
    /* setup disconnect callback */
    rc = MqttClient_SetDisconnectCallback(&client,
        mqtt_disconnect_cb, NULL);
    if (rc != MQTT_CODE_SUCCESS) {
        goto exit;
    }
#endif
#ifdef WOLFMQTT_PROPERTY_CB
    rc = MqttClient_SetPropertyCallback(client,
            mqtt_property_cb, NULL);
    if (rc != MQTT_CODE_SUCCESS) {
        goto exit;
    }
#endif

    /* Connect to broker */
    rc = MqttClient_NetConnect(&client,HOST_NAME,
           HOST_PORT,
        DEFAULT_CON_TIMEOUT_MS, 0, 0);

    log_info("MQTT Socket Connect: %s (%d)",
        MqttClient_ReturnCodeToString(rc), rc);
    if (rc != MQTT_CODE_SUCCESS) {
        goto exit;
    }

    /* Build connect packet */
    XMEMSET(&connect, 0, sizeof(MqttConnect));
    connect.keep_alive_sec = 20;
    connect.clean_session = 1;
    connect.client_id = "wkxboot";

    /* Last will and testament sent by broker to subscribers
        of topic when broker connection is lost */
    /*
    XMEMSET(lwt_msg, 0, sizeof(mqttCtx->lwt_msg));
   connect.lwt_msg = lwt_msg;
   connect.enable_lwt =enable_lwt;
    if (mqttCtx->enable_lwt) {
        //Send client id in LWT payload
       lwt_msg.qos =qos;
       lwt_msg.retain = 0;
       lwt_msg.topic_name = WOLFMQTT_TOPIC_NAME"lwttopic";
       lwt_msg.buffer = (byte*)mqttCtx->client_id;
       lwt_msg.total_len =
          (word16)XSTRLEN(mqttCtx->client_id);
    }
*/
    /* Optional authentication */
    connect.username = "wulong";
   connect.password = "123456";
#ifdef WOLFMQTT_V5
   client.packet_sz_max =max_packet_size;
   client.enable_eauth =enable_eauth;

    if (mqttCtx->client.enable_eauth == 1)
    {
        /* Enhanced authentication */
        /* Add property: Authentication Method */
        MqttProp* prop = MqttClient_PropsAdd(connect.props);
        prop->type = MQTT_PROP_AUTH_METHOD;
        prop->data_str.str = (char*)DEFAULT_AUTH_METHOD;
        prop->data_str.len = (word16)XSTRLEN(prop->data_str.str);
    }
    {
        /* Request Response Information */
        MqttProp* prop = MqttClient_PropsAdd(connect.props);
        prop->type = MQTT_PROP_REQ_RESP_INFO;
        prop->data_byte = 1;
    }
    {
        /* Request Problem Information */
        MqttProp* prop = MqttClient_PropsAdd(connect.props);
        prop->type = MQTT_PROP_REQ_PROB_INFO;
        prop->data_byte = 1;
    }
    {
        /* Maximum Packet Size */
        MqttProp* prop = MqttClient_PropsAdd(connect.props);
        prop->type = MQTT_PROP_MAX_PACKET_SZ;
        prop->data_int = (word32)mqttCtx->max_packet_size;
    }
    {
        /* Topic Alias Maximum */
        MqttProp* prop = MqttClient_PropsAdd(connect.props);
        prop->type = MQTT_PROP_TOPIC_ALIAS_MAX;
        prop->data_short =topic_alias_max;
    }
#endif

    /* Send Connect and wait for Connect Ack */
    rc = MqttClient_Connect(&client, &connect);

    log_info("MQTT Connect: %s (%d)",
        MqttClient_ReturnCodeToString(rc), rc);
    if (rc != MQTT_CODE_SUCCESS) {
        goto disconn;
    }

#ifdef WOLFMQTT_V5
    if (mqttCtx->connect.props != NULL) {
        /* Release the allocated properties */
        MqttClient_PropsFree(mqttCtx->connect.props);
    }
#endif

    /* Validate Connect Ack info */
    log_info("MQTT Connect Ack: Return Code %u, Session Present %d",
        connect.ack.return_code,
        (connect.ack.flags &
            MQTT_CONNECT_ACK_FLAG_SESSION_PRESENT) ?
            1 : 0
    );

#ifdef WOLFMQTT_PROPERTY_CB
        /* Print the acquired client ID */
        log_info("MQTT Connect Ack: Assigned Client ID: %s",
               client_id);
#endif

    /* Build list of topics */
    XMEMSET(&subscribe, 0, sizeof(MqttSubscribe));

    sub_topic.qos = 0;
    sub_topic.topic_filter = DEVICE_LOG_CONFIG;
    subscribe.topic_count = 1;
    subscribe.topics = &sub_topic;



    /* Subscribe Topic */
   subscribe.stat = MQTT_MSG_BEGIN;
   subscribe.packet_id = 0;


    rc = MqttClient_Subscribe(&client, &subscribe);

#ifdef WOLFMQTT_V5
    if (mqttCtx->subscribe.props != NULL) {
        /* Release the allocated properties */
        MqttClient_PropsFree(mqttCtx->subscribe.props);
    }
#endif

    log_info("MQTT Subscribe: %s (%d)",
        MqttClient_ReturnCodeToString(rc), rc);
    if (rc != MQTT_CODE_SUCCESS) {
        goto disconn;
    }

    /* show subscribe results */
    for (i = 0; i <subscribe.topic_count; i++) {
       topic = &subscribe.topics[i];
        log_info("  Topic %s, Qos %u, Return Code %u",
           topic->topic_filter,
           topic->qos,topic->return_code);
    }

    /* Publish Topic */
    XMEMSET(&publish, 0, sizeof(MqttPublish));
   publish.retain = 0;
   publish.qos =0;
   publish.duplicate = 0;
   publish.topic_name = DEVICE_LOG_TOPIC;
   publish.packet_id = 1;
   publish.buffer = (byte*)TEST_MESSAGE;
   publish.total_len = (word16)XSTRLEN(TEST_MESSAGE);
#ifdef WOLFMQTT_V5
    {
        /* Payload Format Indicator */
        MqttProp* prop = MqttClient_PropsAdd(publish.props);
        prop->type = MQTT_PROP_PLAYLOAD_FORMAT_IND;
        prop->data_int = 1;
    }
    {
        /* Content Type */
        MqttProp* prop = MqttClient_PropsAdd(publish.props);
        prop->type = MQTT_PROP_CONTENT_TYPE;
        prop->data_str.str = (char*)"wolf_type";
        prop->data_str.len = (word16)XSTRLEN(prop->data_str.str);
    }
    if ((mqttCtx->topic_alias_max > 0) &&
        (mqttCtx->topic_alias > 0) &&
        (mqttCtx->topic_alias <topic_alias_max)) {
        /* Topic Alias */
        MqttProp* prop = MqttClient_PropsAdd(publish.props);
        prop->type = MQTT_PROP_TOPIC_ALIAS;
        prop->data_short =topic_alias;
    }
#endif

    rc = MqttClient_Publish(&client, &publish);

    log_info("MQTT Publish: Topic %s, %s (%d)",
       publish.topic_name,
        MqttClient_ReturnCodeToString(rc), rc);
    if (rc != MQTT_CODE_SUCCESS) {
        goto disconn;
    }
#ifdef WOLFMQTT_V5
    if (mqttCtx->connect.props != NULL) {
        /* Release the allocated properties */
        MqttClient_PropsFree(mqttCtx->publish.props);
    }
#endif

    /* Read Loop */
    log_info("MQTT Waiting for message...");

    do {
        /* Try and read packet */
        rc = MqttClient_WaitMessage(&client,
                                           50000);

        /* check for test mode */
        if (mStopRead) {
            rc = MQTT_CODE_SUCCESS;
            log_info("MQTT Exiting...");
            break;
        }

        /* check return code */
    #ifdef WOLFMQTT_ENABLE_STDIN_CAP
        else if (rc == MQTT_CODE_STDIN_WAKE) {
            XMEMSET(mqttCtx->rx_buf, 0, MAX_BUFFER_SIZE);
            if (XFGETS((char*)mqttCtx->rx_buf, MAX_BUFFER_SIZE - 1,
                    stdin) != NULL)
            {
                rc = (int)XSTRLEN((char*)mqttCtx->rx_buf);

                /* Publish Topic */
               stat = WMQ_PUB;
                XMEMSET(publish, 0, sizeof(MqttPublish));
               publish.retain = 0;
               publish.qos =qos;
               publish.duplicate = 0;
               publish.topic_name =topic_name;
               publish.packet_id = mqtt_get_packetid();
               publish.buffer =rx_buf;
               publish.total_len = (word16)rc;
                rc = MqttClient_Publish(client,
                       publish);
                log_info("MQTT Publish: Topic %s, %s (%d)",
                   publish.topic_name,
                    MqttClient_ReturnCodeToString(rc), rc);
            }
        }
    #endif
        else if (rc == MQTT_CODE_ERROR_TIMEOUT) {
            /* Keep Alive */
            log_info("Keep-alive timeout, sending ping");

            rc = MqttClient_Ping(&client);
            if (rc != MQTT_CODE_SUCCESS) {
                log_info("MQTT Ping Keep Alive Error: %s (%d)",
                    MqttClient_ReturnCodeToString(rc), rc);
                break;
            }
        }
        else if (rc != MQTT_CODE_SUCCESS) {
            /* There was an error */
            log_info("MQTT Message Wait: %s (%d)",
                MqttClient_ReturnCodeToString(rc), rc);
            break;
        }
    } while (1);

    /* Check for error */
    if (rc != MQTT_CODE_SUCCESS) {
        goto disconn;
    }

   
disconn:
    /* Disconnect */
    rc = MqttClient_Disconnect_ex(&client,
           &disconnect);

    log_info("MQTT Disconnect: %s (%d)",
        MqttClient_ReturnCodeToString(rc), rc);
    if (rc != MQTT_CODE_SUCCESS) {
        goto disconn;
    }

    rc = MqttClient_NetDisconnect(&client);

    log_info("MQTT Socket Disconnect: %s (%d)",
        MqttClient_ReturnCodeToString(rc), rc);

exit:

    /* Cleanup network */
    MqttClientNet_DeInit(&net);

    MqttClient_DeInit(&client);

    return rc;
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
          mqttclient_test();
          /*
            rc = mqtt_task_client_init(&client,&net_work,HOST_SOCKET,send_buffer,MQTT_SEND_BUFFER_SIZE,recv_buffer,MQTT_SEND_BUFFER_SIZE,MQTT_INTERVAL,MQTT_TIMEOUT);
            if (rc != 0) {
                mqtt_msg.head.id = MQTT_TASK_NET_INIT;
                xQueueSend(mqtt_task_msg_hdl,&mqtt_msg,5);
            } else {
                mqtt_message_thread(&client);
                mqtt_msg.head.id = MQTT_TASK_SUBSCRIBE;
                xQueueSend(mqtt_task_msg_hdl,&mqtt_msg,5);
            }
*/        
}
        /*处理消息*/
        /*
        if (mqtt_msg_recv.head.id == MQTT_TASK_SUBSCRIBE) {
            rc = mqtt_task_subscribe(&client,DEVICE_LOG_CONFIG,0,message_handle);
            if (rc != 0) {
                mqtt_msg.head.id = MQTT_TASK_NET_INIT;
                xQueueSend(mqtt_task_msg_hdl,&mqtt_msg,5);    
            } else {
                mqtt_msg.head.id = MQTT_TASK_REPORT_LOG;
                xQueueSend(mqtt_task_msg_hdl,&mqtt_msg,5);
            }
        }
*/
        /*处理消息*/
        /*
        if (mqtt_msg_recv.head.id == MQTT_TASK_REPORT_LOG) {
            rc = mqtt_task_publish(&client,DEVICE_LOG_TOPIC,0,DEVICE_LOG_CONTENT,strlen(DEVICE_LOG_CONTENT));
            if (rc != 0) {
                mqtt_msg.head.id = MQTT_TASK_NET_INIT;
                xQueueSend(mqtt_task_msg_hdl,&mqtt_msg,5);   
            } else {
                mqtt_msg.head.id = MQTT_TASK_REPORT_LOG;
                xQueueSend(mqtt_task_msg_hdl,&mqtt_msg,5);  
            }
        }
*/

    }
    }

}