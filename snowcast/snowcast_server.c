#include "snowcast_helper.c"
#include <fcntl.h>
#include <sys/time.h>
#include <sys/select.h>
#include <string.h>
#define BEFORE_HANDSHAKE 0
#define HELLO_RECEIVED 1
#define READY_TO_TAKE_ORDER 2//aka handshaked
#define STATION_RECEIVED 3
#define SHOULD_PLAY_MUSIC 4
#define MUSIC_PLAYED_AGAIN 5
#define HELLO_OVER_RECEIVED -1 
#define STATION_NONEXIST -2
#define HANDSHAKE_MISSED -3
#define LAST_ANNOUNCE_NOTSENT -4
#define OTHER_ERROR -5
#define MAX_NUMCLIENTS 20
#define MAX_NUMSTATIONS 10


fd_set readReady_fd;
fd_set writeReady_fd;


int maxfd;
int numStations;

int* client_states;//should be size of number of tcp client
int* client_sockets;
uint16_t* requests;
struct sockaddr_in* udpaddrs;
uint16_t* udpports;

int* music_fd;//should be size of numStations
pthread_t* music_threads;

char** allmusicNames;




bool setListenTCPSocket(int* fd, char* portStr){
    struct addrinfo hints, *retr;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family=AF_INET;
    hints.ai_socktype=SOCK_STREAM;
    hints.ai_flags=AI_PASSIVE;
    
    if (getaddrinfo(NULL, portStr, &hints, &retr)!=0){
       fprintf(stderr, "cannot retrieve server's IP address\n");
       return false;
    }

    //loop every retrieved ip address until able to bind one
    struct addrinfo* curinfo;
    int socket_fd=-1;
    for(curinfo=retr; curinfo!=NULL; curinfo=curinfo->ai_next){
       socket_fd=socket(AF_INET, SOCK_STREAM, 0);
       if (socket_fd==-1) continue;
       
       int optval=1;
       setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)); 
       if (bind(socket_fd, curinfo->ai_addr, curinfo->ai_addrlen)==0) break;
       close(socket_fd);             
    }
    freeaddrinfo(retr);

    if (socket_fd==-1){
       fprintf(stderr, "fail to bind socket to any server's address\n");
       close(socket_fd);
       return false;
    }

    //indicate it is a listen socket
    if (listen(socket_fd, SOMAXCONN)!=0){
       fprintf(stderr, "fail to indicate socket as listen socket because %s\n", strerror(errno));
       close(socket_fd);
       return false;
    }
   
    *fd=socket_fd;
    return true;

}





void readloudCmnd(struct twoDataPackage* pkg, uint8_t* cmndtype){
  uint8_t type=pkg->type;
  switch(type){
     case HELLO_COMMAND:{
          printf("client has said Hello!\n");
          printf("client wants to listen on port %" SCNd16 "\n", pkg->data);
          break;
     }
     case STATION_COMMAND:{
          printf("client made an request on listening to station %" SCNd16 "\n", pkg->data);
          break;
     }
     default:{
        printf("unknown command type %d\n", type);
        break;
     }
  }
  *cmndtype=type;

}





struct playMusicArg{
  int stationNumber;//which station
  int udp_socket;
  pthread_mutex_t lock;
};



