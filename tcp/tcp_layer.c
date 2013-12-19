//================================================================================================= 
//      Author: Brett Decker and Xinwei Liu
//=================================================================================================
//      INCLUDE FILES
//=================================================================================================

#include "utility.h"
#include "tcp_layer.h"
#include "net_layer.h"
#include "sliding_window.h"
#include "tcp_util.h"
#include "port_util.h"
#include "util/circular_buffer.h"
#include <errno.h>
#include <assert.h>
#include <time.h>

//=================================================================================================
//      DEFINITIONS AND MACROS
//=================================================================================================

#define VSOCKET_STARTER 2
#define SUCCESS 0
#define MAX_SEQACK_NUM  100000
#define SHUT_WRITE 1
#define SHUT_READ 2
#define SHUT_BOTH 3

#define K 4 // Used in RTO calculation

//=================================================================================================
//      GLOBAL VARIABLES
//=================================================================================================

// These variables are not static because "state_machine.c" needs to use them
pthread_mutex_t g_vsocket_table_mutex = PTHREAD_MUTEX_INITIALIZER;
GHashTable* s_vsocket_table=NULL;


static int g_highest_vsocket=VSOCKET_STARTER;
pthread_t g_thread_id;

static double CLOCKS_PER_USEC = CLOCKS_PER_SEC/1000000;

//=================================================================================================
//      PRIVATE FUNCTIONS
//=================================================================================================

void dropFromSocketTable(int vsocket)
{
    pthread_mutex_lock(&g_vsocket_table_mutex);
    g_hash_table_remove(s_vsocket_table, &vsocket);
    pthread_mutex_unlock(&g_vsocket_table_mutex);
}

void releaseSocket(int vsocket, bool release_port)
{
    uint32_t local_vip;
    uint16_t local_port;
    uint32_t remote_vip;
    uint16_t remote_port;

    // Critical Section
    pthread_mutex_lock(&g_vsocket_table_mutex);
    struct vsocket_infoset* socket_infoset = g_hash_table_lookup(s_vsocket_table, &vsocket);
    // Check for non-existent vsocket
    if (socket_infoset == NULL) {
        pthread_mutex_unlock(&g_vsocket_table_mutex);
        return;
    }
    
    local_vip = socket_infoset->tcb.local_vip;
    local_port = socket_infoset->tcb.local_port;
    remote_vip = socket_infoset->tcb.remote_vip;
    remote_port = socket_infoset->tcb.remote_port;
    pthread_mutex_unlock(&g_vsocket_table_mutex);
    
    
    dropFromSocketTable(vsocket);
    
    ptu_removeSocketAddr(local_vip, local_port, remote_vip, remote_port);
    
    if (release_port == true) {
        ptu_releasePort(local_port);
    }
}


void createSocketThreadFuncs(int vsocket) {
                
    struct handleFuncArg* harg = (struct handleFuncArg*)malloc(sizeof(struct handleFuncArg));
    harg->socket=vsocket;
    struct sendFuncArg* sarg = (struct sendFuncArg*)malloc(sizeof(struct sendFuncArg));
    sarg->socket=vsocket;
    
    pthread_create(&g_thread_id, NULL, sw_socketHandlePacketThreadFunc, (void*)harg);
    pthread_create(&g_thread_id, NULL, sw_socketSendDataThreadFunc, (void*)sarg);
                    
    return;
}


//=================================================================================================
//      PUBLIC FUNCTIONS
//=================================================================================================

//initialize all gtable used in tcp layer and other possible initial set up
void tcp_initialSetUp()
{
    s_vsocket_table=g_hash_table_new(g_int_hash, g_int_equal);
}


int tcp_sendMessage(struct vsocket_infoset* socket_info, uint32_t saddr, uint32_t daddr, 
                    tcp_packet_t* tcp_packet, size_t packet_len)
{
    struct timespec timestamp;
    clock_gettime(CLOCK_MONOTONIC, &timestamp);

    lgr_storeSendPacket(socket_info, &timestamp, tcp_packet, packet_len);
    if (net_sendMessage(saddr, daddr, TCP_PROTOCOL, (char*)tcp_packet, packet_len) != true) {
        return false;
    }
    
    return true;
}


