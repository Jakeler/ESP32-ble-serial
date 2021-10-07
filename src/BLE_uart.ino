/*
    Video: https://www.youtube.com/watch?v=oCMOYS71NIU
    Based on Neil Kolban example for IDF: https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleNotify.cpp
    Ported to Arduino ESP32 by Evandro Copercini

   Create a BLE server that, once we receive a connection, will send periodic notifications.
   The service advertises itself as: 6E400001-B5A3-F393-E0A9-E50E24DCCA9E
   Has a characteristic of: 6E400002-B5A3-F393-E0A9-E50E24DCCA9E - used for receiving data with "WRITE" 
   Has a characteristic of: 6E400003-B5A3-F393-E0A9-E50E24DCCA9E - used to send data with  "NOTIFY"

   The design of creating the BLE server is:
   1. Create a BLE Server
   2. Create a BLE Service
   3. Create a BLE Characteristic on the Service
   4. Create a BLE Descriptor on the characteristic
   5. Start the service.
   6. Start advertising.

   In this example rxValue is the data received (only accessible inside that function).
   And txBuffer is a array of bytes to be sent from Serial to BLE.
*/
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// TODO: check why it does fail with smaller values
#define SEND_BUFFER_SIZE 256

BLEServer *pServer = NULL;
BLECharacteristic * pTxCharacteristic;
bool deviceConnected = false;
bool oldDeviceConnected = false;
uint8_t txBuffer[SEND_BUFFER_SIZE] = { 0 };

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID           "0000ff00-0000-1000-8000-00805f9b34fb" // UART service UUID
#define CHARACTERISTIC_UUID_RX "0000ff01-0000-1000-8000-00805f9b34fb"
#define CHARACTERISTIC_UUID_TX "0000ff02-0000-1000-8000-00805f9b34fb"


class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string rxValue = pCharacteristic->getValue();

      if (rxValue.length() > 0) {
        // Serial.println("*********");
        // Serial.print("Received Value: ");
        for (int i = 0; i < rxValue.length(); i++)
          Serial.print(rxValue[i]);

        // Serial.println();
        // Serial.println("*********");
      }
    }
};

class MySecurity: public BLESecurityCallbacks {
  public:
    // ~BLESecurityCallbacks() {};
    /**
     * @brief Its request from peer device to input authentication pin code displayed on peer device.
     * It requires that our device is capable to input 6-digits code by end user
     * @return Return 6-digits integer value from input device
     * only for IN capabilty
     */
    uint32_t onPassKeyRequest() {
      Serial.println("Got passKey request, sending 000000");
      delay(5000); // TODO: Serial input
      return 0;
    }

    /**
     * @brief Provide us 6-digits code to perform authentication.
     * It requires that our device is capable to display this code to end user
     * @param
     * only for OUT capabilty
     */
    void onPassKeyNotify(uint32_t pass_key) {
      Serial.println("Notify PIN:" + String(pass_key));
    }

    /**
     * @brief Here we can make decision if we want to let negotiate authorization with peer device or not
     * return Return true if we accept this peer device request
     */
    bool onSecurityRequest() {
      Serial.println("Got security request");
      return false;
    }

    /**
     * Display and confirm pin, for IO capability
     */
    bool onConfirmPIN(uint32_t pin) {
      Serial.println("Confirming PIN:" + String(pin));
      for (size_t i = 0; i < 5; i++){
        Serial.print(".");
        delay(1000);
      }
      Serial.println(" OK");
      return true; // TODO: real confirmation
    }

    /**
     * Provide us information when authentication process is completed
     */
    void onAuthenticationComplete(esp_ble_auth_cmpl_t auth) {
      Serial.println("Auth " + String(auth.success? "successful":"FAILED"));
    }
};


void setup() {
  Serial.begin(115200);

  // Create the BLE Device
  BLEDevice::init("My UART Service");
  BLEDevice::setEncryptionLevel(esp_ble_sec_act_t::ESP_BLE_SEC_ENCRYPT_MITM);
  BLEDevice::setSecurityCallbacks(new MySecurity());

  BLESecurity *bleSec = new BLESecurity();
  bleSec->setCapability(ESP_IO_CAP_IO); // Display yes/no on both devices
  bleSec->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_MITM_BOND);
	bleSec->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pTxCharacteristic = pService->createCharacteristic(
                    CHARACTERISTIC_UUID_TX,
                    BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ
                  );
                      
  pTxCharacteristic->addDescriptor(new BLE2902());

  BLECharacteristic * pRxCharacteristic = pService->createCharacteristic(
                       CHARACTERISTIC_UUID_RX,
                      BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
                    );

  pRxCharacteristic->setCallbacks(new MyCallbacks());

  // Start the service
  pService->start();

  // Start advertising
  pServer->getAdvertising()->start();
  Serial.println("Waiting a client connection to notify...");
}

void loop() {

  int availableBytes = Serial.available() % SEND_BUFFER_SIZE;
  if (deviceConnected && availableBytes > 0) {
      Serial.readBytes(txBuffer, availableBytes);
      pTxCharacteristic->setValue(txBuffer, availableBytes);
      pTxCharacteristic->notify();
    delay(10); // bluetooth stack will go into congestion, if too many packets are sent
  }

  // disconnecting
  if (!deviceConnected && oldDeviceConnected) {
      delay(500); // give the bluetooth stack the chance to get things ready
      pServer->startAdvertising(); // restart advertising
      Serial.println("start advertising");
      oldDeviceConnected = deviceConnected;
  }
  // connecting
  if (deviceConnected && !oldDeviceConnected) {
  // do stuff here on connecting
      oldDeviceConnected = deviceConnected;
  }
}