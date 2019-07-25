#include "cmsis_os.h"
#include "m6312.h"
#include "printf.h"
#include "tiny_timer.h"
#include "tasks_init.h"
#include "net_task.h"
#include "socket.h"
#include "mqtt_task.h"
#include "report_task.h"
#include "log.h"


osThreadId  net_task_hdl;       /**< 网络任务句柄*/
osMessageQId net_task_msg_q_id; /**< 网络任务消息id*/

 /** 描述*/
osTimerId query_base_info_timer_id;

/**
* @brief m6312定时器回调
* @details 
* @param 无
* @return 无
* @attention
* @note
*/
static void net_task_query_base_info_timer_callback(void const *argument);


/*m6312定时器*/
void net_task_query_base_info_timer_init(void)
{
    osTimerDef(query_base_info_timer,net_task_query_base_info_timer_callback);

    query_base_info_timer_id = osTimerCreate(osTimer(query_base_info_timer),osTimerOnce,NULL);
    log_assert_null_ptr(query_base_info_timer_id);
}

static void net_task_query_base_info_timer_start(uint32_t timeout)
{
    osTimerStart(query_base_info_timer_id,timeout);
}
/*
static void net_task_query_base_info_timer_stop(void)
{
    osTimerStop(query_base_info_timer_id);
}
*/

static void net_task_query_base_info_timer_callback(void const *argument)
{
    osSignalSet(net_task_hdl,NET_TASK_SIGNAL_QUERY_BASE_INFO);
}

static net_context_t net_context;


/*查询sim id*/
static int net_task_query_sim_id(char *sim_id,uint32_t timeout)
{
    int rc;
    tiny_timer_t timer;
    tiny_timer_init(&timer,0,timeout);

    /**sim卡存在,获取sim id*/
step_sim_id:
    if (tiny_timer_value(&timer) == 0) {
        return -1;
    }
    osDelay(NET_TASK_INIT_STEP_DELAY);
    rc = m6312_get_sim_card_id(sim_id);
    if (rc != 0) {
        goto step_sim_id;
    }

    return 0;
}

/*查询基站信息*/
static int net_task_query_base_info(base_information_t *base_info,uint32_t timeout)
{
    int rc;
    tiny_timer_t timer;

    tiny_timer_init(&timer,0,timeout);
    /*所有基站信息*/
all_base_info:
    if (tiny_timer_value(&timer) == 0) {
        return -1;
    }
    osDelay(NET_TASK_INIT_STEP_DELAY);
    rc = m6312_get_all_base_info(base_info,BASE_CNT_MAX);
    if (rc != 0) {
        goto all_base_info;
    }
   
    return 0;
}


