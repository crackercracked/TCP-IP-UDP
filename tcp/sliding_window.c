//================================================================================================= 
//      Author: Brett Decker and Xinwei Liu
//=================================================================================================
//      INCLUDE FILES
//=================================================================================================

#include "sliding_window.h"
#include "tcp_layer.h"
#include "tcp_util.h"
#include "util/circular_buffer.h"
#include "port_util.h"

#include <unistd.h>

//=================================================================================================
//      DEFINITIONS AND MACROS
//=================================================================================================

#define LOCK_AQUIRED 0

//=================================================================================================
//      GLOBAL VARIABLES
//=================================================================================================

static pthread_mutex_t g_timeout_mutex = PTHREAD_MUTEX_INITIALIZER;

//=================================================================================================
//      PRIVATE FUNCTIONS
//=================================================================================================

void transmitUnACKedData(struct vsocket_infoset* socket_info, uint32_t start_index, 
                         uint16_t data_len, uint32_t seqnum, bool do_rto_calc)
{
    char temp[TCP_MTU];
    memset(temp, 0, TCP_MTU);
    
    // Critical Section
    pthread_mutex_lock(&g_vsocket_table_mutex);
    if (socket_info->tcb.send_next == socket_info->tcb.seq_fin+1) {
        data_len--;
    }
    pthread_mutex_unlock(&g_vsocket_table_mutex);
            
    circular_buffer_get_contents(socket_info->swin_buffer, start_index, data_len, temp);
        
    // Get all variables inside critical section
    pthread_mutex_lock(&g_vsocket_table_mutex);
    uint32_t saddr = (socket_info->tcb).local_vip;
    uint32_t daddr = (socket_info->tcb).remote_vip;
    uint16_t sport = (socket_info->tcb).local_port;
    uint16_t dport = (socket_info->tcb).remote_port;
    uint16_t rws = (socket_info->tcb).rws;
    uint32_t acknum = (socket_info->tcb).recv_next;
    pthread_mutex_unlock(&g_vsocket_table_mutex);
            
    //build packet and send
    tcp_packet_t tcp_packet;
    buildTCPPacket(&tcp_packet, saddr, daddr, sport, dport,
                   seqnum, acknum, TH_ACK, rws, temp, data_len);
     
    // RTO calculation setup             
    if (do_rto_calc == true) {
        // Critical Section
        pthread_mutex_lock(&g_vsocket_table_mutex);
        if (socket_info->exp_acknum == 0) {
            socket_info->exp_acknum = socket_info->tcb.send_next + data_len;
            socket_info->send_time = clock();    
        }
        pthread_mutex_unlock(&g_vsocket_table_mutex);    
    }
                           
    tcp_sendMessage(socket_info, saddr, daddr, &tcp_packet, TCP_HDR_SIZE+data_len);
}


uint32_t getIndexForTCBSendValue(struct vsocket_infoset* socket_info, uint32_t send_val)
{
    pthread_mutex_lock(&g_vsocket_table_mutex);
    uint32_t index = (send_val - socket_info->tcb.seq_send_init)%socket_info->tcb.sws;
    pthread_mutex_unlock(&g_vsocket_table_mutex);
        
    return index;
}


