//================================================================================================= 
//      Author: Brett Decker and Xinwei Liu
//=================================================================================================
//      INCLUDE FILES
//=================================================================================================

#include "utility.h"
#include "port_util.h"

#include "net_layer.h"
#include "tcp_layer.h"


#include <glib.h>

#include <limits.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <readline/readline.h>
#include <readline/history.h>


//=================================================================================================
//      DEFINITIONS AND MACROS
//=================================================================================================

#define MIN_ARGUMENTS           2

#define USER_MAX_CMD_SIZE       15

#define FILE_BUF_SIZE	1024

#define SHUTDOWN_READ	2
#define SHUTDOWN_WRITE	1
#define SHUTDOWN_BOTH	3


//=================================================================================================
//      GLOBAL VARIABLES
//=================================================================================================

static bool g_quit = false;


//=================================================================================================
//      PRIVATE STRUCTURES
//=================================================================================================

struct sendrecvfile_arg {
    int vsocket;
    int fd;
};


//=================================================================================================
//      PRIVATE FUNCTIONS
//=================================================================================================

int v_write_all(int sock, const void *buf, size_t bytes_requested)
{
    int ret;
    size_t bytes_written;

    bytes_written = 0;
    while (bytes_written < bytes_requested) {
        ret = v_write(sock, buf + bytes_written, bytes_requested - bytes_written);
        
        if (ret == -EAGAIN) {
            continue;
        }
        
        if (ret < 0) {
            return ret;
        }
        
        if (ret == 0) {
            fprintf(stderr, "warning: v_write() returned 0 before all bytes written\n");
            return bytes_written;
        }
        
        bytes_written += ret;
        
    } // end while()

    return bytes_written;
}


int v_read_all(int sock, void *buf, size_t bytes_requested)
{
    int ret;
    size_t bytes_read;

    bytes_read = 0;
    while (bytes_read < bytes_requested) {
    
        ret = v_read(sock, buf + bytes_read, bytes_requested - bytes_read);
        if (ret == -EAGAIN){
            continue;
        }
        
        if (ret < 0) {
            return ret;
        }
        
        if (ret == 0) {
            fprintf(stderr, "warning: v_read() returned 0 before all bytes read\n");
            return bytes_read;
        }
        
        bytes_read += ret;
        
    } // end while()

    return bytes_read;
}


void help_cmd(const char *line)
{
    (void)line;

    printf("- help: Print this list of commands.\n"
           "- interfaces: Print information about each interface, one per line.\n"
           "- routes: Print information about the route to each known destination, one per line.\n"
           "- sockets: List all sockets, along with the state the TCP connection associated with them is in, and their current window sizes.\n"
           "- down [integer]: Bring an interface \"down\".\n"
           "- up [integer]: Bring an interface \"up\" (it must be an existing interface, probably one you brought down)\n"
           "- accept [port]: Spawn a socket, bind it to the given port, and start accepting connections on that port.\n"
           "- connect [ip] [port]: Attempt to connect to the given ip address, in dot notation, on the given port.  send [socket] [data]: Send a string on a socket.\n"
           "- recv [socket] [numbytes] [y/n]: Try to read data from a given socket. If the last argument is y, then you should block until numbytes is received, or the connection closes. If n, then don.t block; return whatever recv returns. Default is n.\n"
           "- sendfile [filename] [ip] [port]: Connect to the given ip and port, send the entirety of the specified file, and close the connection.\n"
           "- recvfile [filename] [port]: Listen for a connection on the given port. Once established, write everything you can read from the socket to the given file. Once the other side closes the connection, close the connection as well.\n"
           "- shutdown [socket] [read/write/both]: v_shutdown on the given socket. If read is given, close only the reading side. If write is given, close only the writing side. If both is given, close both sides. Default is write.\n"
           "- close [socket]: v_close on the given socket.\n"
           "- window [socket]: display the send/recv windows.\n"
           "- rwin [socket]: display the contents of the recv window.\n"
           "- quit: exit the program.\n");

    return;
}


void interfaces_cmd(const char *line)
{
    (void)line;

    // Print interfaces
    net_printNetworkInterfaces();
    
    return;
}


