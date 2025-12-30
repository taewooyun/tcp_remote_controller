#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <sys/socket.h>

#include "sensors.h"
#include "command_queue.h"
#include "device_worker.h"

static void control_device(const char *cmd)
{
    char clean[BUF_SIZE];
    strncpy(clean, cmd, BUF_SIZE);
    clean[strcspn(clean, "\r\n")] = 0;

    if (!strcmp(clean, "LED_ON")) led_on();
    else if (!strcmp(clean, "LED_OFF")) led_off();
    else if (!strncmp(clean, "BRIGHTNESS", 10)) {
        int level;
        if (sscanf(clean, "BRIGHTNESS %d", &level) == 1)
            led_brightness_set(level);
    }
    else if (!strcmp(clean, "BUZZER_ON")) buz_music_start();
    else if (!strcmp(clean, "BUZZER_OFF")) buz_music_stop();
    else if (!strcmp(clean, "SENSOR_ON")) cds_sensing_start();
    else if (!strcmp(clean, "SENSOR_OFF")) cds_sensing_stop();
    else if (!strcmp(clean, "SEGMENT_START")) fnd_count_start();
    else if (!strcmp(clean, "SEGMENT_STOP")) fnd_count_stop();

    printf("[DEVICE] execute: %s\n", clean);
}

static void *device_thread(void *arg)
{
    while (1) {
        device_cmd_t *cmd = dequeue_cmd();
        control_device(cmd->cmd);
        send(cmd->client_fd, "OK\n", 3, 0);
        free(cmd);
    }
    return NULL;
}

void start_device_worker(void)
{
    pthread_t tid;
    pthread_create(&tid, NULL, device_thread, NULL);
}
