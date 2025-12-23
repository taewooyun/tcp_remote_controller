#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define TCP_PORT 5000

int main(int argc, char **argv)
{
    int ssock;
    struct sockaddr_in servaddr;
    char mesg[BUFSIZ];

    if(argc < 2) {
        printf("Usage : %s IP_ADRESS\n", argv[0]);
        return -1;
    }

    /* 소켓을 생성 */
    if((ssock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket()");
        return -1;
    }

    /* 소켓이 접속할 주소 지정 */
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;

    /* 문자열을 네트워크 주소로 변경 */
    inet_pton(AF_INET, argv[1], &(servaddr.sin_addr.s_addr));
    servaddr.sin_port = htons(TCP_PORT);

    /* 지정한 주소로 접속 */
    if(connect(ssock, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("connect()");
        return -1;
    }

    int input;

    while (1)
    {
        printf(
            "[ Device Control Menu ]\n"
            "1. LED ON\n"
            "2. LED OFF\n"
            "3. Set Brightness\n"
            "4. BUZZER ON (play melody)\n"
            "5. BUZZER OFF (stop)\n"
            "6. SENSOR ON (감시 시작)\n"
            "7. SENSOR OFF (감시 종료)\n"
            "8. SEGMENT DISPLAY (숫자 표시 후 카운트다운)\n"
            "9. SEGMENT STOP (카운트다운 중단)\n"
            "0. Exit\n"
            "Select: "
        );

        scanf("%d", &input);
        getchar();  // scanf 뒤 개행 제거 (중요)

        memset(mesg, 0, BUFSIZ);

        switch (input)
        {
        case 0:
            strcpy(mesg, "EXIT");
            send(ssock, mesg, strlen(mesg), 0);
            close(ssock);
            return 0;

        case 1:
            strcpy(mesg, "LED_ON");
            break;

        case 2:
            strcpy(mesg, "LED_OFF");
            break;

        case 3:
            strcpy(mesg, "BRIGHTNESS");
            break;

        case 4:
            strcpy(mesg, "BUZZER_ON");
            break;

        case 5:
            strcpy(mesg, "BUZZER_OFF");
            break;

        case 6:
            strcpy(mesg, "SENSOR_ON");
            break;

        case 7:
            strcpy(mesg, "SENSOR_OFF");
            break;

        case 8:
            strcpy(mesg, "SEGMENT_START");
            break;

        case 9:
            strcpy(mesg, "SEGMENT_STOP");
            break;

        default:
            printf("잘못된 입력입니다.\n");
            continue;
        }

        /* 서버로 전송 */
        if (send(ssock, mesg, strlen(mesg), 0) <= 0) {
            perror("send()");
            return -1;
        }

        /* 서버 응답 수신 */
        memset(mesg, 0, BUFSIZ);
        if (recv(ssock, mesg, BUFSIZ, 0) <= 0) {
            perror("recv()");
            return -1;
        }

        printf("Received data : %s\n", mesg);
    }

    close(ssock); 					/* 소켓을 닫는다. */ 

    return 0;
}
