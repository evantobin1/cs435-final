#include <SPI.h>
#include <ArduinoBLE.h>

#define RDY_PIN         D2
#define BLE_ROLE_PIN    D3
#define BUFFER_SIZE     64
#define SPI_FREQUENCY   1000000

// __________Types
typedef enum {
  NO_ERRORS       = 0x00,
  ERROR_SPI       = 0x01,
  ERROR_BLUETOOTH = 0x02
} ERROR_TYPE;

enum {
  HEADER = 0x22,
  WRITE  = 0x01,
  READ   = 0x02
} COMMAND_TYPE;

typedef enum {
  BLE_PERIPHERAL = 0,
  BLE_CENTRAL = 1
} BLE_ROLE;


// __________Constants
const char* serviceUuid = "12345678-1234-1234-1234-123456789012";
const char* characteristicUuid = "87654321-4321-4321-4321-210987654321";


// __________Variables
volatile bool RECEIVING_DATA_SPI = false;
BLE_ROLE role;
static byte BluetoothIncoming[BUFFER_SIZE];
static byte BluetoothIncoming_Size = 0;



// ------------------------------------------------ For use only if you are the peripheral ------------------------------------------------ // 
BLEService peripheralService(serviceUuid);
BLECharacteristic peripheralCharacteristic(characteristicUuid, BLERead | BLEWrite | BLENotify, 16);
// ---------------------------------------------------------------------------------------------------------------------------------------- //



// ------------------------------------------------ For use only if you are the central --------------------------------------------------- // 
BLECharacteristic centralCharacteristic; // Global variable to hold a reference to the peripheral's characteristic
// ---------------------------------------------------------------------------------------------------------------------------------------- //




// ------------------------------------------------ Hardware Interrupts --------------------------------------------------- // 
void slaveSelectAsserted() 
{
  RECEIVING_DATA_SPI = true;
}
// ------------------------------------------------------------------------------------------------------------------------ // 



// -------------------------------------------- Setup Bluetooth ------------------------------------------------- //
void SetupCentral() {
  BLE.setLocalName("Arduino Central");
  if (!CentralConnectionProcedure()) {
    Serial.println("Failed to establish a connection. Reset needed.");
    while(1); // Stop execution here
  }
}

void SetupPeripheral() {
  BLE.setLocalName("Arduino Peripheral");
  BLE.setAdvertisedService(peripheralService);
  peripheralService.addCharacteristic(peripheralCharacteristic);
  BLE.addService(peripheralService);
  peripheralCharacteristic.setEventHandler(BLEWritten, OnBluetoothReceived);

  Serial.println("Peripheral setup complete, advertising service.");
  
  // Peripheral needs to wait for a connection
  PeripheralConnectionProcedure();
}



bool PeripheralConnectionProcedure()
{
  while (!BLE.connected()) 
  {
    Serial.print("Adveritising at ");
    Serial.print(BLE.address());
    Serial.println("...");
    BLE.advertise();
    delay(500); // Wait for connection
  }
  Serial.print("Successful connection to ");
  Serial.println(BLE.central().address());
}

bool CentralConnectionProcedure() 
{
    Serial.println("Starting scan for BLE peripherals...");

    int attempt = 0;
    const int maxAttempts = 100;  // Maximum number of attempts for scanning
    const int maxConnectRetries = 5;  // Maximum retries for connecting to a peripheral
    const int maxServiceRetries = 3;  // Maximum retries for service discovery
    const int maxCharRetries = 2;  // Maximum retries for characteristic discovery

    while (attempt < maxAttempts) {
        BLE.scanForUuid(serviceUuid);  // Start scanning for devices with the specified UUID
        delay(500); // Give some time for devices to respond to the scan

        BLEDevice peripheral = BLE.available();
        if (peripheral) {
            Serial.println("Found a peripheral, attempting to connect...");

            int connectAttempts = 0;
            while (!peripheral.connect() && connectAttempts < maxConnectRetries) {
                Serial.println("Failed to connect, retrying...");
                connectAttempts++;
                delay(1000); // Wait before retrying
            }

            if (peripheral.connected()) {
                Serial.println("Connected to peripheral");
                delay(500); // Wait for the connection to stabilize

                int serviceAttempts = 0;
                while (!peripheral.discoverService(serviceUuid) && serviceAttempts < maxServiceRetries) {
                    Serial.println("Service not found, retrying...");
                    serviceAttempts++;
                    delay(500);
                }

                if (peripheral.hasService(serviceUuid)) {
                    BLECharacteristic charac = peripheral.characteristic(characteristicUuid);

                    int charAttempts = 0;
                    while (!charac && charAttempts < maxCharRetries) {
                        Serial.println("Characteristic not found, retrying...");
                        charAttempts++;
                        delay(500);
                        charac = peripheral.characteristic(characteristicUuid);  // Retry finding the characteristic
                    }

                    if (charac) {
                        Serial.println("Characteristic found");
                        BLE.stopScan();
                        centralCharacteristic = charac;
                        return true;  // Successful connection and characteristic discovery
                    }
                }
                peripheral.disconnect(); // Ensure to disconnect if discovery fails
            } else {
                Serial.println("Connection attempts exhausted.");
            }
        } else {
            Serial.println("No suitable peripherals found this cycle.");
        }
        BLE.stopScan();  // Stop the scan before retrying
        attempt++;
        delay(1000);  // Delay 1 second before the next attempt
    }

    return false;  // Connection or characteristic discovery failed after retries
}


