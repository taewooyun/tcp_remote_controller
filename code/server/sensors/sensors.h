#ifndef SENSORS_H
#define SENSORS_H

/* buzzer */
int buz_music_play(void);
void buz_music_stop(void);
int buz_beep_play(void);

/* led */
int led_on(void);
int led_off(void);
void led_brightness(int);

/* fnd */
int fnd_count(void);
void fnd_stop(void);

/* light */
int cds_value(void);
int cds_sensing_start(void);
void cds_sensing_stop(void);

#endif