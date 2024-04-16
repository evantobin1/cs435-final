#include "mbed.h"

#define BUFFER_SIZE     64

volatile bool SHOULD_WRITE_SPI = false;

unsigned char BluetoothIncoming[BUFFER_SIZE]; // Buffer to store incoming data from other device
unsigned char BluetoothIncoming_Size = 0;

unsigned char BluetoothOutgoing[BUFFER_SIZE]; // Buffer to store outgoing data to other device
unsigned char BluetoothOutgoing_Size = 0;

// Define the SPI pins for the Nucleo F401RE
SPI spi(D11, D12, D13); // MOSI, MISO, SCK,
DigitalOut cs(D10); // Slave Select pin



// Define the button pin
InterruptIn button(BUTTON1); // Use the built-in user button

// This function is called when the button is pressed
unsigned char PollSlaveDevice(unsigned char* inputBuffer)
{
    cs = 0; // Pull the slave select low
    
    // Send out a 'Header' byte
    ThisThread::sleep_for(10ms);
    spi.write(0x22);
    
    // Send out a 'Read Command' byte
    ThisThread::sleep_for(10ms);
    unsigned char confirmation_byte = spi.write(0x02);

    if(confirmation_byte != 0x22)
    {
        cs = 1;
        return 0;
    }
    
    // Read the 'length' byte
    ThisThread::sleep_for(10ms);
    unsigned char length = spi.write(0x00);

    if(length >= BUFFER_SIZE)
    {
        length = 2;
    }
    
    // Read each message
    for(int i = 0; i < length; i++)
    {
        ThisThread::sleep_for(10ms);
        inputBuffer[i] = spi.write(0x00);
    }

    cs = 1; // Pull the salve select high

    // Return the number of bytes that were read
    return length;
}



void WriteSlaveDevice(unsigned char byteCount, unsigned char* bytes)
{
    cs = 0; // Pull the slave select low
    
    // Send out a 'Header' byte
    ThisThread::sleep_for(10ms);
    spi.write(0x22);
    
    // Send out a 'Write Command' byte
    ThisThread::sleep_for(10ms);
    spi.write(0x01);
    
    // Write the 'length' byte
    ThisThread::sleep_for(10ms);
    spi.write(byteCount);
    
    // Write each message byte
    for(int i = 0; i < byteCount; i++)
    {
        ThisThread::sleep_for(10ms);
        spi.write(bytes[i]);
    }

    cs = 1; // Pull the salve select high
}


void ButtonPressed()
{
    SHOULD_WRITE_SPI = true;
}



int main() {
    // Set up the SPI interface

    spi.format(8, 0); // 8 bits per frame, mode 0
    spi.frequency(1000000); // 1 MHz
    cs = 1; // Pull the slave select pin high by default

    button.fall(&ButtonPressed);

    BluetoothOutgoing_Size = 4;
    BluetoothOutgoing[0] = 0x00;
    BluetoothOutgoing[1] = 0x01;
    BluetoothOutgoing[2] = 0x02;
    BluetoothOutgoing[3] = 0x03;

    while (true) {


        // Determine if we should send some outgoing data to the slave device
        if(SHOULD_WRITE_SPI)
        {
            WriteSlaveDevice(BluetoothOutgoing_Size, BluetoothOutgoing);
            SHOULD_WRITE_SPI = false;
        }

        // Poll the device for any incoming data
        BluetoothIncoming_Size = PollSlaveDevice(BluetoothIncoming);

        ThisThread::sleep_for(500ms); // Sleep thread
    }
}
