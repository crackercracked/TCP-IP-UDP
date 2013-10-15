//================================================================================================= 
//      Author: Brett Decker and Xinwei Liu
//=================================================================================================
//      INCLUDE FILES
//=================================================================================================

#include "net_layer.h"

#include "util/colordefs.h"

#include <unistd.h>     // for usleep()

//=================================================================================================
//      DEFINITIONS AND MACROS
//=================================================================================================

typedef enum {
    DOWN = 0,
    UP = 1
} change_interface_t;


#define TOTAL_LOOP_MICRO_SECS   1000000
#define NANO_TO_MICRO_SECS      1000

#define SINGLE_HOP_DISTANCE     1

#define RIP_REQUEST_RATE        5

#define NET_PRINT(string)       printf(_GREEN_ string _NORMAL_)

#define RIP_REQUEST_SIZE        4

//=================================================================================================
//      GLOBAL VARIABLES
//=================================================================================================

// Mutex to protect s_routing_table & s_routing_distances
static pthread_mutex_t g_routing_table_mutex = PTHREAD_MUTEX_INITIALIZER;
static GHashTable* s_routing_table = NULL;
static GHashTable* s_routing_distances = NULL;

// Mutex to protect s_routing_table & s_routing_distances
static pthread_mutex_t g_routing_ttls_mutex = PTHREAD_MUTEX_INITIALIZER;
static GHashTable* s_routing_ttls = NULL;


// Mutex to protect s_interfaces_operable
static pthread_mutex_t g_interfaces_operable_mutex = PTHREAD_MUTEX_INITIALIZER;
static GHashTable* s_interfaces_operable = NULL;


// Mutex to protect s_all_local_vips
static pthread_mutex_t g_local_vips_mutex = PTHREAD_MUTEX_INITIALIZER;
static GHashTable* s_all_local_vips = NULL;


// Mutex to protect function sendPacket()
static pthread_mutex_t g_send_packet_mutex = PTHREAD_MUTEX_INITIALIZER;


static list_t* g_neighbor_keys = NULL;
static GHashTable* g_neighbors = NULL;

static list_t* g_interfaces_list = NULL;

static GHashTable* g_my_vip_from_sender = NULL;


static int TRUE_VALUE = TRUE;      // treat as const, do not change
static int FALSE_VALUE = FALSE;     // treat as const, do not change


//=================================================================================================
//      PRIVATE FUNCTION DECLARATIONS
//=================================================================================================

void buildIPPacket(ip_packet_t* ip_packet, int to_vip_address, int from_vip_address,
                   ip_protocol_t protocol, char* data, size_t data_len);

int sendIPPacket(ip_packet_t* ip_packet, bool routing_msg);
int receiveIPPacket(ip_packet_t* ip_packet, int interface, uint32_t vip_address);

void printRoute(gpointer key_vip, gpointer nxt_vip);
void decrementRouteTTL(gpointer dest_vip, gpointer ttl);
void sendRouteInfoRequest(gpointer key, gpointer neighbor_vip);

void updateInterfaces(gpointer neighbor_vip, gpointer alive);
void buildRIPResponsePacket(uint32_t* sender_vip, char* reply, size_t* reply_len);

void changeInterface(int inteface, change_interface_t change_type);

void updateRoutingTableSet(uint32_t* send_vip, uint32_t* dst_addr, uint32_t* dst_cost);

void printSendIPPacketResult(int result, bool debug);
void printRIPPacket(char* data);
void printIPPacket(ip_packet_t* ip_packet);

//=================================================================================================
//      PUBLIC THREAD FUNCTIONS
//=================================================================================================

void* net_routingThreadFunction(void* arg)
{
    int counter;
    int microseconds;
    
    struct timespec start_time, end_time;
    
    counter = 0;
    //while (g_quit == FALSE) {
    
    // Send RIP request to all neighbors
    // TODO add when we don't crash TA node
    //g_hash_table_foreach(g_neighbors, (GHFunc)sendRouteInfoRequest, NULL);
    
    while (true) {
    
        //printf("routing loop: %d\n", counter);
    
        clock_gettime(CLOCK_MONOTONIC, &start_time);
        
        if ((counter % RIP_REQUEST_RATE) == 0) {
            // Send RIP update to all neighbors
            g_hash_table_foreach(g_neighbors, (GHFunc)updateInterfaces, NULL);
        }
        
        // Decrement TTL for each destination
        // Protect shared variable s_routing_ttls
        pthread_mutex_lock(&g_routing_ttls_mutex);
        g_hash_table_foreach(s_routing_ttls, (GHFunc)decrementRouteTTL, NULL);
        pthread_mutex_unlock(&g_routing_ttls_mutex);

        counter++;
        
        // Keep counter less than 15
        // (for simplicity and no roll over)
        if (counter == (RIP_REQUEST_RATE*3)) {
            counter = 0;
        }
        
        clock_gettime(CLOCK_MONOTONIC, &end_time);
        
        //printf("st secs = %d, nsecs = %d\n", startTime.tv_sec, startTime.tv_nsec);
        //printf("et secs = %d, nsecs = %d\n", endTime.tv_sec, endTime.tv_nsec);
        
        microseconds = (end_time.tv_nsec - start_time.tv_nsec)/NANO_TO_MICRO_SECS;
        
        // Wait for 1 second from start_time
        usleep(TOTAL_LOOP_MICRO_SECS-microseconds);
    
    } // end while()

    return NULL;
}


