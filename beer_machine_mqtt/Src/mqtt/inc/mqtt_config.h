#ifndef  __MQTT_CONFIG_H__
#define  __MQTT_CONFIG_H__
 
#include "cmsis_os.h"

#ifdef  __cplusplus
#define  MQTT_CONFIG_BEGIN extern "C" {
#define  MQTT_CONFIG_END   }
#else
#define  MQTT_CONFIG_BEGIN
#define  MQTT_CONFIG_END
#endif
 
MQTT_CONFIG_BEGIN

#define  WOLFMQTT_DISCONNECT_CB  1

MQTT_CONFIG_END
#endif
