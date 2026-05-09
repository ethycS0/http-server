#include "server.h"
#include "log.h"

#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

const int BUFFER_SIZE = 8192;
const int MAX_LISTENERS = 32;
const char *hello_world =
    "<!DOCTYPE html><html lang=\"en\"><body><h1> HOME </h1><p> Hello, World! :) </p></body></html>";

static server_t server;

int init_server(char *ip_address, int port) {
        if (server.init == true) {
                ERR("Server already Initialized.");
                return -1;
        }

        server.len_sockaddr = sizeof(server.sock_addr);

        struct in_addr ip;
        if (inet_pton(AF_INET, ip_address, &ip) != 1) {
                ERR("Invalid IP address.");
                return -1;
        }

        server.sock_addr.sin_family = AF_INET;
        server.sock_addr.sin_port = htons(port);
        server.sock_addr.sin_addr = ip;

        LOG("Initializing Server.");

        server.socket = socket(AF_INET, SOCK_STREAM, 0);
        if (server.socket < 0) {
                ERR("Socket Initialization Failure.");
                return -1;
        }

        if (bind(server.socket, (struct sockaddr *)&server.sock_addr, server.len_sockaddr) < 0) {
                ERR("Socket Binding Failure.");
                return -1;
        }

        server.init = true;
        LOG("Server Started.");

        return 0;
}

void deinit_server() {
        if (server.init == true) {
                close(server.socket);
                close(server.newsocket);
        }

        server.init = false;
        LOG("Server Stopped.");
}

int server_spin_some() {
        struct sockaddr client_addr;
        socklen_t client_len = sizeof(client_addr);

        server.newsocket = accept(server.socket, &client_addr, &client_len);
        if (server.newsocket < 0) {
                ERR("Failed to accept connection: %d", errno);
                return -1;
        }

        char *read_buffer = (char *)malloc(BUFFER_SIZE);

        if (recv(server.newsocket, read_buffer, BUFFER_SIZE, 0) < 0) {
                ERR("Failed to read data.");
                close(server.newsocket);
                return -1;
        }

        LOG("Received data from client");

        char response[256];
        snprintf(response, sizeof(response), "HTTP/1.1 200 OK\nContent-Type: text/html\nContent-Length: %zu \n\n %s ",
                 strlen(hello_world), hello_world);

        if (send(server.newsocket, response, sizeof(response), 0) != sizeof(response)) {
                ERR("Error Responding to client.");
        } else {
                LOG("Response sent.");
        }

        free(read_buffer);

        close(server.newsocket);

        return 0;
}

int start_listening() {
        if (server.init == false) {
                ERR("Server not initialized. Listening Failed.");
                return -1;
        }

        if (listen(server.socket, MAX_LISTENERS) < 0) {
                ERR("Socket Listening Failed.");
                return -1;
        }

        LOG("Server Listening: %s:%d \n", inet_ntoa(server.sock_addr.sin_addr), ntohs(server.sock_addr.sin_port));

        while (true) {
                LOG("Waiting for new connection...");
                server_spin_some();
        }

        return 0;
}