void tcp_handleTCPPacket(ip_packet_t* ip_packet)
{
    clock_t receipt_time = clock(); // used for RTO
    struct timespec timestamp;
    clock_gettime(CLOCK_MONOTONIC, &timestamp);
    
    ip_packet_t* cp_ip_packet = (ip_packet_t*)malloc(sizeof(ip_packet_t));  
    memcpy(cp_ip_packet, ip_packet, sizeof(ip_packet_t));

    tcp_packet_t* tcp_packet = (tcp_packet_t*)(cp_ip_packet->ip_data);
    
    ip_header_t ip_header = cp_ip_packet->ip_header;

    int tcp_packet_len=ntohs(ip_header.ip_len)-IP_HEADER_BYTES;

    // Check checksum
    pseudo_tcphdr_t pseudo_header;
    pseudo_header.source_addr=ip_header.ip_src;
    pseudo_header.dest_addr=ip_header.ip_dst;
    pseudo_header.reserved=0;
    pseudo_header.protocol=TCP_PROTOCOL;
    pseudo_header.tcp_len=htons(tcp_packet_len);
    
    int tcp_w_pseudo_len=tcp_packet_len+PSEUDO_TCPHDR_SIZE;
    char tcp_w_pseudo[tcp_w_pseudo_len];
    int start=0;
    memcpy(tcp_w_pseudo, (char*)&pseudo_header, PSEUDO_TCPHDR_SIZE);

    start += PSEUDO_TCPHDR_SIZE;
    memcpy(tcp_w_pseudo+start, (char*)tcp_packet, tcp_packet_len);

    uint16_t checksum=tcp_checksum(tcp_w_pseudo, tcp_w_pseudo_len);
    if (checksum != 0) {
       printf("warning!, checksum is not 0 but %u! tcp packet dropped\n", checksum);
       return;
    }

    struct tcphdr tcp_header=tcp_packet->tcp_header;
    uint16_t local_port=ntohs(tcp_header.th_dport);

    int vsocket = ptu_getSocketAtAddr(ntohl(ip_header.ip_dst), local_port, ntohl(ip_header.ip_src), ntohs(tcp_header.th_sport));
    if (vsocket == -1) {
        // Look up listen socket with port
        vsocket = ptu_getSocketAtAddr(0, local_port, 0, 0);
        if (vsocket == -1) {
           printf("no socket are for port %u !\n", local_port);
           ptu_printAddrTup2Socket();
           
           return;
        }
    }

    pthread_mutex_lock(&g_vsocket_table_mutex);
    struct vsocket_infoset* socket_info = g_hash_table_lookup(s_vsocket_table, &vsocket);
    pthread_mutex_unlock(&g_vsocket_table_mutex);

    if (socket_info == NULL) {
        printf("socket %d is not registered with a state !\n", vsocket);
        tcp_printSockets();
        return;
    }
    
    // RTO Calculation
    uint32_t acknum = ntohl(tcp_header.th_ack);
    
    // Critical Section
    pthread_mutex_lock(&g_vsocket_table_mutex);
    if (socket_info->exp_acknum == acknum) {
        double r = (double)(receipt_time - socket_info->send_time)/CLOCKS_PER_USEC;
        
        if (!(socket_info->first_measure)) { // Normal case
        
            socket_info->rttvar_us = (0.75*socket_info->rttvar_us) + (0.25*util_abs(socket_info->srtt_us - r));
            socket_info->srtt_us = (0.875*socket_info->srtt_us) + (0.125*r);
            
        } else { // First measure
        
            socket_info->srtt_us = r;
            socket_info->rttvar_us = r/2.0;
            
            socket_info->first_measure = false;
        }
        
        socket_info->rto_us = socket_info->srtt_us + util_max(1000, K*socket_info->rttvar_us);
        
        socket_info->exp_acknum = 0;
        
    } else if (socket_info->exp_acknum < acknum) { // Missed expected ACK, reset
        socket_info->exp_acknum = 0;
    }
    pthread_mutex_unlock(&g_vsocket_table_mutex);    
    
    lgr_storeRecvPacket(socket_info, &timestamp, tcp_packet, tcp_packet_len);
    bqueue_enqueue(&(socket_info->bq_buffer), cp_ip_packet);
}


int v_socket()
{
    g_highest_vsocket++;
    
    struct vsocket_infoset* vsocket_info=(struct vsocket_infoset*)malloc(sizeof(struct vsocket_infoset)); 
    memset((char*)vsocket_info, 0, sizeof(vsocket_info));

    //initial set up of tcb's info
    vsocket_info->state=TCPS_CLOSED;
    
    vsocket_info->rto_us = 3000000; // 3 seconds
    vsocket_info->first_measure = true;    
    
    // Initialize bqueue
    bqueue_init(&(vsocket_info->bq_buffer));

    //initialize circular buffer
    circular_buffer_init(&(vsocket_info->swin_buffer), DEFAULT_WSIZE);
    circular_buffer_init(&(vsocket_info->rwin_buffer), DEFAULT_WSIZE);
    circular_buffer_init(&(vsocket_info->in_data), DEFAULT_WSIZE*2);//in_data doesn't have to the same as sw wsize
    
    memset(vsocket_info->pending_data, 0, DEFAULT_WSIZE*sizeof(int));
    
    int* newsocket=(int*)malloc(sizeof(int));
    *newsocket=g_highest_vsocket;
    
    pthread_mutex_lock(&g_vsocket_table_mutex);
    g_hash_table_insert(s_vsocket_table, newsocket, vsocket_info);
    pthread_mutex_unlock(&g_vsocket_table_mutex);
    
    return *newsocket;
}


