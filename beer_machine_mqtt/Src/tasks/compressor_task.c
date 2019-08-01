#include "cmsis_os.h"
#include "tasks_init.h"
#include "board.h"
#include "compressor_task.h"
#include "device_config.h"
#include "report_task.h"
#include "device_env.h"
#include "stdlib.h"
#include "stdio.h"
#include "stdbool.h"
#include "log.h"

/*任务句柄*/
osThreadId compressor_task_hdl;
/*消息句柄*/
osMessageQId compressor_task_msg_q_id;

/*定时器句柄*/
osTimerId compressor_timer_id;
osTimerId compressor_run_time_timer_id;


typedef enum
{
    COMPRESSOR_STATUS_INIT = 0,        /*上电后的状态*/
    COMPRESSOR_STATUS_WORK,            /*压缩机工作状态*/
    COMPRESSOR_STATUS_STOP_RDY,        /*正常关机后的就绪状态,可以随时启动*/
    COMPRESSOR_STATUS_STOP_REST,       /*长时间工作后的停机休息的状态*/
    COMPRESSOR_STATUS_STOP_CONTINUE,   /*长时间工作后且停机休息完毕后的状态,需要继续治冷到达最低限温度值*/
    COMPRESSOR_STATUS_STOP_WAIT,       /*两次开机之间的停机等待状态*/
    COMPRESSOR_STATUS_STOP_FAULT,      /*温度传感器故障停机等待状态*/
}compressor_status_t;

typedef struct
{
    float stop;
    float work;
}compressor_temperature_level_t;

typedef struct
{
    compressor_status_t status;
    int8_t temperature_int;
    float temperature_float;
    float temperature_stop;
    float temperature_work;

    bool temperature_err;
    bool is_pwr_enable;
    bool is_temperature_config;
    uint32_t run_time;
    uint32_t run_time_total;
    uint32_t run_start_time;
}compressor_t;

/*压缩机对象实体*/
static compressor_t compressor ={
.status = COMPRESSOR_STATUS_INIT,
.temperature_stop = DEFAULT_COMPRESSOR_TEMPERATURE_START,
.temperature_work = DEFAULT_COMPRESSOR_TEMPERATURE_STOP,
.temperature_err = false
};




static void compressor_timer_init(void);

static void compressor_timer_start(uint32_t timeout);
static void compressor_timer_stop(void);
static void compressor_timer_expired(void const *argument);

static void compressor_pwr_turn_on();
static void compressor_pwr_turn_off();

static void compressor_run_time_timer_init();
static void compressor_run_time_timer_start();
static void compressor_run_time_timer_expired(void const *argument);

/*
* @brief 压缩机定时器
* @param 无
* @return 无
* @note
*/
static void compressor_timer_init(void)
{
    osTimerDef(compressor_timer,compressor_timer_expired);
    compressor_timer_id=osTimerCreate(osTimer(compressor_timer),osTimerOnce,0);
    log_assert_null_ptr(compressor_timer_id);
}

/*
* @brief 定时器启动
* @param timeout 超时时间
* @return 无
* @note
*/
static void compressor_timer_start(uint32_t timeout)
{
    osTimerStart(compressor_timer_id,timeout);  
}

/*
* @brief 工作时间定时器停止
* @param 无
* @return 无
* @note
*/

static void compressor_timer_stop(void)
{
    osTimerStop(compressor_timer_id);  
}

/*
* @brief 定时器回调
* @param argument 回调参数
* @return 无
* @note
*/
static void compressor_timer_expired(void const *argument)
{
    compressor_task_message_t msg;
    /*发送消息*/
    msg.head.id = COMPRESSOR_TASK_MSG_TYPE_TIMER_TIMEOUT;
    log_assert_bool_false(xQueueSend(compressor_task_msg_q_id,&msg,COMPRESSOR_TASK_PUT_MSG_TIMEOUT) == pdPASS);
}

/*压缩机工作时间定时器*/
static void compressor_run_time_timer_init()
{
    osTimerDef(compressor_run_time_timer,compressor_run_time_timer_expired);
    compressor_run_time_timer_id=osTimerCreate(osTimer(compressor_run_time_timer),osTimerOnce,0);
    log_assert_null_ptr(compressor_run_time_timer_id);
}

