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
#include "socket.h"
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

    rc = socket_open((char *)host,port,SOCKET_PROTOCOL_TCP);
    if (rc >= 0) {
        uartCtx->socket_id = rc;
        return 0;
    }
    return -1;
}

static int NetWrite(void *context, const byte* buffer, int buffer_len,
    int timeout_ms)
{
    int rc;
    UartContext *uartCtx = (UartContext*)context;
    (void)uartCtx;

    /* TODO: Implement write call to your UART HW */
    rc = socket_write(uartCtx->socket_id,(const char *)buffer,buffer_len,timeout_ms);
    return rc;
}

static int NetRead(void *context, byte* buffer, int buffer_len,
    int timeout_ms)
{
    UartContext *uartCtx = (UartContext*)context;
    (void)uartCtx;

    /* TODO: Implement read call to your UART HW */
    int rc;

    rc = socket_read(uartCtx->socket_id,(char *)buffer,buffer_len,timeout_ms);

    if (rc == SOCKET_ERR_TIMEOUT) {
        rc = MQTT_CODE_ERROR_TIMEOUT;
    }

    return rc;
}

static int NetDisconnect(void *context)
{
    UartContext *uartCtx = (UartContext*)context;
    (void)uartCtx;
    int rc;
    rc = socket_close(uartCtx->socket_id);
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
