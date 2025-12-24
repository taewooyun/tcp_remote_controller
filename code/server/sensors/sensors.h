#ifndef SENSORS_H
#define SENSORS_H

/* buzzer */
int buz_music_start(void);
void buz_music_stop(void);
int buz_beep(void);

/* led */
int led_on(void);
int led_off(void);
void led_brightness_set(int);

/* fnd */
void fnd_count_start(void);
void fnd_count_stop(void);

/* cds */
int cds_value(void);
void cds_sensing_start(void);
void cds_sensing_stop(void);

/* camera */
void cam_capture_start();
#endif