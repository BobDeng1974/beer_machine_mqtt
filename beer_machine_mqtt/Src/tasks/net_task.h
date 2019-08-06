#ifndef  __NET_TASK_H__
#define  __NET_TASK_H__

#include "m6312.h"
#include "net.h"

extern osThreadId   net_task_hdl;
extern osMessageQId net_task_msg_q_id;


void net_task(void const *argument);


#define  NET_TASK_SIGNAL_INIT                    (1  << 0)
#define  NET_TASK_SIGNAL_NET_READY               (1  << 1)
#define  NET_TASK_SIGNAL_QUERY_SIM_ID            (1  << 2)
#define  NET_TASK_SIGNAL_QUERY_BASE_INFO         (1  << 3)
#define  NET_TASK_ALL_SIGNALS                    ((1 << 4) - 1)

#define  NET_TASK_NET_INIT_TIMEOUT               50000
#define  NET_TASK_INIT_STEP_DELAY                1000          
#define  NET_TASK_PUT_MSG_TIMEOUT                5
#define  NET_TASK_QUERY_TIMEOUT                  10000
#define  NET_TASK_QUERY_BASE_INFO_INTERVAL       (1 * 60 * 1000)
typedef struct
{
    uint8_t is_net_ready;
    uint8_t is_sim_card_exsit;
    uint8_t operator_code;
    char sim_id[M6312_SIM_ID_STR_LEN];
    base_information_t base_info;
}net_context_t;

#endif