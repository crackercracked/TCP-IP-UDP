#ifndef _SLIDING_WINDOW_H_
#define _SLIDING_WINDOW_H_

//================================================================================================= 
//      Author: Brett Decker and Xinwei Liu
//=================================================================================================
//      INCLUDE FILES
//=================================================================================================

#include "utility.h"


//=================================================================================================
//      DEFINITIONS AND MACROS
//=================================================================================================

/*use struct for argument at current as don't what potential field needed in the future, 
  could change to int if it finally turns out no other fields needed*/
struct handleFuncArg{
   int socket;
   
};

struct sendFuncArg{
   int socket;
};


//=================================================================================================
//      GLOBAL VARIABLES
//=================================================================================================

//=================================================================================================
//      PUBLIC FUNCTIONS
//=================================================================================================

bool sw_hasSentAllData(int vsocket);

int sw_writeData(int vsocket, const unsigned char* buf, uint32_t nbyte);

int sw_readData(int vsocket, const unsigned char* buf, uint32_t nbyte);

void* sw_socketHandlePacketThreadFunc(void* arg);

void* sw_socketSendDataThreadFunc(void* arg);

//=================================================================================================
//      END OF FILE
//=================================================================================================

#endif //_SLIDING_WINDOW_H_
