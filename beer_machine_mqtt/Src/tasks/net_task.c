#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"
#include "net_task.h"
#include "m6312.h"
#include "printf.h"
#include "tasks_init.h"
#include "MQTTClient.h"
#include "log.h"


osThreadId  net_task_hdl;       /**< 网络任务句柄*/
osMessageQId net_task_msg_q_id; /**< 网络任务消息id*/

 /** 描述*/
osTimerId m6312_timer_id;

/**
* @brief m6312定时器回调
* @details 
* @param 无
* @return 无
* @attention
* @note
*/
static void net_task_m6312_timer_callback(void const *argument);


/**
* @brief m6312定时器
* @details 
* @param 无
* @return 无
* @attention
* @note
*/
void net_task_m6312_timer_init(void)
{
    osTimerDef(m6312_timer,net_task_m6312_timer_callback);

    m6312_timer_id = osTimerCreate(osTimer(m6312_timer),osTimerOnce,NULL);
    log_assert_null_ptr(m6312_timer_id);
}
/**
* @brief m6312定时器开始
* @details 
* @param 无
* @return 无
* @attention
* @note
*/
static void net_task_m6312_timer_start(uint32_t timeout)
{
    osTimerStart(m6312_timer_id,timeout);
}
/**
* @brief m6312定时器停止
* @details 
* @param 无
* @return 无
* @attention
* @note
*/
static void net_task_m6312_timer_stop(void)
{
    osTimerStop(m6312_timer_id);
}
/**
* @brief m6312定时器回调
* @details 
* @param 无
* @return 无
* @attention
* @note
*/
static void net_task_m6312_timer_callback(void const *argument)
{
    osSignalSet(net_task_hdl,NET_TASK_M6312_SEND_MESSAGE);
}

static void messageArrived(MessageData* data)
{
    log_debug("Message arrived: %s\n", (char*)data->message->payload);
}


