#ifndef _LINK_LAYER_H_
#define _LINK_LAYER_H_

//================================================================================================= 
//      Author: Brett Decker and Xinwei Liu
//=================================================================================================
//      INCLUDE FILES
//=================================================================================================

#include "utility.h"

#include <glib.h>


extern GHashTable* g_read_socket_addr_lookup;

extern fd_set g_input_interfaces;           // All input UDP sockets and STDIN [used by select()]
extern int g_highest_input_interface;       // Highest file descriptor in g_input_interfaces

//=================================================================================================
//      DEFINITIONS AND MACROS
//=================================================================================================


//=================================================================================================
//      PUBLIC FUNCTION DECLARATIONS
//=================================================================================================

void link_sendPacket(char* payload, size_t payload_len, uint32_t vip_address);

void link_receivePacket(char* payload, size_t* payload_len, int socket);

void link_setupForwardingTableAndSockets(list_t* list);


//=================================================================================================
//      END OF FILE
//=================================================================================================

#endif //_LINK_LAYER_H_
