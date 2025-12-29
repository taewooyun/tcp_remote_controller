# 평가 기준
## 1. 멀티 프로세스 또는 스레드 이용한 장치 제어
### 1) 클라이언트 처리 스레드
```c
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
```
- 클라이언트 응답 개별 수행

### 2) 디바이스 제어 스레드
```c
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
```
- 디바이스 작동 개별 수행

### 3) 하드웨어 워커 스레드 구현
#### CDS 감지 스레드
#### BUZZER 음악 실행 스레드
#### FND 카운트 스레드
- 스레드 사용으로 실행 도중 종료 가능


## 2. 대상 장치 : LED/부저/조도센서/7세그먼트
### 1) LED
```c
void led_on()
{
    pinMode(LED, OUTPUT); 		
    digitalWrite(LED, LOW); 
}

void led_off()
{
    pinMode(LED, OUTPUT); 	
    digitalWrite(LED, HIGH);
}

void led_brightness_set(int level)
{
    pinMode(LED, OUTPUT); 		
    softPwmCreate(LED, 0, 100); 
    softPwmWrite(LED, 100 - level); 
}
```
- LED 켜기
- LED 끄기
- LED 밝기 설정

### 2) BUZZER
```c
static void buz_music_play(void)
{
    softToneCreate(BUZ);

    for (int i = 0; i < TOTAL; i++) {
        if (buz_stop_flag)
            break;

        if (notes[i] == REST)
            softToneWrite(BUZ, 0);
        else
            softToneWrite(BUZ, notes[i]);

        delay(beats[i]);
    }

    softToneWrite(BUZ, 0);
}

void buz_music_stop(void)
{
    if (buz_music_running) {
        buz_stop_flag = 1;
        pthread_join(buz_tid, NULL);
    }
}

int buz_beep(void)
{
    softToneCreate(BUZ);

    softToneWrite(BUZ, 1000);
    delay(150);

    softToneWrite(BUZ, 0);
    return 0;
}

```
- 음악 실행/종료
- 비프음 발생

### 3) CDS
```c
static void cds_sensing()
{
    cds_stop_flag = 0;

    pinMode(CDS, INPUT); 	
    pinMode(LED, OUTPUT); 	


    while(1) { 			
        if(cds_stop_flag) break;

        if(digitalRead(CDS) == LOW) { 	
            digitalWrite(LED, LOW); 	
        } 
        else {
            digitalWrite(LED, HIGH); 	
        }
        delay(500);
    }

    digitalWrite(LED, HIGH);
}

void cds_sensing_stop(){
     if (cds_count_running) {
        cds_stop_flag = 1;
        pthread_join(cds_tid, NULL);
    }
}
```
- 조도 센서 감지 시작
  - 빛의 감지에 따라 LED ON/OFF
- 조도 센서 감지 종료

### 4) FND
```c
static void fnd_count(){
    int i;

    for(i=9; 0<=i; --i){
        if(fnd_stop_flag) break;

        fnd_print(i);
        delay(1000);
    }

    if(!fnd_stop_flag) buz_beep();

    for (i = 0; i < 4; ++i) {
        digitalWrite(pins[i], HIGH);
    }
}

void fnd_count_stop(){
     if (fnd_count_running) {
        fnd_stop_flag = 1;
        pthread_join(fnd_tid, NULL);
    }
}
```
- 9 -> 0 까지 10초 동안 카운트 후 부저 비프음 발생
- 실행 중 종료 구현

## 3. 동적 라이브러리 구현
### CMakeLists.txt
```
set(SENSOR_SRCS
    code/server/sensors/led.c
    code/server/sensors/cds.c
    code/server/sensors/buzzer.c
    code/server/sensors/fnd.c
    code/server/sensors/camera.c
)
add_library(sensors SHARED
    ${SENSOR_SRCS}
)
target_include_directories(sensors PUBLIC
    ${CMAKE_SOURCE_DIR}/code/server/sensors
)
target_link_libraries(sensors
    wiringPi
    pthread
)
```
- sensor 작동 코드 동적 라이브러리 구현