//=================================================================================================
//      PUBLIC FUNCTIONS
//=================================================================================================

void net_sendMessage(uint32_t to_vip_address, uint8_t protocol, char* data, size_t data_len)
{
    int result = 0;
    ip_packet_t ip_packet;
    uint32_t* nxt_vip_addr_ptr = NULL;
    uint32_t* from_vip_addr_ptr = NULL;
    int* value_ptr = NULL;


    // Look up how to get to "to_vip_address" and then use the "my_vip_address"
    // according to the intermidiate node (next hop node) as the "from_vip_address"
        
    // Protect shared variable s_routing_table
    pthread_mutex_lock(&g_routing_table_mutex);
    nxt_vip_addr_ptr = (uint32_t*)(g_hash_table_lookup(s_routing_table, &to_vip_address));
    pthread_mutex_unlock(&g_routing_table_mutex);
    
    if (nxt_vip_addr_ptr == NULL) { // Next hop == NULL
        result = UNKNOWN_DESTINATION;
        
    } else { // Build and send packet
    
        from_vip_addr_ptr = (uint32_t*)(g_hash_table_lookup(g_my_vip_from_sender, nxt_vip_addr_ptr));
        
        if (from_vip_addr_ptr == NULL) {
            from_vip_addr_ptr = &to_vip_address;
        }
        
        buildIPPacket(&ip_packet, to_vip_address, (*from_vip_addr_ptr), protocol, data, strlen(data));
        
        // Check if user is sending message to a local VIP
        // Protect shared variable s_all_local_vips
        pthread_mutex_lock(&g_local_vips_mutex);
        value_ptr = g_hash_table_lookup(s_all_local_vips, &to_vip_address);
        pthread_mutex_unlock(&g_local_vips_mutex);
    
        if (value_ptr != NULL) { // Sending to a local VIP
    
            if ((*value_ptr) != FALSE_VALUE) { // Check if VIP is up
                result = SENT_IP_PACKET;
                util_callHandler(ip_packet.ip_header.ip_p, &ip_packet);
                
            } else { // Local VIP is down
                result = LINK_DOWN;
            }

        } else { // Send packet
            result = sendIPPacket(&ip_packet, false); // false (not a routing message)
        }
    }
    
    printSendIPPacketResult(result, true);
}


void net_setupTables(char* link_file)
{
    int index;
    int* key_ptr = NULL;
    int* value_ptr = NULL;
    link_t* link = NULL;
    node_t* current = NULL;
    
    
    // Parse .lnx file
    g_interfaces_list = parse_links(link_file);

    // Create tables (don't need to protect; currently only one thread)
    // ----- Routing Tables -------------------------------------------- //
    s_routing_table = g_hash_table_new(g_int_hash, g_int_equal);
    s_routing_distances = g_hash_table_new(g_int_hash, g_int_equal);
    s_routing_ttls = g_hash_table_new(g_int_hash, g_int_equal);
    // ----- Routing Tables -------------------------------------------- //
    
    // ----- Extra Tables ---------------------------------------------- //
    g_my_vip_from_sender = g_hash_table_new(g_int_hash, g_int_equal);
    s_interfaces_operable = g_hash_table_new(g_int_hash, g_int_equal);
    g_neighbors = g_hash_table_new(g_int_hash, g_int_equal);
    s_all_local_vips = g_hash_table_new(g_int_hash, g_int_equal);
    // ----- Extra Tables ---------------------------------------------- //    
    
    // Create list (keys into neighbors)
    list_init(&g_neighbor_keys);

    index = 0;    
    for (current = g_interfaces_list->head; current != NULL; current = current->next) {
        link = (current->data);
        
        // ----- Routing Tables -------------------------------------------- //
        // ----- Remote VIP ----- //
        g_hash_table_insert(s_routing_table, &(link->remote_virt_ip.s_addr), &(link->remote_virt_ip.s_addr));
        
        // Store value in list to be used by GHashTable
        value_ptr = (int*)malloc(sizeof(int));  // Need to allocate memory for GHashTable
        (*value_ptr) = INFINITY_DISTANCE;
        g_hash_table_insert(s_routing_distances, &(link->remote_virt_ip.s_addr), value_ptr);
        
        // Store value in list to be used by GHashTable
        value_ptr = (int*)malloc(sizeof(int));  // Need to allocate memory for GHashTable
        (*value_ptr) = 0;
        g_hash_table_insert(s_routing_ttls, &(link->remote_virt_ip.s_addr), value_ptr);
        
        
        // ----- Local VIP ----- //
        g_hash_table_insert(s_routing_table, &(link->local_virt_ip.s_addr), &(link->local_virt_ip.s_addr));
        
        // Store value in list to be used by GHashTable
        value_ptr = (int*)malloc(sizeof(int));  // Need to allocate memory for GHashTable
        (*value_ptr) = 0;
        g_hash_table_insert(s_routing_distances, &(link->local_virt_ip.s_addr), value_ptr);
        
        value_ptr = (int*)malloc(sizeof(int));  // Need to allocate memory for GHashTable
        (*value_ptr) = DEFAULT_ROUTE_TTL;
        g_hash_table_insert(s_routing_ttls, &(link->local_virt_ip.s_addr), value_ptr);
        // ----- Routing Tables -------------------------------------------- //
        
        
        // ----- Extra Tables ---------------------------------------------- //
        // Create key and store in list
        key_ptr = (int*)malloc(sizeof(int));
        (*key_ptr) = index;
        list_append(g_neighbor_keys, key_ptr);
        
        g_hash_table_insert(s_interfaces_operable, &(link->remote_virt_ip.s_addr), &TRUE_VALUE);
        g_hash_table_insert(s_all_local_vips, &(link->local_virt_ip.s_addr), &TRUE_VALUE);
        
        g_hash_table_insert(g_neighbors, key_ptr, &(link->remote_virt_ip.s_addr));
        g_hash_table_insert(g_my_vip_from_sender, &(link->remote_virt_ip.s_addr), &(link->local_virt_ip.s_addr));
        // ----- Extra Tables ---------------------------------------------- //
        
        index++;
    } // end for()
    
    
    // Create all UDP connections and store sockets in Forwarding Table
    link_setupForwardingTableAndSockets(g_interfaces_list);
}


