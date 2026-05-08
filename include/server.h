#ifndef HTTP_SERVER
#define HTTP_SERVER

#include <arpa/inet.h>
#include <stdbool.h>
#include <sys/socket.h>

typedef struct server_t {
        bool init;
        int port;
        int socket;
        int newsocket;
        unsigned int len_sockaddr;
        struct sockaddr_in sock_addr;
} server_t;

int init_server(char *ip_address, int port);
void deinit_server();
int start_listening();

#endif // !HTTP_SERVER
