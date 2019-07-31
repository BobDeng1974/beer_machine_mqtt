#ifndef  __CIRCLE_BUFFER_H__
#define  __CIRCLE_BUFFER_H__
#include "stdint.h"
#include "stdbool.h"

#ifdef __IAR_SYSTEMS_ICC__
#include <intrinsics.h>
#endif


#ifdef __cplusplus
    extern "C" {
#endif



typedef struct
{
    char      *buffer;
    uint32_t   read;
    uint32_t   write;
    uint32_t   mask;
    uint32_t   size;
}circle_buffer_t;

/**
* @brief 循环缓存初始化
* @param cb 循环缓存指针
* @param buffer 数据缓存地址
* @param size 数据缓存大小
* @return 无
* @note 
*/
void circle_buffer_init(circle_buffer_t *cb,uint8_t *buffer,uint32_t size);

/*
* @brief 循环缓存刷新
* @param cb 循环缓存指针
* @param 
* @return  刷新的长度
* @note 
*/
uint32_t circle_buffer_flush(circle_buffer_t *cb);

/*
* @brief 循环缓存容量
* @param cb 循环缓存指针
* @return 循环缓存容量
* @note
*/
uint32_t circle_buffer_size(circle_buffer_t *cb);

/*
* @brief 循环缓存是否已满
* @param cb 循环缓存指针
* @return true 已满
* @return false 未满
* @note
*/
bool circle_buffer_is_full(circle_buffer_t *cb);

/*
* @brief 循环缓存是否已空
* @param cb 循环缓存指针
* @return true 已空
* @return false 未空
* @note
*/
bool circle_buffer_is_empty(circle_buffer_t *cb);

/*
* @brief  读取循环缓存中的数据
* @param  cb   循环缓存指针
* @param  dst  目的地址
* @param  size 期望读取的数量
* @return 实际读取的数量
* @note
*/
uint32_t circle_buffer_read(circle_buffer_t *cb,char *dst,uint32_t size);

/*
* @brief 循环缓存写入数据
* @param cb 循环缓存指针
* @param src 数据源地址
* @param size 期望写入的数量
* @return 实际写入的数量
* @note
*/
uint32_t circle_buffer_write(circle_buffer_t *cb,const char *src,uint32_t size);
/*
*  serial critical configuration for IAR EWARM
*/

#ifdef __ICCARM__

#if (defined (__ARM6M__) && (__CORE__ == __ARM6M__))             
#define CIRCLE_BUFFER_ENTER_CRITICAL()                         \
{                                                              \
    unsigned int pri_mask;                                     \
    pri_mask = __get_PRIMASK();                                \
    __set_PRIMASK(1);
    
#define CIRCLE_BUFFER_EXIT_CRITICAL()                          \
    __set_PRIMASK(pri_mask);                                   \
}
#elif ((defined (__ARM7EM__) && (__CORE__ == __ARM7EM__)) || (defined (__ARM7M__) && (__CORE__ == __ARM7M__)))
#ifndef  CIRCLE_BUFFER_MAX_INTERRUPT_PRIORITY
#define  CIRCLE_BUFFER_MAX_INTERRUPT_PRIORITY                 (5 << ( 8 - 4 ))
#endif
#define CIRCLE_BUFFER_ENTER_CRITICAL()                         \
{                                                              \
   unsigned int base_pri;                                      \
   base_pri = __get_BASEPRI();                                 \
   __set_BASEPRI(CIRCLE_BUFFER_MAX_INTERRUPT_PRIORITY);   

#define CIRCLE_BUFFER_EXIT_CRITICAL()                          \
   __set_BASEPRI(base_pri);                                    \
}
  #endif
#endif


/*
*  serial critical configuration for KEIL
*/ 
#ifdef __CC_ARM
#if (defined __TARGET_ARCH_6S_M)
#define CIRCLE_BUFFER_ENTER_CRITICAL()                                             
 {                                                              \
    unsigned int pri_mask;                                      \
    register unsigned char PRIMASK __asm( "primask");           \
    pri_mask = PRIMASK;                                         \
    PRIMASK = 1u;                                               \
    __schedule_barrier();

#define CIRCLE_BUFFER_EXIT_CRITICAL()                           \
    PRIMASK = pri_mask;                                         \
    __schedule_barrier();                                       \
}
#elif (defined(__TARGET_ARCH_7_M) || defined(__TARGET_ARCH_7E_M))
    #ifndef  CIRCLE_BUFFER_MAX_INTERRUPT_PRIORITY
    #define  CIRCLE_BUFFER_MAX_INTERRUPT_PRIORITY                (0x50)
    #endif
#define CIRCLE_BUFFER_ENTER_CRITICAL()                          \
 {                                                              \
     unsigned int base_pri;                                     \
     register unsigned char BASEPRI __asm( "basepri");          \
      base_pri = BASEPRI;                                       \
      BASEPRI = CIRCLE_BUFFER_MAX_INTERRUPT_PRIORITY;           \
      __schedule_barrier();

#define CIRCLE_BUFFER_EXIT_CRITICAL()                           \
     BASEPRI = base_pri;                                        \
     __schedule_barrier();                                      \
}
#endif
#endif  

#ifdef __cplusplus
    }
#endif

#endif


