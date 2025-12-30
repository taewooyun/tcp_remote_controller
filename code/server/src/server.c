#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <unistd.h>

#include "server.h"
#include "client_handler.h"

#define MAX_CLIENT 10

void start_server(int port)
{
    int server_fd, client_fd;
    struct sockaddr_in serv_addr, cli_addr;
    socklen_t cli_len = sizeof(cli_addr);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);

    bind(server_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    listen(server_fd, MAX_CLIENT);

    printf("[SERVER] listening on %d\n", port);

    while (1) {
        client_fd = accept(server_fd, (struct sockaddr *)&cli_addr, &cli_len);

        int *pclient = malloc(sizeof(int));
        *pclient = client_fd;

        pthread_t tid;
        pthread_create(&tid, NULL, client_thread, pclient);
        pthread_detach(tid);
    }
}
