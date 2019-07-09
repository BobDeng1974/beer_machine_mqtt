#include "cmsis_os.h"
#include "led.h"
#include "tasks_init.h"
#include "compressor_task.h"
#include "alarm_task.h"
#include "pressure_task.h"
#include "temperature_task.h"
#include "display_task.h"
#include "log.h"

osThreadId   display_task_handle;
osMessageQId display_task_msg_q_id;
osTimerId    display_timer_id;


typedef struct
{
  int16_t value;
  bool    blink;
  bool    point;
}display_unit_t;

typedef struct
{
  display_unit_t temperature;
  display_unit_t pressure;
  display_unit_t capacity;
  display_unit_t wifi;
  display_unit_t compressor;
bool is_turn_on;
bool is_update;
}display_t;

static display_t display = {
.wifi.value = 3,
.wifi.blink = true
};

typedef enum
{
DISPLAY_STATUS_ON,
DISPLAY_STATUS_OFF
}display_status_t;

static void display_timer_expired(void const *argument);

static void display_timer_init()
{
 osTimerDef(display_timer,display_timer_expired);
 display_timer_id=osTimerCreate(osTimer(display_timer),osTimerOnce,0);
 log_assert(display_timer_id);
}

static void display_timer_start(uint16_t timeout)
{
 osTimerStart(display_timer_id,timeout);  
}
/*
static void display_timer_stop(void)
{
 osTimerStop(display_timer_id);  
 log_debug("显示定时器停止.\r\n");  
}
*/
static void display_timer_expired(void const *argument)
{
  osStatus status;
  static display_status_t display_status = DISPLAY_STATUS_ON;
  display_task_msg_t blink_msg;
  /*点亮超时，就发送熄灭消息*/
  if(display_status == DISPLAY_STATUS_ON){
     blink_msg.type = DISPLAY_TASK_MSG_BLINK_OFF;
     display_status = DISPLAY_STATUS_OFF;
 }else{
     blink_msg.type = DISPLAY_TASK_MSG_BLINK_ON;
     display_status = DISPLAY_STATUS_ON;
 }
 status = osMessagePut(display_task_msg_q_id,*(uint32_t*)&blink_msg,DISPLAY_TASK_PUT_MSG_TIMEOUT);
 if(status !=osOK){
    log_error("put blink msg error:%d\r\n",status); 
 }
}

/*液位容量显示算法*/
static uint8_t display_task_get_capacity_level(uint8_t capacity)
{
    uint8_t level;
    if (capacity >= 18) {
        level = 5;
    }else if (capacity >= 14) {
        level = 4;
    }else if (capacity >= 10) {
        level = 3;
    }else if (capacity >= 6) {
        level = 2;
    }else if (capacity >= 2) {
        level = 1;
    }else {
        level = 0;
    }

    return level;
}