void routes_cmd(const char *line)
{
    (void)line;

    // Print routes
    net_printNetworkRoutes();

    return;
}


void sockets_cmd(const char *line)
{
    (void)line;
    
    // Print sockets
    tcp_printSockets();

    return;
}


void down_cmd(const char *line)
{
    unsigned interface;
    int ret;

    ret = sscanf(line, "down %u", &interface);
    if (ret != 1) {
        fprintf(stderr, "syntax error (usage: down [interface])\n");
        return;
    }

    net_downInterface(interface);

    return;
}


void up_cmd(const char *line)
{
    unsigned interface;
    int ret;

    ret = sscanf(line, "up %u", &interface);
    if (ret != 1){
        fprintf(stderr, "syntax error (usage: up [interface])\n");
        return;
    }

    net_upInterface(interface);
    
    return;
}


void *accept_thread_func(void *arg)
{
    int sock;
    int ret;

    sock = (int)arg;

    while (1) {
        ret = v_accept(sock, NULL);
        if (ret < 0) {
            fprintf(stderr, "v_accept() error on socket %d: %s\n", sock, strerror(-ret));
            return NULL;
        }
        
        printf("v_accept() on socket %d returned %d\n", sock, ret);
        
    } // end while()

    return NULL;
}


void accept_cmd(const char *line)
{
    uint16_t port;
    int ret;
    struct in_addr any_addr;
    int s;
    pthread_t accept_thr;
    pthread_attr_t thread_attr;

    ret = sscanf(line, "accept %" SCNu16, &port);
    if (ret != 1) {
        ret = sscanf(line, "a %" SCNu16, &port);
        if (ret != 1) {
            fprintf(stderr, "syntax error (usage: accept [port])\n");
            return;
        }
    }

    s = v_socket();
    if (s < 0) {
        fprintf(stderr, "v_socket() error: %s\n", strerror(-s));
        return;
    }
    
    any_addr.s_addr = 0;
    ret = v_bind(s, &any_addr, port);//pass in host order!!!
    if (ret < 0) {
        fprintf(stderr, "v_bind() error: %s\n", strerror(-ret));
        return;
    }

    
    ret = v_listen(s);
    
 
    if (ret < 0) {
        fprintf(stderr, "v_listen() error: %s\n", strerror(-ret));
        return;
    }

 //   printf("inside accept cmd before spawning thread\n");    
 //   printAllSocketsState();


    ret = pthread_attr_init(&thread_attr);
    assert(ret == 0);
    ret = pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);
    assert(ret == 0);
    ret = pthread_create(&accept_thr, &thread_attr, accept_thread_func, (void *)s);
    if (ret != 0){
        fprintf(stderr, "pthread_create() error: %s\n", strerror(errno));
        return;
    }
    
    ret = pthread_attr_destroy(&thread_attr);
    assert(ret == 0);

    return;
}


void connect_cmd(const char *line)
{
    char ip_string[LINE_MAX];
    struct in_addr ip_addr;
    uint16_t port;
    int ret;
    int s;
  
    ret = sscanf(line, "connect %s %" SCNu16, ip_string, &port);
    if (ret != 2) {
        ret = sscanf(line, "c %s %" SCNu16, ip_string, &port);
        if (ret != 2) {
            fprintf(stderr, "syntax error (usage: connect [ip address] [port])\n");
            return;
        }
    }
    
    //ret = inet_aton(ip_string, &ip_addr);
    //if (ret == 0) {
    //    fprintf(stderr, "syntax error (malformed ip address)\n");
    //    return;
    //}
    
    ip_addr.s_addr = util_convertVIPString2Int(ip_string);

    s = v_socket();
    if (s < 0) {
        fprintf(stderr, "v_socket() error: %s\n", strerror(-s));
        return;
    }
    
    ret = v_connect(s, &ip_addr, port);
    if (ret < 0) {
        fprintf(stderr, "v_connect() error: %s\n", strerror(-ret));
        return;
    }
    printf("v_connect() returned %d\n", ret);

    return;
}