void handleTCPData(struct vsocket_infoset* socket_info, tcp_packet_t* rv_tcp_packet, size_t rv_tcp_data_len, ip_header_t* ip_header)
{
    uint32_t rv_seqnum = ntohl((rv_tcp_packet->tcp_header).th_seq);
    
    pthread_mutex_lock(&g_vsocket_table_mutex);
    uint32_t recv_next = socket_info->tcb.recv_next;
    uint32_t rws = socket_info->tcb.rws;
    pthread_mutex_unlock(&g_vsocket_table_mutex);
    
    
    // Check if sequence number is valid
    // if RCV.NXT<=SEG.SEQ<RECV.NXT+RCV.WND
    if ((rv_seqnum >= recv_next) &&
        (rv_seqnum < recv_next + rws)) {

        // Data in order if sequence number == recv next
        if (rv_seqnum == recv_next) {
            
            // Clean up pending_data array between socket_info->tcb.recv_next and rv_seqnum.
            size_t start_index = circular_buffer_get_write_index(socket_info->rwin_buffer);
            int index;
            int end = util_min(start_index + rv_tcp_data_len, DEFAULT_WSIZE);
            for (index = start_index; index < end; index++) {
                socket_info->pending_data[index] = 0;
            }
            if (start_index + rv_tcp_data_len > DEFAULT_WSIZE) {
                int wrap = (start_index + rv_tcp_data_len) % DEFAULT_WSIZE;
                for (index = 0; index < wrap; index++) {
                    socket_info->pending_data[index] = 0;
                }
            }
                    
            // Write into circular buffer
            pthread_mutex_lock(&g_vsocket_table_mutex);
            int result = circular_buffer_write(socket_info->rwin_buffer, (void*)rv_tcp_packet->tcp_data, rv_tcp_data_len);
            assert(result == rv_tcp_data_len);
            pthread_mutex_unlock(&g_vsocket_table_mutex);
                        
            // Check for data already received (pending_data array)
            size_t pdata_len = 0;
            size_t write_index = 0;
            size_t total_data_len = rv_tcp_data_len;
            do {
                // Get index
                write_index = circular_buffer_get_write_index(socket_info->rwin_buffer);
                                
                pdata_len = socket_info->pending_data[write_index];
                circular_buffer_increment_write_pointer(socket_info->rwin_buffer, pdata_len);
                socket_info->pending_data[write_index] = 0;
                                
                total_data_len += pdata_len;

            } while (pdata_len != 0);
                                
            // Read as much is possible and copy into in_data
            unsigned char temp[DEFAULT_WSIZE];
            memset(temp, 0, DEFAULT_WSIZE);
            int bytes_read = circular_buffer_read(socket_info->rwin_buffer, &temp, DEFAULT_WSIZE);

            circular_buffer_write(socket_info->in_data, &temp, bytes_read);
                            
            // Critical Section
            pthread_mutex_lock(&g_vsocket_table_mutex);
            // Update the recv_next
            socket_info->tcb.recv_next = socket_info->tcb.recv_next + total_data_len;
            socket_info->tcb.ruws = util_min(socket_info->tcb.ruws + total_data_len, socket_info->tcb.rws);            
            uint32_t seqnum = socket_info->tcb.send_next;
            uint32_t acknum = socket_info->tcb.recv_next;
            uint16_t uws = (uint16_t)(socket_info->tcb.ruws);
            
            uint32_t rv_fin_seqnum = socket_info->tcb.recv_fin;
            recv_next = socket_info->tcb.recv_next;
            pthread_mutex_unlock(&g_vsocket_table_mutex);
            
            // if recv FIN seqnum == recv_next (we aren't expecting any more data)
            // So signal_eof to in_data buffer
            if (rv_fin_seqnum == recv_next) {
                circular_buffer_signal_eof(socket_info->in_data);
                
                // Increment the acknum
                acknum++;
            }
                                                    
            // Send ACK for all data read
            tcp_packet_t ack_tcp_packet;
                            
            uint32_t saddr = ntohl(ip_header->ip_dst);
            uint32_t daddr = ntohl(ip_header->ip_src);
            uint16_t sport = ntohs(rv_tcp_packet->tcp_header.th_dport);
            uint16_t dport = ntohs(rv_tcp_packet->tcp_header.th_sport);
                            
            // Build TCP ACK packet
            buildTCPPacket(&ack_tcp_packet, saddr, daddr, sport, dport, seqnum, acknum, TH_ACK, uws, NULL, 0);
                                           
            tcp_sendMessage(socket_info, saddr, daddr, &ack_tcp_packet, TCP_HDR_SIZE);
                                   
        } else { // Data not in order
                        
            // Critical Section
            pthread_mutex_lock(&g_vsocket_table_mutex);
            size_t offset = rv_seqnum - socket_info->tcb.recv_next;
            pthread_mutex_unlock(&g_vsocket_table_mutex);
                         
            int result = circular_buffer_store(socket_info->rwin_buffer, rv_tcp_packet->tcp_data, rv_tcp_data_len, offset);
            assert(result == rv_tcp_data_len);
                            
            // Update pending_data array
            size_t index = circular_buffer_get_write_index(socket_info->rwin_buffer);
                
            // Critical Section
            pthread_mutex_lock(&g_vsocket_table_mutex);
            socket_info->pending_data[(index+offset) % DEFAULT_WSIZE] = rv_tcp_data_len;
            socket_info->tcb.ruws = util_min(socket_info->tcb.ruws, socket_info->tcb.rws - (offset + rv_tcp_data_len));
                
            uint32_t seqnum = socket_info->tcb.send_next;
            uint32_t acknum = socket_info->tcb.recv_next;
            uint16_t uws = socket_info->tcb.ruws;
            pthread_mutex_unlock(&g_vsocket_table_mutex);
                            
            // Send ACK of recv_next
            tcp_packet_t ack_tcp_packet;
                            
            uint32_t saddr = ntohl(ip_header->ip_dst);
            uint32_t daddr = ntohl(ip_header->ip_src);
            uint16_t sport = ntohs(rv_tcp_packet->tcp_header.th_dport);
            uint16_t dport = ntohs(rv_tcp_packet->tcp_header.th_sport);
                            
            // Build TCP ACK packet
            buildTCPPacket(&ack_tcp_packet, saddr, daddr, sport, dport, seqnum, acknum, TH_ACK, uws, NULL, 0);
                                           
            tcp_sendMessage(socket_info, saddr, daddr, &ack_tcp_packet, TCP_HDR_SIZE);
        }
    // end if sequence number is valid
    } else if (rv_seqnum < recv_next) {
        
        // Critical Section
        pthread_mutex_lock(&g_vsocket_table_mutex);
        uint32_t seqnum = socket_info->tcb.send_next;
        uint32_t acknum = socket_info->tcb.recv_next;
        uint16_t uws = socket_info->tcb.ruws;
        pthread_mutex_unlock(&g_vsocket_table_mutex);
                            
        // Send ACK of recv_next
        tcp_packet_t re_ack_tcp_packet;
      
        uint32_t saddr = ntohl(ip_header->ip_dst);
        uint32_t daddr = ntohl(ip_header->ip_src);
        uint16_t sport = ntohs(rv_tcp_packet->tcp_header.th_dport);
        uint16_t dport = ntohs(rv_tcp_packet->tcp_header.th_sport);
                            
        // Build TCP ACK packet
        buildTCPPacket(&re_ack_tcp_packet, saddr, daddr, sport, dport, seqnum, acknum, TH_ACK, uws, NULL, 0);
                                           
        tcp_sendMessage(socket_info, saddr, daddr, &re_ack_tcp_packet, TCP_HDR_SIZE);
    }
}  


