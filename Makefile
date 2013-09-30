CC = gcc
DEBUGFLAGS = -g -Wall
CFLAGS = -D_REENTRANT $(DEBUGFLAGS) -D_XOPEN_SOURCE=500

LDFLAGS = -lpthread -lrt
OBJ=util/parselinks.o util/ipsum.o util/list.o ip_lib.o
HASHFLAG=`pkg-config --cflags glib-2.0`
HASHLIB=`pkg-config --libs glib-2.0`



obj: util/parselinks.c util/ipsum.c util/list.c ip_lib.c
	$(CC) -c $(CFLAGS) util/parselinks.c util/ipsum.c util/list.c ip_lib.c


udp: link_layer.c $(OBJ)
	gcc -c $(CFLAGS) $(OBJ) $(HASHFLAG) link_layer.c $(HASHLIB)
	

node: node.c $(OBJ) link_layer.o
	gcc $(CFLAGS) $(OBJ) link_layer.o $(HASHFLAG) node.c -o node $(LDFLAGS) $(HASHLIB)


clean:
	rm util/*.o *.o node a.out