void send_cmd(const char *line)
{
    int num_consumed;
    int socket;
    const char *data;
    int ret;

    ret = sscanf(line, "send %d %n", &socket, &num_consumed);
    if (ret != 1) {
        ret = sscanf(line, "s %d %n", &socket, &num_consumed);
        if (ret != 1) {
            ret = sscanf(line, "w %d %n", &socket, &num_consumed);
            if (ret != 1) {
                fprintf(stderr, "syntax error (usage: send [interface] [payload])\n");
                return;
            }
        }
    } 
  
    data = line + num_consumed;
    if (strlen(data) < 2) { // 1-char message, plus newline
        fprintf(stderr, "syntax error (payload unspecified)\n");
        return;
    }

    ret = v_write(socket, (const unsigned char*)data, strlen(data));
    if (ret < 0) {
        fprintf(stderr, "v_write() error: %s\n", strerror(-ret));
        fprintf(stderr, "val: %d\n", -ret);
        return;
    }
    printf("v_write() on %d bytes returned %d\n", (int)(strlen(data)), ret);

    return;
}


void recv_cmd(const char *line)
{
    int socket;
    size_t bytes_requested;
    int bytes_read;
    char should_loop;
    unsigned char *buffer;
    int ret;
  
    ret = sscanf(line, "recv %d %zu %c", &socket, &bytes_requested, &should_loop);
    if (ret != 3) {
        should_loop = 'n';
        ret = sscanf(line, "recv %d %zu", &socket, &bytes_requested);
        if (ret != 2) {
            ret = sscanf(line, "r %d %zu %c", &socket, &bytes_requested, &should_loop);
            if (ret != 3) {
                should_loop = 'n';
                ret = sscanf(line, "r %d %zu", &socket, &bytes_requested);
                if (ret != 2) {
                    fprintf(stderr, "syntax error (usage: recv [interface] [bytes to read] "
                                    "[loop? (y/n), optional])\n");
                    return;
                }
            }
        }
    }

    buffer = (unsigned char *)malloc(bytes_requested+1); // extra for null terminator
    assert(buffer);
    memset(buffer, '\0', bytes_requested+1);
  
    if (should_loop == 'y') {
        bytes_read = v_read_all(socket, buffer, bytes_requested);
    }
    else if (should_loop == 'n') {
        bytes_read = v_read(socket, buffer, bytes_requested);
    }
    else {
        fprintf(stderr, "syntax error (loop option must be 'y' or 'n')\n");
        goto cleanup;
    }

    if (bytes_read < 0) {
        fprintf(stderr, "v_read() error: %s\n", strerror(-bytes_read));
        goto cleanup;
    }
  
    buffer[bytes_read] = '\0';
    printf("v_read() on %zu bytes returned %d; contents of buffer: '%s'\n",
            bytes_requested, bytes_read, buffer);

    cleanup:
        free(buffer);
        return;
}


void *sendfile_thread_func(void *arg)
{
    int sock;
    int fd;
    int ret;
    struct sendrecvfile_arg *thread_arg;
    int bytes_read;
    char buf[FILE_BUF_SIZE];

    thread_arg = (struct sendrecvfile_arg *)arg;
    sock = thread_arg->vsocket;
    fd = thread_arg->fd;
    free(thread_arg);

    clock_t start_time = clock();
    while ((bytes_read = read(fd, buf, sizeof(buf))) != 0){
        if (bytes_read == -1){
            fprintf(stderr, "read() error: %s\n", strerror(errno));
            break;
        }

        ret = v_write_all(sock, buf, bytes_read);
        if (ret < 0){
            fprintf(stderr, "v_write() error: %s\n", strerror(-ret));
            break;
        }
        
        if (ret != bytes_read){
            break;
        }
    } // end while()
    

    ret = v_close(sock);
    if (ret < 0){
        fprintf(stderr, "v_close() error: %s\n", strerror(-ret));
    }
    printf("total time to send: %f\n", (double)(clock()-start_time)/CLOCKS_PER_SEC);
  
    ret = close(fd);
    if (ret == -1){
        fprintf(stderr, "close() error: %s\n", strerror(errno));
    }

    printf("sendfile on socket %d done\n", sock);
    return NULL;
}