void net_freeTables(void)
{
    // TODO
    // s_interfaces_operable
    // g_neighbors
    
    list_free(&g_interfaces_list);
}


// Return value signifies message was printed
bool net_handlePacketInput(int interface)
{
    char* vip_address_ptr = NULL;
    uint32_t dest_vip_addr = 0;
    int* value_ptr = NULL;
    int from_vip_address;
    ip_packet_t ip_packet;
    uint16_t checksum = 0;
    
    
    // NOTE: g_read_socket_addr_lookup returns a string
    vip_address_ptr = (char*)(g_hash_table_lookup(g_read_socket_addr_lookup, &interface));
    from_vip_address = util_convertVIPString2Int(vip_address_ptr);
    if (receiveIPPacket(&ip_packet, interface, from_vip_address) == LINK_DOWN) {
        return false; // Link is down
    }
    
    // Process IP Packet
    
    // Need to switch checksum to host byte order, then if checksum
    // is recomputed, the result is zero if there were no errors.
    //ip_packet.ip_header.ip_sum = ntohs(ip_packet.ip_header.ip_sum);
        
    checksum = (uint16_t)(ip_sum((char*)&(ip_packet.ip_header), IP_HEADER_BYTES));
    
    // Switch back checksum to keep packet as received.
    //ip_packet.ip_header.ip_sum = ntohs(ip_packet.ip_header.ip_sum);
    
    if (checksum != 0) {
        NET_PRINT("\nDropping packet because of bad checksum\n");
        printf("Expected: 0;\tComputed: 0x%X\n", checksum);
        printIPPacket(&ip_packet);
        return true; // Packet dropped
    }
    
    //printf("dest before: %d, %s\n", ip_packet.ip_header.ip_dst, util_convertVIPInt2String(ip_packet.ip_header.ip_dst));
    dest_vip_addr = ntohl(ip_packet.ip_header.ip_dst);
    //printf("dest after: %d, %s\n", dest_vip_addr, util_convertVIPInt2String(dest_vip_addr));
    
    // Protect shared variable s_all_local_vips
    pthread_mutex_lock(&g_local_vips_mutex);
    value_ptr = g_hash_table_lookup(s_all_local_vips, &dest_vip_addr);
    pthread_mutex_unlock(&g_local_vips_mutex);
    
    if (value_ptr != NULL) {
        util_callHandler(ip_packet.ip_header.ip_p, &ip_packet);
        
        return false; // Packet at destination
    }
    
    // Check if packet has expired
    if ((int)(ip_packet.ip_header.ip_ttl) <= 0) {
        NET_PRINT("\nDropping packet because TTL has expired\n");
        return true; // Packet dropped
    }
    
    // Decrement TTL
    ip_packet.ip_header.ip_ttl--;
    
    // Recompute checksum, but first set checksum field to 0
    ip_packet.ip_header.ip_sum = 0;
    
    ip_packet.ip_header.ip_sum = (uint16_t)ip_sum((char*)&(ip_packet.ip_header), IP_HEADER_BYTES);
    
    // Forward packet
    NET_PRINT("\nForwarding packet: ");
    
    printSendIPPacketResult(sendIPPacket(&ip_packet, false), true);
    
    return true;
}


