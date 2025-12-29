#include <wiringPi.h>
#include <softTone.h>
#include <pthread.h>

/* =====================
 * 음계 / 박자 정의
 * ===================== */
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

#define BUZ   21
#define TOTAL 40

/* =====================
 * 악보 데이터
 * ===================== */
static int notes[TOTAL] = {
    Bb5, A5, G5, F5, G5, A5, Bb5, A5, G5, F5, G5,
    Bb5, A5, G5, F5, G5, A5, Bb5, C5, D5,
    Eb5, D5, C5, Bb5, C5, D5, Eb5, D5, C5, Bb5, C5,
    Eb5, D5, C5, Bb5, C5, D5, Eb5, F5, G5
};

static int beats[TOTAL] = {
    E,E,E,Q, E,E,E,E, E,E,Q,
    E,E,E,Q, E,E,E,E,Q,
    E,E,E,Q, E,E,E,E, E,E,Q,
    E,E,E,Q, E,E,E,Q,H
};

/* =====================
 * 내부 상태
 * ===================== */
static volatile int buz_stop_flag = 0;
static volatile int buz_music_running = 0;
static pthread_t buz_tid;

/* =====================
 * 실제 음악 재생 로직
 * (blocking, 스레드 전용)
 * ===================== */
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

/* =====================
 * 음악 재생 스레드
 * ===================== */
static void *buz_music_thread(void *arg)
{
    buz_music_running = 1;
    buz_music_play();
    buz_music_running = 0;
    return NULL;
}

/* =====================
 * 외부 제어 API
 * ===================== */
void buz_music_start(void)
{
    if (!buz_music_running) {
        buz_stop_flag = 0;
        pthread_create(&buz_tid, NULL, buz_music_thread, NULL);
    }
}

void buz_music_stop(void)
{
    if (buz_music_running) {
        buz_stop_flag = 1;
        pthread_join(buz_tid, NULL);
    }
}

/* =====================
 * 단발음 (스레드 불필요)
 * ===================== */
int buz_beep(void)
{
    softToneCreate(BUZ);

    softToneWrite(BUZ, 1000);
    delay(150);

    softToneWrite(BUZ, 0);
    return 0;
}