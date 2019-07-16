/* mqttuart.c
 *
 * Copyright (C) 2006-2018 wolfSSL Inc.
 *
 * This file is part of wolfMQTT.
 *
 * wolfMQTT is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * wolfMQTT is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1335, USA
 */

/* Include the autoconf generated config.h */

#include "mqtt_config.h"
#include "mqtt_client.h"
#include "mqtt_net.h"


/* TODO: Add includes for UART HW */

/* Include the example code */
#include "tiny_timer.h"
#include "m6312.h"
#include "printf.h"


/* this code is a template for using UART for communication */



/* Local context for callbacks */
typedef struct _UartContext {
    int uartPort;
    /* TODO: Add any other context info you want */
    int socket_id;
} UartContext;

UartContext uart_context;

/* Private functions */
static int NetConnect(void *context, const char* host, word16 port,
    int timeout_ms)
{
    UartContext *uartCtx = (UartContext*)context;
    (void)uartCtx;
    int rc;
    char port_str[10];
    
    snprintf(port_str,10,"%d",port);
    rc = m6312_connect(0,(char *)host,port_str,M6312_CONNECT_TCP);
    if (rc == 0) {
        uartCtx->socket_id = 0;
        return 0;
    }
    return -1;
}

static int NetWrite(void *context, const byte* buffer, int buffer_len,
    int timeout_ms)
{
    UartContext *uartCtx = (UartContext*)context;
    (void)uartCtx;

    /* TODO: Implement write call to your UART HW */
    int write_total = 0,write;

    tiny_timer_t timer;

    tiny_timer_init(&timer,0,timeout_ms);

    while (tiny_timer_value(&timer) > 0 && write_total < buffer_len) {
        write = m6312_send(uartCtx->socket_id,(uint8_t *)buffer + write_total,buffer_len - write_total);
        if (write < 0) {
            return -1;
        }
        write_total += write;
    }

    return write_total;
}

static int NetRead(void *context, byte* buffer, int buffer_len,
    int timeout_ms)
{
    UartContext *uartCtx = (UartContext*)context;
    (void)uartCtx;

    /* TODO: Implement read call to your UART HW */
    int read_total = 0,read;

    tiny_timer_t timer;

    tiny_timer_init(&timer,0,timeout_ms);
    while (tiny_timer_value(&timer) > 0 && read_total < buffer_len) {
        read = m6312_recv(uartCtx->socket_id,(uint8_t *)buffer + read_total,buffer_len - read_total);
        if (read < 0) {
            return MQTT_CODE_ERROR_NETWORK;
        }
        read_total += read;
    }

    if (read_total == 0) {
        return MQTT_CODE_ERROR_TIMEOUT;
    }

    if (read_total != buffer_len) {
        return MQTT_CODE_ERROR_NETWORK;
    }


    return read_total;
}

static int NetDisconnect(void *context)
{
    UartContext *uartCtx = (UartContext*)context;
    (void)uartCtx;
    int rc;
    rc = m6312_close(uartCtx->socket_id);
    return rc;
}

/* Public Functions */
int MqttClientNet_Init(MqttNet* net, MQTTCtx* mqttCtx)
{
    if (net) {
        XMEMSET(net, 0, sizeof(MqttNet));
        net->connect = NetConnect;
        net->read = NetRead;
        net->write = NetWrite;
        net->disconnect = NetDisconnect;
        net->context = &uart_context;

    }
    (void)mqttCtx;
    return 0;
}

int MqttClientNet_DeInit(MqttNet* net)
{
    if (net) {
        if (net->context) {
            XMEMSET(net->context, 0, sizeof(UartContext));
        }
        XMEMSET(net, 0, sizeof(MqttNet));
    }
    return 0;
}