void printSocketStatePair(gpointer socket, gpointer socket_info)
{
    TCP_State_t state=((struct vsocket_infoset*)socket_info)->state;
    printf("socket %d -> state: ", *((int*)socket));
    printStateAsString(state);
    printf("\n");
}

void tcp_printSockets(void)
{
    // Critical Section
    pthread_mutex_lock(&g_vsocket_table_mutex);
    
    printf("\n------Socket-----\n");
    g_hash_table_foreach(s_vsocket_table, (GHFunc)printSocketStatePair, NULL);
    printf("------End Sockets-----\n\n");
    
    pthread_mutex_unlock(&g_vsocket_table_mutex);
}

void tcp_printSocketWindowsSizes(int vsocket)
{
    struct vsocket_infoset* socket_info = NULL;
    
    // Critical Section
    pthread_mutex_lock(&g_vsocket_table_mutex);
    socket_info = g_hash_table_lookup(s_vsocket_table, &vsocket);
    
    uint32_t local_ruws = socket_info->tcb.ruws;
    uint32_t remote_ruws = socket_info->tcb.remote_ruws;
    pthread_mutex_unlock(&g_vsocket_table_mutex);
    
    printf("\n------Windows-----\n");
    printf("socket %d -> Send: %d,  Recv: %d\n", vsocket, remote_ruws, local_ruws);
    printf("------End Windows-----\n\n");
}

void tcp_printSocketRecvWin(int vsocket, bool non_read_only)
{
    struct vsocket_infoset* socket_info = NULL;
    
    // Critical Section
    pthread_mutex_lock(&g_vsocket_table_mutex);
    socket_info = g_hash_table_lookup(s_vsocket_table, &vsocket);
    pthread_mutex_unlock(&g_vsocket_table_mutex);
    
    if (non_read_only) {
        circular_buffer_print_unread_contents(socket_info->rwin_buffer);
    } else {
        circular_buffer_print_contents(socket_info->rwin_buffer);
    }
}


