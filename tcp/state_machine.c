//================================================================================================= 
//      Author: Brett Decker and Xinwei Liu
//=================================================================================================
//      INCLUDE FILES
//=================================================================================================

#include "tcp_layer.h"
#include "state_machine.h"


//=================================================================================================
//      DEFINITIONS AND MACROS
//=================================================================================================



//=================================================================================================
//      PRIVATE VARIABLES
//=================================================================================================

TCP_State_t stateChangeForClosed[] = {
    
    TCPS_SYN_SENT,      // TCPA_ACTIVE_OPEN
    TCPS_LISTEN,        // TCPA_PASSIVE_OPEN
    
    TCPS_INVALID,       // TCPA_RECV_SYN
    TCPS_INVALID,       // TCPA_SEND_SYN
    TCPS_INVALID,       // TCPA_RECV_ACK
    TCPS_INVALID,       // TCPA_RECV_SYN_ACK
    
    TCPS_INVALID,       // TCPA_RECEIVE
    TCPS_INVALID,       // TCPA_SEND
    
    TCPS_CLOSED,        // TCPA_CLOSE
    TCPS_INVALID,       // TCPA_SEND_FIN
    TCPS_INVALID,       // TCPA_RECV_FIN
    TCPS_INVALID,       // TCPA_RECV_ACK_FIN
    TCPS_INVALID        // TCPA_TIMEOUT
};

TCP_State_t stateChangeForListen[] = {
    
    TCPS_INVALID,       // TCPA_ACTIVE_OPEN
    TCPS_INVALID,       // TCPA_PASSIVE_OPEN
    
    TCPS_SYN_RCVD,      // TCPA_RECV_SYN
    TCPS_SYN_SENT,      // TCPA_SEND_SYN
    TCPS_INVALID,       // TCPA_RECV_ACK
    TCPS_INVALID,       // TCPA_RECV_SYN_ACK
    
    TCPS_INVALID,       // TCPA_RECEIVE
    TCPS_INVALID,       // TCPA_SEND
    
    TCPS_CLOSED,        // TCPA_CLOSE
    TCPS_INVALID,       // TCPA_SEND_FIN
    TCPS_INVALID,       // TCPA_RECV_FIN
    TCPS_INVALID,       // TCPA_RECV_ACK_FIN
    TCPS_INVALID        // TCPA_TIMEOUT
};

TCP_State_t stateChangeForSYNSent[] = {
    
    TCPS_INVALID,       // TCPA_ACTIVE_OPEN
    TCPS_INVALID,       // TCPA_PASSIVE_OPEN
    
    TCPS_SYN_RCVD,      // TCPA_RECV_SYN
    TCPS_INVALID,       // TCPA_SEND_SYN
    TCPS_INVALID,       // TCPA_RECV_ACK
    TCPS_ESTAB,         // TCPA_RECV_SYN_ACK
    
    TCPS_INVALID,       // TCPA_RECEIVE
    TCPS_INVALID,       // TCPA_SEND
    
    TCPS_CLOSED,        // TCPA_CLOSE
    TCPS_INVALID,       // TCPA_SEND_FIN
    TCPS_INVALID,       // TCPA_RECV_FIN
    TCPS_INVALID,       // TCPA_RECV_ACK_FIN
    TCPS_INVALID        // TCPA_TIMEOUT
};

TCP_State_t stateChangeForSYNRcvd[] = {
    
    TCPS_INVALID,       // TCPA_ACTIVE_OPEN
    TCPS_INVALID,       // TCPA_PASSIVE_OPEN
    
    TCPS_INVALID,       // TCPA_RECV_SYN
    TCPS_INVALID,       // TCPA_SEND_SYN
    TCPS_ESTAB,         // TCPA_RECV_ACK
    TCPS_INVALID,       // TCPA_RECV_SYN_ACK
    
    TCPS_INVALID,       // TCPA_RECEIVE
    TCPS_INVALID,       // TCPA_SEND
    
    TCPS_INVALID,       // TCPA_CLOSE
    TCPS_FIN_WAIT1,     // TCPA_SEND_FIN
    TCPS_INVALID,       // TCPA_RECV_FIN
    TCPS_INVALID,       // TCPA_RECV_ACK_FIN
    TCPS_INVALID        // TCPA_TIMEOUT
};

