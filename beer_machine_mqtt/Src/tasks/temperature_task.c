#include "cmsis_os.h"
#include "tasks_init.h"
#include "stdio.h"
#include "adc_task.h"
#include "alarm_task.h"
#include "compressor_task.h"
#include "temperature_task.h"
#include "display_task.h"
#include "log.h"


osThreadId   temperature_task_handle;
osMessageQId temperature_task_msg_q_id;


/*温度传感器型号：LAT5061G3839G B值：3839K*/
static int16_t const t_r_map[][2]={
  {-12,12224},{-11,11577},{-10,10968},{-9,10394},{-8,9854},{-7,9344},{-6,8864},{-5,8410},{-4,7983},{-3 ,7579},
  {-2 ,7199} ,{-1,6839}  ,{0,6499}   ,{1,6178 } ,{2,5875 },{3,5588 },{4,5317} ,{5,5060} ,{6,4817} ,{7,4587}  ,
  {8,4370}   ,{9,4164}   ,{10,3969}  ,{11,3784} ,{12,3608},{13,3442},{14,3284},{15,3135},{16,2993},{17,2858} ,
  {18,2730}  ,{19,2609}  ,{20,2494}  ,{21,2384} ,{22,2280},{23,2181},{24,2087},{25,1997},{26,1912},{27,1831} ,
  {28,1754}  ,{29,1680}  ,{30,1610}  ,{31,1543} ,{32,1480},{33,1419},{34,1361},{35,1306},{36,1254},{37,1204} ,
  {38,1156}  ,{39,1110}  ,{40,1067}  ,{41,1025} ,{42,985} ,{43,947} ,{44,911} ,{45,876} ,{46,843} ,{47,811}  ,
  {48,781}   ,{49,752}   ,{50,724}   ,{51,697}  ,{52,672} ,{53,647} ,{54,624} ,{55,602} ,{56,580} ,{57,559}
};
#define  HIGH_ERR_R        11577
#define  LOW_ERR_R         580
#define  TR_MAP_IDX_MIN    0
#define  TR_MAP_IDX_MAX    68

typedef struct
{
  int8_t  value;
  int8_t  dir;
  bool    change;
}temperature_t;


static temperature_t   temperature;
               
static uint32_t get_r(uint16_t adc)
{
  float t_sensor_r;
  t_sensor_r = (TEMPERATURE_SENSOR_SUPPLY_VOLTAGE*TEMPERATURE_SENSOR_ADC_VALUE_MAX*TEMPERATURE_SENSOR_BYPASS_RES_VALUE)/(adc*TEMPERATURE_SENSOR_REFERENCE_VOLTAGE)-TEMPERATURE_SENSOR_BYPASS_RES_VALUE;
  return (uint32_t)t_sensor_r;
}
/*获取浮点温度值*/
static float get_fine_t(uint32_t r,uint8_t idx)
{
  uint32_t r1,r2;

  float t;

  r1 = t_r_map[idx][1];
  r2 = t_r_map[idx + 1][1];

  t = t_r_map[idx][0] + (r1 - r) * 1.0 /(r1 - r2) + TEMPERATURE_COMPENSATION_VALUE;
  
  log_debug("temperature: %.2f C.\r\n",t);
   
  return t;  
}
/*获取四舍五入整数温度值*/
static int16_t get_approximate_t(float t_float)
{
  int16_t t = (int16_t)t_float;

  t_float -= t;

  if(t_float >= 0.5 ) {
     t += 1; 
  } else if (t_float <= -0.5){
     t -= 1;
  }
  
  return t;  
}




