#ifndef _LOGGER_H_
#define _LOGGER_H_

//================================================================================================= 
//      Author: Brett Decker and Xinwei Liu
//=================================================================================================
//      INCLUDE FILES
//=================================================================================================

#include "utility.h"
#include "tcp_util.h"
#include <time.h>

//=================================================================================================
//      DEFINITIONS AND MACROS
//=================================================================================================

typedef struct Logger {

    int fd;
    pthread_mutex_t lock;    

} Logger_t;


typedef enum Packet_Type {

    SEND_PKT,
    RECV_PKT

} Packet_Type_t;


#define lgr_storeSendPacket(socket_info, receipt_time, tcp_packet, packet_len) \
        lgr_storeTCPPacket(socket_info, receipt_time, tcp_packet, packet_len, SEND_PKT)
        
#define lgr_storeRecvPacket(socket_info, receipt_time, tcp_packet, packet_len) \
        lgr_storeTCPPacket(socket_info, receipt_time, tcp_packet, packet_len, RECV_PKT)


//=================================================================================================
//      GLOBAL VARIABLES
//=================================================================================================

//=================================================================================================
//      PUBLIC FUNCTIONS
//=================================================================================================

void lgr_open(int vsocket);

void lgr_storeTCPPacket(void* info, struct timespec * receipt_time, tcp_packet_t* tcp_packet,
                        uint32_t packet_len, Packet_Type_t pkt_type);
                        
void lgr_close(int vsocket);


//=================================================================================================
//      END OF FILE
//=================================================================================================

#endif //_LOGGER_H_