void net_printNetworkInterfaces(void)
{
    int interface;
    uint32_t* vip_addr_ptr = NULL;
    int* value_ptr = NULL;
    uint32_t* my_vip_ptr = NULL;
    size_t size;
    
    size = g_hash_table_size(g_neighbors);
    
    printf("\n---Interfaces---\n");
    
    for (interface = 0; interface < size; interface++) {

        vip_addr_ptr = g_hash_table_lookup(g_neighbors, &interface);
        
        if (vip_addr_ptr == NULL) {
            printf("null pointer - line 152\n");
            exit(-1);
        }
        
        // Lookup the node's VIP according to the sender
        my_vip_ptr = g_hash_table_lookup(g_my_vip_from_sender, vip_addr_ptr);
        
        printf("%d - VIP %d.%d.%d.%d -> ", interface, MASK_BYTE1((*my_vip_ptr)), \
            MASK_BYTE2((*my_vip_ptr)), MASK_BYTE3((*my_vip_ptr)), MASK_BYTE4((*my_vip_ptr)));
        
        printf("VIP %d.%d.%d.%d ", MASK_BYTE1((*vip_addr_ptr)), MASK_BYTE2((*vip_addr_ptr)), \
            MASK_BYTE3((*vip_addr_ptr)), MASK_BYTE4((*vip_addr_ptr)));
            
        // Protect shared variable s_interfaces_operable
        pthread_mutex_lock(&g_interfaces_operable_mutex);
        value_ptr = g_hash_table_lookup(s_interfaces_operable, vip_addr_ptr);
        pthread_mutex_unlock(&g_interfaces_operable_mutex);
        
        // Print out if interface is up or down
        if ((*value_ptr) == TRUE) {
            printf("(up)\n");
        } else {
            printf("(down)\n");
        }
        
    } // end for()
    
    printf("---End Interfaces---\n\n");
}


void net_printNetworkRoutes(void)
{
    printf("\n---Routes---\n");
    
    // Protect shared variable s_routing_table
    pthread_mutex_lock(&g_routing_table_mutex);
    g_hash_table_foreach(s_routing_table, (GHFunc)printRoute, NULL);
    pthread_mutex_unlock(&g_routing_table_mutex);
      
    printf("---End Routes---\n\n");
}


void net_downInterface(int interface)
{
    changeInterface(interface, DOWN);
}


void net_upInterface(int interface)
{
    changeInterface(interface, UP);
}


void net_handleTestPacket(ip_packet_t* ip_packet)
{
    printIPPacket(ip_packet);
    
    // To make things look pretty
    printf("> ");
    fflush(stdout);
}


void net_handleRIPPacket(ip_packet_t* ip_packet)
{
    //read command, if command=1, build IP packet from routing tables set 
    //if command=2, update routing tables set
    ip_header_t header=ip_packet->ip_header;
    char* data=ip_packet->ip_data;
   
    uint32_t* sender_vip=(uint32_t*)malloc(sizeof(uint32_t));
   
    *sender_vip=(uint32_t)ntohl(header.ip_src);
   
    uint16_t command=util_getU16((unsigned char*)data);
    data=data+2;
   
  
    if (command == RIP_REQUEST_COMMAND) {//send data;
     
        //build ip packet
        int dst_vip=*(int*)sender_vip;
        int src_vip=(int)ntohl(header.ip_dst);
        
        ip_packet_t ip_packet_to_send;
        char reply[IP_MAX_PACKET_SIZE];
        size_t reply_len;
        
        buildRIPResponsePacket(sender_vip, reply, &reply_len);
        buildIPPacket(&ip_packet_to_send, dst_vip, src_vip, RIP_PROTOCOL, reply, reply_len);
        printSendIPPacketResult(sendIPPacket(&ip_packet_to_send, true), false);
 
    }
    else if (command == RIP_RESPONSE_COMMAND) {
        
        //uint16_t num_entries = ntohs(util_getU16((unsigned char*)data));
        uint16_t num_entries = util_getU16((unsigned char*)data);
        data=data+2;
        
        //printf("num_entries: %d\n", num_entries);
       
        uint16_t num_entries_read=0;
       
        while(num_entries_read<num_entries){
       
            uint32_t* cost=(uint32_t*)malloc(sizeof(uint32_t));
            //*cost=ntohl(util_getU32((unsigned char*)data));
            *cost = util_getU32((unsigned char*)data);
            data=data+4;
            
            uint32_t* addr=(uint32_t*)malloc(sizeof(uint32_t));
            //*addr=ntohl(util_getU32((unsigned char*)data));
            *addr = (util_getU32((unsigned char*)data));
            data=data+4;
            
            //printf("address: %d, %s\n", *addr, util_convertVIPInt2String(*addr));
            
            updateRoutingTableSet(sender_vip, addr, cost);
            num_entries_read++;
           
        } // end while()
    
    } // end else if()
    
}


