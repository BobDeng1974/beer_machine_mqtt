#include "cmsis_os.h"
#include "tasks_init.h"
#include "beer_machine.h"
#include "compressor_task.h"
#include "display_task.h"
#include "alarm_task.h"
#include "device_config.h"
#include "log.h"

osThreadId   compressor_task_handle;
osMessageQId compressor_task_msg_q_id;


osTimerId    compressor_work_timer_id;
osTimerId    compressor_wait_timer_id;
osTimerId    compressor_rest_timer_id;
osTimerId    compressor_pwr_wait_timer_id;


enum
{
    COMPRESSOR_STATUS_INIT = 0,     /*上电后的状态*/
    COMPRESSOR_STATUS_RDY,          /*正常关机后的就绪状态*/
    COMPRESSOR_STATUS_RDY_CONTINUE, /*长时间工作停机后的就绪状态*/
    COMPRESSOR_STATUS_REST,         /*长时间工作后的停机状态*/
    COMPRESSOR_STATUS_WORK,         /*压缩机工作状态*/
    COMPRESSOR_STATUS_WAIT,         /*两次开机之间的状态*/
    COMPRESSOR_STATUS_WAIT_CONTINUE /*异常时两次开机之间的状态*/
}compressor_status_t;

typedef struct
{
    uint8_t status;
    float temperature;
    int16_t temperature_work;
    int16_t temperature_stop;
    uint8_t is_temperature_err;
    uint8_t is_pwr_on_enable;
    uint8_t is_temperature_config;
    uint8_t is_status_change;
}compressor_control_t;

static compressor_control_t compressor = {
.status = COMPRESSOR_STATUS_INIT
};

static void compressor_work_timer_init(void);
static void compressor_wait_timer_init(void);
static void compressor_rest_timer_init(void);

static void compressor_work_timer_start(void);
static void compressor_wait_timer_start(void);
static void compressor_rest_timer_start(void);


static void compressor_work_timer_stop(void);
/*
static void compressor_wait_timer_stop(void);
static void compressor_rest_timer_stop(void);
*/

static void compressor_work_timer_expired(void const *argument);
static void compressor_wait_timer_expired(void const *argument);
static void compressor_rest_timer_expired(void const *argument);
static void compressor_pwr_wait_timer_expired(void const *argument);

static void compressor_pwr_turn_on();
static void compressor_pwr_turn_off();


/*最长工作时间定时器*/
static void compressor_work_timer_init()
{
    osTimerDef(compressor_work_timer,compressor_work_timer_expired);
    compressor_work_timer_id=osTimerCreate(osTimer(compressor_work_timer),osTimerOnce,0);
    log_assert_null_ptr(compressor_work_timer_id);
}

static void compressor_work_timer_start(void)
{
    osTimerStart(compressor_work_timer_id,COMPRESSOR_TASK_WORK_TIMEOUT);  
}
static void compressor_work_timer_stop(void)
{
    osTimerStop(compressor_work_timer_id);  
}

static void compressor_work_timer_expired(void const *argument)
{
    compressor_task_message_t msg;
    /*工作超时 发送消息*/
    msg.head.id = COMPRESSOR_TASK_MSG_WORK_TIMEOUT;
    log_assert_bool_false(xQueueSend(compressor_task_msg_q_id,&msg,COMPRESSOR_TASK_PUT_MSG_TIMEOUT) == pdPASS);
}

/*两次开机之间的等待定时器*/
static void compressor_wait_timer_init()
{
    osTimerDef(compressor_wait_timer,compressor_wait_timer_expired);
    compressor_wait_timer_id=osTimerCreate(osTimer(compressor_wait_timer),osTimerOnce,0);
    log_assert_null_ptr(compressor_wait_timer_id);
}


static void compressor_wait_timer_start(void)
{
    osTimerStart(compressor_wait_timer_id,COMPRESSOR_TASK_WAIT_TIMEOUT);  
}

static void compressor_wait_timer_stop(void)
{
    osTimerStop(compressor_wait_timer_id);  
}