TCP_State_t stateChangeForEstab[] = {
    
    TCPS_INVALID,       // TCPA_ACTIVE_OPEN
    TCPS_INVALID,       // TCPA_PASSIVE_OPEN
    
    TCPS_INVALID,       // TCPA_RECV_SYN
    TCPS_INVALID,       // TCPA_SEND_SYN
    TCPS_INVALID,       // TCPA_RECV_ACK
    TCPS_INVALID,       // TCPA_RECV_SYN_ACK
    
    TCPS_ESTAB,         // TCPA_RECEIVE
    TCPS_ESTAB,         // TCPA_SEND
    
    TCPS_INVALID,       // TCPA_CLOSE
    TCPS_FIN_WAIT1,     // TCPA_SEND_FIN
    TCPS_CLOSE_WAIT,    // TCPA_RECV_FIN
    TCPS_INVALID,       // TCPA_RECV_ACK_FIN
    TCPS_INVALID        // TCPA_TIMEOUT
};

TCP_State_t stateChangeForFINWait1[] = {
    
    TCPS_INVALID,       // TCPA_ACTIVE_OPEN
    TCPS_INVALID,       // TCPA_PASSIVE_OPEN
    
    TCPS_INVALID,       // TCPA_RECV_SYN
    TCPS_INVALID,       // TCPA_SEND_SYN
    TCPS_INVALID,       // TCPA_RECV_ACK
    TCPS_INVALID,       // TCPA_RECV_SYN_ACK
    
    TCPS_FIN_WAIT1,     // TCPA_RECEIVE
    TCPS_INVALID,       // TCPA_SEND
    
    TCPS_INVALID,       // TCPA_CLOSE
    TCPS_INVALID,       // TCPA_SEND_FIN
    TCPS_CLOSING,       // TCPA_RECV_FIN
    TCPS_FIN_WAIT2,     // TCPA_RECV_ACK_FIN
    TCPS_INVALID        // TCPA_TIMEOUT
};

TCP_State_t stateChangeForFINWait2[] = {
    
    TCPS_INVALID,       // TCPA_ACTIVE_OPEN
    TCPS_INVALID,       // TCPA_PASSIVE_OPEN
    
    TCPS_INVALID,       // TCPA_RECV_SYN
    TCPS_INVALID,       // TCPA_SEND_SYN
    TCPS_INVALID,       // TCPA_RECV_ACK
    TCPS_INVALID,       // TCPA_RECV_SYN_ACK
    
    TCPS_FIN_WAIT2,     // TCPA_RECEIVE
    TCPS_INVALID,       // TCPA_SEND
    
    TCPS_INVALID,       // TCPA_CLOSE
    TCPS_INVALID,       // TCPA_SEND_FIN
    TCPS_TIME_WAIT,     // TCPA_RECV_FIN
    TCPS_INVALID,       // TCPA_RECV_ACK_FIN
    TCPS_INVALID        // TCPA_TIMEOUT
};

TCP_State_t stateChangeForClosing[] = {
    
    TCPS_INVALID,       // TCPA_ACTIVE_OPEN
    TCPS_INVALID,       // TCPA_PASSIVE_OPEN
    
    TCPS_INVALID,       // TCPA_RECV_SYN
    TCPS_INVALID,       // TCPA_SEND_SYN
    TCPS_INVALID,       // TCPA_RECV_ACK
    TCPS_INVALID,       // TCPA_RECV_SYN_ACK
    
    TCPS_CLOSING,       // TCPA_RECEIVE
    TCPS_INVALID,       // TCPA_SEND
    
    TCPS_INVALID,       // TCPA_CLOSE
    TCPS_INVALID,       // TCPA_SEND_FIN
    TCPS_INVALID,       // TCPA_RECV_FIN
    TCPS_TIME_WAIT,     // TCPA_RECV_ACK_FIN
    TCPS_INVALID        // TCPA_TIMEOUT
};

