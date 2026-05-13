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
const int MAX_LISTENERS = 4096;
const char *bigger_website =
    "<!DOCTYPE html>\n"
    "<html lang=\"en\">\n"
    "<head>\n"
    "    <meta charset=\"UTF-8\">\n"
    "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
    "    <title>C Web Server Dashboard</title>\n"
    "    <style>\n"
    "        body { font-family: system-ui, -apple-system, sans-serif; background-color: #1e1e2e; color: #cdd6f4; "
    "margin: 0; padding: 0; }\n"
    "        header { background-color: #11111b; border-bottom: 2px solid #89b4fa; padding: 2rem; text-align: center; "
    "}\n"
    "        header h1 { margin: 0; color: #89b4fa; }\n"
    "        main { max-width: 800px; margin: 3rem auto; background: #313244; padding: 2.5rem; border-radius: 12px; "
    "box-shadow: 0 10px 15px rgba(0,0,0,0.3); }\n"
    "        .stats { display: flex; gap: 1.5rem; margin-top: 2rem; }\n"
    "        .stat-card { flex: 1; background: #45475a; padding: 1.5rem; border-radius: 8px; text-align: center; "
    "transition: transform 0.2s; }\n"
    "        .stat-card:hover { transform: translateY(-5px); }\n"
    "        .stat-card h2 { margin: 0; font-size: 2.5rem; color: #a6e3a1; }\n"
    "        .stat-card p { margin: 0.5rem 0 0; font-weight: bold; color: #bac2de; text-transform: uppercase; "
    "letter-spacing: 1px; font-size: 0.85rem; }\n"
    "        footer { text-align: center; padding: 2rem; color: #6c7086; font-size: 0.9rem; }\n"
    "    </style>\n"
    "</head>\n"
    "<body>\n"
    "    <header>\n"
    "        <h1>\xE2\x9A\xA1 Blazing Fast C Server</h1>\n" // \xE2\x9A\xA1 is the UTF-8 hex for the ⚡ emoji
    "        <p>Serving traffic via Lock-Free Ring Buffers</p>\n"
    "    </header>\n"
    "    <main>\n"
    "        <h2 style=\"color: #f9e2af; margin-top: 0;\">System Status: Online</h2>\n"
    "        <p style=\"line-height: 1.6;\">Welcome to the dashboard. This page is currently being served directly "
    "from the server's RAM. The architecture utilizes a lock-free Multi-Producer Multi-Consumer (MPMC) queue, allowing "
    "for zero-mutex thread pool management and sub-millisecond response times.</p>\n"
    "        <div class=\"stats\">\n"
    "            <div class=\"stat-card\">\n"
    "                <h2>19.7k</h2>\n"
    "                <p>Peak RPS</p>\n"
    "            </div>\n"
    "            <div class=\"stat-card\">\n"
    "                <h2>< 8ms</h2>\n"
    "                <p>Max Latency</p>\n"
    "            </div>\n"
    "            <div class=\"stat-card\">\n"
    "                <h2>0</h2>\n"
    "                <p>Dropped Packets</p>\n"
    "            </div>\n"
    "        </div>\n"
    "    </main>\n"
    "    <footer>\n"
    "        Powered by raw C and Linux POSIX APIs.\n"
    "    </footer>\n"
    "</body>\n"
    "</html>";

static server_t server;
pthread_t scaling_thread;
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

                char response[8192];
                snprintf(response, sizeof(response),
                         "HTTP/1.1 200 OK\nContent-Type: text/html\nContent-Length: %zu \n\n %s ",
                         strlen(bigger_website), bigger_website);

                if (send(client_socket, response, sizeof(response), 0) != sizeof(response)) {
                        ERR("Error Responding to client.");
                }

                free(read_buffer);
                close(client_socket);
                atomic_store(&thread_args->t_state.state, WORKER_IDLE);
        }
}

void scale_server() {
        int total_threads = get_current_threads();
        int free_threads = get_current_free_threads();
        int queue_count = get_queue_count(&request_queue);

        LOG("Monitor - Total: %d, Free: %d, Queued: %d", total_threads, free_threads, queue_count);

        if (queue_count > 0 && free_threads == 0) {
                if (total_threads < MAX_WORKERS) {
                        int target_threads = total_threads + queue_count;
                        if (target_threads > MAX_WORKERS) {
                                target_threads = MAX_WORKERS;
                        }
                        scale_threads(target_threads);
                }

        } else if (queue_count == 0 && free_threads > MIN_WORKERS) {
                int target_threads = total_threads - 1;
                if (target_threads >= MIN_WORKERS) {
                        scale_threads(target_threads);
                }
        }
}

void *monitor_thread_spin(void *) {
        while (server.init) {
                sleep(1);
                scale_server();
        }
        return NULL;
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

        pthread_create(&scaling_thread, NULL, monitor_thread_spin, NULL);
        pthread_detach(scaling_thread);

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

        LOG("Server Listening: %s:%d", inet_ntoa(server.sock_addr.sin_addr), ntohs(server.sock_addr.sin_port));

        while (true) {
                server_spin_some();
        }

        return 0;
}
