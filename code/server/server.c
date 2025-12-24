#include "./sensors/sensors.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <wiringPi.h>

#define SERVER_PORT 5000
#define MAX_CLIENT  10
#define BUF_SIZE    128

/* =========================
 * 디바이스 명령 구조체
 * ========================= */
typedef struct device_cmd {
    int client_fd;
    char cmd[BUF_SIZE];
    struct device_cmd *next;
} device_cmd_t;

/* =========================
 * 명령 큐
 * ========================= */
device_cmd_t *cmd_head = NULL;
device_cmd_t *cmd_tail = NULL;

pthread_mutex_t cmd_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  cmd_cond  = PTHREAD_COND_INITIALIZER;


pthread_t buz_tid;
pthread_t cds_tid;
pthread_t fnd_tid;


/* =========================
 * 디바이스 제어 함수 (이미 구현했다고 가정)
 * ========================= */
void control_device(const char *cmd)
{
    char clean[BUF_SIZE];
    strncpy(clean, cmd, BUF_SIZE);
    clean[strcspn(clean, "\r\n")] = 0;   // 개행 제거

    if (strcmp(clean, "LED_ON") == 0) {
        led_on();
    }
    else if (strcmp(clean, "LED_OFF") == 0) {
        led_off();
    }
    else if (strncmp(clean, "BRIGHTNESS", 10) == 0) {
        int level;

        if (sscanf(clean, "BRIGHTNESS %d", &level) == 1) {
            led_brightness_set(level);
        }
    }
    else if (strcmp(clean, "BUZZER_ON") == 0) {
        buz_music_start();
    }
    else if (strcmp(clean, "BUZZER_OFF") == 0) {
        buz_music_stop();
    }
    else if (strcmp(clean, "SENSOR_ON") == 0) {
        cds_sensing_start();
    }
    else if (strcmp(clean, "SENSOR_OFF") == 0) {
        cds_sensing_stop();
    }
    else if (strcmp(clean, "SEGMENT_START") == 0) {
        fnd_count_start();
    }
    else if (strcmp(clean, "SEGMENT_STOP") == 0) {
        fnd_count_stop();
    }
    else if (strcmp(clean, "EXIT") == 0) {

    }
    
    printf("[DEVICE] execute: %s\n", clean);
}

/* =========================
 * 명령 큐 enqueue
 * ========================= */
void enqueue_cmd(int client_fd, const char *cmd)
{
    device_cmd_t *new_cmd = malloc(sizeof(device_cmd_t));
    new_cmd->client_fd = client_fd;
    strncpy(new_cmd->cmd, cmd, BUF_SIZE);
    new_cmd->next = NULL;

    pthread_mutex_lock(&cmd_mutex);

    if (cmd_tail == NULL) {
        cmd_head = cmd_tail = new_cmd;
    } else {
        cmd_tail->next = new_cmd;
        cmd_tail = new_cmd;
    }

    pthread_cond_signal(&cmd_cond);
    pthread_mutex_unlock(&cmd_mutex);
}

/* =========================
 * 명령 큐 dequeue
 * ========================= */
device_cmd_t *dequeue_cmd(void)
{
    pthread_mutex_lock(&cmd_mutex);

    while (cmd_head == NULL)
        pthread_cond_wait(&cmd_cond, &cmd_mutex);

    device_cmd_t *cmd = cmd_head;
    cmd_head = cmd_head->next;
    if (cmd_head == NULL)
        cmd_tail = NULL;

    pthread_mutex_unlock(&cmd_mutex);
    return cmd;
}

/* =========================
 * 디바이스 제어 스레드
 * ========================= */
void *device_thread(void *arg)
{   
    while (1) {
        device_cmd_t *cmd = dequeue_cmd();

        control_device(cmd->cmd);
        send(cmd->client_fd, "OK\n", 3, 0);

        free(cmd);
    }
    return NULL;
}

/* =========================
 * 클라이언트 처리 스레드
 * ========================= */
void *client_thread(void *arg)
{
    int client_fd = *(int *)arg;
    char buf[BUF_SIZE];
    int n;

    free(arg);

    while ((n = recv(client_fd, buf, BUF_SIZE - 1, 0)) > 0) {
        buf[n] = '\0';
        printf("[SERVER] recv: %s", buf);
        enqueue_cmd(client_fd, buf);
    }

    close(client_fd);
    printf("[SERVER] client disconnected\n");
    return NULL;
}

/* =========================
 * main
 * ========================= */

int main(void)
{
    wiringPiSetupGpio(); // BCM setting

    
    int server_fd, client_fd;
    struct sockaddr_in serv_addr, cli_addr;
    socklen_t cli_len = sizeof(cli_addr);

    pthread_t dev_tid;

    /* 디바이스 제어 스레드 생성 */
    pthread_create(&dev_tid, NULL, device_thread, NULL);

    /* 서버 소켓 생성 */
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        exit(1);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(SERVER_PORT);

    if (bind(server_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("bind");
        exit(1);
    }

    listen(server_fd, MAX_CLIENT);
    printf("[SERVER] listening on port %d\n", SERVER_PORT);

    /* 클라이언트 accept 루프 */
    while (1) {
        client_fd = accept(server_fd, (struct sockaddr *)&cli_addr, &cli_len);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }

        printf("[SERVER] client connected\n");

        int *pclient = malloc(sizeof(int));
        *pclient = client_fd;

        pthread_t tid;
        pthread_create(&tid, NULL, client_thread, pclient);
        pthread_detach(tid);
    }

    close(server_fd);
    return 0;
}