static void compressor_wait_timer_expired(void const *argument)
{
    compressor_task_message_t msg;
    /*工作超时 发送消息*/
    msg.head.id = COMPRESSOR_TASK_MSG_WAIT_TIMEOUT;
    log_assert_bool_false(xQueueSend(compressor_task_msg_q_id,&msg,COMPRESSOR_TASK_PUT_MSG_TIMEOUT) == pdPASS);
}
/*上电后等待定时器*/
static void compressor_pwr_wait_timer_init()
{
    osTimerDef(compressor_pwr_wait_timer,compressor_pwr_wait_timer_expired);
    compressor_pwr_wait_timer_id=osTimerCreate(osTimer(compressor_pwr_wait_timer),osTimerOnce,0);
    log_assert_null_ptr(compressor_pwr_wait_timer_id);
}


static void compressor_pwr_wait_timer_start(void)
{
    osTimerStart(compressor_pwr_wait_timer_id,COMPRESSOR_TASK_PWR_WAIT_TIMEOUT);   
}

static void compressor_pwr_wait_timer_expired(void const *argument)
{
    compressor_task_message_t msg;
    /*工作超时 发送消息*/
    msg.head.id = COMPRESSOR_TASK_MSG_PWR_WAIT_TIMEOUT;
    log_assert_bool_false(xQueueSend(compressor_task_msg_q_id,&msg,COMPRESSOR_TASK_PUT_MSG_TIMEOUT) == pdPASS);
}


static void compressor_rest_timer_init(void)
{
    osTimerDef(compressor_rest_timer,compressor_rest_timer_expired);
    compressor_rest_timer_id = osTimerCreate(osTimer(compressor_rest_timer),osTimerOnce,0);
    log_assert_null_ptr(compressor_rest_timer_id);
}

static void compressor_rest_timer_start(void)
{
    osTimerStart(compressor_rest_timer_id,COMPRESSOR_TASK_REST_TIMEOUT);
}


static void compressor_rest_timer_stop(void)
{
    osTimerStop(compressor_rest_timer_id);
}

static void compressor_rest_timer_expired(void const *argument)
{
    compressor_task_message_t msg;
    /*工作超时 发送消息*/
    msg.head.id = COMPRESSOR_TASK_MSG_REST_TIMEOUT;
    log_assert_bool_false(xQueueSend(compressor_task_msg_q_id,&msg,COMPRESSOR_TASK_PUT_MSG_TIMEOUT) == pdPASS);
}

static void compressor_task_send_temperature_msg_to_self(void)
{  
    compressor_task_message_t msg;
    /*构建温度消息*/
    if (compressor.is_temperature_err == 1) {
        msg.head.id = COMPRESSOR_TASK_MSG_TEMPERATURE_ERR;
    } else {
        msg.head.id = COMPRESSOR_TASK_MSG_TEMPERATURE_VALUE;
    }
    msg.content.value_float[0] = compressor.temperature;
    log_assert_bool_false(xQueueSend(compressor_task_msg_q_id,&msg,COMPRESSOR_TASK_PUT_MSG_TIMEOUT) == pdPASS);
}

/*压缩机打开*/
static void compressor_pwr_turn_on()
{
    bsp_compressor_ctrl_on(); 
}
/*压缩机关闭*/
static void compressor_pwr_turn_off()
{
    bsp_compressor_ctrl_off();  
}

/*压缩机发送更新状态消息*/
static void compressor_task_common_send_message(uint32_t msg_id,int32_t msg_value)
{
    compressor_task_message_t msg;

    msg.head.id = msg_id;
    msg.content.value_int32 = msg_value;
    log_assert_bool_false(xQueueSend(compressor_task_msg_q_id,&msg,COMPRESSOR_TASK_PUT_MSG_TIMEOUT) == pdPASS);
}