void* playMusicForever(void* arg){//handle one station possible multiple client
  struct playMusicArg* playArg=(struct playMusicArg*)arg;
  int stationNumber=playArg->stationNumber;

//  printf("%d\n", stationNumber);
  int fd=music_fd[stationNumber];
  char buff[MUSIC_CHUNK_SIZE];//=playArg->chunk;
  struct timeval start, end;
 
//  pthread_mutex_lock(&(playArg->lock));


  bool again=false;
  while(true){

      if(lseek(fd, 0, SEEK_CUR)==SEEK_END){
          printf("start replaying music from begining...\n");
          lseek(fd, 0, SEEK_SET);
          again=true;
          continue;
      }   
      memset(buff, 0, MUSIC_CHUNK_SIZE);       

      if(gettimeofday(&start, NULL)<0){
         fprintf(stderr, "cannot tic toc send start!\n");
         break;
      }


      while(true){
         int readbyte=read(fd, buff, MUSIC_CHUNK_SIZE);
         if (readbyte==-1){
            if (errno==EINTR || errno==EAGAIN) continue;
            else{
                fprintf(stderr, "cannot read in song in fd %d because %s\n", fd, strerror(errno));
                break;
            }
         }
         break;
      }


      //send music stream
      int udp_socket=playArg->udp_socket;
      int cfd;
      for (cfd=0; cfd<MAX_NUMCLIENTS; cfd++){
          if (client_states[cfd]!=SHOULD_PLAY_MUSIC) continue;
          int c_request=requests[cfd];
          if (c_request!=stationNumber) continue;          

          if(again){
             client_states[cfd]=MUSIC_PLAYED_AGAIN;
          }
          struct sockaddr_in udpaddr=udpaddrs[cfd];
          size_t udpaddr_len=sizeof(udpaddr);
          if (sendto(udp_socket, buff, MUSIC_CHUNK_SIZE, 0, (struct sockaddr*)&udpaddr, udpaddr_len)!=MUSIC_CHUNK_SIZE){
              fprintf(stderr, "cannot send music stream because %s!\n", strerror(errno));
              return NULL;
          }
      }
      again=false;

      if(gettimeofday(&end, NULL)<0){
         fprintf(stderr, "cannot tic toc send end!\n");
         break;
      }
      useconds_t total=MUSIC_UNIT_TIME;
      useconds_t elapsed=(end.tv_usec-start.tv_usec);//+1000*(end.tv_sec-start.tv_sec);
      useconds_t microleft=total-elapsed;
      usleep(microleft);
      

  }
//  pthread_mutex_unlock(&(playArg->lock));
  return NULL;
      
}





void statesInitialSetUp(int* states, int len){
  int i;
  for(i=0; i<len; i++){
     states[i]=BEFORE_HANDSHAKE;
  }
}





void numClientsResize(int oldlen, int newlen){
   client_states=(int*)realloc(client_states, sizeof(int)*newlen);
   client_sockets=(int*)realloc(client_states, sizeof(int)*newlen);
   requests=(uint16_t*)realloc(requests, sizeof(uint16_t)*newlen);
   udpaddrs=(struct sockaddr_in*)realloc(udpaddrs, sizeof(struct sockaddr_in)*newlen);
   udpports=(uint16_t*)realloc(udpports, sizeof(uint16_t)*newlen);
   int i;
   for(i=oldlen; i<newlen; i++) client_states[i]=BEFORE_HANDSHAKE;
}






void* takeInCommand(void* arg){
  int udpsocket=*(int*)arg;

  while(true){
     char readbuffer[256];
     memset(readbuffer, 0, 256);
     if (fgets(readbuffer, 256, stdin)!=NULL){
//        printf("%s", readbuffer);

        if(strcmp(readbuffer, "q\n")==0){//quit
           printf("server quiting connection...\n");
           exit(0);
        }

 
        else if(strcmp(readbuffer, "p\n")==0){// "p" interface
           int i;
           int listenBy[numStations][maxfd/numStations+1];
           int eachStaCount[numStations];
           for(i=0; i<numStations; i++) eachStaCount[i]=0;
           for(i=0; i<maxfd; i++){
               if(client_states[i]!=SHOULD_PLAY_MUSIC && client_states[i]!=MUSIC_PLAYED_AGAIN) continue;
               int stationNum=requests[i];
               int count=eachStaCount[stationNum];
               listenBy[stationNum][count]=i;
               eachStaCount[stationNum]++;
           }
                   
           for(i=0; i<numStations; i++){
               printf("server playing Station %d, listened by ", i);
               if (eachStaCount[i]==0) {
                   printf("no one\n");
                   continue;
               }
               int j;
               for(j=0; j<eachStaCount[i]; j++){
                  printf("client with fd %d, ", listenBy[i][j]);
               }
               printf("\n");
           }
        }
        else if(readbuffer[0]=='a' && readbuffer[1]=='d' && readbuffer[2]=='d'&& readbuffer[3]==' '){// add new station
            //parse add new station command
            char* searchstart=readbuffer+4;
            char* searchend=searchstart;
            int songnameSize=0;
            char songName[256];
            while(*searchend!='\0'){
               if (*searchend==' ' || *searchend=='\n'){
                   if (songnameSize>0){
                      songName[songnameSize]='\0';
                      if(numStations+1>MAX_NUMSTATIONS){
                        music_fd=(int*)realloc(music_fd, sizeof(int)*2*numStations);
                        music_threads=(pthread_t*)realloc(music_threads, sizeof(pthread_t)*2*numStations);
                      }

                      music_fd[numStations]=open(songName, O_RDONLY);
                      if(music_fd[numStations]<0){
                          printf("cannot open %s\n", songName);
                          searchstart=searchend+1;
                          searchend=searchstart;
                          memset(songName, 0, 256);
                          songnameSize=0;
                          continue;
                      }
                      printf("add new station %d playing %s!\n", numStations, songName);

                      allmusicNames[numStations]=(char*)malloc(sizeof(char)*songnameSize);
                      memcpy(allmusicNames[numStations], songName, songnameSize);
                      struct playMusicArg arg;
                      arg.stationNumber=numStations;
                      pthread_mutex_t lock;
                      arg.lock=lock;
                      arg.udp_socket=udpsocket;
                      pthread_t thread;
                      pthread_create(&thread, NULL, playMusicForever, (void*)&arg);
                      music_threads[numStations]=thread;
                      numStations+=1;
                      searchstart=searchend+1;
                      searchend=searchstart;
                      memset(songName, 0, 256);
                      songnameSize=0;
                   }
                 
               }
               else{
                  songName[songnameSize]=*searchend;
                  songnameSize++;
                  searchend+=1;
               }
            }
        }//end of else if
     }
  }
  return NULL;
}