void sendfile_cmd(const char *line)
{
    int ret;
    char filename[LINE_MAX];
    char ip_string[LINE_MAX];
    struct in_addr ip_addr;
    uint16_t port; 
    int sock;
    int fd;
    struct sendrecvfile_arg *thread_arg;
    pthread_t sendfile_thr;
    pthread_attr_t thread_attr;

    ret = sscanf(line, "sendfile %s %s %" SCNu16, filename, ip_string, &port);
    if (ret != 3){
        fprintf(stderr, "syntax error (usage: sendfile [filename] [ip address]"
                        "[port])\n");
        return;
    }
  
    //ret = inet_aton(ip_string, &ip_addr);
    //if (ret == 0){
    //    fprintf(stderr, "syntax error (malformed ip address)\n");
    //    return;
    //}
    
    ip_addr.s_addr = util_convertVIPString2Int(ip_string);

    sock = v_socket();
    if (sock < 0) {
        fprintf(stderr, "v_socket() error: %s\n", strerror(-sock));
        return;
    }
  
    ret = v_connect(sock, &ip_addr, port);
    if (ret < 0) {
        fprintf(stderr, "v_connect() error: %s\n", strerror(-ret));
        return;
    }
  
    fd = open(filename, O_RDONLY);
    if (fd == -1){
        fprintf(stderr, "open() error: %s\n", strerror(errno));
    }
  
    thread_arg = (struct sendrecvfile_arg *)malloc(sizeof(struct sendrecvfile_arg));
    assert(thread_arg);
  
    thread_arg->vsocket = sock;
    thread_arg->fd = fd;
    ret = pthread_attr_init(&thread_attr);
    assert(ret == 0);
  
    ret = pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);
    assert(ret == 0);
  
    ret = pthread_create(&sendfile_thr, &thread_attr, sendfile_thread_func, thread_arg);
    if (ret != 0){
        fprintf(stderr, "pthread_create() error: %s\n", strerror(errno));
        return;
    }
  
    ret = pthread_attr_destroy(&thread_attr);
    assert(ret == 0);

    return;
}


void *recvfile_thread_func(void *arg)
{
    int sock;
    int socket_data;
    int fd;
    int ret;
    struct sendrecvfile_arg *thread_arg;
    int bytes_read;
    unsigned char buf[FILE_BUF_SIZE];

    thread_arg = (struct sendrecvfile_arg *)arg;
    sock = thread_arg->vsocket;
    fd = thread_arg->fd;
    free(thread_arg);

    socket_data = v_accept(sock, NULL);
    if (socket_data < 0){
        fprintf(stderr, "v_accept() error: %s\n", strerror(-socket_data));
        return NULL;
    }
    
    ret = v_close(sock);
    if (ret < 0){
        fprintf(stderr, "v_close() error: %s\n", strerror(-ret));
        return NULL;
    }

    while ((bytes_read = v_read(socket_data, buf, FILE_BUF_SIZE)) != 0) {
    
        if (bytes_read < 0){
            fprintf(stderr, "v_read() error: %s\n", strerror(-bytes_read));
            break;
        }
    
        ret = write(fd, buf, bytes_read);
        if (ret < 0) {
            fprintf(stderr, "write() error: %s\n", strerror(errno));
            break;
        }
    } // end while()

    ret = v_close(socket_data);
    if (ret < 0){
        fprintf(stderr, "v_close() error: %s\n", strerror(-ret));
    }
  
    ret = close(fd);
    printf("close(fd) ret: %d\n", ret);
    if (ret == -1) {
        fprintf(stderr, "close() error: %s\n", strerror(errno));
    }

    printf("recvfile on socket %d done\n", socket_data);
    return NULL;
}


