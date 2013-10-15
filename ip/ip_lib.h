#ifndef _IP_LIB_H_
#define _IP_LIB_H_

//================================================================================================= 
//      Author: Brett Decker and Xinwei Liu
//=================================================================================================
//      INCLUDE FILES
//=================================================================================================

#include "utility.h"


//=================================================================================================
//      DEFINITIONS AND MACROS
//=================================================================================================

#define VIP_ADDRESS_SIZE    25

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
//      PUBLIC TYPEDEFS AND STRUCTS
//=================================================================================================


//=================================================================================================
//      PUBLIC FUNCTION DECLARATIONS
//=================================================================================================

void printSendIPPacketResult(int result, bool debug);

int convertVIPString2Int(char* vip_address);

void printRIPPacket(char* data);

void printIPPacket(ip_packet_t* ip_packet);

void printIPPacketData(ip_packet_t* ip_packet);


//=================================================================================================
//      END OF FILE
//=================================================================================================

#endif //_IP_LIB_H_