void handleTCPACK(struct vsocket_infoset* socket_info, tcp_packet_t* rv_tcp_packet, size_t rv_tcp_data_len, ip_header_t* ip_header)
{
    assert(rv_tcp_data_len==0);//ACK packet contains no data

    //check ack number of ACK packet
    uint32_t recv_acknum=ntohl((rv_tcp_packet->tcp_header).th_ack);

    pthread_mutex_lock(&g_vsocket_table_mutex);
    uint32_t send_unack=(socket_info->tcb).send_unack;
    uint32_t send_next=(socket_info->tcb).send_next;
    pthread_mutex_unlock(&g_vsocket_table_mutex);
    
    // Check whether ACK acceptable: send_unack<recv_acknum<=send_next
    if (recv_acknum < send_unack || recv_acknum > send_next) return;
       
    // Update remote_ruws
    pthread_mutex_lock(&g_vsocket_table_mutex);
    (socket_info->tcb).remote_ruws = ntohs((rv_tcp_packet->tcp_header).th_win);
    pthread_mutex_unlock(&g_vsocket_table_mutex);
    
    // read from buffer, could slide window by amount of slide_len=recv_acknum-send_unack
    unsigned char temp[DEFAULT_WSIZE];
    memset(temp, 0, DEFAULT_WSIZE);

    int bytes_read = circular_buffer_read(socket_info->swin_buffer, &temp, recv_acknum-send_unack);
    if (bytes_read > 0) {
    
        // Critical Section
        pthread_mutex_lock(&g_vsocket_table_mutex);
        socket_info->tcb.send_unack = send_unack + bytes_read; //update send_unack
        socket_info->tcb.dup_ack = 0; //reset dup
        socket_info->start_time = clock();
        pthread_mutex_unlock(&g_vsocket_table_mutex);
    }

    if (recv_acknum == send_unack) {
    
        // Increment duplicate ACK counter
        pthread_mutex_lock(&g_vsocket_table_mutex);
        socket_info->tcb.dup_ack = socket_info->tcb.dup_ack + 1;
        pthread_mutex_unlock(&g_vsocket_table_mutex);
    }
}