// Three way handshake from client side: send SYN, recv SYN-ACK, send ACK
int v_connect(int vsocket, struct in_addr* addr, uint16_t port)
{
    int error_code = 0;
    struct vsocket_infoset* socket_info = NULL;
    
    //check whether vsocket is a good socket
    pthread_mutex_lock(&g_vsocket_table_mutex);
    socket_info = g_hash_table_lookup(s_vsocket_table, &vsocket);
    pthread_mutex_unlock(&g_vsocket_table_mutex);
    // Verify socket exists
    if(socket_info==NULL) return EBADFD;
   
    // Check valid state for send SYN
    if (isValidAction(vsocket, TCPA_ACTIVE_OPEN, &error_code) == false) {
        dropFromSocketTable(vsocket);
        return error_code;
    }
   
    tcp_packet_t tcp_packet;
    uint32_t daddr=addr->s_addr;
    ushort dport=port;

    uint32_t saddr=net_getLocalAddrTowardDestination(addr->s_addr);
    if (saddr == 0) { // Destination is unknown
        dropFromSocketTable(vsocket);
        //error code: No route to host
        return -EHOSTUNREACH;
    }
    
    uint16_t sport=ptu_getAvailablePort(); 

    tcp_seq seqnum = rand()%MAX_SEQACK_NUM;
    tcp_seq ack = rand()%MAX_SEQACK_NUM;

    u_short wsize=DEFAULT_WSIZE;
   
    //build packet with no tcp data and //print
    buildTCPPacket(&tcp_packet, saddr, daddr, sport, dport, seqnum, ack, TH_SYN, wsize, NULL, 0);
    
    // Critical Section
    pthread_mutex_lock(&g_vsocket_table_mutex);
    // Setup TCB
    socket_info = g_hash_table_lookup(s_vsocket_table, &vsocket);
    socket_info->tcb.local_vip = saddr;
    socket_info->tcb.local_port = sport;
    socket_info->tcb.remote_vip = daddr;
    socket_info->tcb.remote_port = dport;
    socket_info->tcb.rws = wsize;
    socket_info->tcb.ruws = wsize;
    socket_info->tcb.remote_ruws = 0;
    pthread_mutex_unlock(&g_vsocket_table_mutex);
    
    ptu_addSocketAtAddr(saddr, sport, daddr, dport, vsocket);

    //1. send SYN packet
    //NOTE: no tcp payload, size of tcp_packet=tcp_header
    if (net_sendMessage(saddr, daddr, TCP_PROTOCOL, (char*)&tcp_packet, sizeof(struct tcphdr)) != true) {
        //error code: Communication error on send
        releaseSocket(vsocket, true);
        return -ECOMM;
    }
    
    changeState(vsocket, TCPA_ACTIVE_OPEN);
    
    int retry = 0;
    int seconds = 1;
    struct timespec timeout;
    timeout.tv_sec = seconds;
    timeout.tv_nsec = 0;
 
    bool quit = false;
    while (!quit) {
    
        ip_packet_t* ip_packet = NULL;
        
        int ret = bqueue_timed_dequeue_rel(&(socket_info->bq_buffer), (void**)&ip_packet, &timeout);
        //printf("ret: %d\n", ret);
        if (ret == -ETIMEDOUT) {
            if (retry == 3) {
                // error code: Timer expired 
                releaseSocket(vsocket, true);
                return -ETIME;
            }
            retry++;
            seconds *= 2; // Double timeout
            timeout.tv_sec = seconds; 
            //1. send SYN packet
            //NOTE: no tcp payload, size of tcp_packet=tcp_header
            if (net_sendMessage(saddr, daddr, TCP_PROTOCOL, (char*)&tcp_packet, sizeof(struct tcphdr)) != true) {
                //error code: Communication error on send 
                releaseSocket(vsocket, true);
                return -ECOMM;
            }
            continue;
        } else if (ret == -EINVAL) {
            return ret;
        }
        
        
        tcp_packet_t* recv_tcp_packet = NULL;
        recv_tcp_packet = (tcp_packet_t*)ip_packet->ip_data;
        
        uint8_t recv_flag=(recv_tcp_packet->tcp_header).th_flags;
        
        // Continue handshake 
        if (recv_flag==TH_SYN+TH_ACK) { //from SYN_SENT-> ESTAB
        
            // Check valid state for send SYN
            if (isValidAction(vsocket, TCPA_RECV_SYN_ACK, &error_code) == false) {
                releaseSocket(vsocket, true);
                return error_code;
            }
            
            // Check for correct ack number
            if (ntohl(recv_tcp_packet->tcp_header.th_ack) != seqnum+1) {
                //printf("incorrect ack!\n");
                continue;
            }
            
            // Critical Section
            pthread_mutex_lock(&g_vsocket_table_mutex);
            socket_info = g_hash_table_lookup(s_vsocket_table, &vsocket);
            // Set Send Window Size (from received packet)
            socket_info->tcb.sws = ntohs(recv_tcp_packet->tcp_header.th_win);
            // Set seq send & recv values (from received packet)
            socket_info->tcb.seq_send_init = ntohl((recv_tcp_packet->tcp_header).th_ack);
            socket_info->tcb.send_next = socket_info->tcb.seq_send_init;
            socket_info->tcb.send_unack = socket_info->tcb.seq_send_init;
            socket_info->tcb.seq_recv_init = ntohl(recv_tcp_packet->tcp_header.th_seq) + 1;
            socket_info->tcb.recv_next = socket_info->tcb.seq_recv_init;
            socket_info->tcb.dup_ack = 0;
            socket_info->tcb.remote_ruws =ntohs(recv_tcp_packet->tcp_header.th_win);
            pthread_mutex_unlock(&g_vsocket_table_mutex);
            
            buildTCPPacket(&tcp_packet, saddr, daddr, sport, dport,     
                           ntohl(recv_tcp_packet->tcp_header.th_ack), 
                           ntohl(recv_tcp_packet->tcp_header.th_seq) + 1, TH_ACK, wsize, NULL, 0);
                
            // Send ACK of SYN-ACK
            //NOTE: no tcp payload, size of tcp_packet=tcp_header
            if (net_sendMessage(saddr, daddr, TCP_PROTOCOL, (char*)&tcp_packet, sizeof(struct tcphdr)) != true) {
                //rror code: Communication error on send
                releaseSocket(vsocket, true);
                return -ECOMM;
            }
            
            // Create logger
            lgr_open(vsocket);
            
            changeState(vsocket, TCPA_RECV_SYN_ACK);
            //connection established, create send and handle thread function
            createSocketThreadFuncs(vsocket);
            
            quit = true;
        }
        else if (recv_flag==TH_SYN) {//simult connect, from SYN_SENT->SYN_RCVD
        
            // Check valid state for send SYN
            if (isValidAction(vsocket, TCPA_RECV_SYN, &error_code) == false) {
                releaseSocket(vsocket, true);
                return error_code;
            }
            
            buildTCPPacket(&tcp_packet, saddr, daddr, sport, dport,     
                           rand()%MAX_SEQACK_NUM, //random generated seq number
                           ntohl(recv_tcp_packet->tcp_header.th_seq) + 1, TH_SYN+TH_ACK, wsize, NULL, 0);
            
            //NOTE: no tcp payload, size of tcp_packet=tcp_header
            if (net_sendMessage(ntohl(ip_packet->ip_header.ip_dst), ntohl(ip_packet->ip_header.ip_src),
                                TCP_PROTOCOL, (char*)&tcp_packet, sizeof(struct tcphdr)) != true) {
                // error code: Communication error on send
                releaseSocket(vsocket, true);
                return -ECOMM;
            }
            
            changeState(vsocket, TCPA_RECV_SYN);
            
            // Change last sequence and ack numbers
            seqnum = ntohl(tcp_packet.tcp_header.th_seq);
            ack = ntohl(tcp_packet.tcp_header.th_ack);
	    }
	    else if (recv_flag==TH_ACK) {
	
	        // Check valid state for send SYN
            if (isValidAction(vsocket, TCPA_RECV_ACK, &error_code) == false) {
                releaseSocket(vsocket, true);
                return error_code;
            }
            
            // Check for correct ack number
            if (tcp_packet.tcp_header.th_ack != htonl(seqnum+1)) {
                continue;

            }
            // Check for correct sequence number
            if (tcp_packet.tcp_header.th_seq != htonl(ack)) {
                continue;
            }
            
            // Critical Section
            pthread_mutex_lock(&g_vsocket_table_mutex);
            socket_info = g_hash_table_lookup(s_vsocket_table, &vsocket);
            // Set Send Window Size (from received packet)
            socket_info->tcb.sws = ntohs(recv_tcp_packet->tcp_header.th_win);
            // Set seq send & recv values (from received packet)
            socket_info->tcb.seq_send_init = ntohl((recv_tcp_packet->tcp_header).th_ack);
            socket_info->tcb.send_next = socket_info->tcb.seq_send_init;
            socket_info->tcb.send_unack = socket_info->tcb.seq_send_init;
            socket_info->tcb.seq_recv_init = ntohl(recv_tcp_packet->tcp_header.th_seq); // Length of packet is zero
            socket_info->tcb.recv_next = socket_info->tcb.seq_recv_init;
            socket_info->tcb.remote_ruws = ntohs(recv_tcp_packet->tcp_header.th_win);
            socket_info->tcb.dup_ack = 0;
            pthread_mutex_unlock(&g_vsocket_table_mutex);
            
            // Create logger
            lgr_open(vsocket);
            
            changeState(vsocket, TCPA_RECV_ACK);

            //connection established, create send and handle thread function
            createSocketThreadFuncs(vsocket);

            quit = true;
        }
        else { // Packet didn't have correct flags (not a handshake packet)
            continue;
        }
        
    } // end while()
    
    return SUCCESS;
}


