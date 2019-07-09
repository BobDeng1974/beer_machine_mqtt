/*****************************************************************************
*  NTP同步UTC                                                          
*  Copyright (C) 2019 wkxboot 1131204425@qq.com.                             
*                                                                            
*                                                                            
*  This program is free software; you can redistribute it and/or modify      
*  it under the terms of the GNU General Public License version 3 as         
*  published by the Free Software Foundation.                                
*                                                                            
*  @file     ntp.c                                                   
*  @brief    NTP同步UTC                                                                                                                                                                                             
*  @author   wkxboot                                                      
*  @email    1131204425@qq.com                                              
*  @version  v1.0.0                                                  
*  @date     2019/1/11                                            
*  @license  GNU General Public License (GPL)                                
*                                                                            
*                                                                            
*****************************************************************************/

#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"
#include "string.h"
#include "socket.h"
#include "ntp.h"
#include "tasks_init.h"
#include "log.h"

#define  NTP_TIMEOUT                      5000


#define big_little_swap16(A)              ((((uint16_t)(A) & 0xff00) >> 8) |\
                                          (((uint16_t)(A) & 0x00ff) << 8))
 
 
#define big_little_swap32(A)              ((((uint32_t)(A) & 0xff000000) >> 24)|\
                                          (((uint32_t)(A) & 0x00ff0000) >> 8)  |\
                                          (((uint32_t)(A) & 0x0000ff00) << 8)  |\
                                          (((uint32_t)(A) & 0x000000ff) << 24))

// Structure that defines the 48 byte NTP packet protocol.
  typedef struct
  {

    uint8_t li_vn_mode;      // Eight bits. li, vn, and mode.
                             // li.   Two bits.   Leap indicator.
                             // vn.   Three bits. Version number of the protocol.
                             // mode. Three bits. Client will pick mode 3 for client.

    uint8_t stratum;         // Eight bits. Stratum level of the local clock.
    uint8_t poll;            // Eight bits. Maximum interval between successive messages.
    uint8_t precision;       // Eight bits. Precision of the local clock.

    uint32_t rootDelay;      // 32 bits. Total round trip delay time.
    uint32_t rootDispersion; // 32 bits. Max error aloud from primary clock source.
    uint32_t refId;          // 32 bits. Reference clock identifier.

    uint32_t refTm_s;        // 32 bits. Reference time-stamp seconds.
    uint32_t refTm_f;        // 32 bits. Reference time-stamp fraction of a second.

    uint32_t origTm_s;       // 32 bits. Originate time-stamp seconds.
    uint32_t origTm_f;       // 32 bits. Originate time-stamp fraction of a second.

    uint32_t rxTm_s;         // 32 bits. Received time-stamp seconds.
    uint32_t rxTm_f;         // 32 bits. Received time-stamp fraction of a second.

    uint32_t txTm_s;         // 32 bits and the most important field the client cares about. Transmit time-stamp seconds.
    uint32_t txTm_f;         // 32 bits. Transmit time-stamp fraction of a second.

  } ntp_packet;              // Total: 384 bits or 48 bytes.


#define NTP_TIMESTAMP_DELTA 2208988800ull

#define LI(packet)   (uint8_t) ((packet.li_vn_mode & 0xC0) >> 6) // (li   & 11 000 000) >> 6
#define VN(packet)   (uint8_t) ((packet.li_vn_mode & 0x38) >> 3) // (vn   & 00 111 000) >> 3
#define MODE(packet) (uint8_t) ((packet.li_vn_mode & 0x07) >> 0) // (mode & 00 000 111) >> 0

  
/* t0==client send time  t1==server recv time  t2==server send time   t3==client recv time*/


