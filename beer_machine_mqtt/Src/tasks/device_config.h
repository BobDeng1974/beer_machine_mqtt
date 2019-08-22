#ifndef  __DEVICE_CONFIG_H__
#define  __DEVICE_CONFIG_H__


/*啤酒机默认平台通信参数表*/
#define  URL_LOG                                    "http://syll-test.mymlsoft.com:8089/device/status"
#define  URL_ACTIVE                                 "http://syll-test.mymlsoft.com:8089/device/active"
#define  URL_LOOP_CONFIG                            "http://syll-test.mymlsoft.com:8089/device/cfg"
#define  URL_FAULT                                  "http://syll-test.mymlsoft.com:8089/device/exception/"
#define  URL_FAULT_DELETE                           "http://syll-test.mymlsoft.com:8089/device/exception/recover"
#define  URL_UPGRADE                                "http://syll-test.mymlsoft.com:8089/device/getUpgradeInfo"

#define  BOUNDARY                                   "##wkxboot##"
#define  KEY                                        "ML-freezer"
#define  SOURCE                                     "ML-freezer"
#define  MODEL                                      "SC-496"
#define  TYPE                                       "freezer"
#define  VENDOR                                     "meiling"
#define  OPT_CODE_CHINA_MOBILE                      "46000"
#define  OPT_CODE_CHINA_UNICOM                      "46001"

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
#define  DEFAULT_REPORT_LOG_INTERVAL                (2 * 60 * 1000) /**< 默认日志上报间隔时间ms*/
#define  DEFAULT_REPORT_LOOP_CONFIG_INTERVAL        (5 * 60 * 1000) /**< 默认配置上报间隔时间ms*/

/*虽然压缩机温控可调，但也不能超出极限值*/
#define  DEFAULT_COMPRESSOR_LOW_TEMPERATURE_LIMIT   -50 /**< 可配置温度最低值*/
#define  DEFAULT_COMPRESSOR_HIGH_TEMPERATURE_LIMIT  30  /**< 可配置温度最高值*/

/*定义设备类型*/
#define  CONST_DEVICE_TYPE_C                        0xaa
#define  CONST_DEVICE_TYPE_D                        0xbb
#define  CONST_DEVICE_TYPE_CD                       0xcc
#define  CONST_DEVICE_TYPE                          CONST_DEVICE_TYPE_D




#endif