#ifndef  __NET_TASK_H__
#define  __NET_TASK_H__

#include "m6312.h"
#include "net.h"

extern osThreadId   net_task_hdl;
extern osMessageQId net_task_msg_q_id;

void net_task(void const *argument);


#define  NET_TASK_M6312_REBOOT                  (1  << 0)
#define  NET_TASK_M6312_TURN_OFF_ECHO           (1  << 1)
#define  NET_TASK_M6312_DETECT_SIM_CARD         (1  << 2)
#define  NET_TASK_M6312_SET_GPRS_APN            (1  << 3)
#define  NET_TASK_M6312_DETECT_GPRS_ATTACH      (1  << 4)
#define  NET_TASK_M6312_DETECT_GPRS_NET         (1  << 5)
#define  NET_TASK_M6312_DETECT_CONNECTION_MODE  (1  << 6)
#define  NET_TASK_M6312_DETECT_TRANSPORT_MODE   (1  << 7)
#define  NET_TASK_M6312_DETECT_RECV_CACHE_MODE  (1  << 8)
#define  NET_TASK_M6312_SEND_MESSAGE            (1  << 9)
#define  NET_TASK_M6312_GET_SIM_OPERATOR        (1 << 10)

#define  NET_TASK_ALL_SIGNALS                   ((1 << 11) - 1)
#endif