#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <inttypes.h>
#include <assert.h>
#include <pthread.h>
typedef enum{true=1, false=0} bool;

void print_err_and_exit(char* err_messege){
    fprintf(stderr, "%s", err_messege);
    exit(1);
}



