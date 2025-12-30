#include <stdlib.h>
#include <pthread.h>
#include <string.h>

#include "command_queue.h"

static device_cmd_t *cmd_head = NULL;
static device_cmd_t *cmd_tail = NULL;

static pthread_mutex_t cmd_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  cmd_cond  = PTHREAD_COND_INITIALIZER;

void enqueue_cmd(int client_fd, const char *cmd)
{
    device_cmd_t *new_cmd = malloc(sizeof(device_cmd_t));
    new_cmd->client_fd = client_fd;
    strncpy(new_cmd->cmd, cmd, BUF_SIZE);
    new_cmd->next = NULL;

    pthread_mutex_lock(&cmd_mutex);

    if (!cmd_tail)
        cmd_head = cmd_tail = new_cmd;
    else {
        cmd_tail->next = new_cmd;
        cmd_tail = new_cmd;
    }

    pthread_cond_signal(&cmd_cond);
    pthread_mutex_unlock(&cmd_mutex);
}

device_cmd_t *dequeue_cmd(void)
{
    pthread_mutex_lock(&cmd_mutex);

    while (!cmd_head)
        pthread_cond_wait(&cmd_cond, &cmd_mutex);

    device_cmd_t *cmd = cmd_head;
    cmd_head = cmd_head->next;
    if (!cmd_head)
        cmd_tail = NULL;

    pthread_mutex_unlock(&cmd_mutex);
    return cmd;
}
