//================================================================================================= 
//      Author: Brett Decker and Xinwei Liu
//=================================================================================================
//      INCLUDE FILES
//=================================================================================================

#include "utility.h"

//=================================================================================================
//      DEFINITIONS AND MACROS
//=================================================================================================

#define PROTOCOL_DEFINED    0x0A5A5BED

//=================================================================================================
//      GLOBAL VARIABLES
//=================================================================================================

static handler_t g_handlers[256];   // Max possible values for IP header Protocol
static int g_protocol_defined[256]; // Max possible values for IP header Protocol

//=================================================================================================
//      PUBLIC FUNCTIONS
//=================================================================================================

void util_netRegisterHandler(uint8_t protocol, handler_t handler)
{
    g_protocol_defined[protocol] = PROTOCOL_DEFINED;
    g_handlers[protocol] = handler;
}


void util_callHandler(uint8_t protocol, ip_packet_t* ip_packet)
{
    if (g_protocol_defined[protocol] == PROTOCOL_DEFINED) {
        (g_handlers[protocol])(ip_packet);
    }
}


uint32_t util_convertVIPString2Int(char* vip_address) 
{
    uint32_t vip_integer, byte;
    uint32_t vip_bytes[4];
    
    sscanf(vip_address, "%d.%d.%d.%d", vip_bytes, vip_bytes+1, vip_bytes+2, vip_bytes+3);
    
    vip_integer = 0;
    for (byte = 0; byte < 4; byte++) {
        if ((vip_bytes[byte] < 0) || (vip_bytes[byte] > 255)) {
            return ERROR;
        }
        vip_integer |= (vip_bytes[byte] << (24 - (byte*8)));  //  vip_bytes[0] is MSB
    }

    return vip_integer;
}


char* util_convertVIPInt2String(uint32_t vip_addr_num)
{
    char* buffer=(char*)malloc(50);
    sprintf(buffer, "%d.%d.%d.%d", MASK_BYTE1(vip_addr_num), MASK_BYTE2(vip_addr_num), \
        MASK_BYTE3(vip_addr_num), MASK_BYTE4(vip_addr_num));

    return buffer;
}

char* util_convertVIPInt2StringNoPeriod(uint32_t vip_addr_num)
{
    char* buffer=(char*)malloc(50);
    sprintf(buffer, "%d-%d-%d-%d", MASK_BYTE1(vip_addr_num), MASK_BYTE2(vip_addr_num), \
        MASK_BYTE3(vip_addr_num), MASK_BYTE4(vip_addr_num));

    return buffer;
}

uint16_t util_getU16(unsigned char* buffer)
{
    return (((uint16_t)(buffer[0]) << 8) + (uint16_t)(buffer[1]));
}


uint32_t util_getU32(unsigned char* buffer)
{
    return (((uint16_t)(buffer[0]) << 24) + ((uint16_t)(buffer[1]) << 16) + \
        ((uint16_t)(buffer[2]) << 8) + (uint16_t)(buffer[3])); 
}


uint32_t util_min(uint32_t val1, uint32_t val2)
{
    return val1 >= val2 ? val2 : val1;
}

uint32_t util_max(uint32_t val1, uint32_t val2)
{
    return val1 <= val2 ? val2 : val1;
}

double util_abs(double val)
{
    return val < 0 ? -val : val;
}


//=================================================================================================
//      END OF FILE
//=================================================================================================
