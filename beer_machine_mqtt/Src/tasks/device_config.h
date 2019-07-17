#ifndef  __DEVICE_CONFIG_H__
#define  __DEVICE_CONFIG_H__


/*啤酒机默认平台通信参数表*/
#define  URL_LOG                                    "http://mh1597193030.uicp.cn:35787/device/log/submit"
#define  URL_ACTIVE                                 "http://mh1597193030.uicp.cn:35787/device/info/active"
#define  URL_FAULT                                  "http://mh1597193030.uicp.cn:35787/device/fault/submit"
#define  URL_FAULT_DELETE                           "http://mh1597193030.uicp.cn:35787/device/fault/submit"
#define  URL_UPGRADE                                "http://mh1597193030.uicp.cn:35787/device/info/getUpgradeInfo"
#define  BOUNDARY                                   "##wkxboot##"
#define  KEY                                        "meiling-beer"
#define  SOURCE                                     "coolbeer"
#define  MODEL                                      "pijiuji"


#define  SN_LEN                                     24 /**< SN字节长度*/
#define  SIM_ID_LEN                                 24 /**< SIM ID字节长度*/

#define  ENV_NAME_COMPRESSOR_CTRL                  "c_ctrl"
#define  ENV_NAME_TEMPERATURE_COLD                 "t_cold"    
#define  ENV_NAME_TEMPERATURE_FREEZE               "t_freeze"  

/*啤酒机默认运行配置参数表*/
#define  DEFAULT_COMPRESSOR_COLD_TEMPERATURE         2   /**< 默认压缩机冷藏温度*/
#define  DEFAULT_COMPRESSOR_FREEZE_TEMPERATURE      -5   /**< 默认压缩机冷冻温度*/
#define  DEFAULT_TEMPERATURE_RANGES                  2   /**< 默认压缩机温度上下浮动范围*/
#define  DEFAULT_REPORT_LOG_INTERVAL                (1 * 60 * 1000) /**< 默认日志上报间隔时间ms*/

/*虽然压缩机温控可调，但也不能超出极限值，保证酒的品质*/
#define  DEFAULT_COMPRESSOR_LOW_TEMPERATURE_LIMIT   -20 /**< 可配置温度最低值*/
#define  DEFAULT_COMPRESSOR_HIGH_TEMPERATURE_LIMIT  30  /**< 可配置温度最高值*/









#endif