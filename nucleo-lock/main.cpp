#include "mbed.h"

// Define the SPI pins for the Nucleo F401RE
SPI spi(D11, D12, D13); // MOSI, MISO, SCK

// Define the Slave Select pin if necessary, although for a single master/single slave setup it might not be needed.
DigitalOut cs(D10); // Slave Select pin

// Define the button pin
InterruptIn button(BUTTON1); // Use the built-in user button

// This function is called when the button is pressed
void buttonPressed() {
    cs = 0; // Select the slave device

    // Send some arbitrary bytes
    spi.write(0x22);
    spi.write(0x01);
    spi.write(0x01);

    cs = 1; // Deselect the slave device

    printf("Data sent!\n");
}

int main() {
    // Set up the SPI interface
    spi.format(8, 0); // 8 bits per frame, mode 0
    spi.frequency(1000000); // 1 MHz

    cs = 1; // Slave device not selected by default

    // Attach the button press interrupt to the handler
    button.fall(&buttonPressed);

    while (true) {
        ThisThread::sleep_for(1000ms); // Sleep thread
    }
}
