#include <wiringPi.h>
#include <softTone.h>


#define C5   523
#define D5   587
#define Eb5  622
#define F5   698
#define G5   784
#define A5   880
#define Bb5  932
#define REST 0

#define E 220
#define Q 440
#define H 660

#define BUZ 21
#define TOTAL 40

int notes[TOTAL] = {
    Bb5, A5, G5, F5, G5, A5, Bb5, A5, G5, F5, G5,
    Bb5, A5, G5, F5, G5, A5, Bb5, C5, D5,
    Eb5, D5, C5, Bb5, C5, D5, Eb5, D5, C5, Bb5, C5,
    Eb5, D5, C5, Bb5, C5, D5, Eb5, F5, G5
};

int beats[TOTAL] = {
    E,E,E,Q, E,E,E,E, E,E,Q,
    E,E,E,Q, E,E,E,E,Q,
    E,E,E,Q, E,E,E,E, E,E,Q,
    E,E,E,Q, E,E,E,Q,H
};

volatile int buz_stop_flag = 0;

int buz_music_play()
{
    buz_stop_flag = 0;

    softToneCreate(BUZ);

    for (int i = 0; i < TOTAL; i++) {
        if(buz_stop_flag) break;

        if (notes[i] == REST)
            softToneWrite(BUZ, 0);
        else
            softToneWrite(BUZ, notes[i]);

        delay(beats[i]);
    }

    softToneWrite(BUZ, 0);
}

void buz_music_stop(){
    buz_stop_flag = 1;
}

int buz_beep_play(){
    softToneCreate(BUZ);

    softToneWrite(BUZ, 1000);
    delay(150);

    softToneWrite(BUZ, 0); // 마지막 음 정리

    return 0;
}

// int main(){
//     wiringPiSetupGpio(); // BCM pin
//     buz_music_play();

//     return 0;
// }