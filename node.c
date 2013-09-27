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

#define UDP_FRAME_SIZE      1400
#define IP_MAX_PACKET_SIZE  64000

//=================================================================================================
//      GLOBAL VARIABLES
//=================================================================================================

// TODO 
// static (hash table) s_forwardingTable;
static pthread_mutex_t g_forwardingTableMutex = PTHREAD_MUTEX_INITIALIZER;
// static fd_set g_inputInterfaces; // all input UDP sockets and STDIN
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


void handleForwardingAndUserInput(void)
{
    // Copy file descriptor set & highest fd
    
    // select(); // read ready file descriptors
    
    // for all sockets
    // if (file descriptor == STDIN) {
    //  handleUserInput();
    // } else {
    //  receiveMessage();
    // }
    
}



//=================================================================================================
//      MAIN FUNCTION
//=================================================================================================
int main(int argc, char** argv)
{
    // Executable: ./node file.lnx  [2 args, second is .lnx file]
    // check for .lnx file, print usage statement if not enough args passed
    
    // Parse .lnx file
    // list_t* list = parse_links(char *filename);
    
    // Create Forwarding Table (memory allocation if needed)
    // Create all UDP connections
    // and store sockets in Forwarding Table
    //setupForwardingTableAndSockets(list);
    
    // add STDIN to g_inputInterfaces;
    
    // Routing Stuff (RIP)
    
    // spawn Interface Update Thread
    //pthread_create(&threadID, NULL, thread_function, NULL);
    
    while (!g_quit) {
    
        // Handle Forwarding and User Input
        handleForwardingAndUserInput();
    }
    
    // Free memory, destroy list of links

    return 0;
}


//=================================================================================================
//      END OF FILE
//=================================================================================================
