//================================================================================================= 
//      Author: Brett Decker and Xinwei Liu
//=================================================================================================
//      INCLUDE FILES
//=================================================================================================

#include "link_layer.h"
#include "ip_lib.h"


//=================================================================================================
//      DEFINITIONS AND MACROS
//=================================================================================================

#define UDP_FRAME_SIZE      1400

//=================================================================================================
//      GLOBAL VARIABLES
//=================================================================================================

static GHashTable* s_forwarding_table;//key is vip, value is socket
static GHashTable* g_send_socket_addr_lookup;//send approach 2, not using bind, key is socket, value is real ip 
static GHashTable* g_read_socket_addr_lookup;//key is socket, valie is vip
static pthread_mutex_t g_forwarding_table_mutex = PTHREAD_MUTEX_INITIALIZER; // do we need this?


char* convertVIPInt2String(int addr_num)
{
    char* buffer=(char*)malloc(50);
    sprintf(buffer, "%d.%d.%d.%d", MASK_BYTE1(addr_num), MASK_BYTE2(addr_num), MASK_BYTE3(addr_num), MASK_BYTE4(addr_num));
    return buffer;
}




void printSocketVIPPair(gpointer socket_p, gpointer addr_p)
{//for print read_look_up table
    int socket=*(int*)socket_p;
    char* vip_str=addr_p;
    printf("socket=%d, vip=%s\n", socket, vip_str);

}




void printVIPSocketPair(gpointer addr_p, gpointer socket_p)
{//for print forwarding table
     char* vip_str=addr_p;
     int socket=*(int*)socket_p;
     printf("vip=%s socket=%d\n", vip_str, socket);
}




void printForwardingTable()
{
    printf("---udp forwarding table(vip, send socket)---"); 
    printf("\n"); 
    g_hash_table_foreach(s_forwarding_table, (GHFunc)printVIPSocketPair, NULL);
}


void printReadFromWhichVIPTable()
{
    printf("---udp checking from which vip is reading from(read socket, vip)---"); 
    printf("\n"); 
    g_hash_table_foreach(g_read_socket_addr_lookup, (GHFunc)printSocketVIPPair, NULL);

}



void sendPacket(void) // TODO determine parameters
{
    // Send up to 64k bytes in 1400 byte increments
    
}




void receivePacket(void) // TODO determine parameters
{
    // Read up to 64k bytes in 1400 byte increments
}






void setupForwardingTableAndSockets(list_t* list)
{
    node_t* current_node;
    link_t* current_link;
    s_forwarding_table=g_hash_table_new(g_str_hash, g_str_equal);
    g_read_socket_addr_lookup=g_hash_table_new(g_int_hash, g_int_equal);
    g_send_socket_addr_lookup=g_hash_table_new(g_int_hash, g_int_equal);

    for(current_node=list->head; current_node!=NULL; current_node=current_node->next){
        current_link=(link_t*)(current_node->data);
        char* local_dns=current_link->local_phys_host;
        uint16_t local_port=current_link->local_phys_port;
        struct sockaddr_in local_addr;
        size_t local_addrlen;
        if(!dnsLookUp(local_dns, local_port, &local_addr, &local_addrlen)){
           char vip_str[INET_ADDRSTRLEN];
           inet_ntop(AF_INET, &(current_link->local_virt_ip), vip_str, INET_ADDRSTRLEN);
           fprintf(stderr, "cannot find physical ip address for local vip %s\n", vip_str);
           exit(1);
        }
        int read_socket=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if(read_socket==-1){
           fprintf(stderr, "cannot create read socket because %s\n", strerror(errno));
           exit(1);
        }
        if(bind(read_socket, (struct sockaddr*)&local_addr, local_addrlen)==-1){
           fprintf(stderr, "cannot bind read socket because %s\n", strerror(errno));
           exit(1);
        }
        struct in_addr local_vip=current_link->local_virt_ip;
        char* local_vip_str=convertVIPInt2String(local_vip.s_addr);
        int s=read_socket; printf("%d\n", s);
        g_hash_table_insert(g_read_socket_addr_lookup, (gpointer)&s, (gpointer)local_vip_str);//needed for debug?



        char* remote_dns=current_link->remote_phys_host;
        uint16_t remote_port=current_link->remote_phys_port;
        struct sockaddr_in remote_addr;
        size_t remote_addrlen;
        if(!dnsLookUp(remote_dns, remote_port, &remote_addr, &remote_addrlen)){
           char vip_str[INET_ADDRSTRLEN];
           inet_ntop(AF_INET, &(current_link->remote_virt_ip), vip_str, INET_ADDRSTRLEN);
           fprintf(stderr, "cannot find physical ip address for remote vip %s\n", vip_str);
           exit(1);
        }
        uint32_t send_socket=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if(send_socket==-1){
           fprintf(stderr, "cannot create read socket because %s\n", strerror(errno));
           exit(1);
        }
        //bind remote addr, okay?
        if(bind(send_socket, (struct sockaddr*)&remote_addr, remote_addrlen)==-1){
           fprintf(stderr, "cannot bind write socket because %s\n", strerror(errno));
           exit(1);
        }
        struct in_addr remote_vip=current_link->remote_virt_ip;
        char* remote_vip_str=convertVIPInt2String(remote_vip.s_addr);
        int* socket_p=&send_socket;
        g_hash_table_insert(g_send_socket_addr_lookup, (gpointer)(&send_socket), (gpointer)&remote_addr);
        g_hash_table_insert(s_forwarding_table, (gpointer)remote_vip_str, (gpointer)socket_p);


    }//end of for loop for looping linkedlist


   
}


//helper function, pass in dns_name string and port number and an empty addr and addrlen, function will fill in the empty addr/addrlen
bool dnsLookUp(char* dns_name, uint16_t port, struct sockaddr_in* addr, size_t* addrlen)
{
   struct addrinfo hints, *retr;
   memset(&hints, 0, sizeof(hints));
   hints.ai_family=AF_INET;
   hints.ai_socktype=SOCK_DGRAM;
   if (getaddrinfo(dns_name, NULL, &hints, &retr)!=0){
      fprintf(stderr, "cannot retrieve IP address because %s\n", strerror(errno));
      return false;
   }

   //get IP address from first retrieved
   assert(retr->ai_family==AF_INET);
   *addrlen=retr->ai_addrlen;
   memcpy(addr, retr->ai_addr, *addrlen);
   addr->sin_port=htons(port);
   freeaddrinfo(retr);

   char ip_str[INET_ADDRSTRLEN];
   inet_ntop(AF_INET, &(addr->sin_addr), ip_str, INET_ADDRSTRLEN);
   fprintf(stdout, "have retrieved IP address as %s\n", ip_str);
   return true;
}




//=================================================================================================
//      END OF FILE
//=================================================================================================

//for test
int main(int argc, char** argv)
{
    list_t* list=parse_links(argv[1]);
    setupForwardingTableAndSockets(list);
//    printReadFromWhichVIPTable();
    printForwardingTable();
    return 0;
}

