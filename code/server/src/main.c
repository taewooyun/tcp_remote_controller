#include <wiringPi.h>
#include "server.h"
#include "device_worker.h"

#define SERVER_PORT 5000

int main(void)
{
    wiringPiSetupGpio();

    start_device_worker();
    start_server(SERVER_PORT);

    return 0;
}
