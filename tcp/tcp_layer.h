#ifndef _TCP_LAYER_H_
#define _TCP_LAYER_H_

//================================================================================================= 
//      Author: Brett Decker and Xinwei Liu
//=================================================================================================
//      INCLUDE FILES
//=================================================================================================

#include "tcp_util.h"
#include "state_machine.h"
#include "logger.h"
#include "util/bqueue.h"
#include "util/circular_buffer.h"
#include <glib.h>


//=================================================================================================
//      DEFINITIONS AND MACROS
//=================================================================================================
#define DEFAULT_WSIZE 65535
#define TCP_HDR_SIZE  20


struct tcb_infoset {

   uint32_t local_vip;
   uint16_t local_port;
   uint32_t remote_vip;
   uint16_t remote_port;

   //tcb send variables
   tcp_seq send_unack;      //oldest unacknowledged sequence number
   tcp_seq send_next;       //next sequence number to be sent
   tcp_seq seq_fin;         //seq number of FIN
   uint64_t sws;            //sender max window size
   tcp_seq seq_send_init;   //initial send seq number
   tcp_seq dup_ack;         //counter for duplicate ACK
   tcp_seq remote_ruws;     //remote receiver usable window size

   //tcb recv variables
   tcp_seq recv_fin;
   tcp_seq recv_next;       //next sequence number expected on an incoming segments, 
   uint64_t rws;            //receiver max window size
   tcp_seq ruws;            //receiver usable window size
   tcp_seq seq_recv_init;   //initial recv seq number
};


struct vsocket_infoset {

    TCP_State_t state;
    uint32_t no_read;
    Logger_t logger;
    clock_t start_time;
    
    struct {
        double rttvar_us;
        double srtt_us;
        double rto_us;
        
        clock_t send_time;
        uint32_t exp_acknum;
        
        bool first_measure;
    };
    
    struct tcb_infoset tcb;
    bqueue_t bq_buffer; //for connection and raw tcp packet rcvd
    circular_buffer_t* swin_buffer;
    circular_buffer_t* rwin_buffer;
    
    circular_buffer_t* in_data; //for inorder tcp packet rcvd(filtered by rsw)
    int pending_data[DEFAULT_WSIZE];
    bool quit;
};

//=================================================================================================
//      GLOBAL VARIABLES
//=================================================================================================

extern pthread_mutex_t g_vsocket_table_mutex;
extern GHashTable* s_vsocket_table;

//=================================================================================================
//      PUBLIC FUNCTIONS
//=================================================================================================


void tcp_handleTCPPacket(ip_packet_t* ip_packet);


void continueTCPConnection_S2E(tcp_packet_t* tcp_packet, uint32_t saddr, uint32_t daddr, int vsocket);
void continueTCPConnection_S2R(tcp_packet_t* tcp_packet, uint32_t saddr, uint32_t daddr);


int tcp_sendMessage(struct vsocket_infoset* socket_info, uint32_t saddr, uint32_t daddr, 
                    tcp_packet_t* tcp_packet, size_t packet_len);

void tcp_initialSetUp(void);

void tcp_printSockets(void);

void tcp_printSocketWindowsSizes(int socket);

int v_socket(void);

void tcp_printSocketRecvWin(int vsocket, bool non_read_only);

void printSocketStatePair(gpointer socket, gpointer socket_info);
void printAllSocketsState();
void printPort2SocketPair(gpointer port, gpointer socket);
void printAllPortsSocket();

int v_bind(int vsocket, struct in_addr* addr, uint16_t port);

int v_listen(int vsocket);

int v_accept(int vsocket, struct in_addr* node);

int v_connect(int vsocket, struct in_addr* addr, uint16_t port);

int v_read(int vsocket, const unsigned char* buf, uint32_t nbyte);

int v_write(int vsocket, const unsigned char* buf, uint32_t nbyte);

int v_shutdown(int vsocket, int type);

int v_close(int vsocket);

void testPrint();


//=================================================================================================
//      END OF FILE
//=================================================================================================

#endif //_TCP_LAYER_H_