/**************************main *********************************************/
int main(int argc, char** argv){

  
  client_states=(int*)malloc(sizeof(int)*MAX_NUMCLIENTS);
  client_sockets=(int*)malloc(sizeof(int)*MAX_NUMCLIENTS);
  requests=(uint16_t*)malloc(sizeof(uint16_t)*MAX_NUMCLIENTS);
  udpaddrs=(struct sockaddr_in*)malloc(sizeof(struct sockaddr_in)*MAX_NUMCLIENTS);
  udpports=(uint16_t*)malloc(sizeof(uint16_t)*MAX_NUMCLIENTS);

  music_fd=(int*)malloc(sizeof(int)*MAX_NUMSTATIONS);
  music_threads=(pthread_t*)malloc(sizeof(pthread_t)*MAX_NUMSTATIONS);


  allmusicNames=(char**)malloc(sizeof(char*)*MAX_NUMSTATIONS);
  

  if (argc<3) print_err_and_exit("wrong argument format!\n");

  //read argument
  numStations=argc-2;
  uint16_t tcpport;
  if (sscanf(argv[1], "%" SCNd16, &tcpport)!=1){
     print_err_and_exit("fail to read in tcpport for server\n");
  }
 


  //open music file 
  int i;
  for(i=0; i<numStations; i++){
      music_fd[i]=open(argv[i+2], O_RDONLY);
      if (music_fd[i]==-1){
         fprintf(stderr, "cannot open file %s\n", argv[i+2]);
         return 1;
      }
      allmusicNames[i]=(char*)malloc(sizeof(char)*256);
      memcpy(allmusicNames[i], argv[i+2], strlen(argv[i+2]));
  }



  //set listen socket
  int listen_fd;
  if (setListenTCPSocket(&listen_fd, argv[1])){
     printf("have successfully binded the socket! waiting for connection from client...\n");
  }
  else return 1;



  maxfd=MAX_NUMCLIENTS>listen_fd?MAX_NUMCLIENTS:listen_fd;
  if(maxfd>MAX_NUMCLIENTS){
     numClientsResize(MAX_NUMCLIENTS, maxfd);
  }
  statesInitialSetUp(client_states, maxfd);



  //play music, set udp sockets for clients
  int socket_udp=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);//creat udp socket
  if (socket_udp==-1){
     print_err_and_exit("cannot create udp socket for the listener\n");
  }


  int sid;//station id/number
  struct playMusicArg playArgs[numStations];
  pthread_mutex_t lock[numStations];
  int stationNumbers[numStations];
  for(sid=0; sid<numStations; sid++) stationNumbers[sid]=sid;

  for(sid=0; sid<numStations; sid++){
     playArgs[sid].stationNumber=stationNumbers[sid];
     playArgs[sid].lock=lock[sid];
     playArgs[sid].udp_socket=socket_udp;
     pthread_create(&music_threads[sid], NULL, playMusicForever, (void*)(playArgs+sid));
//     pthread_join(music_threads[sid], NULL);
//     music_threads[sid]=thread;
  }


  //thread for taking command from stdin
  pthread_t mthread;
  pthread_create(&mthread, NULL, takeInCommand, (void*)&socket_udp);

  ///////////////////////////////start prepare interact with client
  fd_set readReady_fd_tmp;
  fd_set writeReady_fd_tmp;
  FD_ZERO(&readReady_fd);
  FD_ZERO(&writeReady_fd);
  FD_ZERO(&readReady_fd_tmp);
  FD_ZERO(&writeReady_fd_tmp);
  FD_SET(listen_fd, &readReady_fd);
  int numsfd=listen_fd;
 
  int client_fd;


  struct sockaddr_in client_addr;
  socklen_t client_addrlen=sizeof(client_addr);
  bool needwait=false; //need to wait if last announce not sent
  int failType;




  /*******************************************/

  


  while(true){//main while loop of server
     readReady_fd_tmp=readReady_fd;
     writeReady_fd_tmp=writeReady_fd;

     if (select(numsfd+1, &readReady_fd_tmp, &writeReady_fd_tmp, NULL, NULL)==-1){
        fprintf(stderr, "cannot select on fds because %s\n", strerror(errno));
        return 1;
     }

     int fd;
     for(fd=0; fd<=numsfd; fd++){
        if (FD_ISSET(fd, &readReady_fd_tmp)){//if a read-ready socket
           if(fd==listen_fd){//new connection
              client_fd=accept(listen_fd, (struct sockaddr*)&client_addr, &client_addrlen);
              if (client_fd<0){
                  fprintf(stderr, "cannot accept %s\n", strerror(errno));
                  continue;
              }
              else{//get new client
                  client_sockets[client_fd]=client_fd;
                  FD_SET(client_fd, &readReady_fd);
                  FD_SET(client_fd, &writeReady_fd);
                  numsfd=client_fd>numsfd?client_fd:numsfd;
                  if (client_fd>maxfd){
                     numClientsResize(maxfd, client_fd+100);
                     maxfd=client_fd+100;
                     
                  }//end of handling edge case for client_fd "overflow"

                  //get udp address, same addr as control but different port
                  struct sockaddr_in udpaddr;
                  memcpy(&udpaddr, &client_addr, client_addrlen);
                  udpaddrs[client_fd]=udpaddr;

              }                 
           }//end of new connection
           else{//new receival
               struct twoDataPackage cmnd;
               uint8_t cmndtype;
               if (recvTwoDataPackage(fd, &cmnd, sizeof(cmnd), &failType)){
                   readloudCmnd(&cmnd, &cmndtype);  
                   switch(cmndtype){
                       case HELLO_COMMAND: {//get udpport
                           if (client_states[fd]==BEFORE_HANDSHAKE){
                               client_states[fd]=HELLO_RECEIVED;
                               udpports[fd]=cmnd.data;
                               udpaddrs[fd].sin_port=htons(udpports[fd]);
                           }
                           else client_states[fd]=HELLO_OVER_RECEIVED;
                           break;
                       }
                       case STATION_COMMAND:{//get station number
                           if (client_states[fd]==READY_TO_TAKE_ORDER || client_states[fd]==SHOULD_PLAY_MUSIC || client_states[fd]==MUSIC_PLAYED_AGAIN){
                               requests[fd]=cmnd.data;
                               if (requests[fd]>=0 && requests[fd]<numStations){
                                   client_states[fd]=STATION_RECEIVED;
                                   needwait=true;
                               }
                               else client_states[fd]=STATION_NONEXIST;
                           }
                           else{
                              if(needwait) client_states[fd]=LAST_ANNOUNCE_NOTSENT;
                              else client_states[fd]=HANDSHAKE_MISSED;
                           }
                           break;
                       }
                       default:{
                           client_states[fd]=OTHER_ERROR;
                           break;
                       }
                   }
               }
               else{
                   if(failType==DISCONNECTED){
                      FD_CLR(fd, &readReady_fd);
                      FD_CLR(fd, &writeReady_fd);
                      client_states[fd]=BEFORE_HANDSHAKE;
                   }
               }
           }//end of new receivel
        }//end of checking read_fd;

      if (FD_ISSET(fd, &writeReady_fd_tmp)){//if a write-ready socket
            if (client_states[fd]==BEFORE_HANDSHAKE) continue;
            int socktype;
            socklen_t socklen=sizeof(int);
            getsockopt(fd, SOL_SOCKET, SO_TYPE, &socktype, &socklen);
	    client_fd=fd;
	    if (client_states[fd]>0){//nothing go wrong
	        switch(client_states[fd]){
                          case HELLO_RECEIVED:{
                               struct twoDataPackage rply;
                               rply.type=WELCOME_REPLY;//send welcome
                               rply.data=numStations;
                               if (sendTwoDataPackage(fd, &rply, sizeof(rply), &failType)){
                                   fprintf(stdout, "have sent a welcome reply to client!\n");
                               }
                               client_states[fd]=READY_TO_TAKE_ORDER;
                               needwait=false;
                               printf("handshaked finished!\n");   
                               break;
                          }//end of case hello_received
                          case STATION_RECEIVED:{//should send announce
                               struct threeDataPackage rply;
                               rply.type=ANNOUNCE_REPLY;
                               rply.data=strlen(allmusicNames[requests[fd]]);
                               strncpy(rply.msg, allmusicNames[requests[fd]], rply.data);
                               size_t sendbyte=sizeof(rply)-(BUFFSIZE-rply.data);
                               if (sendThreeDataPackage(fd, &rply, sendbyte, &failType)){
                                   printf("server has sent an announcement to client!\n");
                               }
                               client_states[fd]=SHOULD_PLAY_MUSIC;
                               needwait=false;
                               break;
                          }//end of case station_recv
                          case MUSIC_PLAYED_AGAIN:{
                               struct threeDataPackage rply;
                               rply.type=ANNOUNCE_REPLY;
                               int whichStation=requests[fd];
                               rply.data=strlen(allmusicNames[whichStation]);
                               strncpy(rply.msg, allmusicNames[whichStation], rply.data);
                               size_t sendbyte=sizeof(rply)-(BUFFSIZE-rply.data);
                               if (sendThreeDataPackage(fd, &rply, sendbyte, &failType)){
                                   printf("Music repeated again! Server has sent an announcement to client!\n");
                               }
                               client_states[fd]=SHOULD_PLAY_MUSIC;
                               break;
                          }
                          
                    }//end of switch for positve client_states[fd]
                }
                else{//something going wrong during the last command from tcp client
                    struct threeDataPackage rply;
                    rply.type=INVALID_COMMAND_REPLY;
                    size_t sendbyte;
                    switch(client_states[fd]){ 
                         case HELLO_OVER_RECEIVED:{
                              char errorMsg[]="Error: server has already received hello before!\n";
                              rply.data=strlen(errorMsg);
                              strncpy(rply.msg, errorMsg, rply.data);
                              sendbyte=sizeof(rply)-(BUFFSIZE-rply.data);
                              break;
                         }//end of over-sent hello error
                         case STATION_NONEXIST:{
                              char errorMsg[]="Error: the station request does not exit!\n";
                              rply.data=strlen(errorMsg);
                              strncpy(rply.msg, errorMsg, rply.data);
                              sendbyte=sizeof(rply)-(BUFFSIZE-rply.data);
                              break;
                         }
                         case HANDSHAKE_MISSED:{
                              char errorMsg[]="Error: client should make handshake first!!\n";
                              rply.data=strlen(errorMsg);
                              strncpy(rply.msg, errorMsg, rply.data);
                              sendbyte=sizeof(rply)-(BUFFSIZE-rply.data);
                              break;
                              
                         }
                         case LAST_ANNOUNCE_NOTSENT:{
                              char errorMsg[]="Error: last announce has not been sent already!\n";
                              rply.data=strlen(errorMsg);
                              strncpy(rply.msg, errorMsg, rply.data);
                              sendbyte=sizeof(rply)-(BUFFSIZE-rply.data);
                              break;
                              
                         }
                    }//end of switch for negative client_states[fd]
                    if (sendThreeDataPackage(client_fd, &rply, sendbyte, &failType)){
                        printf("server has sent an warning to client!\n");
                    }
                    close(fd);
                    FD_CLR(fd, &readReady_fd);
                    FD_CLR(fd, &writeReady_fd);
                    client_states[fd]=BEFORE_HANDSHAKE;                   
                }//end of else client_states[fd]ment 
         }//end of if loop for check writeReady
       
     }//end of for loop

  }//end of main while loop of server


      

  free(client_states);//should be size of number of tcp client
  free(requests);
  free(udpaddrs);
  free(udpports);
  free(music_fd);//should be size of numStations
  free(music_threads);
  free(allmusicNames);          
  free(client_sockets);
  return 0;
}
