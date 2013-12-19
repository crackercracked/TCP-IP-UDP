#ifndef _PORT_UTIL_H_
#define _PORT_UTIL_H_

//================================================================================================= 
//      Author: Brett Decker and Xinwei Liu
//=================================================================================================
//      INCLUDE FILES
//=================================================================================================

#include "utility.h"


//=================================================================================================
//      PUBLIC FUNCTIONS
//=================================================================================================
void ptu_printAddrTup2Socket();

void ptu_initializeTables(void);

void ptu_freeTables(void);

bool ptu_getPort(uint16_t port);

uint16_t ptu_getAvailablePort(void);

void ptu_releasePort(uint16_t port);

void ptu_removeSocketAddr(uint32_t local_vip, uint16_t local_port, uint32_t remote_vip, uint16_t remote_port);

void ptu_addSocketAtAddr(uint32_t local_vip, uint16_t local_port, uint32_t remote_vip, uint16_t remote_port, int vsocket);

int ptu_getSocketAtAddr(uint32_t local_vip, uint16_t local_port, uint32_t remote_vip, uint16_t remote_port);


//=================================================================================================
//      END OF FILE
//=================================================================================================

#endif //_PORT_UTIL_H_