void releaseSocketResources(int vsocket)
{
    // Critical Section 
	pthread_mutex_lock(&g_vsocket_table_mutex);
	struct vsocket_infoset* socket_info = g_hash_table_lookup(s_vsocket_table, &vsocket);
    socket_info->quit = true;
    ptu_releasePort(socket_info->tcb.local_port);
    pthread_mutex_unlock(&g_vsocket_table_mutex);
        
    // Enqueue garbarge packet to get thread to quit
    ip_packet_t ip_packet;
    bqueue_enqueue(&(socket_info->bq_buffer), &ip_packet);
}


void* timeoutThreadFunc(void* arg)
{
    int vsocket = *((int*)arg);
    
    if (pthread_mutex_trylock(&g_timeout_mutex) == LOCK_AQUIRED) {
        sleep(60); // Sleep for 1 min 
        changeState(vsocket, TCPA_TIMEOUT);
        releaseSocketResources(vsocket);
    }

    return NULL;
}
  

//=================================================================================================
//      PUBLIC FUNCTIONS
//=================================================================================================

bool sw_hasSentAllData(int vsocket)
{
    struct vsocket_infoset* socket_info=NULL;
	
	// Critical Section 
	pthread_mutex_lock(&g_vsocket_table_mutex);
	socket_info = g_hash_table_lookup(s_vsocket_table, &vsocket);
    uint32_t send_next = socket_info->tcb.send_next;
    pthread_mutex_unlock(&g_vsocket_table_mutex);
                
    uint32_t write_index = circular_buffer_get_write_index(socket_info->swin_buffer);
    uint32_t next_index = getIndexForTCBSendValue(socket_info, send_next);
             
    return (write_index == next_index);   
}


int sw_writeData(int vsocket, const unsigned char* buf, uint32_t nbyte)
{
    int sent_bytes = 0;
    struct vsocket_infoset* socket_info = NULL;
    
    // Critical Section
    pthread_mutex_lock(&g_vsocket_table_mutex);
    socket_info = (struct vsocket_infoset*)g_hash_table_lookup(s_vsocket_table, &vsocket);    
    
    if (socket_info == NULL) {
        // Release lock
        pthread_mutex_unlock(&g_vsocket_table_mutex);
        return -EBADFD; //fd in bad state
    }
    
    sent_bytes = util_min(circular_buffer_get_available_capacity(socket_info->swin_buffer), nbyte);
    if (sent_bytes == 0) {
        // Release lock
        pthread_mutex_unlock(&g_vsocket_table_mutex);
        return -EAGAIN; //try again
        
    } else {
        int result = circular_buffer_write(socket_info->swin_buffer, (void*)buf, sent_bytes);
        assert(result == sent_bytes);
    }
    
    // Release lock
    pthread_mutex_unlock(&g_vsocket_table_mutex);
    
    return sent_bytes;
}


int sw_readData(int vsocket, const unsigned char* buf, uint32_t nbyte)
{
    struct vsocket_infoset* socket_info = NULL;
    
    // Critical Section
    pthread_mutex_lock(&g_vsocket_table_mutex);
    socket_info = (struct vsocket_infoset*)g_hash_table_lookup(s_vsocket_table, &vsocket);
    pthread_mutex_unlock(&g_vsocket_table_mutex);
    
    if (socket_info == NULL) {
        return -ENOTSOCK; //Socket operation on non-socket
    }

    return circular_buffer_read(socket_info->in_data, (void *)buf, nbyte);
}


