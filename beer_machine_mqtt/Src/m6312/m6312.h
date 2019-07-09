#ifndef  __M6312_H__
#define  __M6312_H__

#include "stdint.h"
#ifdef  __cplusplus
#define  M6312_BEGIN extern "C" {
#define  M6312_END   }
#else
#define  M6312_BEGIN
#define  M6312_END
#endif
 
M6312_BEGIN

#define  CRLF                       "\r\n"

#define  M6312_UART_PORT            1
#define  M6312_UART_BAUD_RATE       115200
#define  M6312_UART_DATA_BIT        8
#define  M6312_UART_STOP_BIT        1


#define  M6312_IMEI_LEN             15
#define  M6312_SN_LEN               20
#define  M6312_SIM_ID_STR_LEN       20
#define  M6312_LAC_STR_LEN          8
#define  M6312_CI_STR_LEN           8

 /** sim卡状态枚举*/
typedef enum
{
    M6312_SIM_CARD_UNKNOW,  /**< sim卡状态未知*/
    M6312_SIM_CARD_NO_EXIST,/**< sim卡不存在*/
    M6312_SIM_CARD_EXIST,/**< sim卡存在 正常*/
    M6312_SIM_CARD_BLOCK/**< sim卡存在 但被锁死*/
}m6312_sim_card_status_t;

 /** IP连接类型枚举*/
typedef enum
{
    M6312_CONNECT_TCP,
    M6312_CONENCT_UDP
}m6312_connect_type_t;

 /** sim卡注册状态枚举*/
typedef enum
{
    M6312_SIM_REGISTER_NO,
    M6312_SIM_REGISTER_YES,
    M6312_SIM_REGISTER_RETRY,
    M6312_SIM_REGISTER_DENY,
    M6312_SIM_REGISTER_UNKNOW,
    M6312_SIM_REGISTER_ROAM
}m6312_sim_register_status_t;

/** GPRS网络附着状态枚举*/
typedef enum
{
    M6312_GPRS_ATTACH = 1,
    M6312_GPRS_DETACH = 0
}m6312_gprs_attach_status_t;

/** GPRS网络上下文状态枚举*/
typedef enum
{
    M6312_GPRS_NET_ACTIVE = 1,
    M6312_GPRS_NET_INACTIVE = 0
}m6312_gprs_net_status_t;

/** M6312接收缓存模式枚举*/
typedef enum
{
    M6312_RECV_CACHE_MODE_CACHED = 1,
    M6312_RECV_CACHE_MODE_NO_CACHED = 0
}m6312_recv_cache_mode_t;

/** M6312连接模式枚举*/
typedef enum
{
    M6312_CONNECTION_MODE_MULTI = 1,
    M6312_CONNECTION_MODE_SINGLE = 0
}m6312_connection_mode_t;

/** M6312连接模式枚举*/
typedef enum
{
    M6312_TRANSPORT_MODE_TRANSPARENT = 1,
    M6312_TRANSPORT_MODE_NO_TRANSPARENT = 0
}m6312_transport_mode_t;

/**SIM卡运营商枚举*/
typedef enum
{
    SIM_OPERATOR_CHINA_MOBILE,
    SIM_OPERATOR_CHINA_UNICOM
}m6312_sim_operator_t;

/**
* @brief M6312模块开机
* @param 无
* @return 开机是否成功
* @retval 0 成功
* @retval -1 失败
* @attention 无
* @note 无
*/
int m6312_pwr_on(void);

/**
* @brief M6312模块关机
* @param 无
* @return 关机是否成功
* @retval 0 成功
* @retval -1 失败
* @attention 无
* @note 无
*/
int m6312_pwr_off(void);

/**
* @brief M6312模块串口初始化关机
* @param 无
* @return 初始化是否成功
* @retval 0 成功
* @retval -1 失败
* @attention 无
* @note 无
*/
int m6312_uart_init(void);

/**
* @brief M6312模块回显控制开关
* @param on_off 开关值 0 关闭 > 0打开
* @return M6312模块回显控制是否成功
* @retval 0 模块回显控制成功
* @retval -1 模块回显控制失败
* @attention 无
* @note 无
*/
int m6312_echo_turn_on_off(uint8_t on_off);

/**
* @brief M6312模块获取sim卡状态
* @param status  @see m6312_sim_card_status_t sim卡状态
* @return 获取状态是否成功
* @attention 无
* @note 无
*/
int m6312_get_sim_card_status(m6312_sim_card_status_t *status);

/**
* @brief M6312模块获取sim卡id
* @param sim_id sim卡id保存的指针
* @return 获取状态是否成功
* @attention 无
* @note 无
*/
int m6312_get_sim_card_id(char *sim_id);


/**
* @brief M6312模块获取GPRS附着状态
* @param status gprs状态指针
* @return 获取状态是否成功
* @attention 无
* @note 无
*/
int m6312_get_gprs_attach_status(m6312_gprs_attach_status_t *status);


/**
* @brief M6312模块设置GPRS附着状态
* @param attach gprs附着状态 @see m6312_gprs_attach_status_t
* @return 设置是否成功
* @retval 0 成功
* @retval -1 失败
* @attention 无
* @note 无
*/
int m6312_set_gprs_attach(m6312_gprs_attach_status_t attach);

