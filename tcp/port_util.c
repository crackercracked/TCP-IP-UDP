//================================================================================================= 
//      Author: Brett Decker and Xinwei Liu
//=================================================================================================
//      INCLUDE FILES
//=================================================================================================

#include "port_util.h"
#include <glib.h>


//=================================================================================================
//      DEFINITIONS AND MACROS
//=================================================================================================

#define MIN_PORT    1024
#define MAX_PORT    65535


typedef struct sock_addr {

    uint32_t local_vip;
    uint16_t local_port;
    uint32_t remote_vip;
    uint16_t remote_port;

} sock_addr_t;


//=================================================================================================
//      GLOBAL VARIABLES
//=================================================================================================

static pthread_mutex_t g_allports_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool s_allports[MAX_PORT-MIN_PORT+1];

static uint16_t next_port = MIN_PORT;


static pthread_mutex_t g_addr_to_sockets_mutex = PTHREAD_MUTEX_INITIALIZER;
static GHashTable* s_addr_to_sockets = NULL;


//=================================================================================================
//      PRIVATE FUNCTIONS
//=================================================================================================

void printSocketAddrTuple(gpointer addrs, gpointer socket)
{
    int sk=*(int*)socket;
    sock_addr_t* addrs_tup_ptr=(sock_addr_t*)addrs;

    
    uint32_t lvip = addrs_tup_ptr->local_vip;
    uint16_t lport = addrs_tup_ptr->local_port;
    uint32_t rvip = addrs_tup_ptr->remote_vip;
    uint16_t rport = addrs_tup_ptr->remote_port;
    
    printf("socket %d->(lvip, lport, rvip, lport)=(%s, %u, %s, %u)\n", sk,
            util_convertVIPInt2String(lvip), lport, util_convertVIPInt2String(rvip), rport);
}




void updateNextPort()
{
    next_port++;
    if (next_port > MAX_PORT) {
        next_port = MIN_PORT;
    }
}


int getSocketForPort(sock_addr_t* sock_addr_ptr)
{
    int* vsocket_ptr = NULL;
    
    // Critical Section
    pthread_mutex_lock(&g_addr_to_sockets_mutex);
    vsocket_ptr = (int*)g_hash_table_lookup(s_addr_to_sockets, sock_addr_ptr);
    pthread_mutex_unlock(&g_addr_to_sockets_mutex);
    
    if (vsocket_ptr == NULL) {
        return -1;
    }
    
    return *vsocket_ptr;
}


void setSocketForPort(sock_addr_t* sock_addr_ptr, int vsocket)
{
    int* vsock_ptr = NULL;
    
    vsock_ptr = (int*)malloc(sizeof(int));
    *vsock_ptr = vsocket;
        
    // Critical Section
    pthread_mutex_lock(&g_addr_to_sockets_mutex);
    g_hash_table_insert(s_addr_to_sockets, sock_addr_ptr, vsock_ptr);
    pthread_mutex_unlock(&g_addr_to_sockets_mutex);
}


gboolean sock_addr_equal(gconstpointer addr1, gconstpointer addr2)
{
    sock_addr_t* s_addr1 = (sock_addr_t*)addr1;
    sock_addr_t* s_addr2 = (sock_addr_t*)addr2;
    
    if (s_addr1->local_vip != s_addr2->local_vip) {
        return FALSE;
    }
    if (s_addr1->local_port != s_addr2->local_port) {
        return FALSE;
    }
    if (s_addr1->remote_vip != s_addr2->remote_vip) {
        return FALSE;
    }
    if (s_addr1->remote_port != s_addr2->remote_port) {
        return FALSE;
    }
    
    return TRUE;
}


//=================================================================================================
//      PUBLIC FUNCTIONS
//=================================================================================================

void ptu_printAddrTup2Socket(){
     g_hash_table_foreach(s_addr_to_sockets, (GHFunc)printSocketAddrTuple, NULL);
}


void ptu_initializeTables(void)
{
    int p;
    
    for (p = MIN_PORT; p < MAX_PORT+1; p++) {
    
        s_allports[p-MIN_PORT] = true;
    }
    
    s_addr_to_sockets = g_hash_table_new(g_int64_hash, sock_addr_equal);
}


void ptu_freeTables(void)
{
    g_hash_table_destroy(s_addr_to_sockets);
}


bool ptu_getPort(uint16_t port)
{
    if (port < MIN_PORT) {
        // error code?
        return false;
    }
    
    // Critical Section
    pthread_mutex_lock(&g_allports_mutex);
    
    // Check if port is already being used
    if (s_allports[port-MIN_PORT] == false) {
        pthread_mutex_unlock(&g_allports_mutex);
        return false;
    }
    
    s_allports[port-MIN_PORT] = false;
    pthread_mutex_unlock(&g_allports_mutex);
    // End Critical Section
    
    return true;
}


uint16_t ptu_getAvailablePort(void)
{
    uint16_t new_port = 0;
    
    while (ptu_getPort(next_port) != true) {     
        updateNextPort();
    }
    
    new_port = next_port;
    updateNextPort();
    
    return new_port;
}


void ptu_releasePort(uint16_t port)
{
    if (port < MIN_PORT) {
        // error code?
        return;
    }
    
    // Critical Section
    pthread_mutex_lock(&g_allports_mutex);
    s_allports[port-MIN_PORT] = true;
    pthread_mutex_unlock(&g_allports_mutex);
}


void ptu_removeSocketAddr(uint32_t local_vip, uint16_t local_port, uint32_t remote_vip, uint16_t remote_port)
{
    sock_addr_t sock_addr;
    
    sock_addr.local_vip = local_vip;
    sock_addr.local_port = local_port;
    sock_addr.remote_vip = remote_vip;
    sock_addr.remote_port = remote_port;
    
    // Critical Section
    pthread_mutex_lock(&g_addr_to_sockets_mutex);
    g_hash_table_remove(s_addr_to_sockets, &sock_addr);
    pthread_mutex_unlock(&g_addr_to_sockets_mutex);
}


void ptu_addSocketAtAddr(uint32_t local_vip, uint16_t local_port, uint32_t remote_vip, uint16_t remote_port, int vsocket)
{
    sock_addr_t* sock_addr_ptr = NULL;
    
    sock_addr_ptr = (sock_addr_t*)malloc(sizeof(sock_addr_t));
    
    sock_addr_ptr->local_vip = local_vip;
    sock_addr_ptr->local_port = local_port;
    sock_addr_ptr->remote_vip = remote_vip;
    sock_addr_ptr->remote_port = remote_port;
        
    setSocketForPort(sock_addr_ptr, vsocket);
}


int ptu_getSocketAtAddr(uint32_t local_vip, uint16_t local_port, uint32_t remote_vip, uint16_t remote_port)
{
    sock_addr_t sock_addr;
    
    sock_addr.local_vip = local_vip;
    sock_addr.local_port = local_port;
    sock_addr.remote_vip = remote_vip;
    sock_addr.remote_port = remote_port;
    
    return getSocketForPort(&sock_addr);
}


//=================================================================================================
//      END OF FILE
//=================================================================================================
