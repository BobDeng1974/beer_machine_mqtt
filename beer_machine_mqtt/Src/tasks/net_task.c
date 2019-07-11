#include "cmsis_os.h"
#include "m6312.h"
#include "printf.h"
#include "tasks_init.h"
#include "net_task.h"
#include "mqtt_task.h"
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
                mqtt_task_msg_t mqtt_msg;
                mqtt_msg.head.id = MQTT_TASK_NET_INIT;
                xQueueSend(mqtt_task_msg_hdl,&mqtt_msg,5);  
        }

                   

    }
}