/* binds a socket to a port
   always bind to interfaces - which means addr is unused.
   returns 0 on success or negative number on failure */
int v_bind(int vsocket, struct in_addr* addr, uint16_t port)
{
    struct vsocket_infoset* socket_info=NULL;
   
    if (ptu_getPort(port) == true) {
        // Critical Section
        pthread_mutex_lock(&g_vsocket_table_mutex);
        socket_info = g_hash_table_lookup(s_vsocket_table, &vsocket);
        
        if (socket_info == NULL || socket_info->state!=TCPS_CLOSED) { //can only bind on closed state
            pthread_mutex_unlock(&g_vsocket_table_mutex);
            return EBADFD; //error codes:  fd in bad state
        }
        
        (socket_info->tcb).local_port = port;
        pthread_mutex_unlock(&g_vsocket_table_mutex);
    } else { // Port already taken
        //error codes:  Address already in use
        return -EADDRINUSE;
    }

    // NOTE: addr is unused

    return 0;
}


/* moves socket into listen state (passive OPEN in the RFC)
   bind the socket to a random port from 1024 to 65535 that is unused
   if not already bound
   returns 0 on success or negative number on failure */
int v_listen(int vsocket)
{
    int error_code;
    int new_port = 0;
    struct vsocket_infoset* socket_info = NULL;
    
    // Check if connection is in a valid state for this action
    if (isValidAction(vsocket, TCPA_PASSIVE_OPEN, &error_code) != true) {
        return error_code;
    }
    
    new_port = ptu_getAvailablePort();
    
    pthread_mutex_lock(&g_vsocket_table_mutex);
    socket_info = g_hash_table_lookup(s_vsocket_table, &vsocket);

    // If not bound, bind to a random port
    if (socket_info->tcb.local_port == 0) {
        socket_info->tcb.local_port = new_port;
    }
    pthread_mutex_unlock(&g_vsocket_table_mutex);
    
    // Add listen socket
    ptu_addSocketAtAddr(0, socket_info->tcb.local_port, 0, 0, vsocket);
    
    changeState(vsocket, TCPA_PASSIVE_OPEN);
   
    return 0;
}