/*压缩机改变到指定状态*/
static void compressor_task_change_to_status(uint8_t status)
{
    compressor.status = status;

    if (compressor.status == COMPRESSOR_STATUS_WORK) {
        compressor.is_status_change = 1;
        log_info("打开压缩机.\r\n");
        /*打开压缩机*/
        compressor_pwr_turn_on();
        /*打开工作定时器*/ 
        compressor_work_timer_start();
    } else if (compressor.status == COMPRESSOR_STATUS_REST || \
               compressor.status == COMPRESSOR_STATUS_WAIT || \
               compressor.status == COMPRESSOR_STATUS_WAIT_CONTINUE) {
        compressor.is_status_change = 1;
        log_info("关闭压缩机.\r\n");
        /*关闭压缩机*/
        compressor_pwr_turn_off();
        /*关闭所有定时器*/ 
        compressor_work_timer_stop();  
        compressor_wait_timer_stop();  
        compressor_rest_timer_stop();

        if (compressor.status == COMPRESSOR_STATUS_REST) {
            compressor_rest_timer_start();
        } else {
            compressor_wait_timer_start();
        }
    }

}

/*压缩机任务*/
void compressor_task(void const *argument)
{
    compressor_task_message_t msg_recv;
    compressor_task_message_t msg_send;
    /*开机先关闭压缩机*/
    compressor_pwr_turn_off();

    compressor_work_timer_init();
    compressor_wait_timer_init();
    compressor_rest_timer_init();
    compressor_pwr_wait_timer_init();

    /*上电等待超时*/
    compressor_pwr_wait_timer_start();
  
    while(1) {
    if (xQueueReceive(compressor_task_msg_q_id, &msg_recv,0xFFFFFFFF) == pdTRUE) {
  
        /*压缩机使能消息*/
        if (msg_recv.head.id == COMPRESSOR_TASK_MSG_PWR_ON_ENABLE) {
            /*开机消息*/
            compressor.is_pwr_on_enable = 1;
            log_debug("使能压缩机.\r\n");
            /*更新压缩机状态*/
            msg_send.head.id = COMPRESSOR_TASK_MSG_UPDATE_STATUS;
            log_assert_bool_false(xQueueSend(compressor_task_msg_q_id,&msg_send,COMPRESSOR_TASK_PUT_MSG_TIMEOUT) == pdPASS);
        }

        /*压缩机失能消息*/
        if (msg_recv.head.id == COMPRESSOR_TASK_MSG_PWR_ON_DISABLE) {
            /*开机消息*/
            compressor.is_pwr_on_enable = 0;
            log_debug("失能压缩机.\r\n");
            /*更新压缩机状态*/
            compressor_task_common_send_message(COMPRESSOR_TASK_MSG_UPDATE_STATUS,0);
        }

        /*压缩机状态更新消息*/
        if (msg_recv.head.id == COMPRESSOR_TASK_MSG_UPDATE_STATUS) {
            /*等待上电完毕后以及配置完成后*/
            if (compressor.status != COMPRESSOR_STATUS_INIT && compressor.is_temperature_config) {
                if (compressor.is_pwr_on_enable == 1) {
                    if (compressor.temperature >= compressor.temperature_work  && compressor.status == COMPRESSOR_STATUS_RDY) {
                        log_info("温度:%d C高于开机温度:%d C.\r\n",compressor.temperature,compressor.temperature_work);
                        compressor_task_change_to_status(COMPRESSOR_STATUS_WORK);
                    } else if (compressor.temperature > compressor.temperature_stop   && compressor.status == COMPRESSOR_STATUS_RDY_CONTINUE){
                        /*温度大于关机温度，同时是超时关机或者异常关机状态时，继续开机*/
                        log_info("温度:%d C高于关机温度:%d C.\r\n",compressor.temperature,compressor.temperature_stop);
                        compressor_task_change_to_status(COMPRESSOR_STATUS_WORK);
                    } else if (compressor.temperature <= compressor.temperature_stop && compressor.status == COMPRESSOR_STATUS_WORK) {
                        log_info("温度:%d C低于关机温度:%d C.\r\n",compressor.temperature,compressor.temperature_stop );
                        compressor_task_change_to_status(COMPRESSOR_STATUS_WAIT);
                    }
                } else {
                    /*禁止开机的情况下*/
                    /*如果依然在工作，需要立即关闭*/
                    if (compressor.status == COMPRESSOR_STATUS_WORK) {
                        log_info("远程控制 立即关机温度:%d C.\r\n",compressor.temperature);
                        compressor_task_change_to_status(COMPRESSOR_STATUS_WAIT_CONTINUE); 
                    }
                }
            } else {
                log_info("压缩机状态:%d 无需处理.\r\n",compressor.status);     
            }
        }

       
        /*压缩机温度配置消息处理*/
        if (msg_recv.head.id == COMPRESSOR_TASK_MSG_TEMPERATURE_CONFIG) { 
            /*缓存温度配置值*/
            compressor.temperature_work = (int16_t)(msg_recv.content.value_float[0]);
            compressor.temperature_stop = (int16_t)(msg_recv.content.value_float[1]);
            compressor.is_temperature_config = 1;
            /*更新压缩机状态*/
            compressor_task_common_send_message(COMPRESSOR_TASK_MSG_UPDATE_STATUS,(int32_t)compressor.temperature);
        }
   
        /*压缩机上电等待完毕消息处理*/
        if (msg_recv.head.id == COMPRESSOR_TASK_MSG_PWR_WAIT_TIMEOUT) { 
            compressor.status = COMPRESSOR_STATUS_RDY_CONTINUE;
            /*更新压缩机状态*/
            compressor_task_common_send_message(COMPRESSOR_TASK_MSG_UPDATE_STATUS,(int32_t)compressor.temperature);
        } 
      
        /*温度消息处理*/
        if (msg_recv.head.id == COMPRESSOR_TASK_MSG_TEMPERATURE_VALUE) { 
            /*缓存温度值*/
            compressor.temperature = msg_recv.content.value_int32; 
            /*更新压缩机状态*/
            compressor_task_common_send_message(COMPRESSOR_TASK_MSG_UPDATE_STATUS,(int32_t)compressor.temperature);
        }
    
        /*温度错误消息处理*/
        if (msg_recv.head.id == COMPRESSOR_TASK_MSG_TEMPERATURE_ERR) {
            log_info("温度错误.\r\n");  
            compressor.is_temperature_err = 1;
            /*温度异常时，如果在工作,就变更为wait continue状态*/
            compressor_task_change_to_status(COMPRESSOR_STATUS_WAIT_CONTINUE);
        }
 
  
    /*压缩机工作超时消息*/
    if (msg_recv.head.id == COMPRESSOR_TASK_MSG_WORK_TIMEOUT) {
        compressor_task_change_to_status(COMPRESSOR_STATUS_REST);
    }

    /*压缩机等待超时消息*/
    if (msg.head.id == COMPRESSOR_TASK_MSG_WAIT_TIMEOUT) {
        if (compressor.status == COMPRESSOR_STATUS_WAIT || compressor.status == COMPRESSOR_STATUS_WAIT_CONTINUE) {
        if (compressor.status == COMPRESSOR_STATUS_WAIT) {
           compressor.status = COMPRESSOR_STATUS_RDY;
           log_info("compressor change status to rdy.\r\n");  
        } else {
           compressor.status = COMPRESSOR_STATUS_RDY_CONTINUE;
           log_info("compressor change status to rdy continue.\r\n");  
        }
        /*构建温度消息*/
        compressor_task_send_temperature_msg_to_self();
    } else {
      log_info("压缩机状态:%d 无需处理.\r\n",compressor.status);     
    }
  }
 
  /*压缩机休息超时消息*/
  if (msg.head.id == COMPRESSOR_TASK_MSG_REST_TIMEOUT) {
     if (compressor.status == COMPRESSOR_STATUS_REST) {
        compressor.status = COMPRESSOR_STATUS_RDY_CONTINUE;
        log_info("compressor change status to rdy continue.\r\n");
        
        compressor_task_send_temperature_msg_to_self();
     } else {
       log_info("压缩机状态:%d 无需处理.\r\n",compressor.status);     
     }
  }
 
  /*处理状态变化显示*/
  if (compressor.status_change == true ) {
     compressor.status_change = false;
     display_msg.head.id =  DISPLAY_TASK_MSG_COMPRESSOR;
     if (compressor.status == COMPRESSOR_STATUS_WORK) { 
        display_msg.blink = true;
     } else {
        display_msg.blink = false;  
     }
     status = osMessagePut(display_task_msg_q_id,*(uint32_t*)&display_msg,COMPRESSOR_TASK_PUT_MSG_TIMEOUT);
     if (status !=osOK) {
        log_error("put compress display msg error:%d\r\n",status);
     }    
  }
 
 }
 }
}

