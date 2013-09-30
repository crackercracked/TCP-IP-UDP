#ifndef _LINK_LAYER_H_
#define _LINK_LAYER_H_

//================================================================================================= 
//      Author: Brett Decker and Xinwei Liu
//=================================================================================================
//      INCLUDE FILES
//=================================================================================================

#include "utility.h"

#include <glib.h>

/*
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


#include "util/parselinks.h"
*/

// Mutex to protect s_routing_table
extern pthread_mutex_t g_routing_table_mutex;
extern GHashTable* s_routing_table;
extern GHashTable* g_read_socket_addr_lookup;
extern fd_set g_input_interfaces;           // All input UDP sockets and STDIN [used by select()]
extern int g_highest_input_interface;       // Highest file descriptor in g_input_interfaces

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



void sendPacket(char* payload, size_t payload_len, int vip_address);
void receivePacket(char* payload, size_t* payload_len, int max_capacity, int socket);
void setupForwardingTableAndSockets(list_t* list);
bool dnsLookUp(char* dnsName, uint16_t port, struct sockaddr_in* addr, size_t* addrlen);



//=================================================================================================
//      END OF FILE
//=================================================================================================

#endif // _LINK_LAYER_H_
