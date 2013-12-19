#ifndef _NET_LAYER_H_
#define _NET_LAYER_H_

//================================================================================================= 
//      Author: Brett Decker and Xinwei Liu
//=================================================================================================
//      INCLUDE FILES
//=================================================================================================

#include "utility.h"

#include "link_layer.h"

#include <glib.h>

//=================================================================================================
//      DEFINITIONS AND MACROS
//=================================================================================================

#define VIP_ADDRESS_SIZE        25

#define IPv4                    0x4
#define IP_HEADER_BYTES         20
#define IP_HEADER_WORDS         5
#define IP_MAX_TTL              16

#define DEFAULT_ROUTE_TTL       12

#define RIP_REQUEST_COMMAND     1
#define RIP_RESPONSE_COMMAND    2

#define INFINITY_DISTANCE       16

#define LINK_DOWN               -1
#define UNKNOWN_DESTINATION     -11
#define RECEIVED_IP_PACKET      0x11
#define SENT_IP_PACKET          0x16


//=================================================================================================
//      GLOBAL VARIABLES
//=================================================================================================


//=================================================================================================
//      PUBLIC FUNCTIONS
//=================================================================================================

void* net_routingThreadFunction(void* arg);

void net_sendMessage(uint32_t to_vip_address, uint8_t protocol, char* data, size_t data_len);

void net_setupTables(char* link_file);

void net_freeTables(void);

bool net_handlePacketInput(int interface);

void net_printNetworkInterfaces(void);

void net_printNetworkRoutes(void);

void net_downInterface(int interface);

void net_upInterface(int interface);

void net_handleTestPacket(ip_packet_t* ip_packet);

void net_handleRIPPacket(ip_packet_t* ip_packet);


//=================================================================================================
//      END OF FILE
//=================================================================================================

#endif //_NET_LAYER_H_
