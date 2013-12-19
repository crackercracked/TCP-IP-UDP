//================================================================================================= 
//      Author: Brett Decker and Xinwei Liu
//=================================================================================================
//      INCLUDE FILES
//=================================================================================================

#include "logger.h"
#include "tcp_layer.h"

#include <string.h>
#include <fcntl.h>

//=================================================================================================
//      DEFINITIONS AND MACROS
//=================================================================================================

#define BUF_SIZE    1024

#define SEND_MSG    "[ Logger: Sent TCP Packet ] Timestamp: "
#define RECV_MSG    "[ Logger: Received TCP Packet ] Timestamp: "


//=================================================================================================
//      PUBLIC FUNCTIONS
//=================================================================================================

void lgr_open(int vsocket)
{
    pthread_mutex_lock(&g_vsocket_table_mutex);
    struct vsocket_infoset* socket_info = g_hash_table_lookup(s_vsocket_table, &vsocket);
    uint32_t saddr = socket_info->tcb.local_vip;
    pthread_mutex_unlock(&g_vsocket_table_mutex);
        
    pthread_mutex_init(&(socket_info->logger.lock), NULL);
    
    char filename[35];
    memset(filename, 0, 35);
    sprintf(filename, "log_s%d_vip_%s.txt", vsocket, util_convertVIPInt2StringNoPeriod(saddr));
    
    // Critical Section
    pthread_mutex_lock(&(socket_info->logger.lock));
    socket_info->logger.fd = open(filename, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
    pthread_mutex_unlock(&(socket_info->logger.lock));
}


void lgr_storeTCPPacket(void* info, struct timespec* receipt_time, tcp_packet_t* tcp_packet,
                        uint32_t packet_len, Packet_Type_t pkt_type)
{
    struct vsocket_infoset* socket_info = (struct vsocket_infoset*)info;
    
    char buf[BUF_SIZE];
    
    memset(buf, 0, BUF_SIZE);

    if (pkt_type == SEND_PKT) {
        strcpy(buf, SEND_MSG);
    } else if (pkt_type == RECV_PKT) {
        strcpy(buf, RECV_MSG);
    }
    
    char temp[30];
    
    //printf("secs: %u\n", (uint32_t)(receipt_time->tv_sec));
    //printf("nsecs: %lu\n", receipt_time->tv_nsec);
    
    memset(temp, 0, 30);
    sprintf(temp, "%u.%9lu", (uint32_t)(receipt_time->tv_sec), receipt_time->tv_nsec);
    strcat(buf, temp);
 
    strcat(buf, "; Seq Num: ");
    memset(temp, 0, 30);
    sprintf(temp, "%d", ntohl(tcp_packet->tcp_header.th_seq));
    strcat(buf, temp);
   
    strcat(buf, "; ACK Num: ");   
    memset(temp, 0, 30);
    sprintf(temp, "%d", ntohl(tcp_packet->tcp_header.th_ack));
    strcat(buf, temp);
    
    strcat(buf, "; Flags: ");   
    memset(temp, 0, 30);
    sprintf(temp, "%d", tcp_packet->tcp_header.th_flags);
    strcat(buf, temp);
    
    strcat(buf, "; Len: ");   
    memset(temp, 0, 30);
    sprintf(temp, "%d", packet_len-20);
    strcat(buf, temp);
    
    strcat(buf, "\n");
    
    // Critical Section
    pthread_mutex_lock(&(socket_info->logger.lock));
    write(socket_info->logger.fd, buf, strlen(buf));
    pthread_mutex_unlock(&(socket_info->logger.lock));
}


void lgr_close(int vsocket)
{
    pthread_mutex_lock(&g_vsocket_table_mutex);
    struct vsocket_infoset* socket_info = g_hash_table_lookup(s_vsocket_table, &vsocket);
    pthread_mutex_unlock(&g_vsocket_table_mutex);
    
    // Critical Section
    pthread_mutex_lock(&(socket_info->logger.lock));
    close(socket_info->logger.fd);
    pthread_mutex_unlock(&(socket_info->logger.lock));
}


//=================================================================================================
//      END OF FILE
//=================================================================================================