static void compressor_run_time_timer_start(void)
{
    osTimerStart(compressor_run_time_timer_id,COMPRESSOR_TASK_RUN_TIME_UPDATE_TIMEOUT);   
}

static void compressor_run_time_timer_expired(void const *argument)
{
    compressor_task_message_t msg;
    /*发送消息*/
    msg.head.id = COMPRESSOR_TASK_MSG_RUN_TIME_UPDATE;
    log_assert_bool_false(xQueueSend(compressor_task_msg_q_id,&msg,COMPRESSOR_TASK_PUT_MSG_TIMEOUT) == pdPASS);
}

static uint32_t compressor_get_current_time()
{
    return osKernelSysTick();
}

/*压缩机打开*/
static void compressor_pwr_turn_on()
{
    bsp_compressor_ctrl_pwr_on();
    /*缓存开机时间*/
    compressor.run_start_time = compressor_get_current_time();
}
/*压缩机关闭*/
static void compressor_pwr_turn_off()
{
    uint32_t current_time;

    bsp_compressor_ctrl_pwr_off();
    /*计算开机时间*/
    current_time = compressor_get_current_time() - compressor.run_start_time;
    compressor.run_time += current_time - compressor.run_start_time;
    compressor.run_time_total += current_time - compressor.run_start_time;
}

