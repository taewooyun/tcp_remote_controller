#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <pthread.h>

// volatile int cam_stop_flag = 0;
static volatile int cam_capture_running = 0;
static pthread_t cam_tid;

static void cam_capture()
{
    char cmd[256];
    char filename[128];

    while (1) {
        time_t now = time(NULL);
        struct tm *t = localtime(&now);

        strftime(filename, sizeof(filename),
                 "./data/%Y%m%d_%H%M%S.jpg", t);

        snprintf(cmd, sizeof(cmd),
                "libcamera-still -n -o %s "
                "> /dev/null 2>&1"
            , filename);

        int ret = system(cmd);

        if (ret == 0) {
            char cwd[4096];
            getcwd(cwd, sizeof(cwd));
            printf("[CAM] saved: %s/%s\n", cwd, filename + 2);
        } else {
            printf("[CAM][ERROR] capture failed (%d)\n", ret);
        }
        sleep(60);   // 10초 주기
    }
}



static void *cam_capture_thread(void *arg)
{
    cam_capture_running = 1;
    cam_capture();
    cam_capture_running = 0;

    return NULL;
}

void cam_capture_start()
{
    if(!cam_capture_running){
        pthread_create(&cam_tid, NULL, cam_capture_thread, NULL);
    }
}