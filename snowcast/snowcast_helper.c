#include "snowcast_header.h"
#define WELCOME_REPLY 0
#define ANNOUNCE_REPLY 1
#define INVALID_COMMAND_REPLY 2
#define HELLO_COMMAND 0
#define STATION_COMMAND 1 
#define BUFFSIZE 256
#define DISCONNECTED -1
#define ERRNO_ERROR -2
#define MUSIC_CHUNK_SIZE 1024
#define MUSIC_UNIT_TIME 62500


struct threeDataPackage{
  uint8_t type;
  uint8_t data;
  char msg[BUFFSIZE];
}__attribute__((packed));


struct twoDataPackage{
  uint8_t type;
  uint16_t data;
}__attribute__((packed));



bool sendTwoDataPackage(int fd, struct twoDataPackage* pkg, size_t pkg_len, int* failType){
  //convert from host order to network order
  pkg->data=htons(pkg->data);

  while(true){
     int writebyte=write(fd, pkg, pkg_len);
     if (writebyte==0){//disconnect  
        *failType=DISCONNECTED;
        fprintf(stderr, "client server disconnected when sending two data!\n");
        return false;   
     }
     else if (writebyte==-1){
         if(errno==EINTR) continue;
         else{
            fprintf(stderr, "cannot send two data package because %s\n", strerror(errno));
            *failType=ERRNO_ERROR;
            return false;
         }
     }
     break;
  }
  //clear out pkg after send it
  memset(pkg, 0, pkg_len);
  return true;
}



bool recvTwoDataPackage(int fd, struct twoDataPackage* pkg, size_t pkg_len, int* failType){
   memset(pkg, 0, pkg_len);//clear out pkg before fill in data

   while(true){
     int readbyte=read(fd, pkg, pkg_len);
     if (readbyte==0){
         *failType=DISCONNECTED;
         fprintf(stderr, "client server disconnect when receiving two data!\n");
         return false; 
     }
     else if (readbyte==-1){
         if(errno==EINTR || errno==EAGAIN) continue;
         else{
            fprintf(stderr, "cannot receive reply because %s\n", strerror(errno));
            *failType=ERRNO_ERROR;
            return false;
         }
     }
     break;
   }

   //network to host order
   pkg->data=ntohs(pkg->data);
   return true;
}




bool sendThreeDataPackage(int fd, struct threeDataPackage* pkg, size_t pkg_len, int* failType){

  while(true){
     int writebyte=write(fd, pkg, pkg_len);
     if (writebyte==0){//disconnect  
        fprintf(stderr, "client server disconnected!\n");
        *failType=DISCONNECTED;
        return false;   
     }
     else if (writebyte==-1){
         if(errno==EINTR) continue;
         else{
            fprintf(stderr, "cannot send command because %s\n", strerror(errno));
            *failType=ERRNO_ERROR;
            return false;
         }
     }
     break;
  }
  //clear out pkg after send it
  memset(pkg, 0, pkg_len);
  return true;
}




bool recvThreeDataPackage(int fd, struct threeDataPackage* pkg, size_t pkg_len, int* failType){
   memset(pkg, 0, pkg_len);//clear out pkg before fill in data
   
   while(true){
     int readbyte=read(fd, pkg, pkg_len);
     if (readbyte==0){
         fprintf(stderr, "client server disconnect!\n");
         *failType=DISCONNECTED;
         return false; 
     }
     else if (readbyte==-1){
         if(errno==EINTR || errno==EAGAIN) continue;
         else{
            fprintf(stderr, "cannot receive reply because %s\n", strerror(errno));
            *failType=ERRNO_ERROR;
            return false;
         }
     }
     break;
   }

   return true;

}