void display_task(void const *argument)
{
  osEvent os_msg;
  display_task_msg_t msg;
 
  /*等待显示芯片上电稳定*/;
  osDelay(200);
  led_display_init();
 
  led_display_temperature_unit(LED_DISPLAY_ON);
  led_display_pressure_unit(LED_DISPLAY_ON);
  led_display_capacity_unit(LED_DISPLAY_ON);
 
  led_display_temperature_icon(LED_DISPLAY_ON);
  led_display_pressure_icon(LED_DISPLAY_ON);
  led_display_capacity_icon_frame(LED_DISPLAY_ON);
 
  led_display_wifi_icon(LED_DISPLAY_ON);
  led_display_compressor_icon(LED_DISPLAY_ON,LED_DISPLAY_ON);
  led_display_brand_icon(LED_DISPLAY_ON);
  
 
  /*依次显示 8-7....0.*/
  for(int8_t dis=88;dis >=0 ;dis-=11){
      led_display_temperature(dis);
      led_display_pressure(dis);
      led_display_capacity(dis);
 
      led_display_refresh();
      osDelay(120);
  }
  /*默认显示容积0L*/
  led_display_capacity(display.capacity.value);
  led_display_capacity_icon_level(display_task_get_capacity_level(display.capacity.value));
 
  /*等待任务同步*/
  /*
  xEventGroupSync(tasks_sync_evt_group_hdl,TASKS_SYNC_EVENT_DISPLAY_TASK_RDY,TASKS_SYNC_EVENT_ALL_TASKS_RDY,osWaitForever);
  log_debug("display task sync ok.\r\n");
  */
  display_timer_init();
  display_timer_start(DISPLAY_TASK_BLINK_ON_TIMER_TIMEOUT);
  
  while(1){
  os_msg = osMessageGet(display_task_msg_q_id,DISPLAY_TASK_MSG_WAIT_TIMEOUT);
  if(os_msg.status == osEventMessage){
     msg = *(display_task_msg_t*)&os_msg.value.v;
     /*温度消息*/
     if(msg.type == DISPLAY_TASK_MSG_TEMPERATURE){
        display.temperature.value = (uint8_t)msg.value == ALARM_TASK_TEMPERATURE_ERR_VALUE ? (uint8_t)msg.value : (int8_t)(msg.value);
        display.temperature.blink = msg.blink;  
        led_display_temperature(display.temperature.value);
        display.is_update = true;
     }
     /*压力消息*/
     if(msg.type == DISPLAY_TASK_MSG_PRESSURE){
        display.pressure.value = msg.value;   
        display.pressure.blink = msg.blink;
        display.pressure.point = msg.point;
        led_display_pressure(display.pressure.value);
        led_display_pressure_point(display.pressure.point ?  LED_DISPLAY_ON : LED_DISPLAY_OFF); 
        display.is_update = true;
     }
  
     /*容量消息*/
     if(msg.type == DISPLAY_TASK_MSG_CAPACITY){
        display.capacity.value = msg.value;
        display.capacity.blink = msg.blink; 
        led_display_capacity(display.capacity.value);
        led_display_capacity_icon_level(display_task_get_capacity_level(display.capacity.value));
        display.is_update = true;
     }
    
     /*wifi消息*/
     if(msg.type == DISPLAY_TASK_MSG_WIFI){
        display.wifi.value = msg.value; 
        display.wifi.blink = msg.blink; 
        led_display_wifi_icon(display.wifi.value);
        display.is_update = true;
     }
  
     /*压缩机消息*/
     if(msg.type == DISPLAY_TASK_MSG_COMPRESSOR){
        display.compressor.blink = msg.blink; 
        led_display_compressor_icon(LED_DISPLAY_ON,LED_DISPLAY_ON);
        display.is_update = true;
     }
  
  /*闪烁关闭消息*/
  if(msg.type == DISPLAY_TASK_MSG_BLINK_OFF){

     if(display.temperature.blink == true){
        led_display_temperature(LED_DISPLAY_OFF);
        display.is_update = true;
     }
  
     if(display.pressure.blink == true){
        led_display_pressure(LED_DISPLAY_OFF);
        if(display.pressure.point == true ){
           led_display_pressure_point(LED_DISPLAY_OFF); 
        }
        display.is_update = true;
     }
  
     if(display.capacity.blink == true){
        led_display_capacity(LED_DISPLAY_OFF);
        led_display_capacity_icon_level(LED_DISPLAY_OFF);
        display.is_update = true;
     }
  
     if(display.wifi.blink == true){
        led_display_wifi_icon(LED_DISPLAY_OFF);
        display.is_update = true;
     }
  
     if(display.compressor.blink == true){
        led_display_compressor_icon(LED_DISPLAY_OFF,LED_DISPLAY_OFF);
        display.is_update = true;
     }
       display_timer_start(DISPLAY_TASK_BLINK_OFF_TIMER_TIMEOUT);
    }
  
  /*闪烁打开消息*/
  if(msg.type == DISPLAY_TASK_MSG_BLINK_ON){
  
     if(display.temperature.blink == true){
        led_display_temperature(display.temperature.value);
        display.is_update = true;
     }
  
    if(display.pressure.blink == true){
       led_display_pressure(display.pressure.value);
       if(display.pressure.point == true){
          led_display_pressure_point(LED_DISPLAY_ON);
       }
       display.is_update = true;
    }
  
    if(display.capacity.blink == true){
       led_display_capacity(display.capacity.value);
       led_display_capacity_icon_level(display_task_get_capacity_level(display.capacity.value));
       display.is_update = true;
    }
    
    if(display.wifi.blink == true){
       led_display_wifi_icon(display.wifi.value);
       display.is_update = true;
    }
  
    if(display.compressor.blink == true){
       led_display_compressor_icon(LED_DISPLAY_ON,LED_DISPLAY_ON);  
       display.is_update = true;
    }
    
    display_timer_start(DISPLAY_TASK_BLINK_ON_TIMER_TIMEOUT);
  }
 
  
  /*更新上述消息内容到芯片*/
  if (display.is_update == true){
      display.is_update = false;  
      /*刷新到芯片RAM*/
      led_display_refresh();  
   }
  
  }
  }
}
   
   
  
