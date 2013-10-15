//================================================================================================= 
//      Author: Brett Decker and Xinwei Liu
//=================================================================================================
//      INCLUDE FILES
//=================================================================================================

#include "utility.h"

#include "net_layer.h"

#include <glib.h>

//=================================================================================================
//      DEFINITIONS AND MACROS
//=================================================================================================

#define MIN_ARGUMENTS           2

#define USER_MAX_CMD_SIZE       15

//=================================================================================================
//      GLOBAL VARIABLES
//=================================================================================================

static int g_quit = FALSE;

//=================================================================================================
//      PRIVATE FUNCTIONS
//=================================================================================================

void handleUserInput(void)
{
    // Valid commands:
    // - interfaces
    // - routes
    // - down (integer)
    // - up (integer)
    // - send (VIP) (protocol) (data)
    // - quit
    
    char command[USER_MAX_CMD_SIZE];
    char buffer[IP_MAX_PACKET_SIZE+100]; // Needs to be >64k because "data" in "send" command could be that big
    char data[IP_MAX_PACKET_SIZE];
    
    memset(command, 0, USER_MAX_CMD_SIZE);
    memset(buffer, 0, IP_MAX_PACKET_SIZE+100);
    memset(data, 0, IP_MAX_PACKET_SIZE);
    
    fgets(buffer, IP_MAX_PACKET_SIZE+100-1, stdin);
    
    // ---- Command: "interfaces" ---- //
    if (strncmp(buffer, "interfaces", 10) == 0) {
        // Print interfaces
        net_printNetworkInterfaces();
    
    // ---- Command: "routes" ---- //    
    } else if (strncmp(buffer, "routes", 6) == 0) {
        // Print routes
        net_printNetworkRoutes();
    
    // ---- Command: "down (integer)" ---- //    
    } else if (strncmp(buffer, "down", 4) == 0) {
        int neighbor = 0;
               
        // Extract integer identifying neighbor
        sscanf(buffer, "%*s %d", &neighbor);
        
        net_downInterface(neighbor);
    
    // ---- Command: "up (integer)" ---- //    
    } else if (strncmp(buffer, "up", 2) == 0) {
        int neighbor = 0; 
    
        // Extract integer identifying neighbor
        sscanf(buffer, "%*s %d", &neighbor);
        
        net_upInterface(neighbor);
    
    // ---- Command: "send (vip) (protocol) (data)" ---- //    
    } else if (strncmp(buffer, "send", 4) == 0) {
        int protocol = 0;
        char vip[VIP_ADDRESS_SIZE];
 
        // Extract string identifying VIP Address, integer for protocol, and string for data
        sscanf(buffer, "%*s %s %d %s", vip, &protocol, data);

        net_sendMessage(util_convertVIPString2Int(vip), protocol, data, strlen(data));
    
    // ---- Command: "quit" ---- //    
    } else if (strncmp(buffer, "quit", 4) == 0) {
        g_quit = TRUE;
    }
    
    // Otherwise do nothing
}


//=================================================================================================
//      MAIN FUNCTION
//=================================================================================================
int main(int argc, char** argv)
{
    pthread_t threadID;
    
    // Executable: ./node file.lnx  [2 args, second is .lnx file]
    if (argc < MIN_ARGUMENTS) {
        printf("ERROR: Not enough arguments passed.\nUsage: ./node file.lnx\n");
        return -1; // Error
    }
    
    // Clear file descriptor set g_input_interfaces
    FD_ZERO(&g_input_interfaces);
    
    util_netRegisterHandler(TEST_PROTOCOL, net_handleTestPacket);
    util_netRegisterHandler(RIP_PROTOCOL, net_handleRIPPacket);
    
    net_setupTables(argv[1]);
    
    // Add STDIN to g_input_interfaces
    FD_SET(STDIN, &g_input_interfaces);
    
    // spawn Routing Update Thread
    pthread_create(&threadID, NULL, net_routingThreadFunction, NULL);
    
    // Handle Packet Forwarding and User Input
    int interface = 0;
    int ready_interfaces = 0;
    fd_set input_interfaces_copy;         // Stores read copy of g_input_interfaces

    printf("> ");
    fflush(stdout);
               
    while (!g_quit) {
    
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
                    
                    if (!g_quit) { // Don't print input prompt if quitting
                        printf("> ");
                        fflush(stdout);
                    }
                    
                } else {
                    if (net_handlePacketInput(interface) == true) {
                        printf("> ");
                        fflush(stdout);
                    }
                }
            } // end if FD_ISSET()
        
        } // end for
    
    } // end while
    
    // Free memory, destroy list of links
    net_freeTables();

    return 0;
}


//=================================================================================================
//      END OF FILE
//=================================================================================================