//=================================================================================================
//      PRIVATE FUNCTIONS
//=================================================================================================

void buildIPPacket(ip_packet_t* ip_packet, int to_vip_address, int from_vip_address,
                   ip_protocol_t protocol, char* data, size_t data_len)
{
    memset((char*)ip_packet, 0, sizeof((*ip_packet)));

    ip_packet->ip_header.ip_v = IPv4;
    ip_packet->ip_header.ip_hl = IP_HEADER_WORDS;
    //ip_packet->ip_header.ip_tos = 0;
    ip_packet->ip_header.ip_len = htons(IP_HEADER_BYTES+data_len);
    //ip_packet->ip_header.ip_id = 0;
    ip_packet->ip_header.ip_off = IP_DF; // defined in utility.h
    ip_packet->ip_header.ip_ttl = IP_MAX_TTL;
    ip_packet->ip_header.ip_p = protocol;
    //ip_packet->ip_header.ip_sum = 0;
    ip_packet->ip_header.ip_src = htonl(from_vip_address);
    ip_packet->ip_header.ip_dst = htonl(to_vip_address);
    
    ip_packet->ip_header.ip_sum = (uint16_t)ip_sum((char*)&(ip_packet->ip_header), IP_HEADER_BYTES);
        
    memcpy(ip_packet->ip_data, data, data_len);
}


int sendIPPacket(ip_packet_t* ip_packet, bool routing_msg)
{
    int key;
    int* dist_value_ptr = NULL;
    int* link_value_ptr = NULL;
    uint32_t* to_vip_addr_ptr = NULL;

    
    // Protect shared variable s_routing_table
    pthread_mutex_lock(&g_routing_table_mutex);
    key = ntohl(ip_packet->ip_header.ip_dst);
    to_vip_addr_ptr = (uint32_t*)(g_hash_table_lookup(s_routing_table, &key));
    pthread_mutex_unlock(&g_routing_table_mutex);
    
    // Check if destination is not in routing table
    if (to_vip_addr_ptr == NULL) {
        return UNKNOWN_DESTINATION;
    }
    
    // Protect shared variable s_interfaces_operable
    pthread_mutex_lock(&g_interfaces_operable_mutex);
    link_value_ptr = (int*)(g_hash_table_lookup(s_interfaces_operable, to_vip_addr_ptr));
    pthread_mutex_unlock(&g_interfaces_operable_mutex);
    
    // Check if link to node at vip_address is up
    if ((*link_value_ptr) == TRUE) {
    
        // Protect shared variable s_interfaces_operable
        pthread_mutex_lock(&g_routing_table_mutex);
        dist_value_ptr = (int*)(g_hash_table_lookup(s_routing_distances, &key));
        pthread_mutex_unlock(&g_routing_table_mutex);
        
        if (((*dist_value_ptr) != INFINITY_DISTANCE) || routing_msg) {
         
            // Protect shared function sendPacket()
            pthread_mutex_lock(&g_send_packet_mutex);        
            link_sendPacket((char*)ip_packet, ntohs(ip_packet->ip_header.ip_len), (*to_vip_addr_ptr));
            pthread_mutex_unlock(&g_send_packet_mutex);
            
            return SENT_IP_PACKET;
            
        } // end if (distance != infinity)
        
        return INFINITY_DISTANCE;
        
    } // end if (link up)

    return LINK_DOWN;
}


int receiveIPPacket(ip_packet_t* ip_packet, int interface, uint32_t vip_address)
{
    size_t in_data_len = 0;
    size_t total_data_len = 0;
    char in_data[IP_MAX_PACKET_SIZE];
    int* value_ptr = NULL;

    // Protect shared variable s_interfaces_operable
    pthread_mutex_lock(&g_interfaces_operable_mutex);
    value_ptr = (int*)(g_hash_table_lookup(s_interfaces_operable, &vip_address));
    pthread_mutex_unlock(&g_interfaces_operable_mutex);
    
    if (value_ptr == NULL) {
        NET_PRINT("NULL pointer!!\n");
    }

    // Receive data
    do {
        memset(in_data, 0, IP_MAX_PACKET_SIZE);
        link_receivePacket(in_data, &in_data_len, interface);

        // If no data is returned, then error occurred
        if (in_data_len == 0) {
            NET_PRINT("UNHANDLED ERROR!\n");
            return LINK_DOWN;
        }
        
        memcpy(((char*)ip_packet)+total_data_len, in_data, in_data_len);
        total_data_len += in_data_len;
        
    } while (ntohs(ip_packet->ip_header.ip_len) > total_data_len);
    
    if ((*value_ptr) == FALSE) { // link is down
        return LINK_DOWN;
    }
    
    return RECEIVED_IP_PACKET;
}


