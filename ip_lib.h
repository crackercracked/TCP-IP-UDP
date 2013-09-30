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

//#define TRUE                1
//#define FALSE               0

#define VIP_ADDRESS_SIZE    25

#define IP_MAX_PACKET_SIZE      64000

#define IPv4                    0x4
#define IP_HEADER_BYTES         20
#define IP_HEADER_WORDS         5
#define IP_TTL_DEFAULT          16


//=================================================================================================
//      PUBLIC TYPEDEFS AND STRUCTS
//=================================================================================================

typedef enum ip_protocol {

    TEST_PROTOCOL = 0x00,
    TCP_PROTOCOL  = 0x06,
    RIP_PROTOCOL  = 0xC8    // (200 decimal)

} ip_protocol_t;

typedef struct {

    ip_header_t ip_header;
    char ip_data[IP_MAX_PACKET_SIZE];
    
} ip_packet_t;


//=================================================================================================
//      PUBLIC FUNCTION DECLARATIONS
//=================================================================================================

void initializeIPHeader(ip_header_t* ip_header);
int convertVIPString2Int(char vip_address[VIP_ADDRESS_SIZE]);
void printOutLinks(list_t* interfaces_list);
void printInterfaces(list_t* interfaces_list);


//=================================================================================================
//      END OF FILE
//=================================================================================================

#endif // _IP_LIB_H_
