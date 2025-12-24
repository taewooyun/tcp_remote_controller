#include <wiringPi.h>
#include <stdio.h>
#include <pthread.h>

#include "sensors.h"

int pins[4] = {23, 18, 15, 14}; // BCM D, C, B, A

static volatile int fnd_stop_flag = 0;
static volatile int fnd_count_running = 0;
static pthread_t fnd_tid;

static void fnd_print(int num)
{
    int i;

    // 핀 출력 설정
    for (i = 0; i < 4; ++i) {
        pinMode(pins[i], OUTPUT);
    }

    // 4비트 출력
    for (i = 0; i < 4; i++) {
        int bit = (num >> i) & 0x01;   // b0, b1, b2, b3
        digitalWrite(pins[3 - i], bit ? HIGH : LOW);
    }
}

static void fnd_count(){
    int i;

    for(i=9; 0<=i; --i){
        if(fnd_stop_flag) break;

        fnd_print(i);
        delay(1000);
    }

    if(!fnd_stop_flag) buz_beep();

    // 초기화
    for (i = 0; i < 4; ++i) {
        digitalWrite(pins[i], HIGH);
    }
}

static void *fnd_count_thread(void *arg)
{
    fnd_count_running = 1;
    fnd_count();
    fnd_count_running = 0;
    return NULL;
}


//

void fnd_count_start(){
    if(!fnd_count_running){
        fnd_stop_flag = 0;
        pthread_create(&fnd_tid, NULL, fnd_count_thread, NULL);
    }
}


void fnd_count_stop(){
     if (fnd_count_running) {
        fnd_stop_flag = 1;
        pthread_join(fnd_tid, NULL);
    }
}


// int main(int argc, char **argv)
// {
//     wiringPiSetupGpio(); // BCM pin
//     fnd_count();

//     return 0;
// }
