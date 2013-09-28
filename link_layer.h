#ifndef _LINK_LAYER_H_
#define _LINK_LAYER_H_

//================================================================================================= 
//      Author: Brett Decker and Xinwei Liu
//=================================================================================================
//      INCLUDE FILES
//=================================================================================================

//=================================================================================================
//      DEFINITIONS AND MACROS
//=================================================================================================

//=================================================================================================
//      PUBLIC FUNCTION DECLARATIONS
//=================================================================================================

void sendPacket(void);
void receivePacket(void);
void setupForwardingTableAndSockets(list_t* list);

//=================================================================================================
//      END OF FILE
//=================================================================================================

#endif // _LINK_LAYER_H_
