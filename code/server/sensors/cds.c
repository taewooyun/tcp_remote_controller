#include <wiringPi.h>
#include <stdio.h>
#include <stdlib.h>

#define CDS 	11 	/* BCM 11 */
#define LED 	24 	/* BCM 24 */

volatile int cds_stop_flag = 0;

int cds_value()
{
    pinMode(CDS, INPUT); 	/* Pin 모드를 입력으로 설정 */

    return digitalRead(CDS);
}

int cds_sensing_start()
{
    cds_stop_flag = 0;

    pinMode(CDS, INPUT); 	/* Pin 모드를 입력으로 설정 */
    pinMode(LED, OUTPUT); 	/* Pin 모드를 출력으로 설정 */


    while(1) { 			/* 조도 센서 검사를 위해 무한 루프를 실행한다. */
        if(cds_stop_flag) break;

        if(digitalRead(CDS) == LOW) { 	/* 빛이 감지되면(LOW) */
            digitalWrite(LED, LOW); 	/* LED 켜기(On) */
        } 
        else {
            digitalWrite(LED, HIGH); 	/* LED 끄기(Off) */
        }
        delay(500);
    }

    return 0;
}

void cds_sensing_stop(){
    cds_stop_flag = 1;
}


// int main( )
// {
//     wiringPiSetupGpio(); // BCM pin
//     cds_sensing(); 		/* 조도 센서 사용을 위한 함수 호출 */
//     return 0;
// }