static void mqtt_client_thread(void* pvParameters)
{
    MQTTClient client;
    Network network;
    unsigned char sendbuf[80], readbuf[80] = {0};
    int rc = 0, count = 0;
    MQTTPacket_connectData connectData = MQTTPacket_connectData_initializer;

    log_debug("mqtt client thread starts\r\n");

    NetworkInit(&network);
    MQTTClientInit(&client, &network, 30000, sendbuf, sizeof(sendbuf), readbuf, sizeof(readbuf));

    char* address = "mqtt.mymlsoft.com";

    if ((rc = NetworkConnect(&network, address, 1883)) != 0) {
        log_error("Return code from network connect is %d\r\n", rc);
        return;
    }

#if defined(MQTT_TASK)

    if ((rc = MQTTStartTask(&client)) != pdPASS) {
        log_error("Return code from start tasks is %d\n", rc);
    } else {
        log_debug("Use MQTTStartTask\n");
    }

#endif

    connectData.MQTTVersion = 3;
    connectData.username.cstring = "a24a642b4d1d473b";
    connectData.password.cstring = "pwd";
    connectData.clientID.cstring = "D3A8002184DATET811260001";

    if ((rc = MQTTConnect(&client, &connectData)) != 0) {
        log_error("Return code from MQTT connect is %d\r\n", rc);
        return;
    } else {
        log_debug("MQTT Connected\r\n");
    }

    if ((rc = MQTTSubscribe(&client, "/sample/sub", QOS2, messageArrived)) != 0) {
        log_error("Return code from MQTT subscribe is %d\r\n", rc);
        return;
    } else {
        log_debug("MQTT subscribe to topic \"/sample/sub\"\r\n");
    }

    while (++count) {
        MQTTMessage message;
        char payload[30];

        message.qos = QOS2;
        message.retained = 0;
        message.payload = payload;
        sprintf(payload, "message number %d\r\n", count);
        message.payloadlen = strlen(payload);

        if ((rc = MQTTPublish(&client, "/sample/pub", &message)) != 0) {
            log_error("Return code from MQTT publish is %d\r\n", rc);
        } else {
            log_debug("MQTT publish topic \"/sample/pub\", message number is %d\r\n", count);
        }

        vTaskDelay(1000 / portTICK_RATE_MS);  //send every 1 seconds
    }

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
void net_task(void const * argument)
{
    int rc;
    osEvent os_event;

    m6312_sim_card_status_t sim_card_status;
    m6312_gprs_attach_status_t gprs_attach_status;
    m6312_gprs_net_status_t gprs_net_status;
    m6312_sim_register_status_t register_status;
    m6312_connection_mode_t connection_mode;
    m6312_recv_cache_mode_t recv_cache_mode;
    m6312_transport_mode_t transport_mode;
    m6312_sim_operator_t sim_operator;
    char sim_id[20];

    m6312_uart_init();
    net_task_m6312_timer_init();
    osSignalSet(net_task_hdl,NET_TASK_M6312_REBOOT);
    log_debug("net task run...\r\n");
    while (1)
    {
        osDelay(100);
        os_event = osSignalWait(NET_TASK_ALL_SIGNALS,osWaitForever);
        if (os_event.status == osEventSignal) {
            /*m6312重启*/
            if (os_event.value.signals & NET_TASK_M6312_REBOOT) {
                //m6312_pwr_off();
                m6312_pwr_on();
                osSignalSet(net_task_hdl,NET_TASK_M6312_TURN_OFF_ECHO);
            }
        }
        /*关闭回显*/
        if (os_event.value.signals & NET_TASK_M6312_TURN_OFF_ECHO) {
            rc = m6312_echo_turn_on_off(0);
            if (rc == 0) {
                osSignalSet(net_task_hdl,NET_TASK_M6312_DETECT_SIM_CARD);  
            } else {
                osSignalSet(net_task_hdl,NET_TASK_M6312_TURN_OFF_ECHO);  
            }
        }

        /*m6312查询sim是否插入*/
        if (os_event.value.signals & NET_TASK_M6312_DETECT_SIM_CARD) {           
            rc = m6312_get_sim_card_status(&sim_card_status);
            /*sim卡存在，检测是否激活*/
            if (rc == 0 && sim_card_status == M6312_SIM_CARD_EXIST) {
                osSignalSet(net_task_hdl,NET_TASK_M6312_GET_SIM_OPERATOR);
            } else {
                /*再次查询*/
                osSignalSet(net_task_hdl,NET_TASK_M6312_DETECT_SIM_CARD);
            }
        }
        /*m6312获取运营商*/
        if (os_event.value.signals & NET_TASK_M6312_GET_SIM_OPERATOR) {
            rc = m6312_get_operator(&sim_operator); 
            if (rc == 0) {
                osSignalSet(net_task_hdl,NET_TASK_M6312_SET_GPRS_APN); 
            } else {
                /*再次查询*/
                osSignalSet(net_task_hdl,NET_TASK_M6312_GET_SIM_OPERATOR);
            }
        }
        /*m6312设置APN*/
        if (os_event.value.signals & NET_TASK_M6312_SET_GPRS_APN) {
            rc = m6312_set_gprs_apn("cmnet"); 
            if (rc == 0) {
                osSignalSet(net_task_hdl,NET_TASK_M6312_DETECT_GPRS_ATTACH); 
            } else {
                /*再次查询*/
                osSignalSet(net_task_hdl,NET_TASK_M6312_SET_GPRS_APN);
            }
        }

        /*m6312查询是否附着网络*/
        if (os_event.value.signals & NET_TASK_M6312_DETECT_GPRS_ATTACH) {
            rc = m6312_get_gprs_attach_status(&gprs_attach_status); 
            if (rc != 0) {
                goto detect_attach_exit;
            }
            /*m6312没有附着网络，就附着网络*/
            if (gprs_attach_status != M6312_GPRS_ATTACH) {
                rc = m6312_set_gprs_attach(M6312_GPRS_ATTACH);
                if (rc != 0) {
                    goto detect_attach_exit;
                }
            }

detect_attach_exit:  
            if (rc == 0) {
                osSignalSet(net_task_hdl,NET_TASK_M6312_DETECT_GPRS_NET); 
            } else {
                /*再次查询*/
                osSignalSet(net_task_hdl,NET_TASK_M6312_DETECT_GPRS_ATTACH);
            }
        }

        /*m6312查询是否激活网络上下文*/
        if (os_event.value.signals & NET_TASK_M6312_DETECT_GPRS_NET) {
            rc = m6312_get_gprs_net_status(&gprs_net_status);
            if (rc != 0) {
                goto detect_net_exit;
            }
            /*m6312如果没有激活网络，就激活网络*/
            if (gprs_net_status != M6312_GPRS_NET_ACTIVE) {
                rc = m6312_set_gprs_net(M6312_GPRS_NET_ACTIVE);
                if (rc != 0) {
                    goto detect_net_exit;
                }                    
            }

detect_net_exit:
            if (rc == 0) {
                /*m6312检测连接模式*/
                osSignalSet(net_task_hdl,NET_TASK_M6312_DETECT_CONNECTION_MODE);
            } else {
                /*再次查询*/
                osSignalSet(net_task_hdl,NET_TASK_M6312_DETECT_GPRS_NET);
            }
        }

        /*m6312查询连接模式*/
        if (os_event.value.signals & NET_TASK_M6312_DETECT_CONNECTION_MODE) {
            rc = m6312_get_connection_mode(&connection_mode);
            if (rc != 0) {
                goto detect_connection_mode_exit;
            }
            /*m6312检测是否设置多路连接模式*/
            if (connection_mode != M6312_CONNECTION_MODE_MULTI) {
                rc = m6312_set_connection_mode(M6312_CONNECTION_MODE_MULTI);
                if (rc != 0) {
                    goto detect_connection_mode_exit;
                }
            }
detect_connection_mode_exit:
            if (rc == 0) {
                osSignalSet(net_task_hdl,NET_TASK_M6312_DETECT_TRANSPORT_MODE);
            } else {
                /*再次查询*/
                osSignalSet(net_task_hdl,NET_TASK_M6312_DETECT_CONNECTION_MODE);
            }
        }

        /*m6312查询传输模式*/
        if (os_event.value.signals & NET_TASK_M6312_DETECT_TRANSPORT_MODE) {
            rc = m6312_get_transport_mode(&transport_mode);
            if (rc != 0) {
                goto detect_transport_mode_exit;
            }

            /*m6312已经激活网络，检测是否设置多路连接模式*/
            if (transport_mode != M6312_TRANSPORT_MODE_NO_TRANSPARENT) {
                rc = m6312_set_transport_mode(M6312_TRANSPORT_MODE_NO_TRANSPARENT);
                if (rc != 0) {
                    goto detect_transport_mode_exit;
                }
            }
detect_transport_mode_exit:
                if (rc == 0) {
                    osSignalSet(net_task_hdl,NET_TASK_M6312_DETECT_RECV_CACHE_MODE);
                } else {
                    /*再次查询*/
                    osSignalSet(net_task_hdl,NET_TASK_M6312_DETECT_TRANSPORT_MODE);
                }
        }
        /*m6312查询接收缓存模式*/
        if (os_event.value.signals & NET_TASK_M6312_DETECT_RECV_CACHE_MODE) {
            rc = m6312_get_recv_cache_mode(&recv_cache_mode);
            if (rc != 0) {
                goto detect_recv_cache_mode_exit;
            }

            /*m6312已经多路连接模式，检测是否设置接收缓存模式*/
            if (recv_cache_mode != M6312_RECV_CACHE_MODE_CACHED) {
                rc = m6312_set_recv_cache_mode(M6312_RECV_CACHE_MODE_CACHED);
                if (rc != 0) {
                    goto detect_recv_cache_mode_exit;
                }
            }
detect_recv_cache_mode_exit:
                if (rc == 0) {
                    osSignalSet(net_task_hdl,NET_TASK_M6312_SEND_MESSAGE);
                } else {
                    /*再次查询*/
                    osSignalSet(net_task_hdl,NET_TASK_M6312_DETECT_RECV_CACHE_MODE);
                }
        }

        /*m6312发送数据*/
        if (os_event.value.signals & NET_TASK_M6312_SEND_MESSAGE) {
            mqtt_client_thread(0);
            m6312_close(0);
            net_task_m6312_timer_start(2000);
        }

                   

    }
}