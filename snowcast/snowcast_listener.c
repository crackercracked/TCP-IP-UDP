#include "snowcast_helper.c"
#include <sys/time.h>

char music_chunk[MUSIC_CHUNK_SIZE];



int main(int argc, char** argv){
  if(argc!=2) print_err_and_exit("wrong input format\n");

  //read updport
  uint16_t udpport;
  if (sscanf(argv[1], "%" SCNd16, &udpport)==-1){
     print_err_and_exit("cannot read udpport\n");
  }

  /* receive break-down:
    create socket and bind it to local address
    indicate it is listener socket
  */
  int socket_fd=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (socket_fd==-1){
     print_err_and_exit("cannot create udp socket\n");
  }
  struct sockaddr_in myaddr;
  memset(&myaddr, 0, sizeof(myaddr));
  myaddr.sin_family=AF_INET;
  myaddr.sin_port=htons(udpport);
  myaddr.sin_addr.s_addr=htonl(INADDR_ANY);
  if ( bind(socket_fd, (struct sockaddr*)(&myaddr), sizeof(myaddr))==-1){
     print_err_and_exit("cannot bind socket\n");
  }


  //receive
  struct sockaddr_in src_addr;
  socklen_t addrlen=sizeof(src_addr);
  struct timeval start, end;

  while(true){    
    if(gettimeofday(&start, NULL)<0){
       fprintf(stderr, "cannot tic toc recv start!\n");
       break;
    }

    int recv=recvfrom(socket_fd, music_chunk, MUSIC_CHUNK_SIZE, 0, (struct sockaddr*)&src_addr, &addrlen);
     if (recv==-1){
        fprintf(stderr, "cannot receive music stream from server because %s\n", strerror(errno));
        break;
     }
     else if (recv==0) break;
    

     if(gettimeofday(&end, NULL)<0){
       fprintf(stderr, "cannot tic toc recv end!\n");
       break;
     }

//     useconds_t total=MUSIC_UNIT_TIME;
//     useconds_t elapsed=(end.tv_usec-start.tv_usec);
//     useconds_t microleft=total-elapsed;
//     usleep(microleft);

     printf("%d\n", recv);
     write(1,  music_chunk, MUSIC_CHUNK_SIZE); 

  } 

  close(socket_fd);
  return 0;
}
