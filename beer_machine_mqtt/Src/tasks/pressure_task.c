#include "cmsis_os.h"
#include "tasks_init.h"
#include "adc_task.h"
#include "stdio.h"
#include "alarm_task.h"
#include "pressure_task.h"
#include "log.h"

osThreadId   pressure_task_handle;
osMessageQId pressure_task_msg_q_id;


typedef struct
{
  uint8_t value;
  bool    change;
  int8_t  dir;
}pressure_t;

static pressure_t  pressure;


static uint8_t get_pressure(uint16_t adc)
{
  float v,p;
  uint8_t int_pressure;
 
  if(adc == ADC_TASK_ADC_ERR_VALUE){
     return PRESSURE_ERR_VALUE_SENSOR;
  }
  v= (adc * PRESSURE_SENSOR_REFERENCE_VOLTAGE)/PRESSURE_SENSOR_ADC_VALUE_MAX;   
  p= (v - PRESSURE_SENSOR_OUTPUT_VOLTAGE_MIN) * (PRESSURE_SENSOR_INPUT_PA_MAX - PRESSURE_SENSOR_INPUT_PA_MIN)/(PRESSURE_SENSOR_OUTPUT_VOLTAGE_MAX - PRESSURE_SENSOR_OUTPUT_VOLTAGE_MIN) + PRESSURE_SENSOR_INPUT_PA_MIN;
 
  /*换算成 kg/cm2*/
  p= (p / PA_VALUE_PER_1KG_CM2);
  
  log_debug("v:%d mv p:%.4fkg/cm2.\r\n",(uint16_t)(v*1000),p);
  
  /*放大10倍 按整数计算*/
  p *= 10;
  
  /*减去外部大气压*/
  if(p > PRESSURE_VALUE_IN_KG_CM2_ERR_MAX ){
     return PRESSURE_ERR_VALUE_SENSOR;  
  }
 
  if(p < PRESSURE_VALUE_IN_KG_CM2_ERR_MIN ){
     return PRESSURE_ERR_VALUE_SENSOR;  
  }
 
  p = p < PRESSURE_VALUE_STANDARD_ATM ? 0 : p - PRESSURE_VALUE_STANDARD_ATM;
 
  int_pressure = (uint8_t)p;
  /*四舍五入*/
  return int_pressure + (p - int_pressure >= 0.50 ? 1 : 0 );
}

void pressure_task(void const *argument)
{
  uint8_t p;
  uint16_t adc;
  osEvent os_msg;
  osStatus status;
  
  pressure_task_msg_t msg;
  alarm_task_msg_t    alarm_msg; 
  
  /*等待任务同步*/
  /*
  xEventGroupSync(tasks_sync_evt_group_hdl,TASKS_SYNC_EVENT_PRESSURE_TASK_RDY,TASKS_SYNC_EVENT_ALL_TASKS_RDY,osWaitForever);
  log_debug("pressure task sync ok.\r\n");
  */
  /*上电默认值*/
  pressure.value = 88;
  
  while(1){
  os_msg = osMessageGet(pressure_task_msg_q_id,PRESSURE_TASK_MSG_WAIT_TIMEOUT);
  if(os_msg.status == osEventMessage){
     msg = *(pressure_task_msg_t*)&os_msg.value.v;
  
     /*压力ADC完成消息处理*/
     if(msg.type == PRESSURE_TASK_MSG_ADC_COMPLETED){
        adc = msg.value;
        p = get_pressure(adc); 
        if(p == PRESSURE_ERR_VALUE_SENSOR){
           p = ALARM_TASK_PRESSURE_ERR_VALUE;
        }
        /*如果压力值无变化继续*/
        if(p == pressure.value){
           continue;  
        }
       /*如果压力值有变化处理下面步骤*/
        if(p > pressure.value){
           pressure.dir += 1;    
        }else if(p < pressure.value){
           pressure.dir -= 1;      
        }  
        /*当满足条件时 接受数据变化*/
        if(pressure.dir > PRESSURE_TASK_PRESSURE_CHANGE_CNT ||
           pressure.dir < -PRESSURE_TASK_PRESSURE_CHANGE_CNT){
           pressure.value = p;
           pressure.change = true; 
  
        }
     
        /*状态变化处理*/
        if(pressure.change == true){
           if(pressure.value == ALARM_TASK_PRESSURE_ERR_VALUE){
             log_error("pressure err.code:E0.\r\n");
           }else{
             log_info("pressure change. dir :%d  value:%.2f kg/cm2.\r\n" ,pressure.dir,p / 10.0); 
           }
           pressure.change = false;
           pressure.dir = 0;
           /*报警消息*/
           alarm_msg.type = ALARM_TASK_MSG_PRESSURE;
           alarm_msg.value = pressure.value;
           status = osMessagePut(alarm_task_msg_q_id,*(uint32_t*)&alarm_msg,PRESSURE_TASK_PUT_MSG_TIMEOUT);  
           if(status !=osOK){
              log_error("put alarm msg error:%d\r\n",status); 
           } 
        } 
      }
   }
 }
}