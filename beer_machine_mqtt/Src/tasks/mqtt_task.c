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
#include "mqtt_config.h"
#include "mqtt_client.h"
#include "mqtt_task.h"
#include "mqtt_context.h"
#include "compressor_task.h"
#include "log.h"

 /** 消息队列handle*/
osMessageQId  mqtt_task_msg_q_id;
/**< 任务句柄*/
osThreadId  mqtt_task_hdl;       

/* Default Configurations */
#define  DEVICE_COMPRESSOR_CTRL_TOPIC      "/cloud/cfreezer/DC2902493310000B06180338/m"
#define  DEVICE_COMPRESSOR_CTRL_RSP_TOPIC  "/cloud/cfreezer/SN999999999/m"

#define  DEFAULT_MQTT_HOST             "mqtt.mymlsoft.com"
#define  DEFAULT_MQTT_PORT             1883
#define  DEFAULT_CMD_TIMEOUT_MS        5000
#define  DEFAULT_CONN_TIMEOUT_MS       5000
#define  DEFAULT_MQTT_QOS              MQTT_QOS_0
#define  DEFAULT_KEEP_ALIVE_SEC        60
#define  DEFAULT_CLIENT_ID             "wkxboot_client"
#define  DEFAULT_USER_NAME             "a24a642b4d1d473b"
#define  DEFAULT_USER_PASSWD           "eyJ0eXAiOiJKV1QiLCJhbGciOiJIUzI1NiJ9.eyJpc3MiOiJ0YW8xLmppYW5nIiwidXNlciI6InRvdSJ9._yv05gR0dAFJeDoEPI8Wo5qB01Gf-cM8_M1SbdoV9jQ"

#define  MQTT_SEND_BUFFER_SIZE         300
#define  MQTT_RECV_BUFFER_SIZE         200
#define  PRINT_BUFFER_SIZE             200

 /** mqtt上下文*/
static MQTTCtx mqtt_context;

/** mqtt接收和发送缓冲*/
static uint8_t send_buffer[MQTT_SEND_BUFFER_SIZE],recv_buffer[MQTT_RECV_BUFFER_SIZE];

/** 保活定时器*/
static osTimerId mqtt_keep_alive_timer_id;

/**
* @brief m6312定时器回调
* @details 
* @param 无
* @return 无
* @attention
* @note
*/
static void mqtt_task_keep_alive_timer_callback(void const *argument);


/**
* @brief m6312定时器
* @details 
* @param 无
* @return 无
* @attention
* @note
*/
void mqtt_task_keep_alive_timer_init(void)
{
    osTimerDef(m6312_keep_alive_timer,mqtt_task_keep_alive_timer_callback);

    mqtt_keep_alive_timer_id = osTimerCreate(osTimer(m6312_keep_alive_timer),osTimerOnce,NULL);
    log_assert_null_ptr(mqtt_keep_alive_timer_id);
}
/**
* @brief m6312定时器开始
* @details 
* @param 无
* @return 无
* @attention
* @note
*/
static void mqtt_task_keep_alive_timer_start(uint32_t timeout)
{
    osTimerStart(mqtt_keep_alive_timer_id,timeout);
}
/**
* @brief m6312定时器停止
* @details 
* @param 无
* @return 无
* @attention
* @note
*/
static void mqtt_task_keep_alive_timer_stop(void)
{
    osTimerStop(mqtt_keep_alive_timer_id);
}
/**
* @brief m6312定时器回调
* @details 
* @param 无
* @return 无
* @attention
* @note
*/
static void mqtt_task_keep_alive_timer_callback(void const *argument)
{
    mqtt_task_msg_t msg;
    msg.head.id = MQTT_TASK_MSG_KEEP_ALIVE;
    log_assert_bool_false(xQueueSend(mqtt_task_msg_q_id,&msg,5) == pdPASS);
}

#include "cJSON.h"

typedef struct
{
    int msg_id;
    int pwr_state;
    int result;
    uint32_t time;
}mqtt_task_compressor_ctrl_t;

