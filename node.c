//================================================================================================= 
//      Author: Brett Decker and Xinwei Liu
//=================================================================================================
//      INCLUDE FILES
//=================================================================================================

#include "utility.h"

#include "ip_lib.h"
#include "link_layer.h"

#include <glib.h>

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

static ip_header_t* g_default_ip_header = NULL;

static int g_quit = FALSE;

static int TRUE_VALUE = TRUE;      // treat as const, do not change
static int FALSE_VALUE = FALSE;     // treat as const, do not change

// Error Printing
//error = socket(....);
//if (error == -1) perror("socket");


//=================================================================================================
//      PRIVATE FUNCTIONS
//=================================================================================================

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


void buildIPPacket(ip_packet_t* ip_packet, int to_vip_address, ip_protocol_t protocol, char* data, size_t data_len)
{
    memcpy(&(ip_packet->ip_header), g_default_ip_header, IP_HEADER_BYTES);
        
    ip_packet->ip_header.ip_len = htonl(IP_HEADER_BYTES+data_len);
    ip_packet->ip_header.ip_p = protocol;
    ip_packet->ip_header.ip_src = htonl(g_my_vip_address);
    ip_packet->ip_header.ip_dst = htonl(to_vip_address);
        
    ip_packet->ip_header.ip_sum = htons((uint16_t)ip_sum((char*)&(ip_packet->ip_header), IP_HEADER_BYTES));
        
    memcpy(ip_packet->ip_data, data, data_len);
}


int sendIPPacket(ip_packet_t* ip_packet)
{
    int* value_ptr = NULL;
    int* to_vip_addr_ptr = NULL;
    
    // Protect shared variable s_routing_table
    pthread_mutex_lock(&g_routing_table_mutex);
    to_vip_addr_ptr = (int*)(g_hash_table_lookup(s_routing_table, &(ip_packet->ip_header.ip_dst)));
    pthread_mutex_unlock(&g_routing_table_mutex);
    
    // Protect shared variable s_interfaces_operable
    pthread_mutex_lock(&g_interfaces_operable_mutex);
    value_ptr = (int*)(g_hash_table_lookup(s_interfaces_operable, to_vip_addr_ptr));
    pthread_mutex_unlock(&g_interfaces_operable_mutex);
    
    // Check if link to node at vip_address is up
    if ((*value_ptr) == TRUE) {
     
        // Protect shared function sendPacket()
        pthread_mutex_lock(&g_send_packet_mutex);
        sendPacket((char*)ip_packet, ip_packet->ip_header.ip_len, (*to_vip_addr_ptr));
        pthread_mutex_unlock(&g_send_packet_mutex);
        
        return SENT_IP_PACKET;
    }

    return LINK_DOWN;
}


int receiveIPPacket(ip_packet_t* ip_packet, int interface, int vip_address)
{
    size_t in_data_len = 0;
    char in_data[IP_MAX_PACKET_SIZE];
    int* value_ptr = NULL;
    
    
    // Protect shared variable s_interfaces_operable
    pthread_mutex_lock(&g_interfaces_operable_mutex);
    value_ptr = (int*)(g_hash_table_lookup(s_interfaces_operable, &vip_address));
    pthread_mutex_unlock(&g_interfaces_operable_mutex);
    
    // Check if link to node at vip_address is up
    if ((*value_ptr) == TRUE) {
    
        receivePacket(in_data, &in_data_len, IP_MAX_PACKET_SIZE, interface);
        memcpy(ip_packet, in_data, IP_HEADER_BYTES);
        
        return RECEIVED_IP_PACKET;
    }

    return LINK_DOWN;
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
        // TODO look up "vip_address" from "interface"
        //htable_put(s_interfaces_operable, vip_address, &FALSE_VALUE);
        pthread_mutex_unlock(&g_interfaces_operable_mutex);
        
    } else if (strncmp(buffer, "up", 2) == 0) {
        // Extract integer identifying interface
        sscanf(buffer, "%*s %d", &interface);
        
        //printf("Interface: %d\n", interface);
        
        // Protect shared variable s_interfaces_operable
        pthread_mutex_lock(&g_interfaces_operable_mutex);
        // TODO look up "vip_address" from "interface"
        //htable_put(s_interfaces_operable, vip_address, &TRUE_VALUE);
        pthread_mutex_unlock(&g_interfaces_operable_mutex);
        
    } else if (strncmp(buffer, "send", 4) == 0) {
        int protocol = 0;
        int vip_address = 0;
        char vip[VIP_ADDRESS_SIZE];
        ip_packet_t ip_packet;

    
        // Extract string identifying VIP Address, integer for protocol, and string for data
        sscanf(buffer, "%*s %s %d %s", vip, &protocol, data);
        
        vip_address = convertVIPString2Int(vip);
        
        printf("VIP Address: %s; %d\n", vip, vip_address);
        printf("Protocol: %d\n", protocol);
        printf("Data: %s, size: %d\n", data, (int)strlen(data));
        
        buildIPPacket(&ip_packet, vip_address, protocol, data, strlen(data));
        if (sendIPPacket(&ip_packet) == LINK_DOWN) {
            printf("Dropping packet because link is down\n");
        }
        
        
    } else if (strncmp(buffer, "quit", 4) == 0) {
        g_quit = TRUE;
    }
}


void handlePacketInput(int interface)
{
    int* vip_address_ptr = NULL;
    ip_packet_t ip_packet;
    uint16_t checksum = 0;
    
    
    vip_address_ptr = (int*)(g_hash_table_lookup(g_read_socket_addr_lookup, &interface));
    if (receiveIPPacket(&ip_packet, interface, (*vip_address_ptr)) == LINK_DOWN) {
        return; // Link is down, so do nothing
    }
    
    // Process IP Packet
    checksum = htons((uint16_t)ip_sum((char*)&(ip_packet.ip_header), IP_HEADER_BYTES));
    if (checksum != ip_packet.ip_header.ip_sum) {
        printf("Dropping packet because of bad checksum\n");
    }
    
    if (ip_packet.ip_header.ip_dst == g_my_vip_address) {
        // TODO send to correct handler
        
    } else { // Forward packet
        if (sendIPPacket(&ip_packet) == LINK_DOWN) {
            printf("Dropping packet because link is down\n");
        }
    }
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