/*初始化*/
static int net_task_net_init(net_context_t *net_ctx,uint32_t timeout)
{
    int rc;
    tiny_timer_t timer;
    m6312_sim_card_status_t sim_card_status;
    m6312_sim_operator_t sim_operator;

    tiny_timer_init(&timer,0,timeout);

    /*重启*/
    m6312_pwr_off();
    m6312_pwr_on();

    /*关闭回显*/
step_echo:
    if (tiny_timer_value(&timer) == 0) {
        return -1;
    }
    osDelay(NET_TASK_INIT_STEP_DELAY);
    rc = m6312_echo_turn_on_off(0);
    if (rc != 0) {
        goto step_echo;
    }
    /*m6312查询sim是否插入*/
step_sim_card:
    if (tiny_timer_value(&timer) == 0) {
        return -1;
    }
    osDelay(NET_TASK_INIT_STEP_DELAY);
    rc = m6312_get_sim_card_status(&sim_card_status);
    if (rc != 0) {
        goto step_sim_card;
    }
    /*sim卡不存在 直接退出*/
    if (sim_card_status == M6312_SIM_CARD_NO_EXIST) {
        net_ctx->is_sim_card_exsit = 0;
        net_ctx->is_net_ready = 0;
        return 0;
    }
    
    /*提示关闭*/
step_report_mode:
    if (tiny_timer_value(&timer) == 0) {
        return -1;
    }
    osDelay(NET_TASK_INIT_STEP_DELAY);
    rc = m6312_set_auto_report_mode(M6312_AUTO_REPORT_MODE_OFF);
    if (rc != 0) {
        goto step_report_mode;
    }
    /*运营商*/
step_operator:
    if (tiny_timer_value(&timer) == 0) {
        return -1;
    }
    osDelay(NET_TASK_INIT_STEP_DELAY);
    rc = m6312_get_operator(&sim_operator);
    if (rc != 0) {
        goto step_operator;
    }
    if (sim_operator == SIM_OPERATOR_CHINA_MOBILE) {
        net_ctx->operator_code = 1;
    } else {
        net_ctx->operator_code = 2;
    }
    /*设置apn*/
step_set_apn:
    if (tiny_timer_value(&timer) == 0) {
        return -1;
    }
    osDelay(NET_TASK_INIT_STEP_DELAY);
    rc = m6312_set_gprs_apn("cmnet"); 
    if (rc != 0) {
        goto step_set_apn;
    }

    /*附着网络*/
step_attach:
    if (tiny_timer_value(&timer) == 0) {
        return -1;
    }
    osDelay(NET_TASK_INIT_STEP_DELAY);
    rc = m6312_set_gprs_attach(M6312_GPRS_ATTACH);
    if (rc != 0) {
        goto step_attach;
    }
    /*激活gprs*/
step_active_gprs:
    if (tiny_timer_value(&timer) == 0) {
        return -1;
    }
    osDelay(NET_TASK_INIT_STEP_DELAY);
    rc = m6312_set_gprs_net(M6312_GPRS_NET_ACTIVE);
    if (rc != 0) {
        goto step_active_gprs;
    }
    /*连接模式*/
step_multi_connection:
    if (tiny_timer_value(&timer) == 0) {
        return -1;
    }
    osDelay(NET_TASK_INIT_STEP_DELAY);
    rc = m6312_set_connection_mode(M6312_CONNECTION_MODE_MULTI);
    if (rc != 0) {
        goto step_multi_connection;
    }
    /*接收缓存*/
step_recv_cached:
    if (tiny_timer_value(&timer) == 0) {
        return -1;
    }
    osDelay(NET_TASK_INIT_STEP_DELAY);
    rc = m6312_set_recv_cache_mode(M6312_RECV_CACHE_MODE_CACHED);
    if (rc != 0) {
        goto step_recv_cached;
    }
    
    net_ctx->is_sim_card_exsit = 1;
    net_ctx->is_net_ready = 1;

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
void net_task(void const * argument)
{
    int rc;
    osEvent os_event;

    socket_init();
    m6312_uart_init();

    net_task_query_base_info_timer_init();
    osSignalSet(net_task_hdl,NET_TASK_SIGNAL_INIT);
    log_debug("net task run...\r\n");

    while (1)
    {
        osDelay(100);
        os_event = osSignalWait(NET_TASK_ALL_SIGNALS,osWaitForever);
        if (os_event.status == osEventSignal) {
            /*m6312初始化*/
            if (os_event.value.signals & NET_TASK_SIGNAL_INIT) {
                rc = net_task_net_init(&net_context,NET_TASK_NET_INIT_TIMEOUT);
                if (rc == 0 && net_context.is_net_ready == 1) {
                    /*告知mqtt任务网络就绪*/
                    mqtt_task_msg_t mqtt_msg;
                    mqtt_msg.head.id = MQTT_TASK_MSG_NET_READY;
                    log_assert_bool_false(xQueueSend(mqtt_task_msg_hdl,&mqtt_msg,5) == pdPASS);
                    /*启动查询SIM ID*/
                    osSignalSet(net_task_hdl,NET_TASK_SIGNAL_QUERY_SIM_ID);
                }
            }
      
            if (os_event.value.signals & NET_TASK_SIGNAL_QUERY_SIM_ID) {
                rc = net_task_query_sim_id(net_context.sim_id,NET_TASK_QUERY_TIMEOUT);
                if (rc == 0) {
                    /*通知sim id给上报任务*/
                    report_task_message_t msg;
                    msg.head.id = REPORT_TASK_MSG_SIM_ID;
                    strcpy(msg.content.sim_id,net_context.sim_id);
                    log_assert_bool_false(xQueueSend(report_task_msg_q_id,&msg,NET_TASK_PUT_MSG_TIMEOUT) == pdPASS);
                    /*启动查询基站信息*/
                    osSignalSet(net_task_hdl,NET_TASK_SIGNAL_QUERY_BASE_INFO);
                }
            }

            if (os_event.value.signals & NET_TASK_SIGNAL_QUERY_BASE_INFO) {
                rc = net_task_query_base_info(&net_context.base_info,NET_TASK_QUERY_TIMEOUT);
                if (rc == 0) {
                    /*通知基站信息给上报任务*/
                    report_task_message_t msg;
                    msg.head.id = REPORT_TASK_MSG_BASE_INFO;
                    msg.content.base_info = net_context.base_info;
                    log_assert_bool_false(xQueueSend(report_task_msg_q_id,&msg,NET_TASK_PUT_MSG_TIMEOUT) == pdPASS);
                    /*开启基站信息定时查询*/
                    net_task_query_base_info_timer_start(NET_TASK_QUERY_BASE_INFO_INTERVAL);
                }
            }
        
        }             
    }
}