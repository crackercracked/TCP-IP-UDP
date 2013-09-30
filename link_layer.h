#ifndef _LINK_LAYER_H_
#define _LINK_LAYER_H_

//================================================================================================= 
//      Author: Brett Decker and Xinwei Liu
//=================================================================================================
//      INCLUDE FILES
//=================================================================================================
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netdb.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <errno.h>
#include <inttypes.h>
#include <assert.h>
#include <glib.h>

#include "util/parselinks.h"
//#include "util/htable.h"





//=================================================================================================
//      DEFINITIONS AND MACROS
//=================================================================================================
typedef enum{true=1, false=0} bool;




//=================================================================================================
//      PUBLIC FUNCTION DECLARATIONS
//=================================================================================================


void printSocketAddrPair(int* socket, struct in_addr* addr);
void printAddrSocketPair(unsigned long* addr_num, int* socket);
void printLookupTable(GHashTable* table);
void printForwardingTable();



void sendPacket(void);
void receivePacket(void);
void setupForwardingTableAndSockets(list_t* list);
bool dnsLookUp(char* dnsName, uint16_t port, struct sockaddr_in* addr, size_t* addrlen);



//=================================================================================================
//      END OF FILE
//=================================================================================================

#endif // _LINK_LAYER_H_
