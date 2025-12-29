#include "./sensors/sensors.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <wiringPi.h>
#include <dirent.h>    // opendir, readdir, closedir
#include <sys/stat.h> // stat, struct stat
#include <sys/types.h>// DIR, off_t (플랫폼 안전)
#include <errno.h>    // errno (에러 처리용, 권장)
#include <dirent.h> // alphasort
#include <signal.h> // deamon
#include <fcntl.h>  // deamon
#include <sys/prctl.h>

#define SERVER_PORT 5000
#define MAX_CLIENT  10
#define BUF_SIZE    128

#define MAX_CAPTURE 10
char capture_list[MAX_CAPTURE][256];
int capture_count = 0;

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
    
        char resp[BUF_SIZE + 16];

        snprintf(resp, sizeof(resp),
                 "OK %s", cmd->cmd);

        send(cmd->client_fd, resp, strlen(resp), 0);

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

/* =========================
 * main
 * ========================= */

void handle_capture_download(int client_fd)
{
    char buf[BUF_SIZE];
    int n;

    /* =========================
     * 1. 이미지 목록 전송
     * ========================= */
    send_capture_list(client_fd);

    /* =========================
     * 2. 클라이언트 선택 대기
     * ========================= */
    n = recv(client_fd, buf, BUF_SIZE - 1, 0);
    if (n <= 0) {
        printf("[DOWNLOAD] client disconnected\n");
        return;
    }

    buf[n] = '\0';
    buf[strcspn(buf, "\r\n")] = 0;
    
    printf("[DOWNLOAD] recv: %s", buf);
    
    // cancel 처리
    if (strcmp(buf, "0") == 0) {
        printf("\n[DOWNLOAD] canceled by client\n");
        return;
    }
    
    /* =========================
     * 3. GET index 파싱
     * ========================= */
    int index;
    if (sscanf(buf, "GET %d", &index) != 1) {
        send(client_fd, "ERR INVALID_CMD\n", 16, 0);
        return;
    }

    char filename[256];
    if (get_capture_filename(index, filename, sizeof(filename)) < 0) {
        send(client_fd, "ERR INVALID_INDEX\n", 18, 0);
        return;
    }

    /* =========================
     * 4. 파일 전송
     * ========================= */
    send_file_with_progress(client_fd, filename);

    printf("[DOWNLOAD] completed: %s\n", filename);
}

int get_capture_filename(int index, char *out, int out_sz)
{
    if (index < 1 || index > capture_count)
        return -1;

    strncpy(out, capture_list[index - 1], out_sz);
    return 0;
}

void send_capture_list(int fd)
{
    struct dirent **namelist;
    int n = scandir("./data", &namelist, NULL, alphasort);
    if (n < 0) return;

    capture_count = 0;
    int idx = 1;

    for (int i = n - 1; i >= 0 && capture_count < MAX_CAPTURE; i--) {
        if (!strstr(namelist[i]->d_name, ".jpg"))
            continue;

        char path[256];
        struct stat st;
        snprintf(path, sizeof(path), "./data/%s", namelist[i]->d_name);
        if (stat(path, &st) == -1)
            continue;

        strncpy(capture_list[capture_count],
                namelist[i]->d_name,
                sizeof(capture_list[capture_count]));

        char line[256];
        snprintf(line, sizeof(line),
                 "%d %s [%ld bytes]\n",
                 idx, namelist[i]->d_name, st.st_size);

        send(fd, line, strlen(line), 0);

        capture_count++;
        idx++;
    }

    send(fd, "LIST_END\n", 9, 0);

    for (int i = 0; i < n; i++)
        free(namelist[i]);
    free(namelist);
}

void send_file_with_progress(int fd, const char *filename)
{
    char path[256];
    snprintf(path, sizeof(path), "./data/%s", filename);

    FILE *fp = fopen(path, "rb");
    if (!fp) {
        send(fd, "ERR FILE_OPEN\n", 14, 0);
        return;
    }

    struct stat st;
    stat(path, &st);

    long total = st.st_size;
    long sent = 0;

    char header[128];
    snprintf(header, sizeof(header),
             "FILE_BEGIN %s %ld\n", filename, total);
    send(fd, header, strlen(header), 0);

    char buf[1024];
    int n;

    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        send(fd, buf, n, 0);
        sent += n;

        int percent = (sent * 100) / total;
        // printf("[DOWNLOAD] %d%%\n", percent);
    }

    send(fd, "\nFILE_END\n", 10, 0);
    fclose(fp);
}

void daemonize(void)
{
    prctl(PR_SET_NAME, "tcp_server", 0, 0, 0); // 프로세스 이름 변경

    pid_t pid;

    /* 1. fork */
    pid = fork();
    if (pid < 0)
        exit(EXIT_FAILURE);
    if (pid > 0)
        exit(EXIT_SUCCESS);  // 부모 종료

    /* 2. 세션 리더 */
    if (setsid() < 0)
        exit(EXIT_FAILURE);

    /* SIGCHLD, SIGHUP 무시 */
    signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP, SIG_IGN);

    /* 3. 두 번째 fork */
    pid = fork();
    if (pid < 0)
        exit(EXIT_FAILURE);
    if (pid > 0)
        exit(EXIT_SUCCESS);

    /* 4. 권한 마스크 */
    umask(0);

    /* 5. 루트 디렉터리로 이동 */
    // chdir("/");

    /* 6. 표준 입출력 닫기 */
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    /* /dev/null 연결 */
    open("/dev/null", O_RDONLY);
    open("/dev/null", O_RDWR);
    open("/dev/null", O_RDWR);
}

void write_pid_file(void)
{
    FILE *fp = fopen("/var/run/tcp_server.pid", "w");
    if (!fp) return;

    fprintf(fp, "%d\n", getpid());
    fclose(fp);
}

int main(void)
{
    daemonize();
    write_pid_file();

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

    cam_capture_start(); // 1분 주기 카메라 촬영 시작

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
