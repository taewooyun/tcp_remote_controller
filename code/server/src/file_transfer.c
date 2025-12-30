#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#include "file_transfer.h"

#define BUF_SIZE 128
#define MAX_CAPTURE 10
char capture_list[MAX_CAPTURE][256];
int capture_count = 0;

static void send_capture_list(int fd)
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

static int get_capture_filename(int index, char *out, int out_sz)
{
    if (index < 1 || index > capture_count)
        return -1;

    strncpy(out, capture_list[index - 1], out_sz);
    return 0;
}

static void send_file_with_progress(int fd, const char *filename)
{
    char path[256];
    snprintf(path, sizeof(path), "./data/%s", filename);

    FILE *fp = fopen(path, "rb");
    struct stat st;
    stat(path, &st);

    char header[128];
    snprintf(header, sizeof(header),
             "FILE_BEGIN %s %ld\n", filename, st.st_size);
    send(fd, header, strlen(header), 0);

    char buf[1024];
    int n;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0)
        send(fd, buf, n, 0);

    send(fd, "\nFILE_END\n", 10, 0);
    fclose(fp);
}

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