//================================================================================================= 
//      Author: Brett Decker and Xinwei Liu
//=================================================================================================
//      INCLUDE FILES
//=================================================================================================

// TODO add #includes needed
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netdb.h>

#include <pthread.h>

#include "util/parselinks.h"
#include "util/ipsum.h"
#include "util/list.h"

//=================================================================================================
//      DEFINITIONS AND MACROS
//=================================================================================================

#define STDIN               0
#define STDOUT              1

#define MIN_ARGUMENTS       2
#define ERROR               -1

#define UDP_FRAME_SIZE      1400
#define IP_MAX_PACKET_SIZE  64000

//=================================================================================================
//      GLOBAL VARIABLES
//=================================================================================================

// TODO 
// static (hash table) s_forwardingTable;
static pthread_mutex_t g_forwardingTableMutex = PTHREAD_MUTEX_INITIALIZER;

static fd_set g_inputInterfaces;            // All input UDP sockets and STDIN
static g_highestInputInterface = 0;         // Highest file descriptor in g_inputInterfaces

// list of all neighbors (identified by VIP address??)

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
    
    

}


void handleForwardingAndUserInput(void)
{
    int interface = 0;
    int readyInterfaces = 0;
    fd_set inputInterfacesCopy;         // Stores read copy of g_inputInterfaces
    
    
    while (!g_quit) {
    
        inputInterfacesCopy = g_inputInterfaces;
        
        // Only care about read file descriptors
        readyInterfaces = select(g_highestInputInterface+1, &inputInterfacesCopy, NULL, NULL, NULL);
        if (readyInterfaces == ERROR) {
            perror("select");
            exit(-1);
        }
        
        for (interface = 0; interface < g_highestInputInterface+1; interface++) {
        
            if (FD_ISSET(interface, &inputInterfacesCopy)) {
            
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
    list_t* list;
    
    // Executable: ./node file.lnx  [2 args, second is .lnx file]
    if (argc < MIN_ARGUMENTS) {
        printf("ERROR: Not enough arguments passed.\nUsage: ./node file.lnx\n");
        return -1; // Error
    }
    
    // Clear file descriptor set g_inputInterfaces
    FD_ZERO(&g_inputInterfaces);
    
    // Parse .lnx file
    list = parse_links(argv[1]);
    
    // Create Forwarding Table (memory allocation if needed)
    // Create all UDP connections
    // and store sockets in Forwarding Table
    setupForwardingTableAndSockets(list);
    
    // Add STDIN to g_inputInterfaces
    FD_SET(STDIN, &g_inputInterfaces);
    
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
