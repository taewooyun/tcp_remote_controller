#ifndef COMMAND_QUEUE_H
#define COMMAND_QUEUE_H

#define BUF_SIZE 128

typedef struct device_cmd {
    int client_fd;
    char cmd[BUF_SIZE];
    struct device_cmd *next;
} device_cmd_t;

void enqueue_cmd(int client_fd, const char *cmd);
device_cmd_t *dequeue_cmd(void);

#endif
