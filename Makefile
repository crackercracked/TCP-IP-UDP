CC = gcc
DEBUGFLAGS = -g -Wall
CFLAGS = -D_REENTRANT $(DEBUGFLAGS) -D_XOPEN_SOURCE=500
LDFLAGS = -lpthread -lrt

all: listen control server


server:   snowcast_server.c
	gcc $(DEBUGFLAGS) $(CFLAGS) snowcast_server.c $(LDFLAGS) -o snowcast_server

listen: snowcast_listener.c
	gcc $(DEBUGFLAGS) $(CFLAGS) snowcast_listener.c -o snowcast_listener


control:  snowcast_control.c
	$(CC) $(DEBUGFLAGS) $(CFLAGS) snowcast_control.c $(LDFLAGS) -o snowcast_control


	

 
clean:
	rm -f snowcast_listener snowcast_control snowcast_server
