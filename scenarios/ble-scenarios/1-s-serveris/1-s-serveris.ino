// Scenario 1: Standard BLE cycle between two ESP32-C6 devices
// Device B (Server) - Data reception
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

// BLE configuration
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

BLEServer *pServer = NULL;
BLECharacteristic *pCharacteristic = NULL;
bool deviceConnected = false;
String receivedData = "";

// Connection status monitoring class
class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
    Serial.println("***SERVER REPORTS: Client connected***");
  }
  
  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    Serial.println("***SERVER REPORTS: Client disconnected***");
   
    // Server begins advertising service again after disconnection
    Serial.println("Starting advertising again...");
    pServer->startAdvertising();
    Serial.println("Advertising renewed.");
  }
};

class MyCharacteristicCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    Serial.println("***SERVER REPORTS: Data received!***");
    
    size_t dataLen = pCharacteristic->getLength();
    const uint8_t* dataPtr = pCharacteristic->getData();
    
    Serial.print("Data length: ");
    Serial.println(dataLen);
    
    if (dataLen > 0 && dataPtr != nullptr) {
      receivedData = "";
      for (size_t i = 0; i < dataLen; i++) {
        receivedData += (char)dataPtr[i];
      }
      
      Serial.println("Received data: " + receivedData);
      
      // Processing received data
      int commaPosition = receivedData.indexOf(",");
      if (commaPosition != -1) {
        String temperatureString = receivedData.substring(0, commaPosition);
        String humidityString = receivedData.substring(commaPosition + 1);
        
        float temperature = temperatureString.toFloat();
        float humidity = humidityString.toFloat();
        
        Serial.print("Received temperature: ");
        Serial.print(temperature);
        Serial.print(" °C, Humidity: ");
        Serial.print(humidity);
        Serial.println(" %");
        
        // Responding to client with confirmation
        String response = "OK:" + String(temperature) + "," + String(humidity);
        pCharacteristic->setValue(response.c_str());
        Serial.println("Confirmation sent: " + response);
      }
    } else {
      Serial.println("ERROR: Data empty or null");
    }
  }
  
  void onRead(BLECharacteristic *pCharacteristic) {
    Serial.println("***SERVER REPORTS: Client reading data***");
  }
};

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n----- SERVER STARTUP -----");
  Serial.println("Device B (Server) - Standard BLE cycle");
 
  // Initialize BLE device
  BLEDevice::init("ESP32-C6_ServerB");
  Serial.println("BLE device initialized");
 
  // Create BLE server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  Serial.println("BLE server created");
 
  // Create BLE service
  BLEService *pService = pServer->createService(SERVICE_UUID);
  Serial.println("BLE service created");
 
  // Create BLE characteristic
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ |
                      BLECharacteristic::PROPERTY_WRITE |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
 
  // Set characteristic value
  pCharacteristic->setValue("Waiting for data...");
  pCharacteristic->setCallbacks(new MyCharacteristicCallbacks());
  Serial.println("BLE characteristic created");
 
  // Start service
  pService->start();
  Serial.println("BLE service started");
 
  // Start advertising service with specified settings
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  
  pAdvertising->setMinInterval(0x20); // 20ms
  pAdvertising->setMaxInterval(0x40); // 40ms
  
  BLEDevice::startAdvertising();
  Serial.println("BLE advertising started");
 
  Serial.println("BLE server ready. Waiting for client...");

  Serial.print("Server MAC address: ");
  Serial.println(BLEDevice::getAddress().toString().c_str());
  Serial.println("----- SERVER STARTUP COMPLETE -----\n");
}

void loop() {
  static bool lastConnectionState = false;
  static unsigned long lastStatusTime = 0;
  
  if (deviceConnected != lastConnectionState) {
    lastConnectionState = deviceConnected;
    Serial.println(deviceConnected ? "STATUS: Client active" : "STATUS: Client inactive");
  }

  if (millis() - lastStatusTime > 10000) {
    lastStatusTime = millis();
    Serial.print("STATUS REMINDER: Server ");
    Serial.println(deviceConnected ? "connected to client" : "waiting for client connection");
    Serial.print("Server MAC: ");
    Serial.println(BLEDevice::getAddress().toString().c_str());
  }

  delay(1000);
}