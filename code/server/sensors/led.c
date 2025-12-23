#include <wiringPi.h>
#include <softPwm.h>
#include <stdio.h>
#include <stdlib.h>

#define LED 24

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

void led_brightness(int level)
{
    pinMode(LED, OUTPUT); 		/* Pin의 출력 설정 */
    softPwmCreate(LED, 0, 100); 	/* PWM의 범위 설정 */

    softPwmWrite(LED, level); 		/* LED 끄기 */
}

// int main(int argc, char **argv)
// {   
//     wiringPiSetupGpio(); // BCM pin
//     led_brightness(50);
//     delay(5000);     

//     return 0;
// }
