//================================================================================================= 
//      Author: Brett Decker and Xinwei Liu
//=================================================================================================
//      INCLUDE FILES
//=================================================================================================

#include "headers.h"

#include "ip_lib.h"
#include "link_layer.h"

#include <glib.h>
/*
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

#include <glib.h>

#include "util/parselinks.h"
#include "util/ipsum.h"
#include "util/list.h"

#include "ip_lib.h"
#include "link_layer.h"
*/
//=================================================================================================
//      DEFINITIONS AND MACROS
//=================================================================================================

#define MIN_ARGUMENTS           2

#define USER_MAX_CMD_SIZE       15

#define LINK_DOWN               -1
#define RECEIVED_IP_PACKET      0x11
#define SENT_IP_PACKET          0x16

//=================================================================================================
//      GLOBAL VARIABLES
//=================================================================================================

// Mutex to protect s_interfaces_operable
static pthread_mutex_t g_interfaces_operable_mutex = PTHREAD_MUTEX_INITIALIZER;
static GHashTable* s_interfaces_operable = NULL;

// Mutex to protect function sendPacket()
static pthread_mutex_t g_send_packet_mutex = PTHREAD_MUTEX_INITIALIZER;

// list of all neighbors (identified by VIP address??)
static list_t* g_interfaces_list = NULL;

static int g_my_vip_address = 0;

static struct ip* g_default_ip_header = NULL;

static int g_quit = FALSE;

static int TRUE_VALUE = TRUE;      // treat as const, do not change
static int FALSE_VALUE = FALSE;     // treat as const, do not change

// Error Printing
//error = socket(....);
//if (error == -1) perror("socket");


//=================================================================================================
//      PRIVATE FUNCTIONS
//=================================================================================================

void initializeIPHeader(struct ip* ip_header)
{
    memset(ip_header, 0, IP_HEADER_BYTES);
    
    // Default settings of IP header
    
    ip_header.ip_v = IPv4;
    ip_header.ip_hl = IP_HEADER_WORDS;
    ip_header.ip_off = IP_DF; // defined in netinet/ip.h
    ip_header.ip_ttl = IP_TTL_DEFAULT;
    
}


int sendIPPacket(int to_vip_address, ip_protocol_t protocol, char* data, size_t data_len)
{

    int* value_ptr = NULL;
    struct ip ip_header;
    char buffer[IP_MAX_PACKET_SIZE];    
    
    
    // Protect shared variable s_interfaces_operable
    pthread_mutex_lock(&g_interfaces_operable_mutex);
    value_ptr = (int*)(g_hash_table_lookup(s_interfaces_operable, &to_vip_address));
    pthread_mutex_unlock(&g_interfaces_operable_mutex);
    
    // Check if link to node at vip_address is up
    if ((*value_ptr) == TRUE) {
    
        memcpy(&ip_header, g_default_ip_header, IP_HEADER_BYTES);
        
        ip_header.ip_len = htonl(IP_HEADER_BYTES+data_len);
        ip_header.ip_p = protocol;
        ip_header.ip_src.s_addr = htonl(g_my_vip_address);
        ip_header.ip_dst.s_addr = htonl(to_vip_address);
        
        ip_header.ip_sum = htons((unsigned short)ip_sum((char*)&ip_header, IP_HEADER_BYTES));
        
        memcpy(buffer, &ip_header, IP_HEADER_BYTES);
        memcpy(&(buffer[IP_HEADER_BYTES]), data, data_len);
    
        // Protect shared function sendPacket()
        pthread_mutex_lock(&g_send_packet_mutex);
        //sendPacket(data, data_len+IP_HEADER_BYTES, vip_address);
        pthread_mutex_unlock(&g_send_packet_mutex);
        
        return SENT_IP_PACKET;
    }

    return LINK_DOWN;
}


int receiveIPPacket(int vip_address, struct ip_full_packet* ip_packet)
{
    int in_data_len = 0;
    char in_data[IP_MAX_PACKET_SIZE];
    int* value_ptr = NULL;
    
    
    // Protect shared variable s_interfaces_operable
    pthread_mutex_lock(&g_interfaces_operable_mutex);
    value_ptr = (int*)(g_hash_table_lookup(s_interfaces_operable, &vip_address));
    pthread_mutex_unlock(&g_interfaces_operable_mutex);
    
    // Check if link to node at vip_address is up
    if ((*value_ptr) == TRUE) {
    
        //receivePacket(in_data, &in_data_len, max_capacity, vip_address);
        memcpy(ip_packet, in_data, IP_HEADER_BYTES);
        

        return RECEIVED_IP_PACKET;
    }

    return LINK_DOWN;
}


void setupInterfacesOperableTable(list_t* interfaces_list, GHashTable* interfaces_operable)
{
    link_t* link;
    node_t* current;

    // Create hash table
    interfaces_operable = g_hash_table_new(g_int_hash, g_int_equal);
    
    for (current = interfaces_list->head; current != NULL; current = current->next) {
        link = (current->data);
        
        g_hash_table_insert(interfaces_operable, &(link->remote_virt_ip.s_addr), &TRUE_VALUE);
    }
}


