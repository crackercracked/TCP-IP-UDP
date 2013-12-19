#ifndef _STATE_MACHINE_H_
#define _STATE_MACHINE_H_

//================================================================================================= 
//      Author: Brett Decker and Xinwei Liu
//=================================================================================================
//      INCLUDE FILES
//=================================================================================================

#include "utility.h"

//=================================================================================================
//      DEFINITIONS AND MACROS
//=================================================================================================

typedef enum TCP_State {

    TCPS_CLOSED = 0,
    TCPS_LISTEN,
    TCPS_SYN_SENT,
    TCPS_SYN_RCVD,
    TCPS_ESTAB,
    TCPS_FIN_WAIT1,
    TCPS_FIN_WAIT2,
    TCPS_CLOSING,
    TCPS_TIME_WAIT,
    TCPS_CLOSE_WAIT,
    TCPS_LAST_ACK,
    TCPS_INVALID        // This state is not a valid TCP State

} TCP_State_t;


typedef enum TCP_Action {

    // Open Actions
    TCPA_ACTIVE_OPEN = 0,
    TCPA_PASSIVE_OPEN,
    
    // Handshake Actions
    TCPA_RECV_SYN,
    TCPA_SEND_SYN,
    TCPA_RECV_ACK,
    TCPA_RECV_SYN_ACK,
    
    // Read/Write Actions
    TCPA_RECEIVE,
    TCPA_SEND,
    
    // Close Actions
    TCPA_CLOSE,
    TCPA_SEND_FIN,
    TCPA_RECV_FIN,
    TCPA_RECV_ACK_FIN,
    TCPA_TIMEOUT

} TCP_Action_t;


//=================================================================================================
//      GLOBAL VARIABLES
//=================================================================================================

//=================================================================================================
//      PUBLIC FUNCTIONS
//=================================================================================================

bool isValidAction(int vsocket, TCP_Action_t action, int* error);

void changeState(int vsocket, TCP_Action_t action);

void printStateAsString(TCP_State_t state);

void printStateDebug(int vsocket);

//=================================================================================================
//      END OF FILE
//=================================================================================================

#endif //_STATE_MACHINE_H_
