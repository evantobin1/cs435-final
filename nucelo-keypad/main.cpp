#include "mbed.h"

const char PASSCODE[] = {9, 3, 3, 1};

// Define the SPI pins for the Nucleo F401RE
SPI spi(D11, D12, D13); // MOSI, MISO, SCK

// Define the Slave Select pin if necessary, although for a single master/single slave setup it might not be needed.
DigitalOut cs(D10); // Slave Select pin

// Define the LEDs that will trigger when the passcode is correct/incorrect
DigitalOut bad_led(D3);  // Red LED 
DigitalOut good_led(D4); // Green LED

typedef enum : char{
    PASSCODE_NOTRECEIVED = 0,
    PASSCODE_GOOD = 0x40,
    PASSCODE_BAD = 0x80,
} ResponseCode;


// TODO deleteme
InterruptIn button(BUTTON1);
volatile bool SHOULD_WRITE_PASSWORD = false;
void ButtonPressed()
{
    SHOULD_WRITE_PASSWORD = true;
}



ResponseCode CheckResponse()
{
    cs = 0; // Assert slave select
    spi.write(0x22); // Write header byte
    spi.write(0x02); // Write read command
    char length = spi.write(0x00); // Read in the length

    // Read in all of the values from 0 -> length (should be only one value because response code is only one byte)
    char buffer[length]; 
    for (int i = 0; i < length; i++)
    {
        buffer[i] = spi.write(0x00);
    }

    if(length == 0x00)
    {
        return PASSCODE_NOTRECEIVED;
    }

    cs = 1; // Deassert slave select

    // Check the response
    ResponseCode response;
    if(buffer[0] == PASSCODE_GOOD)
    {
        return PASSCODE_GOOD;
    }

    return PASSCODE_BAD;
}

void SendPasscode(const char* passcode)
{
    const char CODE_LEN = 0x04;
    cs = 0;                 // Pull slave select low
    spi.write(0x22);        // Write header byte
    spi.write(0x01);        // Write 'write' command
    spi.write(CODE_LEN);    // Write length of '4'

    for(int i = 0; i < CODE_LEN; i++)
    {
        spi.write(passcode[i]);    // Write the response code
    }

    cs = 1;                 // Pull slave select high
}



int main() {
    // Set up the SPI interface
    spi.format(8, 0); // 8 bits per frame, mode 0
    spi.frequency(1000000); // 1 MHz

    cs = 1; // Slave device not selected by default

    // Initialize the leds
    good_led = 0;
    bad_led = 0;

    // TODO deleteme
    button.fall(&ButtonPressed);


    while (true) {
        ThisThread::sleep_for(1s);


        // TODO replace me with the keypad trigger stuff
        if(SHOULD_WRITE_PASSWORD)
        {
            SendPasscode(PASSCODE);

            char response;

            int timeout_attempts = 3;
            while(timeout_attempts >= 0)
            {
                response = CheckResponse();
                timeout_attempts--;
                response |= PASSCODE_NOTRECEIVED;
            }

            if(response == PASSCODE_NOTRECEIVED)
            {
                // Error, passcode status was not received
                continue;
            }

            
            if((response & PASSCODE_GOOD) != 0)
            {
                good_led = 1;
                ThisThread::sleep_for(2s);
                good_led = 0;
            }

            if((response & PASSCODE_BAD) != 0)
            {
                bad_led = 1;
                ThisThread::sleep_for(2s);
                bad_led = 0;
            }
        }

    }
}