CC = gcc
DEBUGFLAGS = -g -Wall
CFLAGS = -D_REENTRANT $(DEBUGFLAGS) -D_XOPEN_SOURCE=500

LDFLAGS = -lpthread -lrt
OBJ=util/parselinks.o util/ipsum.o util/list.o utility.o
HASHFLAG=`pkg-config --cflags glib-2.0`
HASHLIB=`pkg-config --libs glib-2.0`


all: node


OBJ: util/parselinks.c util/ipsum.c util/list.c utility.c
	$(CC) -c $(CFLAGS) util/parselinks.c util/ipsum.c util/list.c utility.c


net_layer.o: net_layer.c
	gcc -c $(CFLAGS) $(HASHFLAG) net_layer.c $(HASHLIB)


link_layer.o: link_layer.c
	gcc -c $(CFLAGS) $(HASHFLAG) link_layer.c $(HASHLIB)
	

node: node.c $(OBJ) link_layer.o net_layer.o
	gcc $(CFLAGS) $(OBJ) link_layer.o net_layer.o $(HASHFLAG) node.c -o node $(LDFLAGS) $(HASHLIB)


clean:
	rm util/*.o *.o node
