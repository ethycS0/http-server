#include "server.h"
#include "log.h"

#include "queue.h"
#include "t_pool.h"

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

queue_t request_queue;

void *handle_requests(void *args) {
        thread_args_t *thread_args = (thread_args_t *)args;

        while (true) {
                int *socket_ptr = (int *)dequeue(&request_queue);
                int client_socket = *socket_ptr;
                free(socket_ptr);
                atomic_store(&thread_args->t_state.state, WORKER_BUSY);

                char *read_buffer = (char *)malloc(BUFFER_SIZE);
                if (recv(client_socket, read_buffer, BUFFER_SIZE, 0) < 0) {
                        ERR("Failed to read data.");
                        close(client_socket);
                        continue;
                }

                LOG("Received data from client");

                char response[256];
                snprintf(response, sizeof(response),
                         "HTTP/1.1 200 OK\nContent-Type: text/html\nContent-Length: %zu \n\n %s ", strlen(hello_world),
                         hello_world);

                if (send(client_socket, response, sizeof(response), 0) != sizeof(response)) {
                        ERR("Error Responding to client.");
                } else {
                        LOG("Response sent.");
                }

                sleep(10);
                LOG("Thread Work done.");

                free(read_buffer);
                close(client_socket);
                atomic_store(&thread_args->t_state.state, WORKER_IDLE);
        }
}

int init_server(char *ip_address, int port) {
        if (server.init == true) {
                ERR("Server already Initialized.");
                return -1;
        }

        init_queue(&request_queue);
        init_workers(&handle_requests, NULL);

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
        int opt = 1;
        if (setsockopt(server.socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
                ERR("Failed to set SO_REUSEADDR option: %d", errno);
                close(server.socket);
                return -1;
        }

        if (bind(server.socket, (struct sockaddr *)&server.sock_addr, server.len_sockaddr) < 0) {
                ERR("Socket Binding Failure: %d", errno);
                return -1;
        }

        server.init = true;
        LOG("Server Started.");

        return 0;
}

void deinit_server() {
        if (server.init == true) {
                close(server.socket);
        }

        server.init = false;
        LOG("Server Stopped.");
}

int server_spin_some() {
        struct sockaddr client_addr;
        socklen_t client_len = sizeof(client_addr);

        int *newsocket = malloc(sizeof(int));
        if (newsocket == NULL) {
                ERR("Failed to allocate memory for socket: %d", errno);
                return -1;
        }

        *newsocket = accept(server.socket, &client_addr, &client_len);

        if (*newsocket < 0) {
                ERR("Failed to accept connection: %d", errno);
                free(newsocket);
                return -1;
        }

        if (enqueue(&request_queue, newsocket) == -1) {
                ERR("Failed to Queue data");
                return -1;
        }

        LOG("Got Connection: %d. Current Free Threads: %d/%d", *newsocket, get_current_free_threads(),
            get_current_threads());

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
