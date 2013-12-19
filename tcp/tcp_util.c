#include "tcp_util.h"


uint16_t tcp_checksum(char* packet, size_t len)
{
  uint16_t *p = (uint16_t*)packet;
  uint16_t answer;
  long sum = 0;
  uint16_t odd_byte = 0;

  while (len > 1) {
    sum += *p++;
    len -= 2;
  }

  /* mop up alen odd byte, if lenecessary */
  if (len == 1) {
    *(uint8_t*)(&odd_byte) = *(uint8_t*)p;
    sum += odd_byte;
  }

  sum = (sum >> 16) + (sum & 0xffff); /* add hi 16 to low 16 */
  sum += (sum >> 16);           /* add carry */
  answer = ~sum;                /* olenes-complemelent, trulencate*/
  return answer;

}



void buildTCPPacket(tcp_packet_t* tcp_packet, uint32_t saddr, uint32_t daddr, u_short sport, u_short dport, 
                    uint32_t seqnum, uint32_t ack, u_char flag, u_short wsize, char* data, size_t data_len)
{
  //populate header
  (tcp_packet->tcp_header).th_sport=htons(sport);
  (tcp_packet->tcp_header).th_dport=htons(dport);
  (tcp_packet->tcp_header).th_seq=htonl(seqnum); //originally htonl
  (tcp_packet->tcp_header).th_ack=htonl(ack); //previously htonl
  (tcp_packet->tcp_header).th_flags=flag;
  (tcp_packet->tcp_header).th_win=htons(wsize);
  (tcp_packet->tcp_header).th_x2=0;
  (tcp_packet->tcp_header).th_off=sizeof(struct tcphdr)/4;
  (tcp_packet->tcp_header).th_urp=htons(0);

 
  //compute checksum by forming pseudo header
  (tcp_packet->tcp_header).th_sum=0;
  pseudo_tcphdr_t pseudo_header;
  pseudo_header.source_addr=htonl(saddr);
  pseudo_header.dest_addr=htonl(daddr);
  pseudo_header.reserved=0;
  pseudo_header.protocol=TCP_PROTOCOL;
  pseudo_header.tcp_len=htons(sizeof(struct tcphdr)+data_len);
  

  //copy pseudo header, tcp header and tcp data to buffer
  int tcp_w_pseudo_len=sizeof(pseudo_header)+sizeof(struct tcphdr)+data_len;

  char tcp_w_pseudo[tcp_w_pseudo_len];
  int start=0;
  memcpy(tcp_w_pseudo+start, (char*)&pseudo_header, sizeof(pseudo_header));
  start+=sizeof(pseudo_header);
  memcpy(tcp_w_pseudo+start, (char*)&(tcp_packet->tcp_header), sizeof(struct tcphdr));
  start+=sizeof(struct tcphdr);
  memcpy(tcp_w_pseudo+start, data, data_len);  

  (tcp_packet->tcp_header).th_sum=tcp_checksum(tcp_w_pseudo, tcp_w_pseudo_len);

  memset(tcp_packet->tcp_data, 0, TCP_MTU);
  
  //copy data
  memcpy(tcp_packet->tcp_data, data, data_len);
}







void printTCPPacket(tcp_packet_t* tcp_packet)
{
    printf("\n------print TCP packet-------\n");
    printf("source port: %u\n", ntohs(tcp_packet->tcp_header.th_sport));
    printf("dest port: %u\n", ntohs(tcp_packet->tcp_header.th_dport));
    printf("sequence number: %u\n", ntohl(tcp_packet->tcp_header.th_seq));
    printf("acknowledge: %u\n", ntohl(tcp_packet->tcp_header.th_ack));
    printf("flag: %u\n", (tcp_packet->tcp_header.th_flags));
    printf("sliding window size: %u\n", ntohs(tcp_packet->tcp_header.th_win));
    printf("checksum: %u\n", (tcp_packet->tcp_header.th_sum));
    printf("tcp data: %s\n", tcp_packet->tcp_data);
    printf("------end of TCP packet-------\n\n");
}



/*
uint32_t convertVIPString2Int(char* vip_address)
{
    uint32_t vip_integer, byte;
    uint32_t vip_bytes[4];

    sscanf(vip_address, "%d.%d.%d.%d", vip_bytes, vip_bytes+1, vip_bytes+2, vip_bytes+3);

    vip_integer = 0;
    for (byte = 0; byte < 4; byte++) {
        if ((vip_bytes[byte] < 0) || (vip_bytes[byte] > 255)) {
            return ERROR;
        }
        vip_integer |= (vip_bytes[byte] << (24 - (byte*8)));  //  vip_bytes[0] is MSB
    }

    return vip_integer;

}



char* convertVIPInt2String(uint32_t vip_addr_num)
{
    char* buffer=(char*)malloc(50);
    sprintf(buffer, "%d.%d.%d.%d", MASK_BYTE1(vip_addr_num), MASK_BYTE2(vip_addr_num), MASK_BYTE3(vip_addr_num), MASK_BYTE4(vip_addr_num));
    return buffer;
}
*/