/*解析协议数据*/
static int mqtt_task_parse_msg(char *rsp_json_str,mqtt_task_compressor_ctrl_t *compressor_ctrl)
{
    int rc = -1;
    cJSON *rsp_json;
    cJSON *temp;
  
    log_debug("parse mqtt rsp.\r\n");
    rsp_json = cJSON_Parse(rsp_json_str);
    if (rsp_json == NULL) {
        log_error("mqtt rsp is not json.\r\n");
        return -1;
    }
    /*controlId*/
    temp = cJSON_GetObjectItem(rsp_json,"controlId");
    if (!cJSON_IsNumber(temp)) {
        log_error("controlId is not num.\r\n");
        goto err_exit;  
    }
    compressor_ctrl->msg_id = temp->valueint;
    /*powerState*/
    temp = cJSON_GetObjectItem(rsp_json,"powerState");
    if (!cJSON_IsNumber(temp)) {
        log_error("powerState is not num.\r\n");
        goto err_exit;  
    }
    compressor_ctrl->pwr_state = temp->valueint;

    /*timestamp*/
    temp = cJSON_GetObjectItem(rsp_json,"timestamp");
    if (!cJSON_IsNumber(temp)) {
        log_error("timestamp is not num.\r\n");
        goto err_exit;  
    }
    compressor_ctrl->time = temp->valueint + 100;/*模拟100ms延时*/

    log_info("mqtt ctrl.id:%d pwr_state:%d time:%d.\r\n",compressor_ctrl->msg_id,compressor_ctrl->pwr_state,compressor_ctrl->time);
    rc = 0;

err_exit:

    cJSON_Delete(rsp_json);
    return rc;
}

/*通知压缩机*/
static int mqtt_task_notify_compressor(int pwr_state)
{
    compressor_task_message_t compressor_msg;

    if (pwr_state == 0) {
        compressor_msg.head.id = COMPRESSOR_TASK_MSG_PWR_ON_DISABLE;
    } else {
        compressor_msg.head.id = COMPRESSOR_TASK_MSG_PWR_ON_ENABLE;
    }
    log_assert_bool_false(xQueueSend(compressor_task_msg_q_id,&compressor_msg,5) == pdPASS);

    return 0;
}

/*通知回应结果*/
static void mqtt_task_notify_response_ctrl_result(mqtt_task_compressor_ctrl_t *compressor_ctrl)
{
    mqtt_task_msg_t mqtt_msg;

    /*构造回应结果*/
    char *json_str;
    cJSON *result_json;
    result_json = cJSON_CreateObject();
    cJSON_AddNumberToObject(result_json,"msgId",compressor_ctrl->msg_id);
    cJSON_AddNumberToObject(result_json,"result",compressor_ctrl->result);
    cJSON_AddNumberToObject(result_json,"responseTime",compressor_ctrl->time);
    json_str = cJSON_PrintUnformatted(result_json);
    cJSON_Delete(result_json);
    strcpy(mqtt_msg.content.value,json_str);
    mqtt_msg.content.size = strlen(mqtt_msg.content.value);
    cJSON_free(json_str);

    /*告知mqtt任务回应topic*/
    mqtt_msg.head.id = MQTT_TASK_MSG_PUBLIC;
    log_assert_bool_false(xQueueSend(mqtt_task_msg_q_id,&mqtt_msg,5) == pdPASS);
}

#include "mqtt_config.h"
#include "mqtt_client.h"
#include "mqtt_net.h"

/* Configuration */

/* Maximum size for network read/write callbacks. There is also a v5 define that
   describes the max MQTT control packet size, DEFAULT_MAX_PKT_SZ. */
#define MAX_BUFFER_SIZE 1024

