#ifndef _UTILITY_H_
#define _UTILITY_H_

//================================================================================================= 
//      Author: Brett Decker and Xinwei Liu
//=================================================================================================
//      INCLUDE FILES
//=================================================================================================

#include <pthread.h>

#include <assert.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <inttypes.h>

#include "util/parselinks.h"
#include "util/ipsum.h"
#include "util/list.h"

//=================================================================================================
//      DEFINITIONS AND MACROS
//=================================================================================================

#define STDIN               0
#define STDOUT              1

#define ERROR               -1


#define IP_MAX_PACKET_SIZE  65535
#define IP_HEADER_SIZE      20

#define MASK_BYTE1(value)   (((value) & 0xFF000000) >> 24)
#define MASK_BYTE2(value)   (((value) & 0x00FF0000) >> 16)
#define MASK_BYTE3(value)   (((value) & 0x0000FF00) >> 8)
#define MASK_BYTE4(value)    ((value) & 0x000000FF)

#define MASK_MSB(value)     (((value) & 0xFF00) >> 8)
#define MASK_LSB(value)      ((value) & 0x00FF)


//=================================================================================================
//      PUBLIC TYPEDEFS AND STRUCTS
//=================================================================================================

// bool type
typedef enum {
    true=1,
    false=0
} bool;


/*
 * Structure of an internet header, naked of options.
 */
typedef struct {

#if __BYTE_ORDER == __LITTLE_ENDIAN
    unsigned int ip_hl:4;		/* header length */
    unsigned int ip_v:4;		/* version */
#endif
#if __BYTE_ORDER == __BIG_ENDIAN
    unsigned int ip_v:4;		/* version */
    unsigned int ip_hl:4;		/* header length */
#endif
    uint8_t  ip_tos;			/* type of service */
    uint16_t ip_len;			/* total length */
    uint16_t ip_id;			    /* identification */
    uint16_t ip_off;			/* fragment offset field */
#define	IP_RF 0x8000			/* reserved fragment flag */
#define	IP_DF 0x4000			/* dont fragment flag */
#define	IP_MF 0x2000			/* more fragments flag */
#define	IP_OFFMASK 0x1fff		/* mask for fragmenting bits */
    uint8_t  ip_ttl;			/* time to live */
    uint8_t  ip_p;			    /* protocol */
    uint16_t ip_sum;		    /* checksum */
    uint32_t ip_src;            /* source address */
    uint32_t ip_dst;	        /* destination address */
    
} ip_header_t;


typedef enum ip_protocol {

    TEST_PROTOCOL = 0x00,
    TCP_PROTOCOL  = 0x06,
    RIP_PROTOCOL  = 0xC8    // (200 decimal)

} ip_protocol_t;


typedef struct {

    ip_header_t ip_header;
    char ip_data[IP_MAX_PACKET_SIZE-IP_HEADER_SIZE];
    
} ip_packet_t;


typedef void (*handler_t)(ip_packet_t*);


//=================================================================================================
//      PUBLIC FUNCTIONS
//=================================================================================================

void util_netRegisterHandler(uint8_t protocol, handler_t handler);
void util_callHandler(uint8_t protocol, ip_packet_t* ip_packet);

uint32_t util_convertVIPString2Int(char* vip_address);
char* util_convertVIPInt2String(uint32_t vip_addr_num);
char* util_convertVIPInt2StringNoPeriod(uint32_t vip_addr_num);

uint16_t util_getU16(unsigned char* buffer);

uint32_t util_getU32(unsigned char* buffer);

uint32_t util_min(uint32_t val1, uint32_t val2);

uint32_t util_max(uint32_t val1, uint32_t val2);

double util_abs(double val);

//=================================================================================================
//      END OF FILE
//=================================================================================================

#endif //_UTILITY_H_
