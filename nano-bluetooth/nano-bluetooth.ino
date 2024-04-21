#include <SPI.h>
#include <ArduinoBLE.h>

#define BUFFER_SIZE     64
#define SPI_FREQUENCY   1000000

const char* serviceUuid = "12345678-1234-1234-1234-123456789012";
const char* characteristicUuid = "87654321-4321-4321-4321-210987654321";

BLEService bleService(serviceUuid);
BLEByteCharacteristic bleCharacteristic(characteristicUuid, BLERead | BLEWrite | BLENotify);

bool isCentral = false;
volatile boolean RECEIVING_DATA = false;

byte BluetoothIncoming[BUFFER_SIZE];
byte BluetoothIncoming_Size = 0;


void setup() {
  Serial.begin(9600);
  delay(1000);  // Delay 1 second to get the serial ready


  // Setup BLE
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(D3, INPUT);
  isCentral = digitalRead(D3);

  if (!BLE.begin()) {
    Serial.println("Starting BLE failed!");
    while (1);
  }

  if (isCentral) {
    Serial.println("Configured as Central");
    setupCentral();
  } else {
    Serial.println("Configured as Peripheral");
    setupPeripheral();
  }

  // Setup SPI after establishing connection
  pinMode(D2, OUTPUT);
  digitalWrite(D2, HIGH);
  pinMode(SS, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(SS), slaveSelectAsserted, FALLING);
  SPI.begin();
  SPI.beginTransaction(SPISettings(SPI_FREQUENCY, MSBFIRST, SPI_MODE0));
}

void loop() {
  BLE.poll();

  if (RECEIVING_DATA) {
    ERROR_TYPE error = ProcessReceivingData();
    RECEIVING_DATA = false;
    if (error != NO_ERRORS) {
      digitalWrite(D2, LOW);
    }
  }
}

void setupCentral() {
  BLE.setLocalName("Arduino Central");
  if (!centralConnectionProcedure()) {
    Serial.println("Failed to establish a connection. Reset needed.");
    while(1); // Stop execution here
  }
}

bool centralConnectionProcedure() {
    Serial.println("Starting scan for BLE peripherals...");
    BLE.scanForUuid(serviceUuid);

    int attempt = 0;
    const int maxAttempts = 100;  // Maximum number of attempts

    while (attempt < maxAttempts) {
        BLEDevice peripheral = BLE.available();
        if (peripheral) {
            Serial.println("Found a peripheral, attempting to connect...");
            if (peripheral.connect()) {
                Serial.println("Connected to peripheral");
                delay(1000); // Wait for the connection to stabilize

                if(peripheral.serviceCount())
                {
                  Serial.println("Serivces found...");
                }
                else
                {
                  Serial.println("No services found...");
                }

                // Manually initiate service discovery
                if (peripheral.discoverAttributes()) {
                  Serial.println("Attributes discovered");
                } else {
                  Serial.println("Attribute discovery failed!");
                  peripheral.disconnect();
                  continue;
                }
                delay(1000); // Wait to ensure discovery completes
                if (peripheral.discoverService(serviceUuid)) {
                    BLECharacteristic charac = peripheral.characteristic(characteristicUuid);
                    if (charac) {
                        Serial.println("Characteristic found");
                        BLE.stopScan();
                        return true;
                    } else {
                        Serial.println("Characteristic not found");
                    }
                } else {
                    Serial.println("Service not found");
                    peripheral.disconnect();
                    continue;
                }
            } else {
                Serial.println("Failed to connect, retrying...");
            }
        } else {
            Serial.println("No suitable peripherals found this cycle.");
        }
        attempt++;
        delay(1000);  // Delay 1 second before next attempt
    }
    BLE.stopScan();
    return false;  // Connection or characteristic discovery failed after retries
}



void setupPeripheral() {
  BLE.setEventHandler(BLEDisconnected, onDisconnect);
  BLE.setEventHandler(BLEConnected, onConnect);
  BLE.setLocalName("Arduino Peripheral");  // Set the local name
  bleService.addCharacteristic(bleCharacteristic);  // Add the characteristic to the service
  BLE.addService(bleService);  // Add the service
  BLE.setAdvertisedService(bleService);    // Add the service UUID to the advertising data
  BLE.advertise();
  Serial.println("Advertising started...");
}

void onDisconnect(BLEDevice central) {
    Serial.println("Disconnected from central, restarting advertising");
    BLE.advertise();
}

void onConnect(BLEDevice central) {
    Serial.println("Connected to central!");
}

void slaveSelectAsserted() {
  RECEIVING_DATA = true;
}

ERROR_TYPE ProcessReceivingData() {
  byte header_byte = SPI.transfer(0x00);
  if (header_byte != HEADER) {
    Serial.println("Error Received from header...");
    return ERROR_SPI;
  }
  byte command_byte = SPI.transfer(0x22);
  switch(command_byte) {
    case WRITE: {
      byte write_length = SPI.transfer(0x00);
      byte bluetooth_outgoing[write_length];
      for (int i = 0; i < write_length; i++) {
        bluetooth_outgoing[i] = SPI.transfer(0x00);
      }
      SendBluetoothData(write_length, bluetooth_outgoing);
      break;
    }
    case READ: {
      SPI.transfer(BluetoothIncoming_Size);
      for (int i = 0; i < BluetoothIncoming_Size; i++) {
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

void SendBluetoothData(byte length, byte* data) {
  if (BLE.connected() && bleCharacteristic.canWrite()) {
    bool success = bleCharacteristic.writeValue(data[0]);
    if (success) {
      Serial.println("Data sent successfully");
    } else {
      Serial.println("Failed to send data");
    }
  } else {
    Serial.println("Not connected to a peripheral or characteristic is not writable");
  }
}

void OnBluetoothRead(BLEDevice central, BLECharacteristic characteristic) {
  Serial.println("Central device read characteristic");
  // Additional handling code here
}

void OnBluetoothReceived(BLEDevice central, BLECharacteristic characteristic) {
  Serial.println("RECEIVED");
  const byte* dataReceived = characteristic.value();
  size_t length = characteristic.valueLength();
  for (size_t i = 0; i < length; i++) {
    BluetoothIncoming[BluetoothIncoming_Size++] = dataReceived[i];
  }
}








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