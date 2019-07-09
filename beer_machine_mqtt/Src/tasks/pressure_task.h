#ifndef  __PRESSURE_TASK_H__
#define  __PRESSURE_TASK_H__
#include "stdint.h"
#include "stdbool.h"


#ifdef  __cplusplus
#define PRESSURE_TASK_BEGIN  extern "C" {
#define PRESSURE_TASK_END    }
#else
#define PRESSURE_TASK_BEGIN  
#define PRESSURE_TASK_END   
#endif


PRESSURE_TASK_BEGIN

extern osThreadId   pressure_task_handle;
extern osMessageQId pressure_task_msg_q_id;

void pressure_task(void const *argument);

#define  PRESSURE_TASK_PRESSURE_CHANGE_CNT         2

#define  PRESSURE_VALUE_STANDARD_ATM               10.33 /*标准大气压 kg/cm表示 放大10倍*/
#define  PRESSURE_VALUE_IN_KG_CM2_ERR_MAX          99    /*最大过载压力 放大10倍   9.9kg/cm2 约0.99Mpa*/
#define  PRESSURE_VALUE_IN_KG_CM2_ERR_MIN          -4    /*最小低载压力 放大10倍  -0.4kg/cm2 约0.04Mpa 比相当于0.6个标准大气压*/

#define  PA_VALUE_PER_1KG_CM2                      98066.5 /*单位换算 1kg/cm2 == 98066.5Pa */


#define  PRESSURE_SENSOR_REFERENCE_VOLTAGE         3.3   /*压力传感器参考电压电压 单位:V*/
#define  PRESSURE_SENSOR_OUTPUT_VOLTAGE_MIN        0.5   /*压力传感器最小输出电压 单位:V*/
#define  PRESSURE_SENSOR_OUTPUT_VOLTAGE_MAX        4.5   /*压力传感器最大输出电压 单位:V*/

#define  PRESSURE_SENSOR_INPUT_PA_MIN             (101325.0)/*压力传感器最小压力 单位:Pa*/
#define  PRESSURE_SENSOR_INPUT_PA_MAX             (5*100000.0 + 101325.0)/*压力传感器最大输入压力 单位:Pa*/

#define  PRESSURE_SENSOR_ADC_VALUE_MAX             4095/*AD转换最大数值*/    



#define  PRESSURE_TASK_MSG_WAIT_TIMEOUT            osWaitForever
#define  PRESSURE_TASK_PUT_MSG_TIMEOUT             5

#define  PRESSURE_ERR_VALUE_SENSOR                 0xFF


typedef enum
{
PRESSURE_TASK_MSG_ADC_COMPLETED,
PRESSURE_TASK_MSG_CAPACITY,
PRESSURE_TASK_MSG_CONFIG
}pressure_task_msg_type_t;



typedef struct
{
uint32_t type:8;
uint32_t value:16;
uint32_t reserved:8;
}pressure_task_msg_t;




PRESSURE_TASK_END

#endif