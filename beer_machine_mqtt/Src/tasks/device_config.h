#ifndef  __DEVICE_CONFIG_H__
#define  __DEVICE_CONFIG_H__


/*啤酒机默认平台通信参数表*/
#define  URL_LOG                                    "http://v2236p1176.imwork.net:22132/device/status"
#define  URL_ACTIVE                                 "http://v2236p1176.imwork.net:22132/device/active"
#define  URL_LOOP_CONFIG                            "http://v2236p1176.imwork.net:22132/device/cfg"
#define  URL_FAULT                                  "http://v2236p1176.imwork.net:22132/device/exception"
#define  URL_FAULT_DELETE                           "http://v2236p1176.imwork.net:22132/device/exception/recover"
#define  URL_UPGRADE                                "http://v2236p1176.imwork.net:22132/device/getUpgradeInfo"

#define  BOUNDARY                                   "##wkxboot##"
#define  KEY                                        "ML-freezer"
#define  SOURCE                                     "ML-freezer"
#define  MODEL                                      "冷柜"
#define  SN_ADDR                                    0x803f800

#define  SN_LEN                                     24 /**< SN字节长度*/

#define  ENV_NAME_COMPRESSOR_CTRL                  "c_ctrl"
#define  ENV_NAME_TEMPERATURE_COLD_MIN             "t_cold_min"
#define  ENV_NAME_TEMPERATURE_COLD_MAX             "t_cold_max"
#define  ENV_NAME_TEMPERATURE_FREEZE_MIN           "t_freeze_min"
#define  ENV_NAME_TEMPERATURE_FREEZE_MAX           "t_freeze_max"
#define  ENV_NAME_LOG_INTERVAL                     "log_interval"

/*啤酒机默认运行配置参数表*/
#define  DEFAULT_COMPRESSOR_TEMPERATURE_START       5   /**< 默认压缩机停机温度*/
#define  DEFAULT_COMPRESSOR_TEMPERATURE_STOP        2   /**< 默认压缩机开机温度*/
#define  DEFAULT_REPORT_LOG_INTERVAL                (1 * 60 * 1000) /**< 默认日志上报间隔时间ms*/
#define  DEFAULT_REPORT_LOOP_CONFIG_INTERVAL        (1 * 60 * 1000) /**< 默认日志上报间隔时间ms*/

/*虽然压缩机温控可调，但也不能超出极限值*/
#define  DEFAULT_COMPRESSOR_LOW_TEMPERATURE_LIMIT   -20 /**< 可配置温度最低值*/
#define  DEFAULT_COMPRESSOR_HIGH_TEMPERATURE_LIMIT  30  /**< 可配置温度最高值*/

/*定义设备类型*/
#define  CONST_DEVICE_TYPE_COLD                     0xaa
#define  CONST_DEVICE_TYPE_FREEZE                   0xbb
#define  CONST_DEVICE_TYPE                          CONST_DEVICE_TYPE_COLD






#endif