void decrementRouteTTL(gpointer dest_vip, gpointer ttl)
{
    int* value_ptr = NULL;
    
    // Protect shared variable s_all_local_vips
    pthread_mutex_lock(&g_local_vips_mutex);
    value_ptr = g_hash_table_lookup(s_all_local_vips, dest_vip);
    pthread_mutex_unlock(&g_local_vips_mutex);
    
    if (value_ptr != NULL) {
        return; // Do nothing, local vip
    }
    
    if ((*(int*)(ttl)) == 0) {
        return; // Don't decrement when already zero
    }
    
    (*(int*)(ttl)) = (*(int*)(ttl)) - 1;
    g_hash_table_replace(s_routing_ttls, dest_vip, ttl);
    
    if ((*(int*)(ttl)) == 0) {
        value_ptr = g_hash_table_lookup(s_routing_distances, dest_vip);
        (*value_ptr) = INFINITY_DISTANCE;
        
        g_hash_table_replace(s_routing_distances, dest_vip, value_ptr);
    }
}


// WARNING: lock to s_routing_table must already be acquired
void printRoute(gpointer key_vip, gpointer nxt_vip)
{
    int* distance = NULL;
    //int* ttl = NULL;
    
    char* key_vip_str = util_convertVIPInt2String((*((uint32_t*)key_vip)));
    char* nxt_vip_str = util_convertVIPInt2String((*((uint32_t*)nxt_vip)));
    
    distance = g_hash_table_lookup(s_routing_distances, key_vip);
    //ttl = g_hash_table_lookup(s_routing_ttls, key_vip);
    
    //printf("    VIP %s through VIP %s (distance %d; ttl %d)\n", key_vip_str, nxt_vip_str, (*distance), (*ttl));
    printf("    VIP %s through VIP %s (distance %d)\n", key_vip_str, nxt_vip_str, (*distance));
}


void sendRouteInfoRequest(gpointer key, gpointer neighbor_vip)
{
    ip_packet_t ip_packet;
    uint32_t* from_vip_ptr = NULL;    
    
    char data[RIP_REQUEST_SIZE] = { 0, RIP_REQUEST_COMMAND, 0, 0 };
    
    from_vip_ptr = g_hash_table_lookup(g_my_vip_from_sender, neighbor_vip);
    
    buildIPPacket(&ip_packet, (*((uint32_t*)(neighbor_vip))), (*from_vip_ptr), RIP_PROTOCOL, data, RIP_REQUEST_SIZE);
    sendIPPacket(&ip_packet, true);
}


//go through all neighbor, if link to neigbor alive, update
void updateInterfaces(gpointer key, gpointer neighbor_vip)
{
    char reply[IP_MAX_PACKET_SIZE];
    size_t reply_len;
    int* value_ptr;
    
    pthread_mutex_lock(&g_interfaces_operable_mutex);
    value_ptr = g_hash_table_lookup(s_interfaces_operable, neighbor_vip);
    pthread_mutex_unlock(&g_interfaces_operable_mutex);
    
    if ((*value_ptr) == TRUE_VALUE) {
        
        ip_packet_t ip_packet;
        uint32_t* dst_vip_ptr=(uint32_t*)neighbor_vip; 
        
        buildRIPResponsePacket(dst_vip_ptr, reply, &reply_len);

        uint32_t* src_vip_ptr = g_hash_table_lookup(g_my_vip_from_sender, dst_vip_ptr);

        buildIPPacket(&ip_packet, *dst_vip_ptr, *src_vip_ptr, RIP_PROTOCOL, reply, reply_len);
        sendIPPacket(&ip_packet, true);
    }
}