void handleUserInput(void)
{
    // Valid commands:
    // - interfaces
    // - routes
    // - down (integer)
    // - up (integer)
    // - send (VIP) (protocol) (data)
    // - quit
    
    int interface = 0;
    int protocol = 0;
    int vip = 0;
    char vip_address[VIP_ADDRESS_SIZE];
    char command[USER_MAX_CMD_SIZE];
    char buffer[IP_MAX_PACKET_SIZE+100]; // Needs to be >64k because "data" in "send" command could be that big
    char data[IP_MAX_PACKET_SIZE];
    
    memset(command, 0, USER_MAX_CMD_SIZE);
    memset(buffer, 0, IP_MAX_PACKET_SIZE+100);
    memset(data, 0, IP_MAX_PACKET_SIZE);
    
    fgets(buffer, IP_MAX_PACKET_SIZE, stdin);
    
    //printf("buffer: %s\n", buffer);
    
    if (strncmp(buffer, "interfaces", 10) == 0) {
        // Print interfaces
        printInterfaces(g_interfaces_list);
        
    } else if (strncmp(buffer, "routes", 6) == 0) {
        // Print routes
        printf("Unimplemented\n");
        
    } else if (strncmp(buffer, "down", 4) == 0) {
        // Extract integer identifying interface
        sscanf(buffer, "%*s %d", &interface);
        
        //printf("Interface: %d\n", interface);
        
        // Protect shared variable s_interfaces_operable
        pthread_mutex_lock(&g_interfaces_operable_mutex);
        //htable_put(s_interfaces_operable, vip_address, &FALSE_VALUE);
        pthread_mutex_unlock(&g_interfaces_operable_mutex);
        
    } else if (strncmp(buffer, "up", 2) == 0) {
        // Extract integer identifying interface
        sscanf(buffer, "%*s %d", &interface);address
        
        //printf("Interface: %d\n", interface);
        
        // Protect shared variable s_interfaces_operable
        pthread_mutex_lock(&g_interfaces_operable_mutex);
        //htable_put(s_interfaces_operable, vip_address, &TRUE_VALUE);
        pthread_mutex_unlock(&g_interfaces_operable_mutex);
        
    } else if (strncmp(buffer, "send", 4) == 0) {
        // Extract string identifying VIP Address, integer for protocol, and string for data
        sscanf(buffer, "%*s %s %d %s", vip_address, &protocol, data);
        
        vip = convertVIPStringToInt(vip_address);
        
        printf("VIP Address: %s; %d\n", vip_address, vip);
        printf("Protocol: %d\n", protocol);
        printf("Data: %s, size: %d\n", data, (addressint)strlen(data));
        
        sendIPPacket(vip, protocol, data, strlen(data));
        
    } else if (strncmp(buffer, "quit", 4) == 0) {
        g_quit = TRUE;
    }
}


void handlePacketInput(int interface)
{
    
    int* vip_address_ptr = NULL;
    
    
    //vip_address_ptr = (int*)(g_hash_table_lookup(g_read_socket_addr_lookup, &interface);
    if (receiveIPPacket((*vip_address_ptr), data, &data_len, IP_MAX_PACKET_SIZE) == LINK_DOWN) {
        return; // Link is down, so do nothing
    }
    
    // TODO process data
    
}


void handlePacketAndUserInput(void)
{
    int interface = 0;
    int ready_interfaces = 0;
    fd_set input_interfaces_copy;         // Stores read copy of g_input_interfaces

    
    while (!g_quit) {
    
        printf("> ");
        fflush(stdout);
    
        input_interfaces_copy = g_input_interfaces;
        
        // Only care about read file descriptors
        ready_interfaces = select(g_highest_input_interface+1, &input_interfaces_copy, NULL, NULL, NULL);
        if (ready_interfaces == ERROR) {
            perror("select");
            exit(-1);
        }
        
        for (interface = 0; interface < g_highest_input_interface+1; interface++) {
        
            if (FD_ISSET(interface, &input_interfaces_copy)) {
            
                if (interface == STDIN) {
                    handleUserInput();
                } else {
                    handlePacketInput(interface);
                }

            } // end if FD_ISSET()
        
        } // end for
    
    } // end while
    
}


//=================================================================================================
//      MAIN FUNCTION
//=================================================================================================
int main(int argc, char** argv)
{
    #ifndef __USE_BSD
        printf("ERROR: __USE_BSD is not defined!\n");
        return -1; // Error
    #endif
    
    // Executable: ./node file.lnx  [2 args, second is .lnx file]
    if (argc < MIN_ARGUMENTS) {
        printf("ERROR: Not enough arguments passed.\nUsage: ./node file.lnx\n");
        return -1; // Error
    }
    
    // Clear file descriptor set g_input_interfaces
    FD_ZERO(&g_input_interfaces);
    
    // Parse .lnx file
    g_interfaces_list = parse_links(argv[1]);
    
    // Store the node's VIP Address
    g_my_vip_address = ((link_t*)(g_interfaces_list->head->data))->local_virt_ip.s_addr;
    
    setupInterfacesOperableTable(g_interfaces_list, s_interfaces_operable);
    
    // Create Forwarding Table (memory allocation if needed)
    // Create all UDP connections
    // and store sockets in Forwarding Table
    //setupForwardingTableAndSockets(g_interfaces_list);
    
    // Add STDIN to g_input_interfaces
    FD_SET(STDIN, &g_input_interfaces);
    
    // Routing Stuff (RIP)
    
    // spawn Interface Update Thread
    //pthread_create(&threadID, NULL, thread_function, NULL);
    
    // Handle Packet Forwarding and User Input
    handlePacketAndUserInput();
    
    // Free memory, destroy list of links

    return 0;
}


//=================================================================================================
//      END OF FILE
//=================================================================================================
