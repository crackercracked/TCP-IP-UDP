CC = gcc
DEBUGFLAGS = -g -Wall
CFLAGS = -D_REENTRANT $(DEBUGFLAGS) -D_XOPEN_SOURCE=500

LDFLAGS = -lpthread -lrt
OBJ=util/parselinks.o util/ipsum.o util/list.o utility.o util/bqueue.o util/dbg.o util/circular_buffer.o
HASHFLAG=`pkg-config --cflags glib-2.0`
HASHLIB=`pkg-config --libs glib-2.0`

#---------------------------
TCP_FLAG=-D_BSD_SOURCE

all: node


OBJ: util/parselinks.c util/ipsum.c util/list.c utility.c util/dbg.c util/bqueue.c util/circular_buffer.c
	$(CC) -c $(CFLAGS) util/parselinks.c util/ipsum.c util/list.c utility.c util/dbg.c util/bqueue.c util/circular_buffer.c

port_util.o: port_util.c
	gcc -c $(CFLAGS) $(HASHFLAG) port_util.c $(HASHLIB)

net_layer.o: net_layer.c 
	gcc -c $(CFLAGS) -lrt $(HASHFLAG)  net_layer.c $(HASHLIB)


link_layer.o: link_layer.c
	gcc -c $(CFLAGS) $(HASHFLAG) link_layer.c $(HASHLIB)
        


tcp_layer.o: tcp_layer.c
	$(CC) -c $(CFLAGS) $(TCP_FLAG) $(HASHFLAG) tcp_layer.c $(HASHLIB)      

tcp_util.o: tcp_util.c
	$(CC) -c $(CFLAGS) $(TCP_FLAG) $(HASHFLAG) tcp_util.c $(HASHLIB)
        
sliding_window.o: sliding_window.c 
	$(CC) -c $(CFLAGS) $(TCP_FLAG) $(HASHFLAG) sliding_window.c $(HASHLIB)  

state_machine.o: state_machine.c 
	$(CC) -c $(CFLAGS) $(TCP_FLAG) $(HASHFLAG) state_machine.c $(HASHLIB)
	
logger.o: logger.c 
	$(CC) -c $(CFLAGS) $(TCP_FLAG) $(HASHFLAG) logger.c $(HASHLIB)





node: node.c $(OBJ) state_machine.o logger.o sliding_window.o link_layer.o net_layer.o tcp_util.o tcp_layer.o port_util.o
	gcc $(CFLAGS) $(TCP_FLAG) -lreadline $(OBJ) port_util.o state_machine.o logger.o sliding_window.o link_layer.o net_layer.o tcp_util.o tcp_layer.o $(HASHFLAG) node.c -o node $(LDFLAGS) $(HASHLIB)


tcp_node: tcp_node.c $(OBJ) port_util.o link_layer.o net_layer.o tcp_util.o tcp_layer.o
	gcc $(CFLAGS) $(TCP_FLAG) $(OBJ) port_util.o link_layer.o net_layer.o tcp_util.o tcp_layer.o $(HASHFLAG) tcp_node.c -o tcp_node $(LDFLAGS) $(HASHLIB)



#----------------------------------


tcp: $(OBJ) test_tcp.c  net_layer.o tcp_util.o tcp_layer.o link_layer.o 
	gcc $(CFLAGS) $(TCP_FLAG) $(OBJ) link_layer.o net_layer.o tcp_util.o tcp_layer.o $(HASHFLAG) test_tcp.c $(HASHLIB)    


#-----------------
clean:
	rm util/*.o *.o node tcp_node *~ util/*~