void buildRIPResponsePacket(uint32_t* sender_vip, char* reply, size_t* reply_len)
{
    // Protect shared variable s_routing_table
    pthread_mutex_lock(&g_routing_table_mutex);
    uint16_t nums_dst=(g_hash_table_size(s_routing_distances));
    pthread_mutex_unlock(&g_routing_table_mutex);
      
    int memory_start=0;
      
    // Add command to reply
    char command[2] = { 0, RIP_RESPONSE_COMMAND };
    memcpy(reply, command, 2);
    memory_start += 2;
      
    // Add number of entries
    nums_dst = htons(nums_dst);
    memcpy(reply+memory_start, &nums_dst, sizeof(uint16_t));
    memory_start += 2;
      
    // Protect shared variable s_routing_table
    pthread_mutex_lock(&g_routing_table_mutex);
    GList* all_dsts=g_hash_table_get_keys(s_routing_distances);
    pthread_mutex_unlock(&g_routing_table_mutex);
      
        GList* cur_dst_ptr;
        for(cur_dst_ptr=all_dsts; cur_dst_ptr!=NULL; cur_dst_ptr=cur_dst_ptr->next){
            uint32_t* cur_dst=cur_dst_ptr->data;
          
            // Protect shared variable s_routing_table
            pthread_mutex_lock(&g_routing_table_mutex);
            uint32_t* cur_cost = g_hash_table_lookup(s_routing_distances, (gpointer)cur_dst);
            pthread_mutex_unlock(&g_routing_table_mutex);
          
            // Copy values of pointers because we don't want to change the original pointers!
            uint32_t* send_cost = (uint32_t*)malloc(sizeof(uint32_t));
            *send_cost = *cur_cost;
            
            uint32_t* send_dest = (uint32_t*)malloc(sizeof(uint32_t));
            *send_dest = *cur_dst;
          
            // Case: send information about self
            // Skip split horizon with poison reverse
            if (*send_cost == 0) { // distance to self
                // Do nothing
            } else {
                // Do split horizon with poison reverse
              
                // Protect shared variable s_routing_table
                pthread_mutex_lock(&g_routing_table_mutex);
                uint32_t* cur_nexthop=(g_hash_table_lookup(s_routing_table, (gpointer)send_dest));//originally nothing
                pthread_mutex_unlock(&g_routing_table_mutex);
              
                if (*cur_nexthop == *sender_vip) {//next hop is not the vip who send request
                    *send_cost = INFINITY_DISTANCE;
                }
            }          
          
            // Convert to network byte order
            *send_cost = htonl((*send_cost));
            *send_dest = htonl((*send_dest));
            
            //printf("send_cost = %d\n", *send_cost);
            //printf("send_dest = %d, %s\n", *send_dest, util_convertVIPInt2String(*send_dest));
            
            memcpy(reply+memory_start, send_cost, sizeof(uint32_t));
            memory_start+=4;
            memcpy(reply+memory_start, send_dest, sizeof(uint32_t));
            memory_start+=4;
        }
        (*reply_len) = memory_start;
}


void changeInterface(int interface, change_interface_t change_type)
{
    // Check for proper input
    if (interface >= g_hash_table_size(g_neighbors)) {
        return; // Bad input
    }    
    
    uint32_t* vip_addr_ptr = NULL;
    uint32_t* local_vip_addr_ptr = NULL;
    int* value_ptr = NULL;
    
    // Set value_ptr according to parameter change_type
    if (change_type == DOWN) {
        value_ptr = &FALSE_VALUE;
    } else if (change_type == UP) {
        value_ptr = &TRUE_VALUE;
    }
    
    vip_addr_ptr = (uint32_t*)g_hash_table_lookup(g_neighbors, &interface);
        
    // Protect shared variable s_interfaces_operable
    pthread_mutex_lock(&g_interfaces_operable_mutex);
    g_hash_table_insert(s_interfaces_operable, vip_addr_ptr, value_ptr);
    pthread_mutex_unlock(&g_interfaces_operable_mutex);
    
    // Get local_vip_addr_ptr
    local_vip_addr_ptr = g_hash_table_lookup(g_my_vip_from_sender, vip_addr_ptr);
    
    // Protect shared variable s_all_local_vips
    pthread_mutex_lock(&g_local_vips_mutex);
    g_hash_table_replace(s_all_local_vips, local_vip_addr_ptr, value_ptr);
    pthread_mutex_unlock(&g_local_vips_mutex);
    
    // Protect shared variable s_routing_distances
    pthread_mutex_lock(&g_routing_table_mutex);
    value_ptr = g_hash_table_lookup(s_routing_distances, local_vip_addr_ptr);
    
    // Set value_ptr according to parameter change_type
    if (change_type == DOWN) {
        *value_ptr = INFINITY_DISTANCE;
    } else if (change_type == UP) {
        *value_ptr = 0;
    }

    g_hash_table_replace(s_routing_distances, local_vip_addr_ptr, value_ptr);
    pthread_mutex_unlock(&g_routing_table_mutex);
}