int ntp_sync_time(uint32_t *new_time)
{
  uint32_t t0,t1,t2,t3,offset,delay;
  int sockfd, n; // Socket file descriptor and the n return result from writing/reading from the socket.
  int portno = 123; // NTP UDP port number.
  /*中国科学院国家授时中心*/
  const char *host1_name = "ntp.ntsc.ac.cn"; // NTP server host-name.
  /*阿里云NTP*/
  const char *host2_name = "ntp1.aliyun.com"; // NTP server host-name.
  static const char *host_name= NULL;
  if(host_name == NULL){
  host_name = host1_name;
  }
  // Create and zero out the packet. All 48 bytes worth.
  ntp_packet packet = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

  memset( &packet, 0, sizeof( ntp_packet ) );

  // Set the first byte's bits to 00,011,011 for li = 0, vn = 3, and mode = 3. The rest will be left set to zero.

  *( ( char * ) &packet + 0 ) = 0x1b; // Represents 27 in base 10 or 00011011 in base 2.

  // Create a UDP socket, convert the host-name to an IP address, set the port number,
  // connect to the server, send the packet, and then read in the return packet.

  sockfd = socket_connect(host_name,portno,SOCKET_PROTOCOL_UDP);; // Create a UDP socket.
  if(sockfd < 0 ){
  log_error( "ERROR opening socket\r\n" );
  return -1;
  }
  /*系统时间 单位S*/
  t0 = osKernelSysTick() / 1000;
  /*添加离开时系统时间*/
  packet.origTm_s = big_little_swap32(t0);
  
 // Send it the NTP packet it wants. If n == -1, it failed.
  n = socket_send(sockfd, ( char* ) &packet, sizeof( ntp_packet ) ,NTP_TIMEOUT);
  if( n < 0 ){
  log_error( "ERROR writing to socket\r\n" );
  /*关闭socket*/
  socket_disconnect(sockfd);
  return -1;
  }
  
  // Wait and receive the packet back from the server. If n == -1, it failed.
  n = socket_recv(sockfd, ( char* ) &packet, sizeof( ntp_packet ) ,NTP_TIMEOUT);
  if ( n < 0 ){
  log_error( "ERROR reading from socket\r\n" );
  /*关闭socket*/
  socket_disconnect(sockfd); 
  return -1;
  }
  /*关闭socket*/
  socket_disconnect(sockfd);  
  /*收到了指定的数据 -没有未同步警告，服务器模式*/
  if(n == sizeof( ntp_packet ) && LI(packet)!= 3){
  /*收到消息的时间*/
  t3 = osKernelSysTick() / 1000;
  // These two fields contain the time-stamp seconds as the packet left the NTP server.
  // The number of seconds correspond to the seconds passed since 1900.
  // big_little_swap32() converts the bit/byte order from the network's to host's "endianness".
  
  packet.txTm_s = big_little_swap32( packet.txTm_s );   // Time-stamp seconds.
  //packet.txTm_f = big_little_swap32( packet.txTm_f ); // Time-stamp fraction of a second.
  packet.rxTm_s = big_little_swap32( packet.rxTm_s );   // Time-stamp seconds.
  // Extract the 32 bits that represent the time-stamp seconds (since NTP epoch) from when the packet left the server.
  // Subtract 70 years worth of seconds from the seconds since 1900.
  // This leaves the seconds since the UNIX epoch of 1970.
  // (1900)------------------(1970)**************************************(Time Packet Left the Server)
  
  t1 = ( uint32_t ) ( packet.rxTm_s - NTP_TIMESTAMP_DELTA );
  t2 = ( uint32_t ) ( packet.txTm_s - NTP_TIMESTAMP_DELTA );
  //Print the time we got from the server, accounting for local timezone and conversion from UTC time.
  delay = (t3 - t0) - (t2 - t1);
  offset = ((t1 - t0) + (t2 - t3))/ 2;
  *new_time = offset + t3;
  log_debug("time delay:%d S.offset:%d S.cur time:%d S.\r\n",delay,offset,*new_time);
  return 0;
  }
  /*切换授时域名*/
  if(host_name == host1_name){
  host_name = host2_name ;
  }else{
  host_name = host1_name ;
  }  
 log_error("ntp recv bad data.\r\n");
 return -1; 
 }