//================================================================================================= 
//      Author: Brett Decker and Xinwei Liu
//=================================================================================================
//      INCLUDE FILES
//=================================================================================================

#include "ip_lib.h"
#include "util/list.h"
#include "util/parselinks.h"

//=================================================================================================
//      PUBLIC FUNCTIONS
//=================================================================================================

int convertVIPStringToInt(char vip_address[VIP_ADDRESS_SIZE]) 
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


void printInterfaces(list_t* interfaces_list)
{
    link_t* link;
    int interface;
    
    node_t* current;
    
    printf("\n--Begin Intefaces--\n");
    
    interface = 0;
    for (current = interfaces_list->head; current != NULL; current = current->next) {
        link = (current->data);
        
        printf("%d - Local Host: \"%s\", Port: %d", interface, \
            link->local_phys_host, link->local_phys_port);
        
        printf("-> VIP %d.%d.%d.%d\n", MASK_BYTE1(link->local_virt_ip.s_addr), MASK_BYTE2(link->local_virt_ip.s_addr), \
            MASK_BYTE3(link->local_virt_ip.s_addr), MASK_BYTE4(link->local_virt_ip.s_addr));
           
        interface++;
        
        printf("%d - Remote Host: \"%s\", Port: %d", interface, \
            link->remote_phys_host, link->remote_phys_port);
        printf("-> VIP %d.%d.%d.%d\n", MASK_BYTE1(link->remote_virt_ip.s_addr), MASK_BYTE2(link->remote_virt_ip.s_addr), \
            MASK_BYTE3(link->remote_virt_ip.s_addr), MASK_BYTE4(link->remote_virt_ip.s_addr));
                
        interface++;
    }
    printf("--End Intefaces--\n\n");
}

//=================================================================================================
//      END OF FILE
//=================================================================================================
