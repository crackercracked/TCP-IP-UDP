#ifndef _HEADERS_H_
#define _HEADERS_H_

#include <pthread.h>

#define _BSD_SOURCE 1   // needed for struct ip to be recognized

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <inttypes.h>

#include "util/parselinks.h"
#include "util/ipsum.h"
#include "util/list.h"



#define MASK_BYTE1(value)   (((value) & 0xFF000000) >> 24)
#define MASK_BYTE2(value)   (((value) & 0x00FF0000) >> 16)
#define MASK_BYTE3(value)   (((value) & 0x0000FF00) >> 8)
#define MASK_BYTE4(value)    ((value) & 0x000000FF)



#endif //_HEADERS_H_
