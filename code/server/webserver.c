#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <termios.h>
#include <wiringPi.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define MAX_TIMINGS 85
#define THRESOLD    40
#define DHT_PIN     4
#define LED_PIN     17
#define MOT_PIN     18

int data[5] = { 0, 0, 0, 0, 0 };

/* 스레드에서 사용하는 뮤텍스 */
static pthread_mutex_t pressure_lock;
static pthread_mutex_t temperature_lock;

static int is_run = 1; 				/* 스레드 종료를 위한 플래그 */
int pressure_fd, temperature_fd;

int kbhit(void);

/* 스레드 처리를 위한 함수 */
void* webserverFunction(void* arg);
static void *clnt_connection(void *arg);
int sendData(FILE* fp, char *ct, char *filename);
void sendOk(FILE* fp);
void sendError(FILE* fp);

void getTemperature(double *temperature, double *humidity); /* 온도와 습도를 위한 함수 */
void getTemp();

int main(int argc, char **argv)
{
    int i = 0;
    pthread_t ptPressure, ptTemperature, ptJoystick, ptWebserver;
    pthread_mutex_init(&pressure_lock, NULL); 		/* 뮤텍스 초기화 */
    pthread_mutex_init(&temperature_lock, NULL);

    /* 프로그램을 시작할 때 서버를 위한 포트 번호를 입력받는다. */
    if(argc!=2) {
        printf("usage: %s <port>\n", argv[0]);
        return -1;
    }

    
    if ( wiringPiSetupGpio() == -1 )    //prefer to BCM pin number system. 
        exit( 1 );

    pinMode(LED_PIN, OUTPUT); // led init
    pinMode(MOT_PIN, OUTPUT); // motor init

    pthread_create(&ptWebserver, NULL, webserverFunction, (void*)(atoi(argv[1])));

    printf("q : Quit\n");
    for(i = 0; is_run;i++) {
        if(kbhit()) { 			/* 키보드가 눌렸는지 확인한다. */
            switch(getchar()) { 	/* 문자를 읽는다. */
	        case 'q': 		/* 읽은 문자가 q이면 종료한다. */
		    pthread_kill(ptWebserver, SIGINT);
		    pthread_cancel(ptWebserver);
		    is_run = 0;
		    goto END;
            };
        }

    //    printf("%20d\t\t\r", i); 	/* 현재 카운트한 숫자를 출력한다. */
        delay(100); 			/* 100밀리초 동안 쉰다. */
    }

END:
    printf("Good Bye!\n");

    pthread_mutex_destroy(&pressure_lock); 	/* 뮤텍스 삭제 */
    pthread_mutex_destroy(&temperature_lock);

    return 0;
}

/* 키보드 입력을 처리하기 위한 함수 */
int kbhit(void)
{
    struct termios oldt, newt; 		/* 터미널에 대한 구조체 */
    int ch, oldf;

    tcgetattr(0, &oldt); 		/* 현재 터미널에 설정된 정보를 가져온다. */
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO); 	/* 정규 모드 입력과 에코를 해제한다. */
    tcsetattr(0, TCSANOW, &newt); 	/* 새 값으로 터미널을 설정한다. */
    oldf = fcntl(0, F_GETFL, 0);
    fcntl(0, F_SETFL, oldf | O_NONBLOCK); 	/* 입력을 논블로킹 모드로 설정한다. */

    ch = getchar();

    tcsetattr(0, TCSANOW, &oldt); 	/* 기존의 값으로 터미널의 속성을 바로 적용한다. */
    fcntl(0, F_SETFL, oldf);
    if(ch != EOF) {
        ungetc(ch, stdin); 		/* 앞에서 읽은 위치로 이전으로 포인터를 돌려준다. */
        return 1;
    }

    return 0;
}


/* 온도와 습도를 가져오기 위한 함수 */
void getTemperature(double *temperature, double *humidity)
{
    uint8_t laststate       = HIGH;
    uint8_t counter         = 0;  
    uint8_t j               = 0, i;

    data[0] = data[1] = data[2] = data[3] = data[4] = 0; 

    pinMode( DHT_PIN, OUTPUT );     // INPUT or OUTPUT
    digitalWrite( DHT_PIN, LOW );   // LOW or HIGH
    delay( 18 );    // unit is msec
    pinMode( DHT_PIN, INPUT );      // INPUT or OUTPUT

    for ( i = 0; i < MAX_TIMINGS; i++ )
    {
            counter = 0;
            while ( digitalRead( DHT_PIN ) == laststate ) 
            {
                    counter++;
                    delayMicroseconds( 1 ); 
                    if ( counter == 255 )  
                    {
                            break;          // timeout !! something must be wrong !!
                    }
            }
            laststate = digitalRead( DHT_PIN );     // update laststate

            if ( counter == 255 )
                    break;  // timeout !! quit !!

            if ( (i >= 4) && (i % 2 == 0) )
            {
                    data[j / 8] <<= 1     ;                 // MSB come first !
                    if ( counter > THRESOLD )       // 27us == 0,  70us == 1
                            data[j / 8] |= 1  ;             // it is '1'
                    j++;
            }
    }

    printf("> %d %d %d %d %d %d\n", j, data[0], data[1], data[2], data[3], data[4]  );

    if ( (j >= 40) && (data[4] == ( (data[0] + data[1] + data[2] + data[3]) & 0xFF) ) )
    {
        printf( "Humidity = %d.%d %% Temperature = %d.%d C\n", data[0], data[1], data[2], data[3]);
    }else  {
        printf( "Data not good, skip\n" );
    }

    *humidity = data[0] + data[1] * 0.1f;
    *temperature = data[2] + data[3] * 0.1f;


}