/**
* @brief M6312模块设置apn
* @param apn 运营商apn
* @return M6312模块设置apn是否成功
* @retval 0 设置apn成功
* @retval -1 设置apn失败
* @attention 无
* @note 无
*/
int m6312_set_gprs_apn(char *apn);


/**
* @brief M6312模块获取gprs网络状态
* @param status m6312模块gprs网络状态指针
* @return M6312模块获取状态是否成功
* @retval 0 获取状态成功
* @retval -1 获取状态失败
* @attention 无
* @note 无
*/
int m6312_get_gprs_net_status(m6312_gprs_net_status_t *status);

/**
* @brief M6312模块gprs网络激活
* @param status 是否激活 @see m6312_gprs_net_status_t
* @return M6312模块gprs激活是否成功
* @retval 0 激活成功
* @retval -1 激活失败
* @attention 无
* @note 无
*/
int m6312_set_gprs_net(m6312_gprs_net_status_t status);

/**
* @brief M6312模块获取sim卡注册状态
* @param status sim卡注册状态
* @return 获取状态是否成功
* @attention 无
* @note 无
*/
int m6312_get_sim_register_status(m6312_sim_register_status_t *status);

/**
* @brief M6312模块获取连接模式
* @param mode 连接模式指针@see m6312_connection_mode_t
* @return M6312模块获取连接模式是否成功
* @retval 0 M6312模块获取连接模式成功
* @retval -1 M6312模块获取连接模式失败
* @attention 无
* @note 无
*/
int m6312_get_connection_mode(m6312_connection_mode_t *mode);


/**
* @brief M6312模块配置连接模式
* @param mode 连接模式 @see m6312_connection_mode_t
* @return M6312模块配置连接模式是否成功
* @retval 0 M6312模块配置连接模式成功
* @retval -1 M6312模块配置连接模式失败
* @attention 无
* @note 无
*/
int m6312_set_connection_mode(m6312_connection_mode_t mode);


/**
* @brief M6312模块获取接收缓存的模式
* @param status m6312接收缓存状态指针@see m6312_recv_cache_mode_t
* @return M6312模块获取连接模式是否成功
* @retval 0 M6312模块获取连接模式成功
* @retval -1 M6312模块获取连接模式失败
* @attention 无
* @note 无
*/
int m6312_get_recv_cache_mode(m6312_recv_cache_mode_t *mode);


/**
* @brief M6312模块设置接收缓存的模式
* @param mode 接收缓存模式 @see m6312_recv_cache_mode_t
* @return M6312模块设置接收缓存模式是否成功
* @retval 0 M6312模块设置接收缓存模式成功
* @retval -1 M6312模块设置接收缓存模式失败
* @attention 无
* @note 无
*/
int m6312_set_recv_cache_mode(m6312_recv_cache_mode_t mode);

/**
* @brief M6312模块获取传输模式
* @param mode 传输模式指针 @see m6312_transport_mode_t
* @return M6312模块获取传输模式是否成功
* @retval 0 M6312模块获取传输模式成功
* @retval -1 M6312模块获取传输模式失败
* @attention 无
* @note 无
*/
int m6312_get_transport_mode(m6312_transport_mode_t *mode);


/**
* @brief M6312模块配置传送模式
* @param mode 传输模式 @see m6312_transport_mode_t
* @return M6312模块配置透明传送是否成功
* @retval 0 M6312模块配置透明传送成功
* @retval -1 M6312模块配置透明传送失败
* @attention 无
* @note 无
*/
int m6312_set_transport_mode(m6312_transport_mode_t mode);

/**
* @brief M6312模块建立TCP或者UDP连接
* @param index 建立连接的通道号
* @param host 要建立连接的主机名
* @param type 建立连接的类型
* @return 建立连接是否成功
* @retval 0 建立连接成功
* @retval -1 建立连接失败
* @attention 无
* @note 无
*/
int m6312_connect(uint8_t index,char *host,char *port,m6312_connect_type_t type);

/**
* @brief M6312模块关闭TCP或者UDP连接
* @param index 建立连接的通道号
* @return 关闭连接是否成功
* @retval 0 关闭连接成功
* @retval -1 关闭连接失败
* @attention 无
* @note 无
*/
int m6312_close(uint8_t index);

/**
* @brief M6312模块发送数据
* @param socket_id 建立连接的通道号
* @param buffer 发送的数据地址
* @param size 发送的数据量
* @return 发送是否成功
* @retval 0 发送成功
* @retval -1 发送失败
* @attention 无
* @note 无
*/
int m6312_send(uint8_t socket_id,uint8_t *buffer,uint16_t size);

/**
* @brief M6312模块接收数据
* @param socket_id 建立连接的通道号
* @param buffer 接收的数据地址
* @param size 接收的数据量
* @return 实际接收数据量
* @retval >0 成功接收的数据量
* @retval -1 接收失败
* @attention 无
* @note 无
*/
int m6312_recv(uint8_t socket_id,uint8_t *recv_buffer,uint16_t size);




M6312_END
#endif