TCP_State_t stateChangeForTimeWait[] = {
    
    TCPS_INVALID,       // TCPA_ACTIVE_OPEN
    TCPS_INVALID,       // TCPA_PASSIVE_OPEN
    
    TCPS_INVALID,       // TCPA_RECV_SYN
    TCPS_INVALID,       // TCPA_SEND_SYN
    TCPS_INVALID,       // TCPA_RECV_ACK
    TCPS_INVALID,       // TCPA_RECV_SYN_ACK
    
    TCPS_TIME_WAIT,     // TCPA_RECEIVE
    TCPS_INVALID,       // TCPA_SEND
    
    TCPS_INVALID,       // TCPA_CLOSE
    TCPS_INVALID,       // TCPA_SEND_FIN
    TCPS_INVALID,       // TCPA_RECV_FIN
    TCPS_INVALID,       // TCPA_RECV_ACK_FIN
    TCPS_CLOSED         // TCPA_TIMEOUT
};

TCP_State_t stateChangeForCloseWait[] = {
    
    TCPS_INVALID,       // TCPA_ACTIVE_OPEN
    TCPS_INVALID,       // TCPA_PASSIVE_OPEN
    
    TCPS_INVALID,       // TCPA_RECV_SYN
    TCPS_INVALID,       // TCPA_SEND_SYN
    TCPS_INVALID,       // TCPA_RECV_ACK
    TCPS_INVALID,       // TCPA_RECV_SYN_ACK
    
    TCPS_CLOSE_WAIT,    // TCPA_RECEIVE
    TCPS_CLOSE_WAIT,    // TCPA_SEND
    
    TCPS_INVALID,       // TCPA_CLOSE
    TCPS_LAST_ACK,      // TCPA_SEND_FIN
    TCPS_INVALID,       // TCPA_RECV_FIN
    TCPS_INVALID,       // TCPA_RECV_ACK_FIN
    TCPS_INVALID        // TCPA_TIMEOUT
};

TCP_State_t stateChangeForLastACK[] = {
    
    TCPS_INVALID,       // TCPA_ACTIVE_OPEN
    TCPS_INVALID,       // TCPA_PASSIVE_OPEN
    
    TCPS_INVALID,       // TCPA_RECV_SYN
    TCPS_INVALID,       // TCPA_SEND_SYN
    TCPS_INVALID,       // TCPA_RECV_ACK
    TCPS_INVALID,       // TCPA_RECV_SYN_ACK
    
    TCPS_LAST_ACK,      // TCPA_RECEIVE
    TCPS_INVALID,       // TCPA_SEND
    
    TCPS_INVALID,       // TCPA_CLOSE
    TCPS_INVALID,       // TCPA_SEND_FIN
    TCPS_INVALID,       // TCPA_RECV_FIN
    TCPS_CLOSED,        // TCPA_RECV_ACK_FIN
    TCPS_INVALID        // TCPA_TIMEOUT
};


//=================================================================================================
//      PRIVATE FUNCTIONS
//=================================================================================================

int getErrorCode(TCP_State_t state, TCP_Action_t action)
{
    if(action==TCPA_ACTIVE_OPEN || action==TCPA_PASSIVE_OPEN){
        return -EALREADY; //already existed
    }      

    
    if(action>=TCPA_RECV_SYN && action==TCPA_RECV_SYN_ACK){
       return -EOPNOTSUPP; //error code: operation not supported on socket
    }


    if(action==TCPA_RECEIVE || action==TCPA_SEND){
       if(state==TCPS_CLOSED || state>TCPS_ESTAB) return -ECONNRESET; //error code: connection closing
       else return -EOPNOTSUPP;
    }


    //for simple delete TCB close could only performed on first third states
    if(action==TCPA_CLOSE){       
       if(state>TCPS_SYN_RCVD) return -EOPNOTSUPP;    
    }

    if(action==TCPA_SEND_FIN){
       if(state==TCPS_CLOSING || state==TCPS_TIME_WAIT || state==TCPS_LAST_ACK){
           return -ECONNRESET;
       }
       else return -EOPNOTSUPP;
    }

    if(action>=TCPA_RECV_FIN) return -EOPNOTSUPP;


    return -1;
}



