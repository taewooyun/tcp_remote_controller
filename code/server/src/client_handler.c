#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "client_handler.h"
#include "command_queue.h"
#include "file_transfer.h"

#define BUF_SIZE 128

void *client_thread(void *arg)
{
    int client_fd = *(int *)arg;
    free(arg);

    char buf[BUF_SIZE];
    int n;

    while ((n = recv(client_fd, buf, BUF_SIZE - 1, 0)) > 0) {
        buf[n] = '\0';

        if (strncmp(buf, "CAPTURE_DOWNLOAD", 16) == 0) {
            handle_capture_download(client_fd);
        } else {
            enqueue_cmd(client_fd, buf);
        }
    }

    close(client_fd);
    printf("[SERVER] client disconnected\n");
    return NULL;
}
