//================================================================================================= 
//      Author: Brett Decker and Xinwei Liu
//=================================================================================================
//      INCLUDE FILES
//=================================================================================================

#include "ip_lib.h"

//=================================================================================================
//      DEFINITIONS AND MACROS
//=================================================================================================



//=================================================================================================
//      GLOBAL VARIABLES
//=================================================================================================



//=================================================================================================
//      PUBLIC FUNCTIONS
//=================================================================================================

void printSendIPPacketResult(int result, bool debug)
{
    if (result == LINK_DOWN) {
        printf("Packet dropped because link is down.\n");
    } else if (result == UNKNOWN_DESTINATION) {
        printf("Packet dropped because destination is unknown.\n");
    } else if (result == INFINITY_DISTANCE) {
        printf("Packet dropped because route to destination is unknown.\n");
    } else if ((result == SENT_IP_PACKET) && (debug == true)) {
        printf("Packet sent successfully.\n");
    }
}


void printRIPPacket(char* data)
{
    int read = 0;
    int entries = 0;
    int i;

    printf("\n--RIP Packet--\n");
    printf("Command: %d\n", util_getU16((unsigned char*)data));
    read += 2;
    
    entries = ntohs(util_getU16((unsigned char*)(data+read)));
    printf("Entries: %d\n", entries);
    read += 2;

    for (i = 0; i < entries; i++) {
    
        printf("Cost: %d\n", ntohl(util_getU32((unsigned char*)(data+read))));
        read += 4;
        
        printf("Dest: %d\n", ntohl(util_getU32((unsigned char*)(data+read))));
        read += 4;
    }
    
    printf("--End RIP Packet--\n\n");
}


void printIPPacket(ip_packet_t* ip_packet)
{
    int i;
    uint32_t value = 0;
    int data_len = 0;
    
    printf("\n--IP Packet--\n");
    
    printf("Version: 0x%X\n", ip_packet->ip_header.ip_v);
    printf("Header Length: %d\n", ip_packet->ip_header.ip_hl);
    printf("Type of Service: 0x%X\n", ip_packet->ip_header.ip_tos);
    printf("Total Length: %d\n", ntohs(ip_packet->ip_header.ip_len));
    printf("ID: 0x%X\n", ip_packet->ip_header.ip_id);
    printf("Offset: 0x%X\n", ip_packet->ip_header.ip_off);
    printf("TTL: %d\n", ip_packet->ip_header.ip_ttl);
    printf("Protocol: %d\n", ip_packet->ip_header.ip_p);
    printf("Checksum (network): 0x%X\n", ip_packet->ip_header.ip_sum);
    printf("Checksum (host): 0x%X\n", ntohs(ip_packet->ip_header.ip_sum));
    
    value = ntohl(ip_packet->ip_header.ip_src);
    printf("Source: %d.%d.%d.%d\n", MASK_BYTE1(value), MASK_BYTE2(value), MASK_BYTE3(value), MASK_BYTE4(value));
    value = ntohl(ip_packet->ip_header.ip_dst);
    printf("Destination: %d.%d.%d.%d\n", MASK_BYTE1(value), MASK_BYTE2(value), MASK_BYTE3(value), MASK_BYTE4(value));
    
    data_len = ntohs(ip_packet->ip_header.ip_len) - (4*ip_packet->ip_header.ip_hl);
    printf("Data: ");
    for (i = 0; i < data_len; i++) {
        printf("%c", ip_packet->ip_data[i]);
    }
    
    printf("\n--End IP Packet--\n\n");
}

void printIPPacketData(ip_packet_t* ip_packet)
{
    int i;
    int data_len = 0;
    
    data_len = ntohs(ip_packet->ip_header.ip_len) - (4*ip_packet->ip_header.ip_hl);
    //printf("data_len: %d\n", data_len);
    
    for (i = 0; i < data_len; i++) {
        printf("%c", ip_packet->ip_data[i]);
    }
}


//=================================================================================================
//      END OF FILE
//=================================================================================================
