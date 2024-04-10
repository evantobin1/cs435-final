#include <SPI.h>

volatile boolean received;
volatile byte receivedData;

void setup() {
  Serial.begin(9600); // Start serial communication at 9600 baud
  pinMode(MISO, OUTPUT); // Set MISO as output (required for SPI slave)
  pinMode(SS, INPUT_PULLUP); // Ensure SS is an input with a pull-up resistor
  received = false;
  receivedData = 0x00;

  // Attach an interrupt to the SS pin to detect when the master device selects the slave
  attachInterrupt(digitalPinToInterrupt(SS), slaveSelect, FALLING);

  SPI.begin(); // Initialize SPI
  SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
  SPI.attachInterrupt(); // Ensure SPI interrupts are enabled (this may need to be customized)
}

// Interrupt service routine for when the slave is selected
void slaveSelect() {
  // This function is called when SS pin goes LOW (master selects the slave)
  received = true;
}

void loop() {
  if (received) {
    // Simulate receiving data (this part is conceptual)
    // On actual hardware, you would handle the SPI data register directly
    receivedData = SPI.transfer(0xFF); // Simulate sending data back to master

    Serial.print("Received data: ");
    Serial.println(receivedData, HEX);
    received = false; // Reset the received flag
  }
}







// SPI interrupt routine
ISR (SPI_STC_vect) {
  byte c = SPDR; // Read byte from SPI Data Register
  // Process received byte (this could be more complex)
  receivedData = c;
}