void recvfile_cmd(const char *line)
{
    int ret;
    char filename[LINE_MAX];
    uint16_t port;
    int sock;
    struct in_addr any_addr;
    pthread_t recvfile_thr;
    pthread_attr_t thread_attr;
    struct sendrecvfile_arg *thread_arg;
    int fd;

    ret = sscanf(line, "recvfile %s %" SCNu16, filename, &port);
    if (ret != 2) {
        fprintf(stderr, "syntax error (usage: recvfile [filename] [port])\n");
        return;
    }

    sock = v_socket();
    if (sock < 0) {
        fprintf(stderr, "v_socket() error: %s\n", strerror(-sock));
        return;
    }
    
    any_addr.s_addr = 0;
    
    ret = v_bind(sock, &any_addr, port);
    if (ret < 0){
        fprintf(stderr, "v_bind() error: %s\n", strerror(-ret));
        return;
    }
    
    ret = v_listen(sock);
    if (ret < 0) {
        fprintf(stderr, "v_listen() error: %s\n", strerror(-ret));
        return;
    }
    
    fd = open(filename, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
    if (fd == -1){
        fprintf(stderr, "open() error: %s\n", strerror(errno));
    }
    
    thread_arg = (struct sendrecvfile_arg *)malloc(sizeof(struct sendrecvfile_arg));
    assert(thread_arg);
    thread_arg->vsocket = sock;
    thread_arg->fd = fd;
    
    ret = pthread_attr_init(&thread_attr);
    assert(ret == 0);
    
    ret = pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);
    assert(ret == 0);
    
    ret = pthread_create(&recvfile_thr, &thread_attr, recvfile_thread_func, thread_arg);
    if (ret != 0) {
        fprintf(stderr, "pthread_create() error: %s\n", strerror(errno));
        return;
    }
    
    ret = pthread_attr_destroy(&thread_attr);
    assert(ret == 0);

    return;
}


void window_cmd(const char *line)
{
    int ret;
    int socket;

    ret = sscanf(line, "window %d", &socket);
    if (ret != 1){
        fprintf(stderr, "syntax error (usage: window [socket])\n");
        return;
    }
    
    tcp_printSocketWindowsSizes(socket);

    return;
}


void shutdown_cmd(const char *line)
{
    char shut_type[LINE_MAX];
    int shut_type_int;
    int socket;
    int ret;

    ret = sscanf(line, "shutdown %d %s", &socket, shut_type);
    if (ret != 2) {
        fprintf(stderr, "syntax error (usage: shutdown [socket] [shutdown type])\n");
        return;
    }

    if (!strcmp(shut_type, "read")) {
        shut_type_int = SHUTDOWN_READ;
    }
    else if (!strcmp(shut_type, "write")) {
        shut_type_int = SHUTDOWN_WRITE;
    }
    else if (!strcmp(shut_type, "both")) {
        shut_type_int = SHUTDOWN_BOTH;
    }
    else {
        fprintf(stderr, "syntax error (type option must be 'read', 'write', or "
                    "'both')\n");
        return;
    }

    ret = v_shutdown(socket, shut_type_int);
    if (ret < 0) {
        fprintf(stderr, "v_shutdown() error: %s\n", strerror(-ret)); 
        return;
    }

    printf("v_shutdown() returned %d\n", ret);
    return;
}


void close_cmd(const char *line)
{
    int ret;
    int socket;

    ret = sscanf(line, "close %d", &socket);
    if (ret != 1){
        fprintf(stderr, "syntax error (usage: close [socket])\n");
        return;
    }

    ret = v_close(socket);
    if (ret < 0){
        fprintf(stderr, "v_close() error: %s\n", strerror(-ret));
        return;
    }

    printf("v_close() returned %d\n", ret);
    return;
}


void rwin_cmd(const char *line)
{
    int ret;
    int socket;

    ret = sscanf(line, "rwin %d", &socket);
    if (ret != 1){
        fprintf(stderr, "syntax error (usage: rwin [socket])\n");
        return;
    }
    
    tcp_printSocketRecvWin(socket, false);

    return;
}


void rwin_rr_cmd(const char *line)
{
    int ret;
    int socket;

    ret = sscanf(line, "rwin-rr %d", &socket);
    if (ret != 1){
        fprintf(stderr, "syntax error (usage: rwin-rr [socket])\n");
        return;
    }
    
    tcp_printSocketRecvWin(socket, true);

    return;
}



void quit_cmd(const char *line)
{
    (void)line;

    g_quit = true;

    return;
}


