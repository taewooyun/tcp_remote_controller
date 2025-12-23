#include <wiringPi.h>
#include <stdio.h>

#include "sensors.h"

int pins[4] = {23, 18, 15, 14}; // BCM D, C, B, A

volatile int fnd_stop_flag = 0;

int fnd_print(int num)
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

    return 0;
}


int fnd_count(){
    fnd_stop_flag = 0;

    int i;

    for(i=3; 0<=i; --i){
        if(fnd_stop_flag) break;

        fnd_print(i);
        delay(1000);
    }

    if(!fnd_stop_flag) buz_beep_play();

    // 초기화
    for (i = 0; i < 4; ++i) {
        digitalWrite(pins[i], HIGH);
    }
}

void fnd_stop(){
    fnd_stop_flag = 1;
}


// int main(int argc, char **argv)
// {
//     wiringPiSetupGpio(); // BCM pin
//     fnd_count();

//     return 0;
// }
