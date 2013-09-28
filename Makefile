CC = gcc
DEBUGFLAGS = -g -Wall
CFLAGS = -D_REENTRANT $(DEBUGFLAGS) -D_XOPEN_SOURCE=500
LDFLAGS = -lpthread -lrt
OBJ=util/parselinks.o util/ipsum.o util/list.o ip_lib.o


all: node

obj: parselinks.c ipsum.c list.c ip_lib.c
	$(CC) -c $(DEBUGFLAGS) $(CFLAGS) util/parselink.c util/ipsum.c util/list.c ip_lib.c


node: node.c $(OBJ)
	gcc $(DEBUGFLAGS) $(CFLAGS) $(OBJ) node.c -o node $(LDFLAGS)


 
clean:
	rm util/*.o node