int16_t get_t(uint16_t adc)
{
 uint32_t r; 
 uint8_t mid=0;
 int low = TR_MAP_IDX_MIN;  
 int high =TR_MAP_IDX_MAX; 
 
 if(adc == ADC_TASK_ADC_ERR_VALUE){
 return TEMPERATURE_ERR_VALUE_SENSOR;
 }
 r=get_r(adc);
 
 if(r <= LOW_ERR_R){
    log_error("NTC 低过最高温度阻值！r=%d\r\n",r); 
    return TEMPERATURE_ERR_VALUE_SENSOR;
 }else if(r >= HIGH_ERR_R){
    log_error("NTC 高过最低温度阻值！r=%d\r\n",r); 
 return TEMPERATURE_ERR_VALUE_SENSOR;
 }
 
 while (low <= high) {  
    mid = (low + high) / 2;  
 if(r > t_r_map[mid][1]){
    if(r <= t_r_map[mid-1][1]){
       /*返回指定精度的温度*/
       return get_approximate_t(get_fine_t(r, mid - 1));
    }else{
       high = mid - 1;  
    }
 }else{
   if(r > t_r_map[mid+1][1]){
     /*返回带有温度补偿值的温度*/
     return get_approximate_t(get_fine_t(r, mid));
   } else{
      low = mid + 1;   
   }
 }  
}

 return TEMPERATURE_ERR_VALUE_SENSOR;
}


void temperature_task(void const *argument)
{
  uint16_t bypass_r_adc;
  int16_t  t;
  
  osEvent  os_msg;
  osStatus status;
  temperature_task_msg_t msg;

  alarm_task_msg_t      alarm_msg;
   
  /*等待任务同步*/
  /*
  xEventGroupSync(tasks_sync_evt_group_hdl,TASKS_SYNC_EVENT_TEMPERATURE_TASK_RDY,TASKS_SYNC_EVENT_ALL_TASKS_RDY,osWaitForever);
  log_debug("temperature task sync ok.\r\n");
  */
  temperature.value = 88;

  while(1){
  os_msg = osMessageGet(temperature_task_msg_q_id,TEMPERATURE_TASK_MSG_WAIT_TIMEOUT);
  if(os_msg.status == osEventMessage){
     msg = *(temperature_task_msg_t*)&os_msg.value.v;
 
     /*温度ADC转换完成消息处理*/
     if(msg.type == TEMPERATURE_TASK_MSG_ADC_COMPLETED){
        bypass_r_adc = msg.value;
        t = get_t(bypass_r_adc);  
        /*判断是否在报警范围*/ 
       if(t == TEMPERATURE_ERR_VALUE_SENSOR || t > TEMPERATURE_ALARM_VALUE_MAX || t < TEMPERATURE_ALARM_VALUE_MIN){
          t = ALARM_TASK_TEMPERATURE_ERR_VALUE;  
        }     
   
       if( t == (uint8_t)temperature.value){
          continue;  
       } 
       
       if(t > temperature.value){
          temperature.dir += 1;    
       }else if(t < temperature.value){
          temperature.dir -= 1;      
       }
       /*当满足条件时 接受数据变化*/
       if(temperature.dir > TEMPERATURE_TASK_TEMPERATURE_CHANGE_CNT ||
          temperature.dir < -TEMPERATURE_TASK_TEMPERATURE_CHANGE_CNT){
          temperature.value = t;
          temperature.change = true;
       }
       if(temperature.change == true){
         
          if((uint8_t)temperature.value == ALARM_TASK_TEMPERATURE_ERR_VALUE){
             log_error("temperature err.code:E1.\r\n");
          }else{
             log_info("teperature changed dir:%d value:%d C.\r\n",temperature.dir,temperature.value);
          }
          temperature.change = false;  
          temperature.dir = 0;
          
          /*报警消息*/
          alarm_msg.type = ALARM_TASK_MSG_TEMPERATURE;
          alarm_msg.value = temperature.value;

          status = osMessagePut(alarm_task_msg_q_id,*(uint32_t*)&alarm_msg,TEMPERATURE_TASK_PUT_MSG_TIMEOUT);
          if(status !=osOK){
             log_error("put display t msg error:%d\r\n",status); 
          } 
       }
     }
   }
  }
  }