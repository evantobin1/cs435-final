#include "mbed.h"

const char PASSCODE[] = {9, 3, 2, 1};

// Define the SPI pins for the Nucleo F401RE
SPI spi(D11, D12, D13); // MOSI, MISO, SCK

// Define the Slave Select pin if necessary, although for a single master/single slave setup it might not be needed.
DigitalOut cs(D10); // Slave Select pin

PwmOut servo (D3);

typedef enum {
    PASSCODE_NOTRECIEVED,
    PASSCODE_GOOD = 0x40,
    PASSCODE_BAD = 0x80,
} ResponseCode;


ResponseCode CheckPasscode()
{
    cs = 0;
    ThisThread::sleep_for(50ms);
    spi.write(0x22);
    ThisThread::sleep_for(50ms);
    spi.write(0x02);
    ThisThread::sleep_for(50ms);

    char length = spi.write(0x00);
    ThisThread::sleep_for(50ms);

    char buffer[length]; 
    for (int i = 0; i < length; i++)
    {
        buffer[i] = spi.write(0x00);
        ThisThread::sleep_for(50ms);
    }

    cs = 1;

    if (length == 0)
    {
        return PASSCODE_NOTRECIEVED;
    }

    if (sizeof(buffer) != sizeof(PASSCODE))
    {
        return PASSCODE_BAD;
    }

    for (int i = 0; i < sizeof(PASSCODE) / sizeof(char); i++)
    {
        if (buffer[i] != PASSCODE[i])
        {
            return PASSCODE_BAD;
        }
    }
    return PASSCODE_GOOD;
}

void SendResponse(ResponseCode response)
{

    if(response == PASSCODE_NOTRECIEVED)
    {
        return; // No need to send a response if they never even sent a passcode
    }

    cs = 0;                 // Pull slave select low
    ThisThread::sleep_for(50ms);
    spi.write(0x22);        // Write header byte
    ThisThread::sleep_for(50ms);
    spi.write(0x01);        // Write 'write' command
    ThisThread::sleep_for(50ms);
    spi.write(0x01);        // Write length of '1'
    ThisThread::sleep_for(50ms);
    spi.write(response);    // Write the response code
    ThisThread::sleep_for(50ms);
    cs = 1;                 // Pull slave select high
}


void OpenDoor()
{
    servo.pulsewidth_us(1000);
}

void CloseDoor()
{
    servo.pulsewidth_us(2000);
}



int main() {
    // Set up the SPI interface
    spi.format(8, 0); // 8 bits per frame, mode 0
    spi.frequency(1000000); // 1 MHz

    cs = 1; // Slave device not selected by default

    servo.period_ms(20);

    // Begin with the door closed
    CloseDoor();

    while (true) {
        ThisThread::sleep_for(1s);

        ResponseCode response = CheckPasscode();

        if(response == PASSCODE_GOOD)
        {
            OpenDoor();
        }

        if(response == PASSCODE_BAD)
        {
            CloseDoor();
        }

        SendResponse(response);
    }
}