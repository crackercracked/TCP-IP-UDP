//================================================================================================= 
//      Author: Brett Decker and Xinwei Liu
//=================================================================================================
//      INCLUDE FILES
//=================================================================================================

#include "ip_lib.h"

//=================================================================================================
//      PUBLIC FUNCTIONS
//=================================================================================================

void initializeIPHeader(ip_header_t* ip_header)
{
    memset(ip_header, 0, IP_HEADER_BYTES);
    
    // Default settings of IP header
    
    ip_header->ip_v = IPv4;
    ip_header->ip_hl = IP_HEADER_WORDS;
    ip_header->ip_off = IP_DF; // defined in netinet/ip.h
    ip_header->ip_ttl = IP_TTL_DEFAULT;
    
}


int convertVIPString2Int(char vip_address[VIP_ADDRESS_SIZE]) 
{
    int vip_integer, byte;
    int vip_bytes[4];
    
    sscanf(vip_address, "%d.%d.%d.%d", vip_bytes, vip_bytes+1, vip_bytes+2, vip_bytes+3);
    
    vip_integer = 0;
    for (byte = 0; byte < 4; byte++) {
        if ((vip_bytes[byte] < 0) || (vip_bytes[byte] > 255)) {
            return ERROR;
        }
        vip_integer |= (vip_bytes[byte] << (24 - (byte*8)));  //  vip_bytes[0] is MSb
    }

    return vip_integer;
}


void printOutLinks(list_t* interfaces_list)
{
    link_t* link;
    int linkIndex;
    
    node_t* current;
    
    printf("\n--Begin Links--\n");
    
    linkIndex = 0;
    for (current = interfaces_list->head; current != NULL; current = current->next) {
        link = (current->data);
        
        printf("%d - Local Host: \"%s\", Port: %d", linkIndex, \
            link->local_phys_host, link->local_phys_port);
        
        printf("-> VIP %d.%d.%d.%d\n", MASK_BYTE1(link->local_virt_ip.s_addr), MASK_BYTE2(link->local_virt_ip.s_addr), \
            MASK_BYTE3(link->local_virt_ip.s_addr), MASK_BYTE4(link->local_virt_ip.s_addr));
        
        printf("%d - Remote Host: \"%s\", Port: %d", linkIndex, \
            link->remote_phys_host, link->remote_phys_port);
        printf("-> VIP %d.%d.%d.%d\n", MASK_BYTE1(link->remote_virt_ip.s_addr), MASK_BYTE2(link->remote_virt_ip.s_addr), \
            MASK_BYTE3(link->remote_virt_ip.s_addr), MASK_BYTE4(link->remote_virt_ip.s_addr));
                
        linkIndex++;
    }
    printf("--End Links--\n\n");
}


void printInterfaces(list_t* interfaces_list)
{
    link_t* link;
    int interface;
    
    node_t* current;
    
    printf("\n--Begin Interfaces--\n");
    
    interface = 0;
    for (current = interfaces_list->head; current != NULL; current = current->next) {
        link = (current->data);
        
        printf("%d - VIP %d.%d.%d.%d -> ", interface, MASK_BYTE1(link->local_virt_ip.s_addr), \
            MASK_BYTE2(link->local_virt_ip.s_addr), MASK_BYTE3(link->local_virt_ip.s_addr), \
            MASK_BYTE4(link->local_virt_ip.s_addr));
        
        printf("VIP %d.%d.%d.%d\n", MASK_BYTE1(link->remote_virt_ip.s_addr), MASK_BYTE2(link->remote_virt_ip.s_addr), \
            MASK_BYTE3(link->remote_virt_ip.s_addr), MASK_BYTE4(link->remote_virt_ip.s_addr));
                
        interface++;
    }
    printf("--End Interfaces--\n\n");
}

//=================================================================================================
//      END OF FILE
//=================================================================================================
