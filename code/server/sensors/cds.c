#include <wiringPi.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define CDS 	11 	/* BCM 11 */
#define LED 	12 	/* BCM 12 */

volatile int cds_stop_flag = 0;
static volatile int cds_count_running = 0;
static pthread_t cds_tid;

int cds_value()
{
    pinMode(CDS, INPUT); 	

    return digitalRead(CDS);
}

static void cds_sensing()
{
    cds_stop_flag = 0;

    pinMode(CDS, INPUT); 	
    pinMode(LED, OUTPUT); 	


    while(1) { 			
        if(cds_stop_flag) break;

        if(digitalRead(CDS) == LOW) { 	
            digitalWrite(LED, LOW); 	
        } 
        else {
            digitalWrite(LED, HIGH); 	
        }
        delay(500);
    }

    digitalWrite(LED, HIGH);
}

static void *cds_sensing_thread(void *arg)
{
    cds_count_running = 1;
    cds_sensing();
    cds_count_running = 0;
    return NULL;
}

void cds_sensing_start(){
    if(!cds_count_running){
        cds_stop_flag = 0;
        pthread_create(&cds_tid, NULL, cds_sensing_thread, NULL);
    }
}


void cds_sensing_stop(){
     if (cds_count_running) {
        cds_stop_flag = 1;
        pthread_join(cds_tid, NULL);
    }
}


// int main( )
// {
//     wiringPiSetupGpio(); // BCM pin
//     cds_sensing(); 		/* 조도 센서 사용을 위한 함수 호출 */
//     return 0;
// }
