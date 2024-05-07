#include <SPI.h>
#include <ArduinoBLE.h>

#define RDY_PIN         D2
#define BLE_ROLE_PIN    D3
#define BUFFER_SIZE     64
#define SPI_FREQUENCY   1000000

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

const char* serviceUuid = "12345678-1234-1234-1234-123456789012";
const char* characteristicUuid = "87654321-4321-4321-4321-210987654321";

volatile bool RECEIVING_DATA_SPI = false;
BLE_ROLE role;
static byte BluetoothIncoming[BUFFER_SIZE] = {0x40};
static byte BluetoothIncoming_Size = 1;

BLEService peripheralService(serviceUuid);
BLECharacteristic peripheralCharacteristic(characteristicUuid, BLERead | BLEWrite | BLENotify, 16);
BLECharacteristic centralCharacteristic;

void slaveSelectAsserted() {
  RECEIVING_DATA_SPI = true;
}

void SetupPeripheral() {
  BLE.setLocalName("Arduino Peripheral");
  BLE.setAdvertisedService(peripheralService);
  peripheralService.addCharacteristic(peripheralCharacteristic);
  BLE.addService(peripheralService);
  peripheralCharacteristic.setEventHandler(BLEWritten, OnBluetoothReceived);
  
  Serial.println("Peripheral setup complete, advertising service.");
  BLE.advertise(); 
  while (true) {
    delay(1); // Wait for connection

    if(BLE.connected())
    {
      break;
    }
    Serial.println("Not connected to central.");
  }

  digitalWrite(RDY_PIN, HIGH); // Indicate ready when connected
  Serial.println("Peripheral connected.");
}

void SetupCentral() {
  pinMode(RDY_PIN, OUTPUT);
  digitalWrite(RDY_PIN, LOW);  // Start with RDY_PIN low to indicate not ready

  BLE.begin();
  BLE.setLocalName("Arduino Central");
  Serial.println("Scanning for BLE peripherals...");

  while (true) {

    BLEDevice peripheral;

    Serial.println("- Discovering peripheral device...");

    do
    {
      BLE.scanForUuid(serviceUuid);
      peripheral = BLE.available();
    } while (!peripheral);

    Serial.println("* Peripheral device found!");
    Serial.print("* Device MAC address: ");
    Serial.println(peripheral.address());
    Serial.print("* Device name: ");
    Serial.println(peripheral.localName());
    Serial.print("* Advertised service UUID: ");
    Serial.println(peripheral.advertisedServiceUuid());
    Serial.println(" ");
    BLE.stopScan();
    
    Serial.println("- Connecting to peripheral device...");

    if (peripheral.connect()) {
      Serial.println("* Connected to peripheral device!");
      Serial.println(" ");
    } else {
      Serial.println("* Connection to peripheral device failed!");
      Serial.println(" ");
      continue;
    }

    Serial.println("- Discovering peripheral device attributes...");
    if (peripheral.discoverAttributes()) {
      Serial.println("* Peripheral device attributes discovered!");
      Serial.println(" ");
    } else {
      Serial.println("* Peripheral device attributes discovery failed!");
      Serial.println(" ");
      peripheral.disconnect();
      continue;
    }

    centralCharacteristic = peripheral.characteristic(characteristicUuid);
      
    if (!centralCharacteristic) {
      Serial.println("* Peripheral device does not have gesture_type characteristic!");
      peripheral.disconnect();
      continue;
    }

    centralCharacteristic.setEventHandler(BLEWritten, OnBluetoothReceived);

    break;
  }

  digitalWrite(RDY_PIN, HIGH); // Indicate ready when connected
  Serial.println("Central connected.");
}

void setup() {
  pinMode(BLE_ROLE_PIN, INPUT);
  pinMode(RDY_PIN, OUTPUT);
  pinMode(SS, INPUT_PULLUP);

  // delay(3000);
  Serial.begin(9600);

  if (!BLE.begin()) {
    Serial.println("Starting BLE failed!");
    while (1);
  }

  role = (BLE_ROLE)digitalRead(BLE_ROLE_PIN);
  
  switch(role) {
    case BLE_PERIPHERAL:
      SetupPeripheral();
      break;
    case BLE_CENTRAL:
      SetupCentral();
      break;
  }

  attachInterrupt(digitalPinToInterrupt(SS), slaveSelectAsserted, FALLING);
  SPI.begin();
  SPI.beginTransaction(SPISettings(SPI_FREQUENCY, MSBFIRST, SPI_MODE0));
}

void loop() {
  if (RECEIVING_DATA_SPI) {
    ERROR_TYPE error = ProcessReceivingData();
    RECEIVING_DATA_SPI = false;
    if (error != NO_ERRORS) {
      digitalWrite(RDY_PIN, LOW);
    }
  }

  BLE.poll();
}


ERROR_TYPE ProcessReceivingData() {
  byte header_byte = SPI.transfer(0x00);
  if (header_byte != HEADER) {
    Serial.println("Error Received from header...");
    return ERROR_SPI;
  }

  byte command_byte = SPI.transfer(0x22);
  switch (command_byte) {
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



void OnBluetoothReceived(BLEDevice central, BLECharacteristic characteristic) 
{
  const byte* dataReceived = characteristic.value();
  size_t length = characteristic.valueLength();
  Serial.println("Received: ");
  for (size_t i = 0; i < length; i++) 
  {
    BluetoothIncoming[BluetoothIncoming_Size++] = dataReceived[i];
    Serial.println(dataReceived[i]);
    Serial.println(", ");
  }
  Serial.println("\n");
}



void SendBluetoothData(byte length, byte* data) {
  bool success = false;
  if (BLE.connected()) {
    if (role == BLE_PERIPHERAL) {
      success = peripheralCharacteristic.writeValue(data, length);
    } else if (role == BLE_CENTRAL) {
      success = centralCharacteristic.writeValue(data, length);
    }

    Serial.print("Sent ");
    for(int i = 0; i < length; i++)
    {
      Serial.print(data[i]);
    }
    Serial.println(".");

    if (success) {
      Serial.println("Data sent successfully");
    } else {
      Serial.println("Failed to send data");
    }
  } else {
    Serial.println("Not connected.");
  }
}
