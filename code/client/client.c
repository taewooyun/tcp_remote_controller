#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define TCP_PORT 5000
#define BUF_SIZE 1024

/* =========================
 * 라인 단위 수신 (TCP 필수)
 * ========================= */
int recv_line(int fd, char *buf, int max)
{
    int i = 0;
    char c;

    while (i < max - 1) {
        if (recv(fd, &c, 1, 0) <= 0)
            return -1;
        buf[i++] = c;
        if (c == '\n')
            break;
    }
    buf[i] = '\0';
    return i;
}

/* =========================
 * CAPTURE_DOWNLOAD 처리
 * ========================= */
void handle_capture_download_client(int sock)
{
    char buf[BUF_SIZE];

    /* 서버는 이미 LIST_BEGIN ~ LIST_END 를 보냄 */
    printf("\n==== Capture List ====\n");

    while (1) {
        if (recv_line(sock, buf, sizeof(buf)) <= 0)
            return;

        if (strcmp(buf, "LIST_END\n") == 0)
            break;

        printf("%s", buf);
    }

    /* 사용자 선택 */
    int index;
    printf("Select index: ");
    scanf("%d", &index);
    getchar(); // 개행 제거

     /* CANCEL 처리 */
    if (index == 0) {
        send(sock, "0\n", 2, 0);
        
        printf("Download canceled\n");
        return;
    }

    /* 범위 체크 */
    if (index < 1 || index > 10) {
        printf("Invalid index\n");
        send(sock, "0\n", 2, 0);
        return;
    }

    snprintf(buf, sizeof(buf), "GET %d\n", index-1); // 인덱스 맞추기
    send(sock, buf, strlen(buf), 0);

    /* FILE_BEGIN 수신 */
    if (recv_line(sock, buf, sizeof(buf)) <= 0)
        return;

    char filename[256];
    long filesize;

    if (sscanf(buf, "FILE_BEGIN %s %ld", filename, &filesize) != 2) {
        printf("Invalid FILE_BEGIN header\n");
        return;
    }

    printf("Downloading %s (%ld bytes)\n", filename, filesize);

    /* 파일 열기 */
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        perror("fopen");
        return;
    }

    long received = 0;
    int percent = -1;
    char data[BUF_SIZE];

    /* 파일 데이터 수신 */
    while (received < filesize) {
        long remain = filesize - received;
        ssize_t n = recv(sock, data,
                         remain > BUF_SIZE ? BUF_SIZE : remain,
                         0);
        if (n <= 0)
            break;

        fwrite(data, 1, n, fp);
        received += n;

        int p = (received * 100) / filesize;
        if (p != percent) {
            percent = p;
            printf("\rProgress: %d%%", percent);
            fflush(stdout);
        }
    }

    printf("\nDownload complete\n");
    fclose(fp);

    /* FILE_END 소비 */
    recv_line(sock, buf, sizeof(buf));
}

/* =========================
 * main
 * ========================= */
int main(int argc, char **argv)
{
    int sock;
    struct sockaddr_in servaddr;
    char mesg[BUF_SIZE];
    int input, param;

    if (argc < 2) {
        printf("Usage: %s <SERVER_IP>\n", argv[0]);
        return -1;
    }

    /* 소켓 생성 */
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return -1;
    }

    /* 서버 주소 설정 */
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(TCP_PORT);
    inet_pton(AF_INET, argv[1], &servaddr.sin_addr);

    /* 서버 접속 */
    if (connect(sock, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("connect");
        return -1;
    }

    while (1) {
        printf(
            "\n[ Device Control Menu ]\n"
            "1. LED ON\n"
            "2. LED OFF\n"
            "3. Set Brightness\n"
            "4. BUZZER ON\n"
            "5. BUZZER OFF\n"
            "6. CDS SENSOR ON\n"
            "7. CDS SENSOR OFF\n"
            "8. SEGMENT START\n"
            "9. SEGMENT STOP\n"
            "10. CAPTURE DOWNLOAD\n"
            "0. Exit\n"
            "Select: "
        );

        scanf("%d", &input);
        getchar(); // 개행 제거

        memset(mesg, 0, sizeof(mesg));

        switch (input) {
        case 0:
            snprintf(mesg, sizeof(mesg), "EXIT\n");
            send(sock, mesg, strlen(mesg), 0);
            close(sock);
            return 0;

        case 1: snprintf(mesg, sizeof(mesg), "LED_ON\n"); break;
        case 2: snprintf(mesg, sizeof(mesg), "LED_OFF\n"); break;

        case 3:
            printf("Brightness (0~100): ");
            scanf("%d", &param);
            getchar();
            snprintf(mesg, sizeof(mesg), "BRIGHTNESS %d\n", param);
            break;

        case 4: snprintf(mesg, sizeof(mesg), "BUZZER_ON\n"); break;
        case 5: snprintf(mesg, sizeof(mesg), "BUZZER_OFF\n"); break;
        case 6: snprintf(mesg, sizeof(mesg), "SENSOR_ON\n"); break;
        case 7: snprintf(mesg, sizeof(mesg), "SENSOR_OFF\n"); break;
        case 8: snprintf(mesg, sizeof(mesg), "SEGMENT_START\n"); break;
        case 9: snprintf(mesg, sizeof(mesg), "SEGMENT_STOP\n"); break;

        case 10:
            snprintf(mesg, sizeof(mesg), "CAPTURE_DOWNLOAD\n");
            send(sock, mesg, strlen(mesg), 0);
            handle_capture_download_client(sock);
            continue;   // 아래 recv() 안 탐

        default:
            printf("Invalid selection\n");
            continue;
        }

        /* 일반 제어 명령 전송 */
        send(sock, mesg, strlen(mesg), 0);

        /* OK 응답 수신 */
        memset(mesg, 0, sizeof(mesg));
        if (recv_line(sock, mesg, sizeof(mesg)) <= 0) {
            perror("recv");
            break;
        }

        printf("Server: %s", mesg);
    }

    close(sock);
    return 0;
}
