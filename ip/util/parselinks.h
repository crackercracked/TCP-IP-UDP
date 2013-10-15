#ifndef _PARSELINKS_H_
#define _PARSELINKS_H_

#include <netinet/if_ether.h>
#include <netinet/in.h>

#include "list.h"

#define HOST_MAX_LENGTH 256 // RFC 2181

typedef struct {
	// Info about the local side of the link.
	char local_phys_host[HOST_MAX_LENGTH];
	uint16_t local_phys_port;
	struct in_addr local_virt_ip;
	
	// Info about the remote side of the link.
	char remote_phys_host[HOST_MAX_LENGTH];
  	uint16_t remote_phys_port;
  	struct in_addr remote_virt_ip;
} link_t;

/*
 * Returns a list of link_t's. The user is responsible for 
 * calling free_links on the list after using it to avoid memory leaks.
 */
list_t* parse_links(char *filename);

/*
 * Frees the memory used by the list of links, including
 * the links themselves. Call this when you are done with the links.
 */
void free_links(list_t *links);

#endif