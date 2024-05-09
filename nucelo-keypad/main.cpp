#include "mbed.h"
#include "keypad.h"
#include "NHD_0216HZ.h"


typedef enum : char{
    PASSCODE_NOTRECEIVED = 0,
    PASSCODE_GOOD = 0x40,
    PASSCODE_BAD = 0x80,
} ResponseCode;


const char CODE_LEN = 0x04;

unsigned char PasswordBuffer[4];
unsigned char PasswordBuffer_Size = 0;

SPI spi(D11, D12, D13); // SPI pins for the Nucleo F401RE... MOSI, MISO, SCK
DigitalOut cs(D10); // Slave Select pin
DigitalOut bad_led(D3);  // Red LED 
DigitalOut good_led(D4); // Green LED
Keypad keypad( PC_3,PC_2,PA_0,PA_1,PA_4,PB_0,PC_1,PC_0 ); // Keypad pins




ResponseCode CheckResponse()
{
    cs = 0; // Assert slave select
    ThisThread::sleep_for(50ms);
    spi.write(0x22); // Write header byte
    ThisThread::sleep_for(50ms);
    spi.write(0x02); // Write read command
    ThisThread::sleep_for(50ms);
    char length = spi.write(0x00); // Read in the length
    ThisThread::sleep_for(50ms);

    // Read in all of the values from 0 -> length (should be only one value because response code is only one byte)
    char buffer[length]; 
    for (int i = 0; i < length; i++)
    {
        buffer[i] = spi.write(0x00);
        ThisThread::sleep_for(50ms);
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

void SendPasscode(unsigned char* passcode)
{
    cs = 0;                 // Pull slave select low
    ThisThread::sleep_for(100ms);
    spi.write(0x22);        // Write header byte
    ThisThread::sleep_for(50ms);
    spi.write(0x01);        // Write 'write' command
    ThisThread::sleep_for(50ms);
    spi.write(CODE_LEN);    // Write length of '4'
    ThisThread::sleep_for(50ms);

    for(int i = 0; i < CODE_LEN; i++)
    {
        spi.write(passcode[i]);    // Write the response code
        ThisThread::sleep_for(50ms);
    }

    cs = 1;                 // Pull slave select high
}

void ScanKeypad()
{
    unsigned char key = keypad.getKey();    
    if(key != KEY_RELEASED)
    {
        if (PasswordBuffer_Size < CODE_LEN) 
        {
            PasswordBuffer[PasswordBuffer_Size++] = key - '0'; // Assuming ASCII input
            char displayChar[2] = {key, '\0'};  // Create a string for display
            print_lcd(&spi, displayChar);  // Display the character on the LCD
            ThisThread::sleep_for(600ms); // delay between each button press
        }
    }
}


int main() {    
    keypad.enablePullUp(); // Setup the keypad
    
    // Set up the SPI interface
    spi.format(8, 3); // 8 bits per frame, mode 0
    spi.frequency(1000000); // 1 MHz

    cs = 1; // Slave device not selected by default

    // Initialize the leds
    good_led = 0;
    bad_led = 0;

    
    init_spi(&spi); // Initialize the SPI interface
    init_lcd(&spi); // Initialize the LCD
    clear_lcd(&spi);


    while (true) {
        ThisThread::sleep_for(10ms);
        ScanKeypad();


        if(PasswordBuffer_Size >= CODE_LEN)
        {
            PasswordBuffer_Size = 0;
            SendPasscode(PasswordBuffer);

            ThisThread::sleep_for(2s);

            char response = PASSCODE_NOTRECEIVED;

            int timeout_attempts = 3;
            while(timeout_attempts >= 0 && response == PASSCODE_NOTRECEIVED)
            {
                response |= CheckResponse();
                timeout_attempts--;
                ThisThread::sleep_for(200ms);
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