/* accept a requested connection (behave like unix socket’s accept)
   returns new socket handle on success or negative number on failure */
int v_accept(int vsocket, struct in_addr* node)
{
    int error_code = 0;
    int newsocket = 0;
    struct vsocket_infoset* entry_ptr = NULL;
    
    
    // Check if vsocket (listen socket) is in valid state
    if (!isValidAction(vsocket, TCPA_RECV_SYN, &error_code)){
        //error code: File descriptor in bad state
        return error_code;
    }
    
    //current approach is create temp socket before handshake done, but keep its info a separate "ghost" table 
    pthread_mutex_lock(&g_vsocket_table_mutex);
    struct vsocket_infoset* listen_socket_info=g_hash_table_lookup(s_vsocket_table, &vsocket);
    pthread_mutex_unlock(&g_vsocket_table_mutex);
   
    if (listen_socket_info==NULL) {
          //error code: Socket operation on non-socket
          return -ENOTSOCK;
    }

    ip_packet_t* ip_packet=NULL;    
    bool recv_connection = false;
    
    uint32_t last_seqnum = 0;
    uint32_t last_acknum = 0;


    while (!recv_connection) { // get socket bq_buffer from vsocket and keep dequeue it until get a connection

        bqueue_dequeue(&(listen_socket_info->bq_buffer), (void**)&ip_packet);

        tcp_packet_t* tcp_packet=(tcp_packet_t*)ip_packet->ip_data;

        uint8_t recv_flag=(tcp_packet->tcp_header).th_flags;
        if (recv_flag==TH_SYN) { //client request new connection through listen socket
            
            recv_connection = true;
            
            newsocket = v_socket();
                
            uint32_t local_vip = ntohl((ip_packet->ip_header).ip_dst);
            uint16_t local_port = ntohs((tcp_packet->tcp_header).th_dport);
            uint32_t remote_vip = ntohl((ip_packet->ip_header).ip_src);
            uint16_t remote_port = ntohs((tcp_packet->tcp_header).th_sport);
                
            pthread_mutex_lock(&g_vsocket_table_mutex);
            entry_ptr = (struct vsocket_infoset*)g_hash_table_lookup(s_vsocket_table, &newsocket);
            entry_ptr->state = TCPS_LISTEN;
            entry_ptr->tcb.local_vip = local_vip;
            entry_ptr->tcb.local_port = local_port;
            entry_ptr->tcb.remote_vip = remote_vip;
            entry_ptr->tcb.remote_port = remote_port;
            entry_ptr->tcb.rws = DEFAULT_WSIZE;
            entry_ptr->tcb.ruws = DEFAULT_WSIZE;
            entry_ptr->tcb.sws = ntohs(tcp_packet->tcp_header.th_win);
            pthread_mutex_unlock(&g_vsocket_table_mutex); 
                
            ptu_addSocketAtAddr(local_vip, local_port, remote_vip, remote_port, newsocket);

            // Send SYN-ACK
            tcp_packet_t syn_ack_packet;
              
            uint32_t seqnum = rand()%MAX_SEQACK_NUM;
            uint32_t acknum = ntohl(tcp_packet->tcp_header.th_seq)+1;
              
            buildTCPPacket(&syn_ack_packet, ntohl(ip_packet->ip_header.ip_dst), ntohl(ip_packet->ip_header.ip_src), 
                           ntohs(tcp_packet->tcp_header.th_dport), ntohs(tcp_packet->tcp_header.th_sport), 
                           seqnum, acknum, TH_SYN+TH_ACK, ntohs(tcp_packet->tcp_header.th_win), NULL, 0);

            if (net_sendMessage(ntohl(ip_packet->ip_header.ip_dst), ntohl(ip_packet->ip_header.ip_src),
                                TCP_PROTOCOL, (char*)&syn_ack_packet, sizeof(struct tcphdr)) != true) {
                //error code: Communication error on send
                releaseSocket(newsocket, false); // Don't release port (used by listen socket)
                return -ECOMM;
            }
              
            //reset last_seq_sent, last_ack_sent
            last_seqnum = seqnum;
            last_acknum = acknum;
              
            changeState(newsocket, TCPA_RECV_SYN);
        }
        else { // Invalid packet
            continue;
        }
    } // end while()
    

    ip_packet = NULL; // clear pointer
    struct timespec timeout;
    timeout.tv_sec = 8;
    timeout.tv_nsec = 0;
    
    while (1) {
    
        // Critical Section
        pthread_mutex_lock(&g_vsocket_table_mutex);
        entry_ptr = (struct vsocket_infoset*)g_hash_table_lookup(s_vsocket_table, &newsocket);
        pthread_mutex_unlock(&g_vsocket_table_mutex);
        
        int ret = bqueue_timed_dequeue_rel(&(entry_ptr->bq_buffer), (void**)&ip_packet, &timeout);
        if (ret != 0) {
            releaseSocket(newsocket, false); // Don't release port (used by listen socket)
            return ret;
        }
    
        tcp_packet_t* tcp_packet=(tcp_packet_t*)ip_packet->ip_data;

        uint8_t recv_flag=(tcp_packet->tcp_header).th_flags;
        if (recv_flag==TH_ACK) {
         
            if (!isValidAction(newsocket, TCPA_RECV_ACK, &error_code)) { //check valid action before checking ack number
                  releaseSocket(newsocket, false); // Don't release port (used by listen socket)
                  return error_code;
            }

            //Check ack number match
            if (last_acknum == ntohl(tcp_packet->tcp_header.th_seq) &&
                last_seqnum+1 == ntohl(tcp_packet->tcp_header.th_ack)) {
                
                // Set up initial send seq to be ack number rcvd
                pthread_mutex_lock(&g_vsocket_table_mutex);
                entry_ptr = (struct vsocket_infoset*)g_hash_table_lookup(s_vsocket_table, &newsocket);
                (entry_ptr->tcb).seq_send_init = ntohl(tcp_packet->tcp_header.th_ack);
                (entry_ptr->tcb).send_next = (entry_ptr->tcb).seq_send_init;
                (entry_ptr->tcb).send_unack = (entry_ptr->tcb).seq_send_init;
                (entry_ptr->tcb).seq_recv_init = ntohl(tcp_packet->tcp_header.th_seq); // Length of data is zero
                (entry_ptr->tcb).recv_next = (entry_ptr->tcb).seq_recv_init;
                (entry_ptr->tcb).dup_ack = 0;
                (entry_ptr->tcb).remote_ruws =ntohs(tcp_packet->tcp_header.th_win);
                pthread_mutex_unlock(&g_vsocket_table_mutex); 
                
                // Create logger
                lgr_open(newsocket);
                
                changeState(newsocket, TCPA_RECV_ACK);

                //connection established, create send and handle thread function

                createSocketThreadFuncs(newsocket);
                
                return newsocket;
            }
        }
    
    } // end while(1)
    
}