void* webserverFunction(void* arg)
{
    int ssock;
    pthread_t thread;
    struct sockaddr_in servaddr, cliaddr;
    unsigned int len;
    int port = (int)(arg);

    /* 서버를 위한 소켓을 생성한다. */
    ssock = socket(AF_INET, SOCK_STREAM, 0);
    if(ssock == -1) {
        perror("socket()");
        exit(1);
    }

    /* 입력받는 포트 번호를 이용해서 서비스를 운영체제에 등록한다. */
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);

    int opt = 1;
    setsockopt(ssock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if(bind(ssock, (struct sockaddr *)&servaddr, sizeof(servaddr))==-1) {
        perror("bind()");
        exit(1);
    }
    
    /* 최대 10대의 클라이언트의 동시 접속을 처리할 수 있도록 큐를 생성한다. */
    if(listen(ssock, 10) == -1) {
        perror("listen()");
        exit(1);
    }

    while(is_run) {
        char mesg[BUFSIZ];
        int csock;

        /* 클라이언트의 요청을 기다린다. */
        len = sizeof(cliaddr);
        csock = accept(ssock, (struct sockaddr*)&cliaddr, &len);

        /* 네트워크 주소를 문자열로 변경 */
        inet_ntop(AF_INET, &cliaddr.sin_addr, mesg, BUFSIZ);
        printf("Client IP : %s:%d\n", mesg, ntohs(cliaddr.sin_port));

        /* 클라이언트의 요청이 들어오면 스레드를 생성하고 클라이언트의 요청을 처리한다. */
        pthread_create(&thread, NULL, clnt_connection, &csock);
    //    pthread_join(thread, NULL);
    }

    return 0;
}
    
void *clnt_connection(void *arg)
{
    /* 스레드를 통해서 넘어온 arg를 int 형의 파일 디스크립터로 변환한다. */
    int csock = *((int*)arg);
    FILE *clnt_read, *clnt_write;
    char reg_line[BUFSIZ], reg_buf[BUFSIZ];
    char method[BUFSIZ], type[BUFSIZ];
    char filename[BUFSIZ], *ret;

    /* 파일 디스크립터를 FILE 스트림으로 변환한다. */
    clnt_read = fdopen(csock, "r");
    clnt_write = fdopen(dup(csock), "w");

    /* 한 줄의 문자열을 읽어서 reg_line 변수에 저장한다. */
    fgets(reg_line, BUFSIZ, clnt_read);
    
    /* reg_line 변수에 문자열을 화면에 출력한다. */
    fputs(reg_line, stdout);

    /* ' ' 문자로 reg_line을 구분해서 요청 라인의 내용(메소드)를 분리한다. */
    ret = strtok(reg_line, "/ ");
    strcpy(method, (ret != NULL)?ret:"");
    if(strcmp(method, "POST") == 0) { 		/* POST 메소드일 경우를 처리한다. */
        sendOk(clnt_write); 			/* 단순히 OK 메시지를 클라이언트로 보낸다. */
        goto END;
    } else if(strcmp(method, "GET") != 0) {	/* GET 메소드가 아닐 경우를 처리한다. */
        sendError(clnt_write); 			/* 에러 메시지를 클라이언트로 보낸다. */
        goto END;
    }

    ret = strtok(NULL, " "); 			/* 요청 라인에서 경로(path)를 가져온다. */
    strcpy(filename, (ret != NULL)?ret:"");
    if(filename[0] == '/') { 			/* 경로가 '/'로 시작될 경우 /를 제거한다. */
        for(int i = 0, j = 0; i < BUFSIZ; i++, j++) {
            if(filename[0] == '/') j++;
            filename[i] = filename[j];
            if(filename[j] == '\0') break;
        }
    }

    /* URL에 경로(path)이 없을 때 기본 처리 */
    if(!strlen(filename)) strcpy(filename, "index.html");

    /* 라즈베리 파이를 제어하기 위한 HTML 코드를 분석해서 처리한다. */
    if(strstr(filename, "?") != NULL) {
        char optLine[BUFSIZ];
        char optStr[32][BUFSIZ];
        char opt[BUFSIZ], var[BUFSIZ], *tok;
        int count = 0;

        ret = strtok(filename, "?");
        if(ret == NULL) goto END;
        strcpy(filename, ret);
        ret = strtok(NULL, "?");
        if(ret == NULL) goto END;
        strcpy(optLine, ret);

        /* 옵션을 분석한다. */
        tok = strtok(optLine, "&");
        while(tok != NULL) {
            strcpy(optStr[count++], tok);
            tok = strtok(NULL, "&");
        }
 
        /* 분석한 옵션을 처리한다. */
        for(int i = 0; i < count; i++) {
            strcpy(opt, strtok(optStr[i], "="));
            strcpy(var, strtok(NULL, "="));
            printf("%s = %s\n", opt, var);
            if(!strcmp(opt, "led") && !strcmp(var, "On")) { 		/* 8×8 LED 매트릭스를 켬 */
                digitalWrite(LED_PIN, HIGH); 	/* HIGH(1) 값을 출력: LED 켜기 */
                digitalWrite(MOT_PIN, HIGH); 	/* HIGH(1) 값을 출력: LED 켜기 */
            } else if(!strcmp(opt, "led") && !strcmp(var, "Off")) { 	/* 8×8 LED 매트릭스를 끔 */
                digitalWrite(LED_PIN, LOW); 	/* HIGH(1) 값을 출력: LED 끄기 */
                digitalWrite(MOT_PIN, LOW); 	/* HIGH(1) 값을 출력: LED 켜기 */
            }
        }
    }

    /* 메시지 헤더를 읽어서 화면에 출력하고 나머지는 무시한다. */
    do {
        fgets(reg_line, BUFSIZ, clnt_read);
        fputs(reg_line, stdout);
        strcpy(reg_buf, reg_line);
        char* buf = strchr(reg_buf, ':');
    } while(strncmp(reg_line, "\r\n", 2)); 	/* 요청 헤더는 ‘\r\n’으로 끝난다. */

    /* 파일의 이름을 이용해서 클라이언트로 파일의 내용을 보낸다. */
    sendData(clnt_write, type, filename);

END:
    fclose(clnt_read); 				/* 파일의 스트림을 닫는다. */
    fclose(clnt_write);
    pthread_exit(0); 				/* 스레드를 종료시킨다. */

    return (void*)NULL;
}
    
