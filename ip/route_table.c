#define INFINITY_DISTANCE 16

void deleteDstRecord(uint32_t* dst_vip)
{
    g_hash_table_remove(s_routing_table, dst_vip);
    g_hash_table_remove(s_routing_distances, dst_vip);
    g_hash_table_remove(s_routing_ttls, dst_vip);
}



void initializeRoutingTableSet(list_t* list)
{
    s_routing_table = g_hash_table_new(g_int_hash, g_int_equal);
    s_routing_distances = g_hash_table_new(g_int_hash, g_int_equal);
    s_routing_ttls = g_hash_table_new(g_int_hash, g_int_equal);
    node_t* current = NULL;
    link_t* link = NULL;
    for (current = list->head; current != NULL; current = current->next){
        link = (link_t*)(current->data);
        g_hash_table_insert(s_routing_table, (gpointer)&(link->remote_virt_ip.s_addr), (gpointer)&(link->remote_virt_ip.s_addr));
        //might want to store pointer in a list?
        uint32_t* dist=(uint32_t*)malloc(sizeof(uint32_t));
        *dist=SINGLE_HOP_DISTANCE;
        g_hash_table_insert(s_routing_distances, (gpointer)&(link->remote_virt_ip.s_addr), (gpointer)dist);
        int* ttl=(int*)malloc(sizeof(int));
        *ttl=DEFAULT_ROUTE_TTL;
        g_hash_table_insert(s_routing_ttls, (gpointer)&(link->remote_virt_ip.s_addr), ttl);
    }

}



void updateRoutingTableSet(uint32_t* send_vip, uint32_t* dst_addr, uint32_t* dst_cost)
{
   if(!g_hash_table_contains(s_routing_table, (gpointer)send_vip)){
        //if dst_addr never show up before
        g_hash_table_insert(s_routing_table, (gpointer)dst_addr, (gpointer)send_vip);
        
        *dst_cost=(*dst_cost)+SINGLE_HOP_DISTANCE;
        g_hash_table_insert(s_routing_distances, (gpointer)dst_addr, (gpointer)dst_cost);
        int* time_left=(int*)malloc(sizeof(int));
        *time_left=DEFAULT_ROUTE_TTL;
        g_hash_table_insert(s_routing_ttls, (gpointer)dst_addr, (gpointer)time_left);
   }
   else{ 
       //already contain the routing table set
       int* current_dst=g_hash_table_lookup(s_routing_distances, (gpointer)dst_addr);
       //need to update next-hop
       if(*current_dst>*dst_cost){
          g_hash_table_replace(s_routing_table, (gpointer)dst_addr, (gpointer)send_vip);
          g_hash_table_replace(s_routing_distances, (gpointer)dst_addr, (gpointer)dst_cost);
          int* time_left=(int*)malloc(sizeof(int));
          *time_left=DEFAULT_ROUTE_TTL;
          g_hash_table_replace(s_routing_ttls, (gpointer)dst_addr, time_left);
          
       }
   }
  
}




void handleRIPPacket(ip_packet_t* ip_packet)
{
   //read command, if command=1, build IP packet from routing tables set 
   //if command=2, update routing tables set
   ip_header_t header=ip_packet->ip_header;
   char* data=ip_packet->ip_data;
   uint32_t* sender_vip=(uint32_t*)malloc(sizeof(uint32_t));
   uint32_t* receiver_ip=(uint32_t*)malloc(sizeof(uint32_t));
   *sender_vip=(uint32_t)ntohl(header.ip_src);
   *receiver_ip=(int32_t)ntohl(header.ip_src);
   uint16_t command=ntohs(util_getU16(data));
   data=data+2;
 
   if(command==1){//send data;
      //build ip packet
      int dst_vip=*(int*)sender_vip;
      int src_vip=(int)ntohl(header.ip_dst);
      int nums_dst=g_hash_table_size(s_routing_distances);
      size_t reply_len=(sizeof(uint32_t)+sizeof(uint32_t))*nums_dst;
      char* reply=(char*)malloc(reply_len);

      int memory_start=0;
      GList* all_dsts=g_hash_table_get_keys(s_routing_distances);
      GList* cur_dst_ptr;
      for(cur_dst_ptr=all_dsts; cur_dst_ptr!=NULL; cur_dst_ptr=cur_dst_ptr->next){
          uint32_t* cur_dst=cur_dst_ptr->data;
          uint32_t* cur_cost=g_hash_table_lookup(s_routing_distances, (gpointer)cur_dst);
          
          //do split horizon with poison reverse
          uint32_t* cur_nexthop=g_hash_table_lookup(s_routing_table, (gpointer)cur_dst);
          if(*cur_nexthop!=*sender_vip){//next hop is not the vip who send request
             memcpy(reply+memory_start, cur_cost, sizeof(uint32_t));
             memory_start+=4;
             memcpy(reply+memory_start, cur_dst, sizeof(uint32_t));
             memory_start+=4;
          }      
          else{
              uint32_t infinity=INFINITY_DISTANCE;
              memcpy(reply+memory_start, cur_cost, sizeof(uint32_t));
              memory_start+=4;
              memcpy(reply+memory_start, &infinity, sizeof(uint32_t));      
              memory_start+=4;
               
          }
      }
      ip_packet_t ip_packet;
      buildIPPacket(&ip_packet, dst_vip, src_vip, 200, reply, reply_len);
      
      //send ip packet;
      int send_reply_result=sendIPPacket(&ip_packet);
      switch(send_reply_result){
          case SENT_IP_PACKET:{
               printf("have already send reply to %s who request for updated routing information\n", convertVIPInt2String(dst_vip));
               break;
          }
          case UNKNOWN_DESTINATION:{
               printf("cannot send reply for updated routing info because error in destination vip\n");
               break;
          }
          case LINK_DOWN:{
               printf("cannot send reply for updated routing info because link is down\n");
               break;

          }
      }

   }
   else if(command==2){
       uint16_t num_entries=ntohs(util_getU32(data));
       data=data+2;
       uint16_t num_entries_read=0;
       while(num_entries_read<num_entries){
            uint32_t* cost=(uint32_t*)malloc(sizeof(uint32_t));
            *cost=ntohl(util_getU32(data));
            data=data+4;
            uint32_t* addr=(uint32_t*)malloc(sizeof(uint32_t));
            *addr=ntohl(util_getU32(data));
            data=data+4;
            updateRoutingTableSet(sender_vip, addr, cost);
            num_entries_read++;
           
       }     
   }
}
