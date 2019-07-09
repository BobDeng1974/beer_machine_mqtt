#include "cmsis_os.h"
#include "tasks_init.h"
#include "beer_machine.h"
#include "temperature_task.h"
#include "pressure_task.h"
#include "alarm_task.h"
#include "compressor_task.h"
#include "display_task.h"
#include "report_task.h"
#include "device_config.h"
#include "log.h"

osThreadId   alarm_task_handle;
osMessageQId alarm_task_msg_q_id;
osTimerId    alarm_timer_id;

typedef struct
{
  bool     alarm;  
  bool     warning;
  bool     config;
  uint8_t   value;
  uint8_t   high;
  uint8_t   low;
}alarm_unit_t;

typedef struct
{
  bool         is_pwr_on;
  alarm_unit_t temperature;
  alarm_unit_t pressure;
  alarm_unit_t capacity;
}alarm_t;

static  alarm_t  alarm;


/*蜂鸣器通电断电时间 单位:ms*/
#define  ALARM_TASK_TIMER_TIMEOUT             1000

#define  ALARM_TASK_PUT_MSG_TIMEOUT           5
#define  ALARM_TASK_MSG_WAIT_TIMEOUT          osWaitForever


static void alarm_timer_expired(void const *argument);

static void alarm_buzzer_pwr_turn_on();
static void alarm_buzzer_pwr_turn_off();




static void alarm_buzzer_pwr_turn_on()
{
 bsp_buzzer_ctrl_on();
}

static void alarm_buzzer_pwr_turn_off()
{
 bsp_buzzer_ctrl_off();
}

static void alarm_timer_init()
{
 osTimerDef(alarm_timer,alarm_timer_expired);
 alarm_timer_id=osTimerCreate(osTimer(alarm_timer),osTimerPeriodic,0);
 log_assert(alarm_timer_id);
}

static void alarm_timer_start(void)
{
 osTimerStart(alarm_timer_id,ALARM_TASK_TIMER_TIMEOUT);  
}

static void alarm_timer_expired(void const *argument)
{
 osStatus status;
 alarm_task_msg_t alarm_timeout_msg;
 
 alarm_timeout_msg.type = ALARM_TASK_MSG_TIMER_TIMEOUT;
 status = osMessagePut(alarm_task_msg_q_id,*(uint32_t*)&alarm_timeout_msg,ALARM_TASK_PUT_MSG_TIMEOUT);  
 if(status !=osOK){
 log_error("put alarm timeout msg error:%d\r\n",status); 
 } 
}

static int alarm_task_send_msg(osMessageQId msg_id,uint32_t msg)
{
  osStatus status;
  status = osMessagePut(msg_id,msg,ALARM_TASK_PUT_MSG_TIMEOUT);
  if(status != osOK){
     log_error("alarm task put msg err.\r\n");
     return -1;
  }  
  return 0;
}