/* read on an open socket (RECEIVE in the RFC)
   return num bytes read or negative number on failure or 0 on eof */
int v_read(int vsocket, const unsigned char* buf, uint32_t nbyte)
{
    int error_code;
    
    // Check if vsocket is in valid state
    if (!isValidAction(vsocket, TCPA_RECEIVE, &error_code)){
        //error code: Transport endpoint is not connected 
        return error_code;
    }
    
    struct vsocket_infoset* socket_info=NULL;
	
	pthread_mutex_lock(&g_vsocket_table_mutex);
	socket_info = g_hash_table_lookup(s_vsocket_table, &vsocket);
	uint32_t no_read = socket_info->no_read;
	pthread_mutex_unlock(&g_vsocket_table_mutex);
    
    if (no_read == 1) {
        return -EPERM; //error code: operation not permitted
    }
    
    int ret = sw_readData(vsocket, buf, nbyte);
    
    return ret;
}


/* write on an open socket (SEND in the RFC)
   return num bytes written or negative number on failure */
int v_write(int vsocket, const unsigned char* buf, uint32_t nbyte)
{
    int error_code;

    // Check if vsocket is in valid state
    if (!isValidAction(vsocket, TCPA_SEND, &error_code)){
        //error code: Transport endpoint is not connected 
        return -ENOTCONN;
    }
    
    return sw_writeData(vsocket, buf, nbyte);
}