int sendData(FILE* fp, char *ct, char *filename)
{
    /* 클라이언트로 보낼 성공에 대한 응답 메시지 */
    char protocol[ ] = "HTTP/1.1 200 OK\r\n";
    char server[ ] = "Server:Netscape-Enterprise/6.0\r\n";
    char cnt_type[ ] = "Content-Type:text/html\r\n";
    char end[ ] = "\r\n"; 			/* 응답 헤더의 끝은 항상 \r\n */
    char html[BUFSIZ];
    double temperature, humidity;


    getTemperature(&temperature, &humidity);
    
    pinMode(LED_PIN, OUTPUT);
    sprintf(html, "<html><head><meta http-equiv=\"Content-Type\" " \
                  "content=\"text/html; charset=UTF-8\" />" \
                  "<title>Raspberry Pi Controller</title></head><body><table>" \
                  "<tr><td>Temperature</td><td colspan=2>" \
                  "<input readonly name=\"temperature\"value=%.2f></td></tr>" \
                  "<tr><td>Humidity</td><td colspan=2>" \
                  "<input readonly name=\"humidity\"value=%.2f></td></tr>" \
                  "</table>" \
                  "<form action=\"index.html\" method=\"GET\" "\
                  "onSubmit=\"document.reload()\"><table>" \
                  "<tr><td>8x8 LED Matrix</td><td>" \
                  "<input type=radio name=\"led\" value=\"On\" checked=checked>On</td>" \
                  "<td><input type=radio name=\"led\" value=\"Off\">Off</td>" \
                  "</tr><tr><td>Submit</td>" \
                  "<td colspan=2><input type=submit value=\"Submit\"></td></tr>" \
                  "</table></form></body></html>",
                  temperature, humidity);

    fputs(protocol, fp);
    fputs(server, fp);
    fputs(cnt_type, fp);
    fputs(end, fp);
    fputs(html, fp);
    fflush(fp);

    return 0;
}
    
void sendOk(FILE* fp)
{
    /* 클라이언트에 보낼 성공에 대한 HTTP 응답 메시지 */
    char protocol[ ] = "HTTP/1.1 200 OK\r\n";
    char server[ ] = "Server: Netscape-Enterprise/6.0\r\n\r\n";

    fputs(protocol, fp);
    fputs(server, fp);
    fflush(fp);
}
    
void sendError(FILE* fp)
{
    /* 클라이언트로 보낼 실패에 대한 HTTP 응답 메시지 */
    char protocol[ ] = "HTTP/1.1 400 Bad Request\r\n";
    char server[ ] = "Server: Netscape-Enterprise/6.0\r\n";
    char cnt_len[ ] = "Content-Length:1024\r\n";
    char cnt_type[ ] = "Content-Type:text/html\r\n\r\n";

    /* 화면에 표시될 HTML의 내용 */
    char content1[ ] = "<html><head><title>BAD Connection</title></head>";
    char content2[ ] = "<body><font size=+5>Bad Request</font></body></html>";
    printf("send_error\n");

    fputs(protocol, fp);
    fputs(server, fp);
    fputs(cnt_len, fp);
    fputs(cnt_type, fp);
    fputs(content1, fp);
    fputs(content2, fp);
    fflush(fp);
}