void alarm_task(void const *argument)
{
  osEvent  os_msg;

  
  alarm_task_msg_t       msg;
  alarm_task_msg_t       alarm_msg;
  display_task_msg_t     display_msg;
  compressor_task_msg_t  compressor_msg;
  report_task_msg_t      report_msg;
    
  /*等待任务同步*/
  /*
  xEventGroupSync(tasks_sync_evt_group_hdl,TASKS_SYNC_EVENT_ALARM_TASK_RDY,TASKS_SYNC_EVENT_ALL_TASKS_RDY,osWaitForever);
  log_debug("alarm task sync ok.\r\n");
  */
  alarm_buzzer_pwr_turn_off();
  alarm.is_pwr_on = false;
  
  alarm_timer_init();
  alarm_timer_start();

  while(1){ 
  os_msg = osMessageGet(alarm_task_msg_q_id,ALARM_TASK_MSG_WAIT_TIMEOUT); 
  if(os_msg.status == osEventMessage){
     msg = *(alarm_task_msg_t*)&os_msg.value.v;

     /*温度值消息处理*/ 
     if(msg.type == ALARM_TASK_MSG_TEMPERATURE){
        alarm.temperature.value = msg.value;
        if(alarm.temperature.value == ALARM_TASK_TEMPERATURE_ERR_VALUE){
           alarm.temperature.alarm = true;
                
           display_msg.blink = true;
           display_msg.point = false;
           
           compressor_msg.type = COMPRESSOR_TASK_MSG_TEMPERATURE_ERR;            
           
           report_msg.type = REPORT_TASK_MSG_TEMPERATURE_ERR;     
                              
        /*非温度故障处理*/    
        }else {     
           /*如果先前是故障状态*/
           if(alarm.temperature.alarm == true){
              alarm.temperature.alarm = false;
              report_msg.type = REPORT_TASK_MSG_TEMPERATURE_ERR_CLEAR;     
           }else{
              report_msg.type = REPORT_TASK_MSG_TEMPERATURE_VALUE;  
           }
           if(alarm.temperature.config == true && ((int8_t)alarm.temperature.value > (int8_t)alarm.temperature.high || (int8_t)alarm.temperature.value < (int8_t)alarm.temperature.low)){
             alarm.temperature.warning = true;             
             display_msg.blink = true;
           }else{
             alarm.temperature.warning = false;             
             display_msg.blink = false;
           }             
          compressor_msg.type = COMPRESSOR_TASK_MSG_TEMPERATURE_VALUE;                                 
        }
        
       display_msg.type = DISPLAY_TASK_MSG_TEMPERATURE;
       display_msg.value = alarm.temperature.value;
       compressor_msg.value = alarm.temperature.value; 
       report_msg.value = alarm.temperature.value;  
       /*压缩机温度消息*/
       alarm_task_send_msg(compressor_task_msg_q_id,*(uint32_t*)&compressor_msg);
       /*显示屏温度消息*/
       alarm_task_send_msg(display_task_msg_q_id,*(uint32_t*)&display_msg);
       /*上报任务温度消息*/
       alarm_task_send_msg(report_task_msg_q_id,*(uint32_t*)&report_msg);         
     }
     
     /*压力值消息处理*/ 
     if(msg.type == ALARM_TASK_MSG_PRESSURE){
        alarm.pressure.value = msg.value;
        /*故障处理*/
        if(alarm.pressure.value == ALARM_TASK_PRESSURE_ERR_VALUE){
           alarm.pressure.alarm = true;
           
           display_msg.blink = true;
           display_msg.point = false;
           
           report_msg.type = REPORT_TASK_MSG_PRESSURE_ERR;                                  
        /*非故障处理*/    
        }else {        
           /*如果先前是故障状态*/
           if(alarm.pressure.alarm == true){
              alarm.pressure.alarm = false;
              report_msg.type = REPORT_TASK_MSG_PRESSURE_ERR_CLEAR;     
           }else{
              report_msg.type = REPORT_TASK_MSG_PRESSURE_VALUE;      
           }
             
           if(alarm.pressure.config == true && (alarm.pressure.value > alarm.pressure.high || alarm.pressure.value < alarm.pressure.low)){
             alarm.pressure.warning = true;             
             display_msg.blink = true;       
           }else{
             alarm.pressure.warning = false;             
             display_msg.blink = false;         
           }
           display_msg.point = true;
        }
        
                             
        display_msg.type = DISPLAY_TASK_MSG_PRESSURE;
        display_msg.value = alarm.pressure.value;
           
        report_msg.value = alarm.pressure.value;  
        /*显示屏压力消息*/
        alarm_task_send_msg(display_task_msg_q_id,*(uint32_t*)&display_msg);
        /*上报任务压力消息*/
        alarm_task_send_msg(report_task_msg_q_id,*(uint32_t*)&report_msg);  
     }
     
     /*液位值消息处理*/ 
     if(msg.type == ALARM_TASK_MSG_CAPACITY){
        alarm.capacity.value = msg.value;
        /*故障处理*/
        if(alarm.capacity.value == ALARM_TASK_CAPACITY_ERR_VALUE){
           alarm.capacity.alarm = true;
           display_msg.blink = true;
           
           report_msg.type = REPORT_TASK_MSG_CAPACITY_ERR;                                   
        /*非故障处理*/    
        }else {        
           /*如果先前是故障状态*/
           if(alarm.capacity.alarm == true){
              alarm.capacity.alarm = false;
              report_msg.type =REPORT_TASK_MSG_CAPACITY_ERR_CLEAR;       
           }else{
              report_msg.type = REPORT_TASK_MSG_CAPACITY_VALUE;     
           }
             
           if(alarm.capacity.config == true && (alarm.capacity.value > alarm.capacity.high || alarm.capacity.value < alarm.capacity.low)){
             alarm.pressure.warning = true;             
             display_msg.blink = true;       
           }else{
             alarm.capacity.warning = false;             
             display_msg.blink = false;         
           }  
        }
       display_msg.type = DISPLAY_TASK_MSG_CAPACITY;
       display_msg.value = alarm.capacity.value;
        
       report_msg.value = alarm.capacity.value; 
       /*显示屏容量消息*/
       alarm_task_send_msg(display_task_msg_q_id,*(uint32_t*)&display_msg);
       /*上报任务容量消息*/
       alarm_task_send_msg(report_task_msg_q_id,*(uint32_t*)&report_msg);     
     } 
     
   /*压缩机的温度配置消息处理*/ 
   if(msg.type == ALARM_TASK_MSG_TEMPERATURE_CONFIG){
      int8_t temp_high,temp_low;
      /*判断温度的合法性*/
      temp_high = (int8_t)(msg.reserved >> 8);
      temp_low = (int8_t)(msg.reserved & 0xFF);
      if(temp_high > DEFAULT_COMPRESSOR_HIGH_TEMPERATURE_LIMIT || temp_low < DEFAULT_COMPRESSOR_LOW_TEMPERATURE_LIMIT){
        log_error("config compressor t is invalid. high:%d low:%d \r\n",temp_high,temp_low);
      }else{
        /*这个是温度报警的值*/
        alarm.temperature.high = temp_high + 1;
        alarm.temperature.low = temp_low - 1;  
        alarm.temperature.config = true;
        if(alarm.temperature.value != ALARM_TASK_TEMPERATURE_ERR_VALUE){
           /*发给自己的温度报警配置消息，以便更新报警状态*/
           alarm_msg.type = ALARM_TASK_MSG_TEMPERATURE;
           alarm_msg.value = alarm.temperature.value;
           alarm_task_send_msg(alarm_task_msg_q_id,*(uint32_t*)&alarm_msg);
    
           /*这个是压缩机工作的温度值配置*/
           compressor_task_msg_t compressor_msg;
           compressor_msg.type = COMPRESSOR_TASK_MSG_TEMPERATURE_CONFIG;
           compressor_msg.reserved = temp_high << 8 | temp_low ;
           alarm_task_send_msg(compressor_task_msg_q_id,*(uint32_t*)&compressor_msg);
        }
     }     
   }   
    /*压力配置消息处理*/ 
   if(msg.type == ALARM_TASK_MSG_PRESSURE_CONFIG){
      alarm.pressure.high = msg.reserved >> 8;
      alarm.pressure.low =  msg.reserved & 0xFF;
      alarm.pressure.config = true;
      if(alarm.pressure.value != ALARM_TASK_PRESSURE_ERR_VALUE){
         /*发给自己的压力消息*/
         alarm_msg.type = ALARM_TASK_MSG_PRESSURE;
         alarm_msg.value = alarm.pressure.value;
         alarm_task_send_msg(alarm_task_msg_q_id,*(uint32_t*)&alarm_msg);
      }
   }  

   /*容量配置消息处理*/ 
   if(msg.type == ALARM_TASK_MSG_CAPACITY_CONFIG){
      alarm.capacity.high = msg.reserved >> 8;
      alarm.capacity.low =  msg.reserved & 0xFF;
      alarm.capacity.config = true;
      if(alarm.capacity.value != ALARM_TASK_CAPACITY_ERR_VALUE){
         /*发给自己的容量消息*/
         alarm_msg.type = ALARM_TASK_MSG_CAPACITY;
         alarm_msg.value = alarm.capacity.value;
         alarm_task_send_msg(alarm_task_msg_q_id,*(uint32_t*)&alarm_msg);
      }
   }  
   
   /*压缩机LOCK配置消息处理*/ 
   if(msg.type == ALARM_TASK_MSG_COMPRESSOR_LOCK_CONFIG){
      compressor_task_msg_t compressor_msg;
      compressor_msg.type = COMPRESSOR_TASK_MSG_LOCK_CONFIG;
      compressor_msg.value = msg.value;
      /*发给压缩机lock配置消息*/
      alarm_task_send_msg(compressor_task_msg_q_id,*(uint32_t*)&compressor_msg);
   }  
   
  /*蜂鸣器报警消息处理*/ 
  if(msg.type == ALARM_TASK_MSG_TIMER_TIMEOUT){
  /*如果有任意一个报警状态存在 就继续操作蜂鸣器*/
  
  if(alarm.temperature.alarm == true || alarm.pressure.alarm == true || alarm.capacity.alarm == true){
  if(alarm.is_pwr_on == true){
  alarm.is_pwr_on = false;
  alarm_buzzer_pwr_turn_off();
  }else{
  alarm.is_pwr_on = true;
  alarm_buzzer_pwr_turn_on();
  }
  }else if(alarm.is_pwr_on != false){
  alarm.is_pwr_on = false;
  alarm_buzzer_pwr_turn_off();     
  }
  
  }

  }
  }
}