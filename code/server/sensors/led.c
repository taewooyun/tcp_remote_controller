#include <wiringPi.h>
#include <softPwm.h>
#include <stdio.h>
#include <stdlib.h>

#define LED 12

void led_on()
{
    pinMode(LED, OUTPUT); 		/* Pin의 출력 설정 */
    digitalWrite(LED, LOW); // LOW(on), HIGH(off)
}

void led_off()
{
    pinMode(LED, OUTPUT); 		/* Pin의 출력 설정 */
    digitalWrite(LED, HIGH); // LOW(on), HIGH(off)
}

void led_brightness_set(int level)
{
    pinMode(LED, OUTPUT); 		/* Pin의 출력 설정 */
    softPwmCreate(LED, 0, 100); 	/* PWM의 범위 설정 */
    softPwmWrite(LED, 100 - level); // active-low 기준
}

// int main(int argc, char **argv)
// {   
//     wiringPiSetupGpio(); // BCM pin
//     led_brightness(50);
//     delay(5000);     

//     return 0;
// }