void* sw_socketHandlePacketThreadFunc(void* arg)
{
    int error_code = 0;

    struct handleFuncArg* harg=(struct handleFuncArg*)arg;
    int vsocket = harg->socket;

    ip_packet_t* ip_packet=NULL;   
    pthread_mutex_lock(&g_vsocket_table_mutex);
    struct vsocket_infoset* socket_info = g_hash_table_lookup(s_vsocket_table, &vsocket);
    pthread_mutex_unlock(&g_vsocket_table_mutex);    

    
    while (!socket_info->quit) {
        
        bqueue_dequeue(&(socket_info->bq_buffer), (void**)&ip_packet);
        
        if (socket_info->quit) {
            break;
        }

        tcp_packet_t* rv_tcp_packet=(tcp_packet_t*)(ip_packet->ip_data);
        int rv_tcp_data_len=ntohs((ip_packet->ip_header).ip_len)-IP_HEADER_SIZE-TCP_HEADER_SIZE;

        uint8_t recv_flag=(rv_tcp_packet->tcp_header).th_flags;
        
        // Handle any data received
        if (rv_tcp_data_len > 0) { //recv data

            handleTCPData(socket_info, rv_tcp_packet, rv_tcp_data_len, &(ip_packet->ip_header));
                  
        } else {
        
            // Handle ACK
            if ((recv_flag & TH_ACK) != 0) {

                handleTCPACK(socket_info, rv_tcp_packet, rv_tcp_data_len, &(ip_packet->ip_header));
                
                pthread_mutex_lock(&g_vsocket_table_mutex);
                uint32_t seq_fin = socket_info->tcb.seq_fin;
                pthread_mutex_unlock(&g_vsocket_table_mutex);

                // if ACK's acknum=seq_fin+1 => ACK of FIN packet received
                if (ntohl(rv_tcp_packet->tcp_header.th_ack) == seq_fin + 1) {
                
                    if (isValidAction(vsocket, TCPA_RECV_ACK_FIN, &error_code)){
                        changeState(vsocket, TCPA_RECV_ACK_FIN);
                        
                        // if valid action, then our state is CLOSED, and our state was LAST-ACK
                        if (isValidAction(vsocket, TCPA_CLOSE, &error_code)) {
                            releaseSocketResources(vsocket);
                        }
                    }
                } // end recv ACK of FIN

            } // end if flag & TH_ACK

            if ((recv_flag & TH_FIN) != 0) {
        
                if (!isValidAction(vsocket, TCPA_RECV_FIN, &error_code)){
                    continue;
                }
                   
                // Critical Section
                pthread_mutex_lock(&g_vsocket_table_mutex);
                socket_info->tcb.recv_fin = ntohl(rv_tcp_packet->tcp_header.th_seq);
                uint32_t rv_fin_seqnum = socket_info->tcb.recv_fin;
                uint32_t recv_next = socket_info->tcb.recv_next;
                pthread_mutex_unlock(&g_vsocket_table_mutex);
                
                changeState(vsocket, TCPA_RECV_FIN);
                
                // if recv FIN seqnum == recv_next (we aren't expecting any more data)
                // So signal_eof to in_data buffer
                if (rv_fin_seqnum != recv_next) {
                    continue;
                }

                circular_buffer_signal_eof(socket_info->in_data);
            
                //Build and send ACK, acknum is seq_rcvd+1
                tcp_packet_t ackfin_packet;
                
                // Critical Section
                pthread_mutex_lock(&g_vsocket_table_mutex);
                uint32_t saddr = socket_info->tcb.local_vip;
                uint16_t sport = socket_info->tcb.local_port;
                uint32_t daddr = socket_info->tcb.remote_vip;
                uint16_t dport = socket_info->tcb.remote_port;
                uint16_t wsize = socket_info->tcb.ruws;
                uint32_t seqnum = socket_info->tcb.send_next;
                  
                if (seqnum == socket_info->tcb.seq_fin) { // We already sent FIN
                    seqnum++;
                }
                   
                pthread_mutex_unlock(&g_vsocket_table_mutex);
                   
                buildTCPPacket(&ackfin_packet, saddr, daddr, sport, dport, 
                               seqnum, 1+rv_fin_seqnum, TH_ACK, wsize, NULL, 0);  

                tcp_sendMessage(socket_info, saddr, daddr, &ackfin_packet, TCP_HDR_SIZE);
                 
            } // end if flag & TH_FIN
            
            // Check is new state is TCPS_TIME_WAIT
            if (isValidAction(vsocket, TCPA_TIMEOUT, &error_code)) {
                pthread_t thread_id;
                pthread_create(&thread_id, NULL, timeoutThreadFunc, (void*)(&vsocket));
            }
            
        } // end else (not data packet)

        free(ip_packet);
       
    } //end while loop
    
    free(harg);
    
    return NULL;
}


