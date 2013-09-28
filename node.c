//================================================================================================= 
//      Author: Brett Decker and Xinwei Liu
//=================================================================================================
//      INCLUDE FILES
//=================================================================================================

// TODO add #includes needed
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netdb.h>

#include <pthread.h>

#include "util/parselinks.h"
#include "util/ipsum.h"
#include "util/list.h"

#include "ip_lib.h"

//=================================================================================================
//      DEFINITIONS AND MACROS
//=================================================================================================

#define STDIN               0
#define STDOUT              1

#define TRUE                1
#define FALSE               0

#define MIN_ARGUMENTS       2

#define UDP_FRAME_SIZE      1400
#define IP_MAX_PACKET_SIZE  64000

#define USER_MAX_CMD_SIZE   15

//=================================================================================================
//      GLOBAL VARIABLES
//=================================================================================================

// TODO 
// static (hash table) s_forwarding_table;
static pthread_mutex_t g_forwarding_table_mutex = PTHREAD_MUTEX_INITIALIZER;

static fd_set g_input_interfaces;            // All input UDP sockets and STDIN
static int g_highest_input_interface = 0;    // Highest file descriptor in g_input_interfaces

// list of all neighbors (identified by VIP address??)
static list_t* g_interfaces_list = NULL;

static int g_quit = FALSE;

// Error Printing
//error = socket(....);
//if (error == -1) perror("socket");


//=================================================================================================
//      PRIVATE FUNCTIONS
//=================================================================================================

void sendIPPacket(void) // add parameter socket or VIP address?
{
    // Send up to 64k bytes in 1400 byte increments
    
}

void receiveIPPacket(int inputSocket) // socket??
{
    // Read up to 64k bytes in 1400 byte increments
}


void setupForwardingTableAndSockets(list_t* list)
{
    // Create socket for each inferface (2 interfaces per link_t)
    // Add socket with VIP to Forwarding Table
    // Also add socket in a global file descriptor set (for select())
    
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
        
        printf("Interface: %d\n", interface);
        
        // TODO pull down interface (what exactly does this mean?)
        
    } else if (strncmp(buffer, "up", 2) == 0) {
        // Extract integer identifying interface
        sscanf(buffer, "%*s %d", &interface);
        
        printf("Interface: %d\n", interface);
        
        // TODO add interface back
        
    } else if (strncmp(buffer, "send", 4) == 0) {
        // Extract string identifying VIP Address, integer for protocol, and string for data
        sscanf(buffer, "%*s %s %d %s", vip_address, &protocol, data);
        
        vip = convertVIPStringToInt(vip_address);
        
        printf("VIP Address: %s; %d\n", vip_address, vip);
        printf("Protocol: %d\n", protocol);
        printf("Data: %s, size: %d\n", data, (int)strlen(data));
        
        // TODO
        // build IP header and send message
        
    } else if (strncmp(buffer, "quit", 4) == 0) {
        g_quit = TRUE;
    }
}


void handleForwardingAndUserInput(void)
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
                    // handle forwarding message
                    //receiveMessage();
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
    // Executable: ./node file.lnx  [2 args, second is .lnx file]
    if (argc < MIN_ARGUMENTS) {
        printf("ERROR: Not enough arguments passed.\nUsage: ./node file.lnx\n");
        return -1; // Error
    }
    
    // Clear file descriptor set g_input_interfaces
    FD_ZERO(&g_input_interfaces);
    
    // Parse .lnx file
    g_interfaces_list = parse_links(argv[1]);
    
    // Create Forwarding Table (memory allocation if needed)
    // Create all UDP connections
    // and store sockets in Forwarding Table
    setupForwardingTableAndSockets(g_interfaces_list);
    
    // Add STDIN to g_input_interfaces
    FD_SET(STDIN, &g_input_interfaces);
    
    // Routing Stuff (RIP)
    
    // spawn Interface Update Thread
    //pthread_create(&threadID, NULL, thread_function, NULL);
    
    // Handle Forwarding and User Input
    handleForwardingAndUserInput();
    
    // Free memory, destroy list of links

    return 0;
}


//=================================================================================================
//      END OF FILE
//=================================================================================================