TCP_State_t determineStateChange(TCP_State_t state, TCP_Action_t action)
{
    switch (state) {

        case TCPS_CLOSED:
            return stateChangeForClosed[action];
            
        case TCPS_LISTEN:
            return stateChangeForListen[action];
            
        case TCPS_SYN_SENT:
            return stateChangeForSYNSent[action];
            
        case TCPS_SYN_RCVD:
            return stateChangeForSYNRcvd[action];
            
        case TCPS_ESTAB:
            return stateChangeForEstab[action];
            
        case TCPS_FIN_WAIT1:
            return stateChangeForFINWait1[action];
            
        case TCPS_FIN_WAIT2:
            return stateChangeForFINWait2[action];
            
        case TCPS_CLOSING:
            return stateChangeForClosing[action];
            
        case TCPS_TIME_WAIT:
            return stateChangeForTimeWait[action];
            
        case TCPS_CLOSE_WAIT:
            return stateChangeForCloseWait[action];
            
        case TCPS_LAST_ACK:
            return stateChangeForLastACK[action];
        
        default:
            fprintf(stderr, "Not a valid TCP_State.\n");
            assert(false);
            break;
    } // end switch()
    
    return TCPS_INVALID;
}


//=================================================================================================
//      PUBLIC FUNCTIONS
//=================================================================================================

void changeState(int vsocket, TCP_Action_t action)
{
    struct vsocket_infoset* entry_ptr = NULL;
    
    // Get current connection state
    pthread_mutex_lock(&g_vsocket_table_mutex);
    entry_ptr = (struct vsocket_infoset*)g_hash_table_lookup(s_vsocket_table, &vsocket);
    
    if (entry_ptr == NULL) {
        pthread_mutex_unlock(&g_vsocket_table_mutex);
        return;
    }
    
    // Determine state change according to action
    entry_ptr->state = determineStateChange(entry_ptr->state, action);
    pthread_mutex_unlock(&g_vsocket_table_mutex);
}




bool isValidAction(int vsocket, TCP_Action_t action, int* error)
{
    (*error) = -1;
    
    TCP_State_t new_state = TCPS_INVALID;
    struct vsocket_infoset* entry_ptr = NULL;
    
    // Get current connection state
    pthread_mutex_lock(&g_vsocket_table_mutex);
    entry_ptr = (struct vsocket_infoset*)g_hash_table_lookup(s_vsocket_table, &vsocket);
    
    if (entry_ptr == NULL) {
        *error=ENOTSOCK; //error code: not a socket
        pthread_mutex_unlock(&g_vsocket_table_mutex);
        return false;
    }
    
    // Determine state change according to action
    TCP_State_t state=entry_ptr->state;
    pthread_mutex_unlock(&g_vsocket_table_mutex);
    
    new_state = determineStateChange(state, action);
    if (new_state == TCPS_INVALID) {
        // Get error code
        (*error)=getErrorCode(state, action);
        return false;
    } else {
        return true;
    }
}

void printStateDebug(int vsocket)
{
    struct vsocket_infoset* entry_ptr = NULL;
    
    // Get current connection state
    pthread_mutex_lock(&g_vsocket_table_mutex);
    entry_ptr = (struct vsocket_infoset*)g_hash_table_lookup(s_vsocket_table, &vsocket);
    TCP_State_t state = entry_ptr->state;
    pthread_mutex_unlock(&g_vsocket_table_mutex);
    
    printStateAsString(state);
}


void printStateAsString(TCP_State_t state)
{     
    switch (state) {

        case TCPS_CLOSED:
            printf("TCP_CLOSED");
            break;
            
        case TCPS_LISTEN:
            printf("TCP_LISTEN");
            break;
            
        case TCPS_SYN_SENT:
            printf("TCP_SYN_SENT");
            break;
            
        case TCPS_SYN_RCVD:
            printf("TCP_SYN_RCVD");
            break;
            
        case TCPS_ESTAB:
            printf("TCP_ESTAB");
            break;
            
        case TCPS_FIN_WAIT1:
            printf("TCP_FIN_WAIT1");
            break;
            
        case TCPS_FIN_WAIT2:
            printf("TCP_FIN_WAIT2");
            break;
            
        case TCPS_CLOSING:
            printf("TCP_CLOSING");
            break;
            
        case TCPS_TIME_WAIT:
            printf("TCP_TIME_WAIT");
            break;
            
        case TCPS_CLOSE_WAIT:
            printf("TCP_CLOSE_WAIT");
            break;
            
        case TCPS_LAST_ACK:
            printf("TCP_LAST_ACK");
            break;
            
        case TCPS_INVALID:
            printf("TCP_INVALID");
            break;

    } // end switch()
}





//=================================================================================================
//      END OF FILE
//=================================================================================================
