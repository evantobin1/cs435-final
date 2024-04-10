#include <SPI.h>
#include <ArduinoBLE.h>

#define BUFFER_SIZE     64
#define SPI_FREQUENCY   4000000

BLEService BLE_Service("12345678-1234-1234-1234-123456789012"); // Create a BLE service
BLEByteCharacteristic BLE_Characteristic("87654321-4321-4321-4321-210987654321", BLERead | BLEWrite); // Create a BLE characteristic

volatile boolean RECEIVING_DATA;

byte BluetoothIncoming[BUFFER_SIZE]; // Buffer to store incoming data from other device
byte BluetoothIncoming_Size = 0;

typedef enum 
{
  NO_ERRORS       = 0x00,
  ERROR_SPI       = 0x01,
  ERROR_BLUETOOTH = 0x02
} ERROR_TYPE;

enum
{
  HEADER = 22,
  WRITE  = 1,
  READ   = 2
} COMMAND_TYPE;





// ------------------------------ Setup ------------------------------ //


void setup() {
  Serial.begin(9600); // Start serial communication at 9600 baud
  
  if (!BLE.begin()) {
    Serial.println("Starting BLE failed!");
    while (1);
  }

  BLE.setLocalName("MyBLEDevice");
  BLE.setAdvertisedService(BLE_Service); // Set the service to be advertised
  BLE_Service.addCharacteristic(BLE_Characteristic); // Add the characteristic to the service
  BLE.addService(BLE_Service); // Add the service

  BLE_Characteristic.setEventHandler(BLEWritten, OnBluetoothReceived); // Set event handler for write events

  BLE.advertise(); // Start advertising

  Serial.println("BLE device active, waiting for connections...");
  
  
  pinMode(MISO, OUTPUT); // Set MISO as output (required for SPI slave)
  pinMode(SS, INPUT_PULLUP); // Ensure SS is an input with a pull-up resistor
  RECEIVING_DATA = false;

  // Attach an interrupt to the SS pin to detect when the master device selects the slave
  attachInterrupt(digitalPinToInterrupt(SS), slaveSelectAsserted, FALLING);

  SPI.begin(); // Initialize SPI as slave
  SPI.beginTransaction(SPISettings(SPI_FREQUENCY, MSBFIRST, SPI_MODE0));
}




// ------------------------------ Loop ------------------------------- //

void loop() {
  ERROR_TYPE error;
  if (RECEIVING_DATA)
  {
    error = ProcessReceivingData();
    RECEIVING_DATA = false; // Reset the flag after processing data
  }


  if (error != NO_ERRORS)
  {
    // TODO something
  }
}



// ---------------------------- Interrupts ----------------------------- //

// Interrupt service routine for when the slave is selected
void slaveSelectAsserted() {
  RECEIVING_DATA = true;
}





// ---------------------- Communication Functions ---------------------- //



//
// Continously reads data bytes until the slave is deasserted
//
ERROR_TYPE ProcessReceivingData()
{
  // Read byte 0 (shift in dummy byte)
  delayMicroseconds(100); // TODO figure out actual amount of time from slave select to sending first bit
  byte header_byte = SPI.transfer(0x00);
  if (header_byte != HEADER)
  {
    return ERROR_SPI;
  }

  // Read byte 1 (shift in dummy byte)
  delayMicroseconds(100);
  byte command_byte = SPI.transfer(0x00);
  switch(command_byte)
  {
    case WRITE:
    {
      // Read byte 2 (shift in dummy byte)
      delayMicroseconds(100);
      byte write_length = SPI.transfer(0x00);

      byte bluetooth_outgoing[write_length];

      // Fill in the outgoing message
      for(int i = 0; i < write_length; i++)
      {
        // Read byte 3 and onward and place them in the bluetooth_outgoing array
        delayMicroseconds(100);
        bluetooth_outgoing[i] = SPI.transfer(0x00);
      }

      // Finally, send the data
      SendBluetoothData(write_length, bluetooth_outgoing);
      break;
    }
    case READ:
    {
      // Write byte 2
      delayMicroseconds(100);
      byte command_byte = SPI.transfer(BluetoothIncoming_Size);

      for(int i = 0; i < BluetoothIncoming_Size; i++)
      {
        // Contonously write out the incoming bytes from the bluetooth
        delayMicroseconds(100);
        byte command_byte = SPI.transfer(BluetoothIncoming[i]);
      }

      BluetoothIncoming_Size = 0;
      break;
    }
    default:
      return ERROR_SPI;
  }  
}

//
// Sends the data in the via bluetooth to the other device
//
ERROR_TYPE SendBluetoothData(byte argc, byte argv[])
{
  for (int i = 0; i < argc; i++) 
  {
    BLE_Characteristic.writeValue(argv[i]);
  }
}

//
// Receives the data via bluetooth and puts it in the BluetoothIncoming buffer
//
void OnBluetoothReceived(BLEDevice central, BLECharacteristic characteristic) {
  byte value = *(characteristic.value());
  BluetoothIncoming[BluetoothIncoming_Size++] = value;
}