/*
* @brief 压缩机任务
* @param argument 任务参数
* @return 无
* @note
*/
void compressor_task(void const *argument)
{
    float setting_min,setting_max;
    compressor_task_message_t msg_recv,msg_temp;
    
    /*上电先关闭压缩机*/
    compressor_pwr_turn_off();
    /*定时器初始化*/
    compressor_timer_init();
    /*消除编译警告*/
    compressor_timer_stop();

    compressor_run_time_timer_init();
    compressor_run_time_timer_start();

    /*上电等待*/
    compressor_timer_start(COMPRESSOR_TASK_PWR_ON_WAIT_TIMEOUT);
  
    while (1) {
    if (xQueueReceive(compressor_task_msg_q_id, &msg_recv,0xFFFFFFFF) == pdTRUE) {
        /*压缩机定时器超时消息，压缩机根据超时事件更新工作状态*/
        if (msg_recv.head.id == COMPRESSOR_TASK_MSG_TYPE_TIMER_TIMEOUT){       
            if (compressor.status == COMPRESSOR_STATUS_INIT) {
                /*上电等待完毕*/
                log_info("压缩机上电等待完毕.\r\n");
                compressor.status = COMPRESSOR_STATUS_STOP_RDY; 
                 /*发送消息更新压缩机工作状态*/
                msg_temp.head.id = COMPRESSOR_TASK_MSG_TYPE_UPDATE_STATUS;
                log_assert_bool_false(xQueueSend(compressor_task_msg_q_id,&msg_temp,COMPRESSOR_TASK_PUT_MSG_TIMEOUT) == pdPASS);   
       
            } else if (compressor.status == COMPRESSOR_STATUS_WORK) {
                /*压缩机工作时间到达最大时长*/
                log_info("压缩机到达最大工作时长.停机%d分钟.\r\n",COMPRESSOR_TASK_REST_TIMEOUT / (60 * 1000));
                compressor.status = COMPRESSOR_STATUS_STOP_REST;
                /*关闭压缩机*/
                compressor_pwr_turn_off(); 
                /*等待休息完毕*/
                compressor_timer_start(COMPRESSOR_TASK_REST_TIMEOUT);                
            } else if (compressor.status == COMPRESSOR_STATUS_STOP_REST || compressor.status == COMPRESSOR_STATUS_STOP_WAIT || compressor.status == COMPRESSOR_STATUS_STOP_FAULT) {
                if (compressor.status == COMPRESSOR_STATUS_STOP_REST) {
                    /*压缩机休息完毕*/
                    log_info("压缩机休息完毕.\r\n");
                    compressor.status = COMPRESSOR_STATUS_STOP_CONTINUE;
                } else if (compressor.status == COMPRESSOR_STATUS_STOP_WAIT) {
                    /*压缩机等待完毕*/
                    log_info("压缩机等待完毕.\r\n");
                    compressor.status = COMPRESSOR_STATUS_STOP_RDY;
                } else {
                    /*压缩机传感器错误等待完毕*/
                    log_info("压缩机温度错误等待完毕.\r\n");
                    compressor.status = COMPRESSOR_STATUS_STOP_CONTINUE;
                }
                /*发送消息更新压缩机工作状态*/
                msg_temp.head.id = COMPRESSOR_TASK_MSG_TYPE_UPDATE_STATUS;
                log_assert_bool_false(xQueueSend(compressor_task_msg_q_id,&msg_temp,COMPRESSOR_TASK_PUT_MSG_TIMEOUT) == pdPASS);           
            } else {
                /*压缩机休息完毕*/
                log_warning("压缩机代码内部错误.忽略.\r\n");          
            }
        }
      
        /*温度更新消息处理*/
        if (msg_recv.head.id == COMPRESSOR_TASK_MSG_TYPE_TEMPERATURE_UPDATE){ 
            /*去除温度错误标志*/
            compressor.temperature_err = false;
            /*缓存温度值*/
            compressor.temperature_float = msg_recv.content.temperature_float; 
            /*发送消息更新压缩机工作状态*/
            msg_temp.head.id = COMPRESSOR_TASK_MSG_TYPE_UPDATE_STATUS;
            log_assert_bool_false(xQueueSend(compressor_task_msg_q_id,&msg_temp,COMPRESSOR_TASK_PUT_MSG_TIMEOUT) == pdPASS);     
        }

        /*温度错误消息处理*/
        if (msg_recv.head.id == COMPRESSOR_TASK_MSG_TYPE_TEMPERATURE_ERR) { 
            compressor.temperature_err = true;
            if (compressor.status == COMPRESSOR_STATUS_WORK){
                /*温度异常时，如果在工作,就变更为STOP_FAULT状态*/
                compressor.status = COMPRESSOR_STATUS_STOP_FAULT;
                /*关闭压缩机和工作定时器*/
                compressor_pwr_turn_off(); 
                /*同样打开等待定时器*/ 
                compressor_timer_start(COMPRESSOR_TASK_WAIT_TIMEOUT);  
                log_info("温度错误.关压缩机.等待%d分钟.\r\n",COMPRESSOR_TASK_WAIT_TIMEOUT / (60 * 1000));
            } else {
                log_info("温度错误.压缩机已经停机状态:%d.跳过.\r\n",compressor.status);
            }
        }
 
        /*压缩机根据温度更新工作状态*/
        if (msg_recv.head.id == COMPRESSOR_TASK_MSG_TYPE_UPDATE_STATUS){ 
            /*只有在没有温度错误和等待上电完毕后才处理压缩机的启停*/
            if (compressor.is_pwr_enable == true && compressor.is_temperature_config == true && compressor.temperature_err == false && compressor.status != COMPRESSOR_STATUS_INIT) {
                if (compressor.temperature_float <= compressor.temperature_stop && compressor.status == COMPRESSOR_STATUS_WORK){                  
                    compressor.status = COMPRESSOR_STATUS_STOP_WAIT;
                    /*关闭压缩机*/
                    compressor_pwr_turn_off(); 
                    /*打开等待定时器*/ 
                    compressor_timer_start(COMPRESSOR_TASK_WAIT_TIMEOUT);
                    log_info("温度:%.2f C低于关机温度:%.2f C.关机等待%d分钟.\r\n",compressor.temperature_float,compressor.temperature_stop,COMPRESSOR_TASK_WAIT_TIMEOUT / (60 * 1000));
                } else if (compressor.temperature_float >= compressor.temperature_work && compressor.status == COMPRESSOR_STATUS_STOP_RDY ) {       
                    /*温度大于开机温度，同时是正常关机状态时，开机*/
                    compressor.status = COMPRESSOR_STATUS_WORK; 
                    /*打开压缩机*/
                    compressor_pwr_turn_on();
                    /*打开工作定时器*/ 
                    compressor_timer_start(COMPRESSOR_TASK_WORK_TIMEOUT); 
                    log_info("温度:%.2f C高于开机温度:%.2f C.正常开压缩机.\r\n",compressor.temperature_float,compressor.temperature_work);
                }else if (compressor.temperature_float > compressor.temperature_stop && compressor.status == COMPRESSOR_STATUS_STOP_CONTINUE) {
                    /*超时关机或者异常关机状态后，温度大于关机温度，继续开机*/ 
                    compressor.status = COMPRESSOR_STATUS_WORK; 
                    /*打开压缩机*/
                    compressor_pwr_turn_on();
                    /*打开工作定时器*/ 
                    compressor_timer_start(COMPRESSOR_TASK_WORK_TIMEOUT); 
                    log_info("温度:%.2f C高于关机温度:%.2f C.继续开压缩机.\r\n",compressor.temperature_float,compressor.temperature_stop);
                } else {
                    log_info("压缩机状态:%d无需处理.\r\n",compressor.status);
                }
            } else {
                log_info("压缩机正在上电等待或者温度错误.跳过.\r\n");
            }
        }
        /*查询压缩机工作温度配置消息*/
        if (msg_recv.head.id == COMPRESSOR_TASK_MSG_TYPE_QUERY_TEMPERATURE_SETTING){ 
            /*发送消息给通信任务*/
        }

        /*配置压缩机工作温度区间消息*/
        if (msg_recv.head.id == COMPRESSOR_TASK_MSG_TYPE_TEMPERATURE_SETTING){ 
            setting_min = msg_recv.content.temperature_setting_min;
            setting_max = msg_recv.content.temperature_setting_max;
            if (setting_min < DEFAULT_COMPRESSOR_LOW_TEMPERATURE_LIMIT) {
                log_error("温度设置值:%d ± %d C无效.< min:%d C.\r\n.",setting_min,DEFAULT_COMPRESSOR_LOW_TEMPERATURE_LIMIT);
            } else if(setting_max > DEFAULT_COMPRESSOR_HIGH_TEMPERATURE_LIMIT) {
                log_error("温度设置值:%d ± %d C无效.> max:%d C.\r\n.",setting_max,DEFAULT_COMPRESSOR_HIGH_TEMPERATURE_LIMIT);
            } else {     
                compressor.is_temperature_config = true;
                if (compressor.temperature_work != setting_max || compressor.temperature_stop != setting_min) {
                    /*复制当前温度区间到缓存*/
                    compressor.temperature_work = setting_max;
                    compressor.temperature_stop = setting_min;         
                    log_debug("设置温度区间%.2fC~%.2fC成功.\r\n",compressor.temperature_stop,compressor.temperature_work);
                    /*发送消息更新压缩机工作状态*/
                    msg_temp.head.id = COMPRESSOR_TASK_MSG_TYPE_UPDATE_STATUS;
                    log_assert_bool_false(xQueueSend(compressor_task_msg_q_id,&msg_temp,COMPRESSOR_TASK_PUT_MSG_TIMEOUT) == pdPASS);                   
                } else {
                    log_debug("温度设置值与当前一致.跳过.\r\n");
                }
            }                                
        } 
        /*压缩机远程锁定压缩机*/
        if (msg_recv.head.id == COMPRESSOR_TASK_MSG_PWR_ON_DISABLE){
            log_info("lock compressor.\r\n");
            compressor.is_pwr_enable = false;
            if (compressor.status == COMPRESSOR_STATUS_WORK) {
                compressor.status = COMPRESSOR_STATUS_STOP_WAIT;
                /*关闭压缩机*/
                compressor_pwr_turn_off(); 
                /*打开等待定时器*/ 
                compressor_timer_start(COMPRESSOR_TASK_WAIT_TIMEOUT); 
            }
        }

        /*压缩机远程释放压缩机*/
        if (msg_recv.head.id == COMPRESSOR_TASK_MSG_PWR_ON_DISABLE){
            log_info("unlock compressor.\r\n");
            compressor.is_pwr_enable = true;
            /*发送消息更新压缩机工作状态*/
            msg_temp.head.id = COMPRESSOR_TASK_MSG_TYPE_UPDATE_STATUS;
            log_assert_bool_false(xQueueSend(compressor_task_msg_q_id,&msg_temp,COMPRESSOR_TASK_PUT_MSG_TIMEOUT) == pdPASS);    
        }

        /*压缩机工作时间统计*/
        if (msg_recv.head.id == COMPRESSOR_TASK_MSG_RUN_TIME_UPDATE){ 
            report_task_message_t report_msg;
            report_msg.head.id = REPORT_TASK_MSG_COMPRESSOR_RUN_TIME;
            report_msg.content.run_time = compressor.run_time;
            log_assert_bool_false(xQueueSend(report_task_msg_q_id,&report_msg,COMPRESSOR_TASK_PUT_MSG_TIMEOUT) == pdPASS);    
            compressor.run_time = 0;
            

        }


    }
    }
 }