/* shutdown an open socket. If type is 1, close the writing part of
   the socket (CLOSE call in the RFC. This should send a FIN, etc.)
   If 2 is specified, close the reading part (no equivalent in the RFC;
   v_read calls should just fail, and the window size should not grow any
   more). If 3 is specified, do both. The socket is not invalidated.
   returns 0 on success, or negative number on failure.
   If the writing part is closed, any data not yet ACKed should still be retransmitted. */
int v_shutdown(int vsocket, int type)
{
	if (type<SHUT_WRITE || type>SHUT_BOTH) return -ESRCH; //error code: No such process
	
	struct vsocket_infoset* socket_info=NULL;
	
	pthread_mutex_lock(&g_vsocket_table_mutex);
	socket_info = g_hash_table_lookup(s_vsocket_table, &vsocket);
	pthread_mutex_unlock(&g_vsocket_table_mutex);

	int error_code;
    if (type==SHUT_READ || type==SHUT_BOTH) {//if type is 2 or 3 do shut read
    
        pthread_mutex_lock(&g_vsocket_table_mutex);
	    socket_info->no_read = 1;
	    pthread_mutex_unlock(&g_vsocket_table_mutex);
    }

	if (type==SHUT_WRITE || type==SHUT_BOTH) {//if type 1 or 3 do shut write, note: TCPA_CLOSE is also good action for BAD_RD state
        // Check if state valid
        if (isValidAction(vsocket, TCPA_CLOSE, &error_code)) {
	        changeState(vsocket, TCPA_CLOSE);
            return 0;
        }
                
        if (isValidAction(vsocket, TCPA_SEND_FIN, &error_code)) {
                    
            // Wait for all data to be transmitted
            while (sw_hasSentAllData(vsocket) == false);
               
            // Build and send FIN Packet
            tcp_packet_t tcp_packet;
            
            // Critical Section       
            pthread_mutex_lock(&g_vsocket_table_mutex);
            
            uint32_t saddr = socket_info->tcb.local_vip;
            uint16_t sport = socket_info->tcb.local_port;
            uint32_t daddr = socket_info->tcb.remote_vip;
            uint16_t dport = socket_info->tcb.remote_port;
            uint32_t acknum = socket_info->tcb.recv_next;
            uint16_t wsize = socket_info->tcb.ruws;      
            uint32_t rv_fin_seqnum = socket_info->tcb.recv_fin;
            uint32_t recv_next = socket_info->tcb.recv_next;
            uint32_t seqnum = socket_info->tcb.send_next;
            
            socket_info->tcb.seq_fin = seqnum;
            socket_info->tcb.send_next = socket_info->tcb.send_next + 1; // Increment send_next
            circular_buffer_increment_write_pointer(socket_info->swin_buffer, 1);
            pthread_mutex_unlock(&g_vsocket_table_mutex);
            // End Critical Section
                   
            // if recv FIN seqnum == recv_next (we already received FIN)
            if (rv_fin_seqnum == recv_next) {
                // Increment the acknum
                acknum++;
            }
     
            buildTCPPacket(&tcp_packet, saddr, daddr, sport, dport, seqnum, acknum, TH_FIN+TH_ACK, wsize, NULL, 0);
                    
            tcp_sendMessage(socket_info, saddr, daddr, &tcp_packet, TCP_HEADER_SIZE);

            changeState(vsocket, TCPA_SEND_FIN);
                   
        } // end if valid action TCPA_SEND_FIN

	} // end if SHUT_WRITE || SHUT_BOTH
 
	return SUCCESS;
}


/* Invalidate this socket, making the underlying connection inaccessible to
   any of these API functions. If the writing part of the socket has not been
   shutdown yet, then do so. The connection shouldn’t be terminated, though;
   any data not yet ACKed should still be retransmitted. */
int v_close(int vsocket)
{  
	int error_code;
	if (isValidAction(vsocket, TCPA_CLOSE, &error_code)){
	    changeState(vsocket, TCPA_CLOSE);
        return 0;
    }
    
    if (isValidAction(vsocket, TCPA_SEND_FIN, &error_code)){
        return v_shutdown(vsocket, SHUT_WRITE);   
    }
    
    // Return error on five states FIN_WAIT1, FIN_WAIT2, CLOSING, TIME_WAIT, LAST_ACK
    return error_code;
}


//=================================================================================================
//      END OF FILE
//=================================================================================================
