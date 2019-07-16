#ifndef __MQTT_CONTEXT_H__
#define __MQTT_CONTEXT_H__

#include "mqtt_config.h"
#include "stdlib.h"
#include "mqtt_types.h"
#include "mqtt_client.h"

#ifdef __cplusplus
    extern "C" {
#endif

/** MQTT Client state */
typedef enum _MQTTCtxState {
    WMQ_BEGIN = 0,
    WMQ_NET_INIT,
    WMQ_INIT,
    WMQ_TCP_CONN,
    WMQ_MQTT_CONN,
    WMQ_SUB,
    WMQ_PUB,
    WMQ_WAIT_MSG,
    WMQ_UNSUB,
    WMQ_DISCONNECT,
    WMQ_NET_DISCONNECT,
    WMQ_DONE
} MQTTCtxState;

/* MQTT Client context */
typedef struct _MQTTCtx {
    MQTTCtxState stat;
    void* app_ctx; /* For storing application specific data */
    /* client and net containers */
    MqttClient client;
    MqttNet net;
    /* temp mqtt containers */
    MqttConnect connect;
    MqttMessage lwt_msg;
    MqttSubscribe subscribe;
    MqttUnsubscribe unsubscribe;
    MqttTopic topics[4], *topic;
    MqttPublish publish;
    MqttDisconnect disconnect;

    /* configuration */
    MqttQoS qos;
    const char* app_name;
    const char* host;
    const char* username;
    const char* password;
    const char* topic_name;
    const char* pub_file;
    const char* client_id;
    byte *tx_buffer, *rx_buffer;
    word32 tx_buffer_size;
    word32 rx_buffer_size;
    int return_code;
    int use_tls;
    int retain;
    int enable_lwt;

    word32 cmd_timeout_ms;
    word32 conn_timeout_ms;

    word16 keep_alive_sec;
    word16 port;

    byte    clean_session;

} MQTTCtx;

#endif /* __MQTT_CONTEXT_H__ */
