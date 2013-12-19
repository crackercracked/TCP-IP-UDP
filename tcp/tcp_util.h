#ifndef _TCP_UTIL_H_
#define _TCP_UTIL_H_

//================================================================================================= 
//      Author: Brett Decker and Xinwei Liu
//=================================================================================================
//      INCLUDE FILES
//=================================================================================================

#include "utility.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <inttypes.h>


#define __FAVOR_BSD 
#include <netinet/tcp.h>

#include "net_layer.h"

//=================================================================================================
//      DEFINITIONS AND MACROS
//=================================================================================================

#define TCP_HEADER_SIZE 20
#define TCP_MTU         1350 //(IP_MAX_PACKET_SIZE-IP_HEADER_SIZE-TCP_HEADER_SIZE) // 65495

#define PSEUDO_TCPHDR_SIZE 12

typedef struct{
  struct tcphdr tcp_header;
  char tcp_data[TCP_MTU];
} tcp_packet_t;


//pseudo header for computing tcp checksum
typedef struct{
  uint32_t source_addr;
  uint32_t dest_addr;
  uint8_t reserved; //equals 0
  uint8_t protocol;
  uint16_t tcp_len;
} pseudo_tcphdr_t;


//=================================================================================================
//      GLOBAL VARIABLES
//=================================================================================================


//=================================================================================================
//      PUBLIC FUNCTIONS
//=================================================================================================

uint16_t tcp_checksum(char* packet, size_t len);

void buildTCPPacket(tcp_packet_t* tcp_packet, uint32_t saddr, uint32_t daddr, u_short sport, u_short dport, uint32_t seqnum, uint32_t ack, u_char flag, u_short wsize, char* data, size_t data_len);

void printTCPPacket(tcp_packet_t* tcp_packet);


uint32_t convertVIPString2Int(char* vip_address);
char* convertVIPInt2String(uint32_t vip_addr_num);



//=================================================================================================
//      END OF FILE
//=================================================================================================

#endif //_TCP_UTIL_H_
