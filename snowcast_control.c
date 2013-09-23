#include "snowcast_helper.c"

//return have successfully or not get an IP address
bool dnsLookUp(char* dnsName, uint16_t port, struct sockaddr_in* addr, size_t* addrlen){
   struct addrinfo hints, *retr;
   memset(&hints, 0, sizeof(hints));
   hints.ai_family=AF_INET;
   hints.ai_socktype=SOCK_STREAM;
   if (getaddrinfo(dnsName, NULL, &hints, &retr)!=0){
      fprintf(stderr, "cannot retrieve IP address\n");
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
   fprintf(stdout, "have retrieved server IP address as %s\n", ip_str);
   return true;
}



//return whether have succeeded in connecting to server addr
bool connectTCP(int* fd, struct sockaddr_in* dst_addr, size_t addrlen){
   int socket_fd=socket(AF_INET, SOCK_STREAM, 0);
   if (socket_fd==-1){
      fprintf(stderr, "cannot create TCP socket\n");
      return false;
   }
   
   if (connect(socket_fd, (struct sockaddr*)dst_addr, addrlen)==-1){
      fprintf(stderr, "cannot make tcp connection because %s\n", strerror(errno));
      return false;
   }
   *fd=socket_fd;
   return true;
}





void readloudRply(void* pkg, uint8_t* rplyType){
  uint8_t type=*(uint8_t*)pkg;
  *rplyType=type;
  switch(type){
     case WELCOME_REPLY:{
          struct twoDataPackage* rply = (struct twoDataPackage*)pkg; 
          printf("received welcome from server! server says there are %" SCNd16 " number of station available!\n", rply->data);
          break;
     }
     case ANNOUNCE_REPLY:{
          struct threeDataPackage* rply = (struct threeDataPackage*)pkg;     
          printf("server sent an Announcement: The song playing right now is %s\n", rply->msg);
      //    printf("please type in station number or q to exit: >> ");
          break;
     }
     case INVALID_COMMAND_REPLY:{
          struct threeDataPackage* rply= (struct threeDataPackage*)pkg;
          printf("server sent an warning!: %s\n", rply->msg);
          break;
     }
     default:{
        printf("unknown reply type %d\n", type);
        break;
     }
  }

}


void* receiveGeneral(void* arg){
   int socket_fd=*(int*)arg;
   struct threeDataPackage rply;
   uint8_t rplyType;
   int failType;
   while(true){
      if(recvThreeDataPackage(socket_fd, &rply, sizeof(rply), &failType)){
          readloudRply(&rply, &rplyType);
          if (rplyType==INVALID_COMMAND_REPLY){
              close(socket_fd);
              exit(1);
          }
      }
      else{
           printf("didn't successfully receive confirmation from server, please try another station!\n");
           if (failType==DISCONNECTED) exit(1);
           else continue;
      
      }
        
   }
   return NULL;
}



/******************************************* main ************************************************/

int main(int argc, char** argv){
  if (argc!=4) print_err_and_exit("wrong argu format!\n");

  //read port number
  uint16_t serverport;
  uint16_t udpport;
  if (sscanf(argv[2], "%" SCNd16, &serverport)!=1){
     print_err_and_exit("cannot read in server port\n");
  }  

  if (sscanf(argv[3], "%" SCNd16, &udpport)!=1){
     print_err_and_exit("cannot read in udp port\n");
  }

  //read and lookup DNS name
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  size_t addrlen;
  if (!dnsLookUp(argv[1], serverport, &addr, &addrlen)){
     print_err_and_exit("cannot retrieved an IP address\n");
  }

  //connect to server
  int socket_fd;
  if(connectTCP(&socket_fd, &addr, addrlen)){
     printf("connection between tcp client and server established!\n");
  }
  else print_err_and_exit("cannot make tcp connection between tcp client and server\n");


  //start handshake by sending udpport and receive nums of station
  bool handshaked=false;
  int failType;
  uint8_t rplyType;
  while(!handshaked){    
       struct twoDataPackage cmnd;
       cmnd.type=HELLO_COMMAND;
       cmnd.data=udpport;
       if (sendTwoDataPackage(socket_fd, &cmnd, sizeof(cmnd), &failType)){
          fprintf(stdout, "have successfully send hello to server\n");
       }
       else continue;//send failed, send hello again
       
       struct twoDataPackage rply;
       if(recvTwoDataPackage(socket_fd, &rply, sizeof(rply), &failType)){
          readloudRply(&rply, &rplyType);
          if(rplyType==WELCOME_REPLY) handshaked=true; 
       }
       else{
           if(failType==DISCONNECTED) break;
       }
  }
  if(handshaked) printf("finished handshake!\n");  

  uint16_t stationNumber;
  pthread_t thread;
  pthread_create(&thread, NULL, receiveGeneral, (void*)&socket_fd);
  printf("please type in station number or q to exit:\n");



  while(true){//start main program, set station and etc....

     char readbuffer[256];
     scanf("%s", readbuffer);
     if (strcmp(readbuffer, "q")==0){
        printf("client has quited the connection!\n");
        return 0;
     }



    //send station number
    stationNumber=atoi(readbuffer);
    struct twoDataPackage cmnd;
    cmnd.type=STATION_COMMAND;
    cmnd.data=stationNumber;
    if ( sendTwoDataPackage(socket_fd, &cmnd, sizeof(cmnd), &failType) ){
       printf("the client have told server to play station %" SCNd16 "\n", stationNumber);
    }
    else{
       printf("didn't successfully send station number to server last time, please try again!\n");
       if (failType==DISCONNECTED) exit(1);
       else continue;
    }

      



  }//end of while loop



  return 0;
}