//be responsible of sending tcp data
void* sw_socketSendDataThreadFunc(void* arg)
{
    const double CLOCKS_PER_USEC = CLOCKS_PER_SEC/1000000;
    
    struct sendFuncArg* sarg=(struct sendFuncArg*)arg;
    int vsocket = sarg->socket;

    // Critical Section
    pthread_mutex_lock(&g_vsocket_table_mutex);
    struct vsocket_infoset* socket_info = g_hash_table_lookup(s_vsocket_table, &vsocket);
    socket_info->start_time = clock();
    pthread_mutex_unlock(&g_vsocket_table_mutex);
    

    while (!socket_info->quit) {
            
        pthread_mutex_lock(&g_vsocket_table_mutex);
        int dup_ack_count = socket_info->tcb.dup_ack;
        uint32_t send_next = socket_info->tcb.send_next;
        uint32_t send_unack = socket_info->tcb.send_unack;
        uint16_t remote_ruws = socket_info->tcb.remote_ruws;
        clock_t start_time = socket_info->start_time;
        double rto_us = socket_info->rto_us;
        pthread_mutex_unlock(&g_vsocket_table_mutex);
        
        // Handle three things:
        // #1 - if (timeout) retransmit, reset timer
        if ((double)(clock()-start_time)/(CLOCKS_PER_USEC) > (2*rto_us)) {
            
            if (send_next != send_unack) {
            
                uint32_t data_len = util_min((send_next - send_unack), TCP_MTU);
                
                uint32_t unack_index = getIndexForTCBSendValue(socket_info, send_unack);
                // Retransmit
                transmitUnACKedData(socket_info, unack_index, data_len, send_unack, false);
            }
            
            // Critical Section
            pthread_mutex_lock(&g_vsocket_table_mutex);
            socket_info->exp_acknum = 0;
            socket_info->start_time = clock();
            pthread_mutex_unlock(&g_vsocket_table_mutex);
        }
        
        // #2 - if (dup_ack == 3) retransmit, reset timer and dup_ack
        if (dup_ack_count >= 3) {
            // Critical Section
            pthread_mutex_lock(&g_vsocket_table_mutex);
            socket_info->tcb.dup_ack = 0;
            pthread_mutex_unlock(&g_vsocket_table_mutex);
            
            uint32_t data_len = util_min((send_next - send_unack), TCP_MTU);
            
            if (data_len != 0) {
            
                uint32_t unack_index = getIndexForTCBSendValue(socket_info, send_unack);
	            // Retransmit
                transmitUnACKedData(socket_info, unack_index, data_len, send_unack, false);
            
                // Critical Section
                pthread_mutex_lock(&g_vsocket_table_mutex);
                socket_info->start_time = clock();
                socket_info->exp_acknum = 0;
                pthread_mutex_unlock(&g_vsocket_table_mutex);
            
            }
        }      

        // #3 - send new data, update send_next
        // Critical Section
        pthread_mutex_lock(&g_vsocket_table_mutex);
        send_next = socket_info->tcb.send_next;
        send_unack = socket_info->tcb.send_unack;
        uint32_t write_index = circular_buffer_get_write_index(socket_info->swin_buffer);
        pthread_mutex_unlock(&g_vsocket_table_mutex);
        
        uint32_t next_index = getIndexForTCBSendValue(socket_info, send_next);
        if (write_index != next_index) {

            uint32_t data_len = 0;
        
            if (write_index > next_index) {
                data_len = write_index - next_index;
            }
            if (write_index < next_index) {
                data_len = write_index + (DEFAULT_WSIZE - next_index);
            } 
            data_len = util_min(data_len, util_min(remote_ruws, TCP_MTU));
         
            // Update send_next
            pthread_mutex_lock(&g_vsocket_table_mutex);
            socket_info->tcb.send_next = send_next + data_len;
            pthread_mutex_unlock(&g_vsocket_table_mutex);
            
            transmitUnACKedData(socket_info, next_index, data_len, send_next, true);
            
        } else if (send_next == send_unack && circular_buffer_is_full(socket_info->swin_buffer)) {
            
            uint32_t data_len = util_min(remote_ruws, TCP_MTU);
            
            // Update send_next
            pthread_mutex_lock(&g_vsocket_table_mutex);
            socket_info->tcb.send_next = send_next + data_len;
            pthread_mutex_unlock(&g_vsocket_table_mutex);
           
            transmitUnACKedData(socket_info, next_index, data_len, send_next, false);
        }
        
    } // end while()
    
    return NULL;
}


//=================================================================================================
//      END OF FILE
//=================================================================================================