## 4. 클라이언트 프로그램 강제 종료 방지, 시그널 처리(INT 허용)
```c
void sigint_handler(int signo)
{
    printf("\nSIGINT received. Client exiting safely...\n");

    if (g_sock != -1) {
        send(g_sock, "EXIT\n", 5, 0);
        close(g_sock);
    }

    exit(0);
}

int main()
{
    signal(SIGINT, sigint_handler);

    signal(SIGTERM, SIG_IGN);
    signal(SIGHUP, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    ...
}
```
- `SIGINT` : 소켓 종료 처리
- `SIGTERM`, `SIGHUP`, `SIGQUIT` : 강제 종료 방지 처리


## 5. 서버 프로세스 데몬화
```c
void daemonize(void)
{
    prctl(PR_SET_NAME, "tcp_server", 0, 0, 0); // 프로세스 이름 변경

    pid_t pid;

    pid = fork();
    if (pid < 0)
        exit(EXIT_FAILURE);
    if (pid > 0)
        exit(EXIT_SUCCESS);  // 부모 종료 : 데몬화

    if (setsid() < 0)
        exit(EXIT_FAILURE);

    signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP, SIG_IGN);

    pid = fork();
    if (pid < 0)
        exit(EXIT_FAILURE);
    if (pid > 0)
        exit(EXIT_SUCCESS);

    umask(0);

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    open("/dev/null", O_RDONLY);
    open("/dev/null", O_RDWR);
    open("/dev/null", O_RDWR);
}

int main(void)
{
    daemonize();
    ...
}
```
- 데몬 프로세스 구현

## 6. 빌드 자동화
### CMakeLists.txt
```
# 프로젝트 메타 정보
# =========================
cmake_minimum_required(VERSION 3.16)
project(tcp_remote_controller C)

# 언어 표준
# =========================
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 출력 디렉토리 설정
# =========================
file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/exec)
file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/exec/lib)
file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/exec/client)
file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/exec/server)
file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/exec/server/data)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/exec)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/exec/lib)

# 공유 라이브러리
# =========================
set(SENSOR_SRCS
    code/server/sensors/led.c
    code/server/sensors/cds.c
    code/server/sensors/buzzer.c
    code/server/sensors/fnd.c
    code/server/sensors/camera.c
)
add_library(sensors SHARED
    ${SENSOR_SRCS}
)
target_include_directories(sensors PUBLIC
    ${CMAKE_SOURCE_DIR}/code/server/sensors
)
target_link_libraries(sensors
    wiringPi
    pthread
)

# 실행파일_서버
# =========================
add_executable(server
    code/server/server.c
)
target_link_libraries(server
    sensors
    pthread
)
set_target_properties(server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/exec/server
    BUILD_RPATH "$ORIGIN/../lib"
)

# 실행파일_서버
# =========================
add_executable(client
    code/client/client.c
)
set_target_properties(client PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/exec/client
)

# 컴파일 옵션
# =========================
foreach(target sensors server client)
    target_compile_options(${target} PRIVATE
        -Wall
        -Wextra
    )
endforeach()

```
- CMake를 활용한 빌드 자동화

## 7. 추가 기능
### 감시 카메라 이미지 다운로드
```c
void handle_capture_download(int client_fd)
{
    char buf[BUF_SIZE];
    int n;

    send_capture_list(client_fd);

    n = recv(client_fd, buf, BUF_SIZE - 1, 0);
    if (n <= 0) {
        printf("[DOWNLOAD] client disconnected\n");
        return;
    }

    buf[n] = '\0';
    buf[strcspn(buf, "\r\n")] = 0;
    
    printf("[DOWNLOAD] recv: %s", buf);
    
    if (strcmp(buf, "0") == 0) {
        printf("\n[DOWNLOAD] canceled by client\n");
        return;
    }
    
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
    }

    send(fd, "\nFILE_END\n", 10, 0);
    fclose(fp);
}
```
### 동작 흐름

| SEQ | ACTOR |  |
| --- | --- | --- |
| 1 | server | 1분 주기로 카메라 촬영, 파일 저장 |
| 2 | client | 카메라 기능 호출 |
| 3 | server | 최근 10개의 이미지 목록 전달 |
| 4 | client | 이미지 선택 |
| 5 | server | 이미지 전송 |
| 6 | client | 파일 다운로드 완료 |