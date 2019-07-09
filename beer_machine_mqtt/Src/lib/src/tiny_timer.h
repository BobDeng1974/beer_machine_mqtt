#ifndef  __TINY_TIMER_H__
#define  __TINY_TIMER_H__

#include "cmsis_os.h"

#ifdef  __cplusplus
#define  TINY_TIMER_BEGIN extern "C" {
#define  TINY_TIMER_END   }
#else
#define  TINY_TIMER_BEGIN
#define  TINY_TIMER_END
#endif
 
TINY_TIMER_BEGIN

 /** @brief 定时器定义*/
typedef struct
{
    uint8_t dir; /**< 定时器增长方向*/
    uint32_t timeout;/**< 定时器定时值*/
    uint32_t start;/**< 定时器初始值*/
}tiny_timer_t;


/**
* @brief 定时器初始化
* @details
* @param timer 定时器指针
* @param dir 定时器增长方向
* @param timeout 定时器超时时间
* @return 初始化结果
* @retval 0 成功
* @retval -1 失败
* @attention
* @note
*/
int tiny_timer_init(tiny_timer_t *timer,uint8_t dir,uint32_t timeout);

/**
* @brief 定时器当前值
* @details
* @param timer 定时器指针
* @return 定时器当前值
* @attention 如果是向上增长，当流逝的时间大于超时值时，返回超时值；
*            如果时向下增长，当流逝的时间大于超时值时，返回0 。
* @note
*/
uint32_t tiny_timer_value(tiny_timer_t *timer);



TINY_TIMER_BEGIN

#endif
