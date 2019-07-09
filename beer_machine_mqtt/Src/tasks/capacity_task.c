#include "cmsis_os.h"
#include "stdio.h"
#include "serial.h"
#include "tasks_init.h"
#include "alarm_task.h"
#include "pressure_task.h"
#include "display_task.h"
#include "report_task.h"
#include "capacity_task.h"
#include "log.h"

osThreadId   capacity_task_handle;
osMessageQId capacity_task_msg_q_id;

int capacity_serial_handle;

static serial_hal_driver_t *capacity_serial_uart_driver = &st_serial_uart_hal_driver;


typedef struct
{
  uint16_t high;
  uint8_t  value;
  int8_t   dir;
  uint8_t  err_cnt;
  bool     change;
}beer_capacity_t;

static beer_capacity_t beer_capacity;

/* 函数名：gsm_m6312_serial_hal_init
*  功能：  m6312 2g模块执行硬件和串口初始化
*  参数：  无 
*  返回：  GSM_ERR_OK：成功 其他：失败
*/
static int capacity_serial_hal_init(void)
{
	int rc ;
    rc = serial_create(&capacity_serial_handle,CAPACITY_BUFFER_SIZE,CAPACITY_BUFFER_SIZE);
    if (rc != 0) {
    	log_error("capacity create serial hal err.\r\n");
    	return -1;
    }
    log_debug("capacity create serial hal ok.\r\n");
   
    rc = serial_register_hal_driver(capacity_serial_handle,capacity_serial_uart_driver);
	if (rc != 0) {
      log_error("capacity register serial hal driver err.\r\n");
      return -1;
    }
	log_debug("capacity register serial hal driver ok.\r\n");
	rc = serial_open(capacity_serial_handle,CAPACITY_SERIAL_PORT,CAPACITY_SERIAL_BAUDRATES,CAPACITY_SERIAL_DATA_BITS,CAPACITY_SERIAL_STOP_BITS);
	if (rc != 0) {
    	log_error("capacity open serial hal err.\r\n");
    	return -1;
  	}
	log_debug("capacity open serial hal ok.\r\n");
    
    return 0;
}

static uint16_t capacity_task_get_high(void)
{
  int select_size,read_size,read_total = 0;
  char buffer[4];
  uint16_t high;
  
  /*传感器数据传输间隔2S 设置3s超时*/
  serial_flush(capacity_serial_handle);
  select_size = serial_select(capacity_serial_handle,3000);
  if(select_size <= 0){
     log_error("capacity sensor no data.\r\n");
     return CAPACITY_TASK_SENSOR_ERR_VALUE;
  }
  /*读到数据处理*/
  do{
  /*数据量异常处理*/
  if(select_size + read_total > 4){
     log_error("capacity sensor data too large.\r\n");
     return CAPACITY_TASK_SENSOR_ERR_VALUE;    
  }
  read_size = serial_read(capacity_serial_handle,buffer + read_total,select_size); 
  if(read_size < 0){
     return CAPACITY_TASK_SENSOR_ERR_VALUE;
  }
  read_total +=read_size; 
  select_size = serial_select(capacity_serial_handle,100);
  if(select_size < 0){
     return CAPACITY_TASK_SENSOR_ERR_VALUE;  
  }
  }while(select_size != 0);
  
  /*调试输出*/
  for(uint8_t i = 0 ; i < read_total ; i++ ) {
      log_array("%2X",buffer[i]);
  }

  /*协议头错误 或者校验错误*/
  if(read_total != 4 || buffer[0] != 0xff || (( buffer[0] + buffer[1] + buffer[2]) & 0xff) != buffer[3]){
      log_error("液位传感器协议数据错误.\r\n");
      return CAPACITY_TASK_SENSOR_ERR_VALUE;  
  }



  /*计算液位*/
  high = (buffer[1] * 256 + buffer[2]) & 0xFFFF;

  return high;
}



void capacity_task(void const *argument)
{
  osStatus status;
  float    capacity;
  uint16_t int_capacity;

  alarm_task_msg_t      alarm_msg; 

  /*串口驱动初始化*/
  capacity_serial_hal_init();
  /*等待任务同步*/
  /*
  xEventGroupSync(tasks_sync_evt_group_hdl,TASKS_SYNC_EVENT_CAPACITY_TASK_RDY,TASKS_SYNC_EVENT_ALL_TASKS_RDY,osWaitForever);
  log_debug("capacity task sync ok.\r\n");
  */
  /*上电默认值*/
  beer_capacity.value = 88;
  int_capacity = 88;
  while(1){

  beer_capacity.high = capacity_task_get_high(); 

  /*如果传感器异常一定时间就设定错误*/
  if(beer_capacity.high == CAPACITY_TASK_SENSOR_ERR_VALUE){
     beer_capacity.err_cnt ++;
     if(beer_capacity.err_cnt >= CAPACITY_TASK_ERR_CNT){
        beer_capacity.err_cnt = 0;
        int_capacity = ALARM_TASK_CAPACITY_ERR_VALUE;
     }
  }else{
    beer_capacity.err_cnt = 0;

    if (beer_capacity.high <= LIMIT_LOW_START + LIMIT_LOW_VALUE) {
        capacity = 0;
    } else {
        capacity = (beer_capacity.high -  LIMIT_LOW_START - LIMIT_LOW_VALUE) * S + LIMIT_LOW_CAPACITY_VALUE;              
    }
    capacity /= 1000000.0 ;/*单位L*/ 
    
    log_debug("beer high:%dmm.capacity:%.2fL.\r\n",beer_capacity.high,capacity);
    /*容量限制*/
    if (capacity > 20.0 ) {
        capacity = 20.0;
    }
    

    int_capacity = (uint8_t)capacity;
    int_capacity += capacity - int_capacity >= 0.5 ? 1 : 0;
   }

   /*当前整数容量没有变化，继续*/
   if (int_capacity == beer_capacity.value) {
        continue;
    }
   
    int_capacity > beer_capacity.value ? beer_capacity.dir ++ : beer_capacity.dir --;
    if (beer_capacity.dir >= CAPACITY_TASK_DIR_CHANGE_CNT || beer_capacity.dir <= -CAPACITY_TASK_DIR_CHANGE_CNT ){
        beer_capacity.change = true;
        beer_capacity.value = int_capacity;
    }
   

   if (beer_capacity.change == true){
      if (beer_capacity.value == ALARM_TASK_CAPACITY_ERR_VALUE) {
         log_error("capacity err.code:E2.\r\n");
      }else {
         log_info("capacity changed dir:%d value:%dL.\r\n",beer_capacity.dir,beer_capacity.value);
      }
     beer_capacity.change = false;
     beer_capacity.dir = 0;
     /*发送报警消息*/   
     alarm_msg.type = ALARM_TASK_MSG_CAPACITY;
     alarm_msg.value = beer_capacity.value;  
     status = osMessagePut(alarm_task_msg_q_id,*(uint32_t*)&alarm_msg,CAPACITY_TASK_PUT_MSG_TIMEOUT);
     if(status != osOK){
        log_error("capacity put alarm task msg err:%d.\r\n",status);
     }
  }
  
  }
}