/* callback indicates a network error occurred */
static int mqtt_task_disconnect_cb(MqttClient* client, int error_code, void* ctx)
{
    (void)client;
    (void)ctx;
    log_info("Network Error Callback: %s (error %d)\r\n",
             MqttClient_ReturnCodeToString(error_code), error_code);
    return 0;
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

static int mqtt_task_message_cb(MqttClient *client, MqttMessage *msg,
    byte msg_new, byte msg_done)
{
    byte buf[PRINT_BUFFER_SIZE+1];
    word32 len;
    MQTTCtx* mqtt_ctx = (MQTTCtx*)client->ctx;

    (void)mqtt_ctx;

    if (msg_new) {
        /* Determine min size to dump */
        len = msg->topic_name_len;
        if (len > PRINT_BUFFER_SIZE) {
            len = PRINT_BUFFER_SIZE;
        }
        XMEMCPY(buf, msg->topic_name, len);
        buf[len] = '\0'; /* Make sure its null terminated */

        /* Print incoming message */
        log_info("MQTT Message: Topic %s, Qos %d, Len %u\r\n",
                 buf, msg->qos, msg->total_len);

    }

    /* Print message payload */
    len = msg->buffer_len;
    if (len > PRINT_BUFFER_SIZE) {
        len = PRINT_BUFFER_SIZE;
    }
    XMEMCPY(buf, msg->buffer, len);
    buf[len] = '\0'; /* Make sure its null terminated */
    log_info("Payload (%d - %d): %s\r\n",
             msg->buffer_pos, msg->buffer_pos + len, buf);

    if (msg_done) {
        log_info("MQTT Message: Done\r\n");
        /*处理消息*/
        mqtt_task_compressor_ctrl_t compressor_ctrl;
        if (mqtt_task_parse_msg((char *)buf,&compressor_ctrl) == 0) {
            if (mqtt_task_notify_compressor(compressor_ctrl.pwr_state) == 0) {
                compressor_ctrl.result = 1;
                mqtt_task_notify_response_ctrl_result(&compressor_ctrl);
            }
        }
    }

    /* Return negative to terminate publish processing */
    return MQTT_CODE_SUCCESS;
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
static int mqtt_task_contex_init(MQTTCtx *mqtt_ctx)
{
    XMEMSET(mqtt_ctx, 0, sizeof(MQTTCtx));
    mqtt_ctx->host = DEFAULT_MQTT_HOST;
    mqtt_ctx->port = DEFAULT_MQTT_PORT;
    mqtt_ctx->qos = DEFAULT_MQTT_QOS;
    mqtt_ctx->clean_session = 1;
    mqtt_ctx->keep_alive_sec = DEFAULT_KEEP_ALIVE_SEC;
    mqtt_ctx->client_id = DEFAULT_CLIENT_ID;

    mqtt_ctx->cmd_timeout_ms = DEFAULT_CMD_TIMEOUT_MS;
    mqtt_ctx->conn_timeout_ms = DEFAULT_CONN_TIMEOUT_MS;
    mqtt_ctx->tx_buffer = send_buffer;
    mqtt_ctx->tx_buffer_size = MQTT_SEND_BUFFER_SIZE;
    mqtt_ctx->rx_buffer = recv_buffer;
    mqtt_ctx->rx_buffer_size = MQTT_RECV_BUFFER_SIZE;
    mqtt_ctx->use_tls = 0;
    mqtt_ctx->username = DEFAULT_USER_NAME;
    mqtt_ctx->password = DEFAULT_USER_PASSWD;

    log_info("MQTT Client: QoS %d, Use TLS %d\r\n",mqtt_ctx->qos,mqtt_ctx->use_tls);
    return 0;
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
static int mqtt_task_connect(MQTTCtx *mqtt_ctx)
{
    int rc;

    /* Initialize Network */
    rc = MqttClientNet_Init(&mqtt_ctx->net,mqtt_ctx);
    log_info("MQTT Net Init: %s (%d)\r\n",MqttClient_ReturnCodeToString(rc), rc);
    if (rc != MQTT_CODE_SUCCESS) {
        return -1;
    }

    /* Initialize MqttClient structure */
    rc = MqttClient_Init(&mqtt_ctx->client, &mqtt_ctx->net,
        mqtt_task_message_cb,
        mqtt_ctx->tx_buffer, mqtt_ctx->tx_buffer_size,
        mqtt_ctx->rx_buffer,  mqtt_ctx->rx_buffer_size,
        mqtt_ctx->cmd_timeout_ms);

    log_info("MQTT Init: %s (%d)\r\n",MqttClient_ReturnCodeToString(rc), rc);
    if (rc != MQTT_CODE_SUCCESS) {
        return -1;
    }

    /* setup disconnect callback */
    rc = MqttClient_SetDisconnectCallback(&mqtt_ctx->client,mqtt_task_disconnect_cb,mqtt_ctx);
    if (rc != MQTT_CODE_SUCCESS) {
        return -1;
    }

    /* Connect to broker */
    rc = MqttClient_NetConnect(&mqtt_ctx->client,mqtt_ctx->host,mqtt_ctx->port,mqtt_ctx->conn_timeout_ms,mqtt_ctx->use_tls,0);

    log_info("MQTT Socket Connect: %s (%d)\r\n",MqttClient_ReturnCodeToString(rc), rc);
    if (rc != MQTT_CODE_SUCCESS) {
        return -1;
    }

    /* Build connect packet */
    XMEMSET(&mqtt_ctx->connect, 0, sizeof(MqttConnect));
    mqtt_ctx->connect.keep_alive_sec = mqtt_ctx->keep_alive_sec;
    mqtt_ctx->connect.clean_session = mqtt_ctx->clean_session;
    mqtt_ctx->connect.client_id = mqtt_ctx->client_id;

    /* Optional authentication */
    mqtt_ctx->connect.username = mqtt_ctx->username;
    mqtt_ctx->connect.password = mqtt_ctx->password;

    /* Send Connect and wait for Connect Ack */
    rc = MqttClient_Connect(&mqtt_ctx->client, &mqtt_ctx->connect);

    log_info("MQTT Connect: %s (%d)\r\n",MqttClient_ReturnCodeToString(rc), rc);
    if (rc != MQTT_CODE_SUCCESS) {
        return -1;
    }

    /* Validate Connect Ack info */
    log_info("MQTT Connect Ack: Return Code %u, Session Present %d\r\n",
            mqtt_ctx->connect.ack.return_code,
            (mqtt_ctx->connect.ack.flags &
            MQTT_CONNECT_ACK_FLAG_SESSION_PRESENT) ?
            1 : 0);

    return 0;
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
static int mqtt_task_disconnect(MQTTCtx *mqtt_ctx)
{
    int rc;
    /* Disconnect */
    rc = MqttClient_Disconnect_ex(&mqtt_ctx->client, &mqtt_ctx->disconnect);

    log_info("MQTT Disconnect: %s (%d)\r\n",MqttClient_ReturnCodeToString(rc), rc);

    rc = MqttClient_NetDisconnect(&mqtt_ctx->client);
    log_info("MQTT Socket Disconnect: %s (%d)\r\n",MqttClient_ReturnCodeToString(rc), rc);

    /* Cleanup network */
    MqttClientNet_DeInit(&mqtt_ctx->net);
    MqttClient_DeInit(&mqtt_ctx->client);

    return rc;    
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
static int mqtt_task_subscribe_topics(MQTTCtx *mqtt_ctx)
{
    int rc,i;
    /* Build list of topics */
    XMEMSET(&mqtt_ctx->subscribe, 0, sizeof(MqttSubscribe));

    mqtt_ctx->topics[0].qos = MQTT_QOS_0;
    mqtt_ctx->topics[0].topic_filter = DEVICE_COMPRESSOR_CTRL_TOPIC;
    mqtt_ctx->subscribe.topic_count = 1;
    mqtt_ctx->subscribe.topics = &mqtt_ctx->topics[0];

    /* Subscribe Topic */
    mqtt_ctx->subscribe.stat = MQTT_MSG_BEGIN;
    mqtt_ctx->subscribe.packet_id = 0;
    rc = MqttClient_Subscribe(&mqtt_ctx->client, &mqtt_ctx->subscribe);

    log_info("MQTT Subscribe: %s (%d)\r\n",MqttClient_ReturnCodeToString(rc), rc);
    if (rc != MQTT_CODE_SUCCESS) {
        return -1;
    }

    /* show subscribe results */
    for (i = 0; i < mqtt_ctx->subscribe.topic_count; i++) {
        mqtt_ctx->topic = &mqtt_ctx->topics[i];
        log_info("Subscribe Topic %s, Qos %u, Return Code %u\r\n",
                    mqtt_ctx->topic->topic_filter,
                    mqtt_ctx->topic->qos, mqtt_ctx->topic->return_code);
    }

    return 0;
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
static int mqtt_task_publish_topic(MQTTCtx *mqtt_ctx,char *topic,uint8_t *payload,uint16_t size)
{
    int rc;
    /* Publish Topic */
    XMEMSET(&mqtt_ctx->publish, 0, sizeof(MqttPublish));
    mqtt_ctx->publish.retain = 0;
    mqtt_ctx->publish.qos = mqtt_ctx->qos;
    mqtt_ctx->publish.duplicate = 0;
    mqtt_ctx->publish.topic_name = topic;
    mqtt_ctx->publish.packet_id = 0;
    mqtt_ctx->publish.buffer = (byte*)payload;
    mqtt_ctx->publish.total_len = size;
    rc = MqttClient_Publish(&mqtt_ctx->client, &mqtt_ctx->publish);

    log_info("MQTT Publish: Topic %s, %s (%d)\r\n",&mqtt_ctx->publish.topic_name,MqttClient_ReturnCodeToString(rc), rc);
    if (rc != MQTT_CODE_SUCCESS) {
        return -1;
    }

    return 0;
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
static int mqtt_task_keep_alive(MQTTCtx *mqtt_ctx)
{
    int rc;
    /* Keep Alive */
    log_info("Keep-alive timeout, sending ping\r\n");

    rc = MqttClient_Ping(&mqtt_ctx->client);
    if (rc != MQTT_CODE_SUCCESS) {
        log_info("MQTT Ping Keep Alive Error: %s (%d)\r\n",MqttClient_ReturnCodeToString(rc), rc);
        return -1;
    }

    log_info("MQTT Ping Keep Alive ok: %s (%d)\r\n",MqttClient_ReturnCodeToString(rc), rc);
    return 0;
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
static int mqtt_task_wait_message(MQTTCtx *mqtt_ctx)
{
    int rc;
    /* Read Loop */
    log_info("MQTT Waiting for message...\r\n");

    /* Try and read packet */
    rc = MqttClient_WaitMessage(&mqtt_ctx->client,mqtt_ctx->cmd_timeout_ms);

    if (rc != MQTT_CODE_SUCCESS && rc != MQTT_CODE_ERROR_TIMEOUT) {
        /* There was an error */
        log_info("MQTT Message Wait: %s (%d)\r\n", MqttClient_ReturnCodeToString(rc), rc);
        return -1;
    }

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

    mqtt_task_keep_alive_timer_init();
    while(1)
    {
    osDelay(500);
    if (xQueueReceive(mqtt_task_msg_q_id, &mqtt_msg_recv,0xFFFFFFFF) == pdTRUE) {
        /*处理消息*/
        if (mqtt_msg_recv.head.id == MQTT_TASK_MSG_NET_READY) {
            mqtt_task_contex_init(&mqtt_context);
            /*发送连接信号*/
            mqtt_msg.head.id = MQTT_TASK_MSG_NET_CONNECT;
            log_assert_bool_false(xQueueSend(mqtt_task_msg_q_id,&mqtt_msg,5) == pdPASS);
        }

        /*处理建立连接*/
        if (mqtt_msg_recv.head.id == MQTT_TASK_MSG_NET_CONNECT) {
            if (mqtt_context.is_connected == 0) {
                rc = mqtt_task_connect(&mqtt_context);
                if (rc != 0) {
                    mqtt_msg.head.id = MQTT_TASK_MSG_NET_DISCONNECT;
                    log_assert_bool_false(xQueueSend(mqtt_task_msg_q_id,&mqtt_msg,5) == pdPASS);
                } else {
                    /*只要建立连接，准备心跳*/
                    if (mqtt_context.keep_alive_sec > 0) {
                        mqtt_task_keep_alive_timer_start(mqtt_context.keep_alive_sec * 1000 / 2);
                    }
                    mqtt_context.is_connected = 1;
                    mqtt_msg.head.id = MQTT_TASK_MSG_SUBSCRIBE;
                    log_assert_bool_false(xQueueSend(mqtt_task_msg_q_id,&mqtt_msg,5) == pdPASS);
                }
            }
        }
        /*处理断开连接*/
        if (mqtt_msg_recv.head.id == MQTT_TASK_MSG_NET_DISCONNECT) {
            if (mqtt_context.is_connected == 1) {
                /*只要端开连接，取消心跳*/
                if (mqtt_context.keep_alive_sec > 0) {
                    mqtt_task_keep_alive_timer_stop();
                }
            }
            mqtt_task_disconnect(&mqtt_context);
            mqtt_context.is_connected = 0;
            mqtt_msg.head.id = MQTT_TASK_MSG_NET_CONNECT;
            log_assert_bool_false(xQueueSend(mqtt_task_msg_q_id,&mqtt_msg,5) == pdPASS);
            
        }

        /*处理订阅*/
        if (mqtt_msg_recv.head.id == MQTT_TASK_MSG_SUBSCRIBE) {
            if (mqtt_context.is_connected == 0) {
                continue;
            }
            rc = mqtt_task_subscribe_topics(&mqtt_context);
            if (rc != 0) {
                mqtt_msg.head.id = MQTT_TASK_MSG_NET_DISCONNECT;
                log_assert_bool_false(xQueueSend(mqtt_task_msg_q_id,&mqtt_msg,5) == pdPASS);   
            } else {
                mqtt_msg.head.id = MQTT_TASK_MSG_WAIT_MESSAGE;
                log_assert_bool_false(xQueueSend(mqtt_task_msg_q_id,&mqtt_msg,5) == pdPASS);
            }
        }

        /*处理发布*/
        if (mqtt_msg_recv.head.id == MQTT_TASK_MSG_PUBLIC) {

            if (mqtt_context.is_connected == 0) {
                continue;
            }
            rc = mqtt_task_publish_topic(&mqtt_context,DEVICE_COMPRESSOR_CTRL_RSP_TOPIC,(uint8_t *)mqtt_msg_recv.content.value,mqtt_msg_recv.content.size);
            if (rc != 0) {
                mqtt_msg.head.id = MQTT_TASK_MSG_NET_DISCONNECT;
                log_assert_bool_false(xQueueSend(mqtt_task_msg_q_id,&mqtt_msg,5) == pdPASS);
            } else {
                mqtt_msg.head.id = MQTT_TASK_MSG_WAIT_MESSAGE;
                log_assert_bool_false(xQueueSend(mqtt_task_msg_q_id,&mqtt_msg,5) == pdPASS);
            }
        }

        /*处理等待消息*/
        if (mqtt_msg_recv.head.id == MQTT_TASK_MSG_WAIT_MESSAGE) {
            
            if (mqtt_context.is_connected == 0) {
                continue;
            }

            rc = mqtt_task_wait_message(&mqtt_context);
            if (rc != 0) {
                mqtt_msg.head.id = MQTT_TASK_MSG_NET_DISCONNECT;
                log_assert_bool_false(xQueueSend(mqtt_task_msg_q_id,&mqtt_msg,5) == pdPASS);
            } else {
                mqtt_msg.head.id = MQTT_TASK_MSG_WAIT_MESSAGE;
                log_assert_bool_false(xQueueSend(mqtt_task_msg_q_id,&mqtt_msg,5) == pdPASS);
            }
        }

        /*处理心跳*/
        if (mqtt_msg_recv.head.id == MQTT_TASK_MSG_KEEP_ALIVE) {
            if (mqtt_context.is_connected == 0) {
                continue;
            }
            rc = mqtt_task_keep_alive(&mqtt_context);
            if (rc != 0) {
                mqtt_msg.head.id = MQTT_TASK_MSG_NET_DISCONNECT;
                log_assert_bool_false(xQueueSend(mqtt_task_msg_q_id,&mqtt_msg,5) == pdPASS);
            } else {
                /*再次开启心跳*/
                mqtt_task_keep_alive_timer_start(mqtt_context.keep_alive_sec * (1000 / 2));
            }
        }


    }
    }

}