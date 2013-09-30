#ifndef _UTILITY_H_
#define _UTILITY_H_

#include <pthread.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <inttypes.h>

#include "util/parselinks.h"
#include "util/ipsum.h"
#include "util/list.h"


#define STDIN               0
#define STDOUT              1

#define ERROR               -1


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
    uint16_t ip_sum;			/* checksum */
    uint32_t ip_src;            /* source address */
    uint32_t ip_dst;	        /* destination address */
    
} ip_header_t;


#define MASK_BYTE1(value)   (((value) & 0xFF000000) >> 24)
#define MASK_BYTE2(value)   (((value) & 0x00FF0000) >> 16)
#define MASK_BYTE3(value)   (((value) & 0x0000FF00) >> 8)
#define MASK_BYTE4(value)    ((value) & 0x000000FF)


#endif //_UTILITY_H_