void updateRoutingTableSet(uint32_t* send_vip, uint32_t* dst_addr, uint32_t* dst_cost)
{
    // ---- Critical Section ---------------------------------------------------------- //
    
    // Protect shared variable s_routing_table
    pthread_mutex_lock(&g_routing_table_mutex);
    
    // Set TTL
    int* time_left=(int*)malloc(sizeof(int));
    *time_left=DEFAULT_ROUTE_TTL;    
    
    // Don't change infinty distance
    if (*dst_cost!=INFINITY_DISTANCE) {
        *dst_cost=(*dst_cost)+SINGLE_HOP_DISTANCE;
    }
    
    if (!g_hash_table_contains(s_routing_table, (gpointer)dst_addr)){
   
        //if dst_addr never show up before
        g_hash_table_insert(s_routing_table, (gpointer)dst_addr, (gpointer)send_vip);
        g_hash_table_insert(s_routing_distances, (gpointer)dst_addr, (gpointer)dst_cost);
        
        // Protect shared variable s_routing_table
        pthread_mutex_lock(&g_routing_ttls_mutex);
        g_hash_table_insert(s_routing_ttls, (gpointer)dst_addr, (gpointer)time_left);
        pthread_mutex_unlock(&g_routing_ttls_mutex);
    }
    else {
        //already contain the routing table set
        int* current_dst=g_hash_table_lookup(s_routing_distances, (gpointer)dst_addr);
        
        //need to update next-hop
        if (*current_dst > *dst_cost) {
       
            g_hash_table_replace(s_routing_table, (gpointer)dst_addr, (gpointer)send_vip);
            g_hash_table_replace(s_routing_distances, (gpointer)dst_addr, (gpointer)dst_cost);
            
            // Protect shared variable s_routing_table
            pthread_mutex_lock(&g_routing_ttls_mutex);
            g_hash_table_insert(s_routing_ttls, (gpointer)dst_addr, (gpointer)time_left);
            pthread_mutex_unlock(&g_routing_ttls_mutex);
            
        } else if (*current_dst == *dst_cost) {
            // Protect shared variable s_routing_table
            pthread_mutex_lock(&g_routing_ttls_mutex);
            g_hash_table_insert(s_routing_ttls, (gpointer)dst_addr, (gpointer)time_left);
            pthread_mutex_unlock(&g_routing_ttls_mutex);
        }
        
        // Check if route was poisoned
        if (*dst_cost == INFINITY_DISTANCE) {
            uint32_t* send_thr_vip_ptr = (uint32_t*)g_hash_table_lookup(s_routing_table, (gpointer)dst_addr);
            if (*send_thr_vip_ptr == (*send_vip)) {
                g_hash_table_replace(s_routing_distances, (gpointer)dst_addr, (gpointer)dst_cost);
            }
        }
       
    } // end else
   
    pthread_mutex_unlock(&g_routing_table_mutex);
   
    // ---- Critical Section ---------------------------------------------------------- //
}


void printSendIPPacketResult(int result, bool debug)
{
    if (result == LINK_DOWN) {
        NET_PRINT("\nPacket dropped because link is down.\n");
    } else if (result == UNKNOWN_DESTINATION) {
        NET_PRINT("\nPacket dropped because destination is unknown.\n");
    } else if (result == INFINITY_DISTANCE) {
        NET_PRINT("\nPacket dropped because route to destination is unknown.\n");
    } else if ((result == SENT_IP_PACKET) && (debug == true)) {
        NET_PRINT("\nPacket sent successfully.\n");
    }
}


void printRIPPacket(char* data)
{
    int read = 0;
    int entries = 0;
    int i;

    printf("\n--RIP Packet--\n");
    printf("Command: %d\n", util_getU16((unsigned char*)data));
    read += 2;
    
    entries = ntohs(util_getU16((unsigned char*)(data+read)));
    printf("Entries: %d\n", entries);
    read += 2;

    for (i = 0; i < entries; i++) {
    
        printf("Cost: %d\n", ntohl(util_getU32((unsigned char*)(data+read))));
        read += 4;
        
        printf("Dest: %d\n", ntohl(util_getU32((unsigned char*)(data+read))));
        read += 4;
    }
    
    printf("--End RIP Packet--\n\n");
}


void printIPPacket(ip_packet_t* ip_packet)
{
    int i;
    uint32_t value = 0;
    int data_len = 0;
    
    printf("\n--IP Packet--\n");
    
    printf("Version: 0x%X\n", ip_packet->ip_header.ip_v);
    printf("Header Length: %d\n", ip_packet->ip_header.ip_hl);
    printf("Type of Service: 0x%X\n", ip_packet->ip_header.ip_tos);
    printf("Total Length: %d\n", ntohs(ip_packet->ip_header.ip_len));
    printf("ID: 0x%X\n", ip_packet->ip_header.ip_id);
    printf("Offset: 0x%X\n", ip_packet->ip_header.ip_off);
    printf("TTL: %d\n", ip_packet->ip_header.ip_ttl);
    printf("Protocol: %d\n", ip_packet->ip_header.ip_p);
    printf("Checksum : 0x%X\n", ip_packet->ip_header.ip_sum);
    
    value = ntohl(ip_packet->ip_header.ip_src);
    printf("Source: %d.%d.%d.%d\n", MASK_BYTE1(value), MASK_BYTE2(value), MASK_BYTE3(value), MASK_BYTE4(value));
    value = ntohl(ip_packet->ip_header.ip_dst);
    printf("Destination: %d.%d.%d.%d\n", MASK_BYTE1(value), MASK_BYTE2(value), MASK_BYTE3(value), MASK_BYTE4(value));
    
    data_len = ntohs(ip_packet->ip_header.ip_len) - (4*ip_packet->ip_header.ip_hl);
    printf("Data: ");
    for (i = 0; i < data_len; i++) {
        printf("%c", ip_packet->ip_data[i]);
    }
    
    printf("\n--End IP Packet--\n\n");
}


//=================================================================================================
//      END OF FILE
//=================================================================================================