struct {
    const char *command;
    void (*handler)(const char *);
} cmd_table[] = {
  {"help", help_cmd},
  {"interfaces", interfaces_cmd},
  {"li", interfaces_cmd},
  {"routes", routes_cmd},
  {"lr", routes_cmd},
  {"sockets", sockets_cmd},
  {"ls", sockets_cmd},
  {"down", down_cmd},
  {"up", up_cmd},
  {"accept", accept_cmd},
  {"a", accept_cmd},
  {"connect", connect_cmd},
  {"c", connect_cmd},
  {"send", send_cmd},
  {"s", send_cmd},
  {"w", send_cmd},
  {"recv", recv_cmd},
  {"r", recv_cmd},
  {"sendfile", sendfile_cmd},
  {"recvfile", recvfile_cmd},
  {"window", window_cmd},
  {"shutdown", shutdown_cmd},
  {"close", close_cmd},
  {"rwin", rwin_cmd},
  {"rwin-rr", rwin_rr_cmd},
  {"quit", quit_cmd},
  {"q", quit_cmd}
};


void driver()
{
    char cmd[LINE_MAX];
    int ret;
    unsigned i;
    char* line;

    rl_bind_key('\t', rl_complete);

    while (g_quit == false) {
        line = readline("> ");

        if (!line) {
            break;
        }

        ret = sscanf(line, "%s", cmd);
        if (ret != 1) {
            fprintf(stderr, "syntax error (first argument must be a command)\n");
            continue;
        }

        for (i = 0; i < sizeof(cmd_table) / sizeof(cmd_table[0]); i++) {
            if (!strcmp(cmd, cmd_table[i].command)) {
                cmd_table[i].handler(line);
                break;
            }
        } // end for()

        if (i == sizeof(cmd_table) / sizeof(cmd_table[0])) {
            fprintf(stderr, "error: no valid command specified\n");
            continue;
        }
        
        add_history(line);
        free(line);
    }
    
    return;
}


void* handleInputThreadFunction(void* arg)
{
    // Handle Network Input
    int interface = 0;
    int ready_interfaces = 0;
    fd_set input_interfaces_copy;         // Stores read copy of g_input_interfaces

               
    while (!g_quit) {
    
        input_interfaces_copy = g_input_interfaces;
        
        // Only care about read file descriptors
        ready_interfaces = select(g_highest_input_interface+1, &input_interfaces_copy, NULL, NULL, NULL);
        if (ready_interfaces == ERROR) {
            perror("select");
            exit(-1);
        }
        
        for (interface = 0; interface < g_highest_input_interface+1; interface++) {
        
            if (FD_ISSET(interface, &input_interfaces_copy)) {
                net_handlePacketInput(interface);
            } // end if FD_ISSET()
        
        } // end for
    
    } // end while
    
    return NULL;
}


//=================================================================================================
//      MAIN FUNCTION
//=================================================================================================

int main(int argc, char** argv)
{
    pthread_t routing_threadID, input_threadID;
    
    // Executable: ./node file.lnx  [2 args, second is .lnx file]
    if (argc < MIN_ARGUMENTS) {
        fprintf(stderr, "ERROR: Not enough arguments passed.\nUsage: ./%s file.lnx\n", argv[0]);
        return -1; // Error
    }
    /*
    bqueue_t bq;
    
    bqueue_init(&bq);
    
    int* value = NULL;
    
    int i;
    for (i = 0; i < 10; i++) {
        bqueue_enqueue(&bq, (void*)&i);
    }
    for (i = 0; i < 10; i++) {
        bqueue_dequeue(&bq, (void**)&value);
        printf("value: %d\n", *value);
    }
    */

    // Clear file descriptor set g_input_interfaces
    FD_ZERO(&g_input_interfaces);
    
    util_netRegisterHandler(TEST_PROTOCOL, net_handleTestPacket);
    util_netRegisterHandler(RIP_PROTOCOL, net_handleRIPPacket);
    util_netRegisterHandler(TCP_PROTOCOL, tcp_handleTCPPacket);
    
    net_setupTables(argv[1]);
    
    ptu_initializeTables();
    
    tcp_initialSetUp();
    
    srand(time(NULL)); // Set seed for rand()
    
    // spawn Routing Update Thread
    pthread_create(&routing_threadID, NULL, net_routingThreadFunction, NULL);
    
    pthread_create(&input_threadID, NULL, handleInputThreadFunction, NULL);
    
    driver();
    
    // Free memory, destroy list of links
    net_freeTables();
    
    ptu_freeTables();

    return 0;
}