void SendBluetoothData(byte length, byte* data) 
{

  if(!BLE.connected())
  {
    Serial.println("Error, tried to send bluetooth data, connection doesn't exist.");
    return;
  }

  bool success;

  switch(role)
  {
    case BLE_PERIPHERAL:
      success = peripheralCharacteristic.writeValue(data, length);
      break;
    case BLE_CENTRAL:
      success = centralCharacteristic.writeValue(data, length);
      break;
  }

  if (success) 
  {
    Serial.println("Data sent successfully");
  } 
  else 
  {
    Serial.println("Failed to send data");
  }
}


void OnBluetoothReceived(BLEDevice central, BLECharacteristic characteristic) 
{
  const byte* dataReceived = characteristic.value();
  size_t length = characteristic.valueLength();
  Serial.print("Received: ");
  for (size_t i = 0; i < length; i++) 
  {
    BluetoothIncoming[BluetoothIncoming_Size++] = dataReceived[i];
    Serial.print(dataReceived[i]);
    Serial.print(", ");
  }
  Serial.print("\n");
}


void CheckIfStillConnectedBLE()
{
  if(BLE.connected())
  {
    digitalWrite(RDY_PIN, HIGH);
    return;
  }

  // Take off the ready LED
  digitalWrite(RDY_PIN, LOW);
  
  switch(role)
    {
      case BLE_PERIPHERAL:
        PeripheralConnectionProcedure();
        break;

      case BLE_CENTRAL:
        CentralConnectionProcedure();
        break;
    }
}




// ------------------------------------------------------ Handle SPI --------------------------------------------------------- //
ERROR_TYPE ProcessReceivingData() 
{
  byte header_byte = SPI.transfer(0x00);
  if (header_byte != HEADER) 
  {
    Serial.println("Error Received from header...");
    return ERROR_SPI;
  }
  byte command_byte = SPI.transfer(0x22);
  switch(command_byte) 
  {
    case WRITE: 
    {
      byte write_length = SPI.transfer(0x00);
      byte bluetooth_outgoing[write_length];
      for (int i = 0; i < write_length; i++) {
        bluetooth_outgoing[i] = SPI.transfer(0x00);
      }
      SendBluetoothData(write_length, bluetooth_outgoing);
      break;
    }
    case READ: 
    {
      SPI.transfer(BluetoothIncoming_Size);
      for (int i = 0; i < BluetoothIncoming_Size; i++) 
      {
        SPI.transfer(BluetoothIncoming[i]);
      }
      BluetoothIncoming_Size = 0;
      break;
    }
    default:
      return ERROR_SPI;
  }  
  return NO_ERRORS;
}



// --------------------------------------------------------------------------------------------------------------------- //
// ------------------------------------------------------ Main --------------------------------------------------------- //
// --------------------------------------------------------------------------------------------------------------------- //

void setup() 
{
  pinMode(BLE_ROLE_PIN, INPUT);
  pinMode(RDY_PIN, OUTPUT);
  pinMode(SS, INPUT_PULLUP);

  Serial.begin(9600);
  delay(3000);  // Delay to begin serial

  Serial.println("---------------------------------------------------------");
  Serial.println("---------------------------------------------------------");
  Serial.println("------- Beginning Arduino Nano BLE SPI controller -------");
  Serial.println("---------------------------------------------------------");
  Serial.println("---------------------------------------------------------");
  Serial.println("");
  Serial.println("");
  
  // If the bluetooth initialization fails, stop the code
  if (!BLE.begin()) {
    Serial.println("Starting BLE failed!");
    while (1);
  }

  // Determine if we are a BLE central or peripheral
  role = (BLE_ROLE)digitalRead(BLE_ROLE_PIN);
  
  // Initialize our setup to the required role
  switch(role)
  {
    case BLE_PERIPHERAL:
      Serial.println("Configured as Peripheral");
      SetupPeripheral();
      break;
    case BLE_CENTRAL:
      Serial.println("Configured as Central");
      SetupCentral();
      break;
  }

  // Initialize the SPI controller
  attachInterrupt(digitalPinToInterrupt(SS), slaveSelectAsserted, FALLING);
  SPI.begin();
  SPI.beginTransaction(SPISettings(SPI_FREQUENCY, MSBFIRST, SPI_MODE0));
}

void loop() 
{

  // Check for SPI communication
  if (RECEIVING_DATA_SPI) 
  {
    ERROR_TYPE error = ProcessReceivingData();
    RECEIVING_DATA_SPI = false;
    if (error != NO_ERRORS) 
    {
      digitalWrite(RDY_PIN, LOW);
    }
  }

  // Ensure the bluetooth is still connected
  CheckIfStillConnectedBLE();

  // Poll the bluetooth
  BLE.poll();
}