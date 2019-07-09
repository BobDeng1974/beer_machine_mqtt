#ifndef  __NET_H__
#define  __NET_H__




#define  NET_WIFI_RSSI_LEVEL_1           -90
#define  NET_WIFI_RSSI_LEVEL_2           -80
#define  NET_WIFI_RSSI_LEVEL_3           -60

typedef struct
{
  char lac[8];
  char ci[8];
  char rssi[4];
}gsm_base_t;

typedef enum
{
 NET_STATUS_INIT,
 NET_STATUS_ONLINE,
 NET_STATUS_OFFLINE
}net_status_t;

/* 函数：net_init
*  功能：网络初始化
*  参数：无
*  返回：0：成功 其他：失败
*/
int net_init(void);

/* 函数：net_wifi_comm_if_init
*  功能：初始化wifi通信接口
*  参数：无
*  返回：0 成功 其他：失败
*/ 
int net_wifi_comm_if_init(uint32_t timeout);

/* 函数：net_wifi_reset
*  功能：复位WiFi
*  返回：0  成功 其他：失败
*/ 
int net_wifi_reset(uint32_t timeout);

/* 函数：net_wifi_switch_to_station
*  功能：wifi切换到station模式
*  参数：无
*  返回：0 成功 其他：失败
*/ 
int net_wifi_switch_to_station(void);

/* 函数：net_wifi_switch_to_ap
*  功能：wifi切换到AP模式
*  参数：无
*  返回：0 成功 其他：失败
*/ 
int net_wifi_switch_to_ap(void);

/* 函数：net_query_wifi_mac
*  功能：询问WiFi mac 地址
*  参数：mac wifi mac
*  返回：0：成功 其他：失败
*/ 
int net_query_wifi_mac(char *mac);

/* 函数：net_query_wifi_ap_ssid
*  功能：询问WiFi目前连接的ap
*  参数：mac wifi mac
*  返回：0：成功 其他：失败
*/ 
int net_query_wifi_ap_ssid(char *ssid);


/* 函数：net_wifi_switch_to_ap
*  功能：wifi切换到AP模式
*  参数：无
*  返回：0 成功 其他：失败
*/ 
int net_wifi_connect_ap(char *ssid,char *passwd);


/* 函数：net_wifi_config
*  功能：wifi配网
*  参数：无
*  返回：0 成功 其他：失败
*/ 
int net_wifi_config(char *ssid,char* passwd,uint32_t timeout);

/* 函数：net_query_wifi_rssi_level
*  功能：询问WiFi的level值
*  参数：rssi值指针
*  返回：0  成功 其他：失败
*/ 
int net_query_wifi_rssi_level(char *ssid,int *rssi,int *level);

/* 函数：net_gsm_comm_if_init
*  功能：gsm模块通信接口初始化
*  参数：无
*  返回：0 成功 其他：失败
*/ 
int net_gsm_comm_if_init(uint32_t timeout);

/* 函数：net_gsm_gprs_init
*  功能：gprs网络初始化
*  参数：无
*  返回：0 成功 其他：失败
*/ 
int net_gsm_gprs_init(uint32_t timeout);

/* 函数：net_gsm_reset
*  功能：复位gsm
*  返回：0  成功 其他：失败
*/ 
int net_gsm_reset(uint32_t timeout);

/* 函数：net_module_query_sim_id
*  功能：查询sim id
*  参数：sim_id sim id指针
*  参数：timeout 超时时间
*  返回：0 成功 其他：失败
*/ 
int net_query_sim_id(char *sim_id,uint32_t timeout);

/* 函数：net_query_gsm_location
*  功能：询问gsm基站位置信息
*  参数：location位置信息指针
*  返回：0 成功 其他：失败
*/ 
int net_query_gsm_location(gsm_base_t *base,uint8_t cnt);




#endif