//================================================================================================= 
//      Author: Brett Decker and Xinwei Liu
//=================================================================================================
//      INCLUDE FILES
//=================================================================================================

#include "util/htable.h"

#include "ip_lib.h"

//=================================================================================================
//      DEFINITIONS AND MACROS
//=================================================================================================

#define UDP_FRAME_SIZE      1400

//=================================================================================================
//      GLOBAL VARIABLES
//=================================================================================================

static htable* s_forwarding_table;
static pthread_mutex_t g_forwarding_table_mutex = PTHREAD_MUTEX_INITIALIZER; // do we need this?

void sendPacket(void) // TODO determine parameters
{
    // Send up to 64k bytes in 1400 byte increments
    
}

void receivePacket(void) // TODO determine parameters
{
    // Read up to 64k bytes in 1400 byte increments
}

void setupForwardingTableAndSockets(list_t* list)
{
    // Create socket for each inferface (2 interfaces per link_t)
    // Add socket with VIP to Forwarding Table
    // Also add socket in a global file descriptor set (for select())
    
}


//=================================================================================================
//      END OF FILE
//=================================================================================================
