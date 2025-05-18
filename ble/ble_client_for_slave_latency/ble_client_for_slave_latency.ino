#include <BLEDevice.h>
#include <BLEClient.h>
#include <BLEScan.h>

#define DEVICE_NAME "ESP32-C6_BLE_TEST"
#define SERVICE_UUID "180D"
#define CHAR_UUID "2A38"

BLEClient* g_client = NULL;
BLERemoteCharacteristic* g_dataChar = NULL;
bool g_connected = false;
BLEAddress* g_serverAddr = NULL;

void notifyCallback(BLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
  Serial.print("Notify: ");
  Serial.print(length);
  Serial.println(" bytes");
  
  if (length > 0) {
    Serial.print("Data: ");
    for (int i = 0; i < min(10, (int)length); i++) {
      Serial.print(pData[i], HEX);
      Serial.print(" ");
    }
    Serial.println();
  }
}

class ClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
    g_connected = true;
    Serial.println("Connected!");
  }

  void onDisconnect(BLEClient* pclient) {
    g_connected = false;
    Serial.println("Disconnected");
  }
};

class ScanCallback: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice device) {
    if (device.getName() == DEVICE_NAME) {
      Serial.print("Found: ");
      Serial.println(device.toString().c_str());
      
      device.getScan()->stop();
      
      g_serverAddr = new BLEAddress(device.getAddress());
      Serial.print("Addr: ");
      Serial.println(g_serverAddr->toString().c_str());
    }
  }
};

bool connectServer() {
  if (!g_serverAddr) {
    Serial.println("No device found");
    return false;
  }
  
  Serial.print("Connecting: ");
  Serial.println(g_serverAddr->toString().c_str());
  
  g_client = BLEDevice::createClient();
  g_client->setClientCallbacks(new ClientCallback());
  
  if (!g_client->connect(*g_serverAddr)) {
    Serial.println("Connection failed");
    return false;
  }
  
  BLERemoteService* service = g_client->getService(BLEUUID(SERVICE_UUID));
  if (!service) {
    Serial.print("Service not found: ");
    Serial.println(SERVICE_UUID);
    g_client->disconnect();
    return false;
  }
  
  g_dataChar = service->getCharacteristic(BLEUUID(CHAR_UUID));
  if (!g_dataChar) {
    Serial.print("Char not found: ");
    Serial.println(CHAR_UUID);
    g_client->disconnect();
    return false;
  }
  
  if(g_dataChar->canNotify()) {
    g_dataChar->registerForNotify(notifyCallback);
    Serial.println("Notifications ready");
  }
  
  return true;
}

void setup() {
  Serial.begin(115200);
  while(!Serial) {}
  
  Serial.println();
  Serial.println("BLE Client v0.9");
  Serial.println();
  
  BLEDevice::init("BLE_CLIENT");
  
  BLEScan* scan = BLEDevice::getScan();
  scan->setAdvertisedDeviceCallbacks(new ScanCallback());
  scan->setActiveScan(true);
  scan->setInterval(100);
  scan->setWindow(99);
  
  Serial.println("Scanning...");
  scan->start(10, false);
  
  delay(11000);  
  
  if (g_serverAddr) {
    if (connectServer()) {
      Serial.println("Connected OK");
    }
  } else {
    Serial.println("Device not found");
  }
}

void loop() {
  if (!g_connected && g_serverAddr) {
    Serial.println("Reconnecting...");
    if (connectServer()) {
      Serial.println("Back online");
    } else {
      Serial.println("Still disconnected");
      delay(5000);
    }
  }
  
  delay(1000);
}