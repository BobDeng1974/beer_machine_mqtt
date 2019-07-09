#include "cmsis_os.h"
#include "adc.h"
#include "tasks_init.h"
#include "adc_task.h"
#include "temperature_task.h"
#include "pressure_task.h"
#include "beer_machine.h"
#include "log.h"

osThreadId   adc_task_handle;


typedef struct
{
uint32_t cusum;
uint16_t average;
uint16_t cnt;
uint16_t err;
}adc_sample_t;

static volatile uint16_t sample[2];
static adc_sample_t temperature,pressure;


void  HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
osSignalSet(adc_task_handle,ADC_TASK_SIGNAL_ADC_COMPLETED);   
}


void  HAL_ADC_ErrorCallback(ADC_HandleTypeDef *hadc)
{
osSignalSet(adc_task_handle,ADC_TASK_SIGNAL_ADC_ERROR);   
}


static int adc_start()
{
 HAL_StatusTypeDef status;
 status =HAL_ADC_Start_DMA(&hadc1,(uint32_t*)sample,2);
 if(status != HAL_OK){
 log_error("start adc dma error:%d\r\n",status);
 return -1;
 }
 return 0;
}

static int adc_stop()
{
 HAL_StatusTypeDef status;
 status =HAL_ADC_Stop_DMA(&hadc1);
 if(status != HAL_OK){
 log_error("stop adc dma error:%d\r\n",status);
 return -1;
 }
 return 0;
}


static void adc_calibration()
{
 HAL_ADCEx_Calibration_Start(&hadc1);
}

static void adc_reset()
{
 HAL_ADC_MspDeInit(&hadc1);
 MX_ADC1_Init();
 adc_stop();
 adc_calibration();
 adc_stop();
}
void adc_task(void const * argument)
{
 osEvent signals;
 osStatus status;
 
 temperature_task_msg_t temperature_msg;
 pressure_task_msg_t    pressure_msg;
 int  result;
 
 /*等待任务同步*/
 /*
 xEventGroupSync(tasks_sync_evt_group_hdl,TASKS_SYNC_EVENT_ADC_TASK_RDY,TASKS_SYNC_EVENT_ALL_TASKS_RDY,osWaitForever);
 log_debug("adc task sync ok.\r\n");
 */
 adc_stop();
 adc_calibration();
 adc_stop();
 while(1){
 osDelay(ADC_TASK_INTERVAL);
 result = adc_start();
 if(result !=0 ){
 adc_reset();
 continue;
 } 
  
 signals = osSignalWait(ADC_TASK_SIGNALS_ALL,ADC_TASK_ADC_TIMEOUT);
 if(signals.status == osEventSignal ){
 
 if(signals.value.signals & ADC_TASK_SIGNAL_ADC_COMPLETED){ 
/*温度采样值计算*/    
 if(temperature.cnt < ADC_TASK_ADC_SAMPLE_MAX){
 /*采样值异常处理*/
 if(sample[ADC_TASK_TEMPERATURE_IDX] <= ADC_TASK_ADC_VALUE_MIN ||\
  sample[ADC_TASK_TEMPERATURE_IDX] >= ADC_TASK_ADC_VALUE_MAX){ 
  temperature.cusum = 0;
  temperature.cnt = 0;
  temperature.err ++; 
  }else{
  temperature.cusum += sample[ADC_TASK_TEMPERATURE_IDX];   
  temperature.cnt ++;
  temperature.err = 0;
  }
  if(temperature.err >= ADC_TASK_ADC_ERR_MAX){
  log_error("temperature sample error.\r\n");
  temperature.err = 0;
  /*构建采样错误消息*/
  temperature_msg.type = TEMPERATURE_TASK_MSG_ADC_COMPLETED;
  temperature_msg.value = ADC_TASK_ADC_ERR_VALUE;   
  status = osMessagePut(temperature_task_msg_q_id,*(uint32_t*)&temperature_msg,ADC_TASK_PUT_MSG_TIMEOUT);
  if(status !=osOK){
  log_error("put temperature msg error:%d\r\n",status);
  }
  }
 }else{
 temperature.average = temperature.cusum / temperature.cnt;  
 /*总和清零 等待取样*/
 temperature.cusum = 0;
 temperature.cnt = 0;
 temperature_msg.type = TEMPERATURE_TASK_MSG_ADC_COMPLETED;
 temperature_msg.value = temperature.average;
 status = osMessagePut(temperature_task_msg_q_id,*(uint32_t*)&temperature_msg,ADC_TASK_PUT_MSG_TIMEOUT);
 if(status !=osOK){
 log_error("put temperature msg error:%d\r\n",status);
 }
 }
 
 /*压力采样计算*/
 if(pressure.cnt < ADC_TASK_ADC_SAMPLE_MAX){
 /*采样值异常处理*/
 if(sample[ADC_TASK_PRESSURE_IDX] <= ADC_TASK_ADC_VALUE_MIN ||\
  sample[ADC_TASK_PRESSURE_IDX] >= ADC_TASK_ADC_VALUE_MAX){ 
  pressure.cusum = 0;
  pressure.cnt = 0;
  pressure.err ++; 
  }else{
  pressure.cusum += sample[ADC_TASK_PRESSURE_IDX];   
  pressure.cnt ++;
  pressure.err = 0;
  }
  if(pressure.err >= ADC_TASK_ADC_ERR_MAX){
  log_error("pressure sample error.\r\n");
  pressure.err = 0;
  /*构建采样错误消息*/
  pressure_msg.type = PRESSURE_TASK_MSG_ADC_COMPLETED;
  pressure_msg.value = ADC_TASK_ADC_ERR_VALUE;   
  status = osMessagePut(pressure_task_msg_q_id,*(uint32_t*)&pressure_msg,ADC_TASK_PUT_MSG_TIMEOUT);
  if(status !=osOK){
  log_error("put pressure msg error:%d\r\n",status);
  }
  }
 }else {
  pressure.average = pressure.cusum / pressure.cnt;  
  /*总和清零 等待取样*/
  pressure.cusum = 0;
  pressure.cnt = 0;
  pressure_msg.type = PRESSURE_TASK_MSG_ADC_COMPLETED;
  pressure_msg.value = pressure.average;
  status = osMessagePut(pressure_task_msg_q_id,*(uint32_t*)&pressure_msg,ADC_TASK_PUT_MSG_TIMEOUT);
  if(status !=osOK){
  log_error("put pressure msg error:%d\r\n",status);
  }
 
 }
 }

 /*ADC采样硬件故障信号*/
 if(signals.value.signals & ADC_TASK_SIGNAL_ADC_ERROR){
 log_error("adc error.reset.\r\n");
 adc_reset();
 }
 
}
}
}

