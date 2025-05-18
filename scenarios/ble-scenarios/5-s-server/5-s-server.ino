// Device B (Server) - Data reception and response sending

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHAR_TEMP_UUID      "beb5483e-36e1-4688-b7f5-ea07361b26a8" // For temperature reception
#define CHAR_RESPONSE_UUID  "beb5483e-36e1-4688-b7f5-ea07361b26a9" // For response sending

BLEServer *pServer = NULL;
BLECharacteristic *pTempCharacteristic = NULL;
BLECharacteristic *pResponseCharacteristic = NULL;
bool deviceConnected = false;
String receivedData = "";

class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
    Serial.println("***SERVER REPORTS: Client connected***");
  }
  
  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    Serial.println("***SERVER REPORTS: Client disconnected***");
   
    Serial.println("Restarting advertising...");
    pServer->startAdvertising();
    Serial.println("Advertising renewed.");
  }
};

class TempCharacteristicCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    Serial.println("***SERVER REPORTS: Temperature data received!***");
    
    uint8_t* pData = pCharacteristic->getData();
    size_t dataLength = pCharacteristic->getLength();
    
    if (dataLength > 0) {
      receivedData = "";
      for (int i = 0; i < dataLength; i++) {
        receivedData += (char)pData[i];
      }
      
      Serial.println("Data received: " + receivedData);
      
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
        
        String response = "CONFIRMATION: T=" + String(temperature) + "°C, H=" + String(humidity) + "% RECEIVED " + String(millis());
        
        pResponseCharacteristic->setValue(response.c_str());
        
        Serial.println("Sending notification: " + response);
        pResponseCharacteristic->notify();
        Serial.println("Notification sent!");
      }
    } else {
      Serial.println("ERROR: Empty data received");
    }
  }
  
  void onRead(BLECharacteristic *pCharacteristic) {
    Serial.println("***SERVER REPORTS: Client reading temperature data***");
  }
};

class ResponseCharacteristicCallbacks: public BLECharacteristicCallbacks {
  void onRead(BLECharacteristic *pCharacteristic) {
    Serial.println("***SERVER REPORTS: Client reading response data***");
  }
  
  void onNotify(BLECharacteristic *pCharacteristic) {
    Serial.println("***SERVER REPORTS: Response notification sent***");
  }
};

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n----- SERVER STARTUP (SCENARIO 5) -----");
  Serial.println("Device B (Server) - Two-way BLE communication");
 
  BLEDevice::init("ESP32-C6_Server_S5");
  Serial.println("BLE device initialized");
 
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  Serial.println("BLE server created");
 
  BLEService *pService = pServer->createService(SERVICE_UUID);
  Serial.println("BLE service created");
 
  pTempCharacteristic = pService->createCharacteristic(
                      CHAR_TEMP_UUID,
                      BLECharacteristic::PROPERTY_READ |
                      BLECharacteristic::PROPERTY_WRITE
                    );
 
  pTempCharacteristic->setValue("Waiting for temperature data...");
  pTempCharacteristic->setCallbacks(new TempCharacteristicCallbacks());
  Serial.println("Temperature characteristic created");
  
  pResponseCharacteristic = pService->createCharacteristic(
                      CHAR_RESPONSE_UUID,
                      BLECharacteristic::PROPERTY_READ |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
  
  pResponseCharacteristic->setValue("Waiting to send response...");
  pResponseCharacteristic->setCallbacks(new ResponseCharacteristicCallbacks());
  Serial.println("Response characteristic created");
 
  pService->start();
  Serial.println("BLE service started");
 
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