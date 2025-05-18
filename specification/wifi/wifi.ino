#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_sleep.h>
#include <cmath> 

const char* ssid = "test";
const char* password = "test";

const int TEST_DURATION_MS = 15000;
const bool FORCE_MAXIMUM_POWER = true;

unsigned long testStartMillis = 0;
unsigned long currentTestElapsedTime = 0;

#define TEST_MODE TX_802_11AX

enum TestMode {
  TX_802_11B,
  TX_802_11G,
  TX_802_11N_HT20,
  TX_802_11N_HT40,
  TX_802_11AX
};

TestMode currentMode = TEST_MODE;
unsigned long testStartTime = 0;

void setupWiFi();
void configureTestMode(TestMode mode);
void runTest();
void startTxTraffic();
void startCpuLoadTask();
void connectToWiFi();
void printCurrentMode();

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  if (wakeup_reason != ESP_SLEEP_WAKEUP_UNDEFINED) {
    Serial.println("Woke up from deep sleep! Reason code: " + String(wakeup_reason));
  }
  
  Serial.println("ESP32-C6 Wi-Fi Current Consumption Test");
  Serial.println("===========================================================");
  Serial.println("Test runs for " + String(TEST_DURATION_MS/1000) + " seconds");
  
  setupWiFi();
  runTest();
}

void loop() {
  currentTestElapsedTime = millis() - testStartMillis;
  
  static unsigned long lastTimePrint = 0;
  if (millis() - lastTimePrint >= 1000) {
    Serial.printf("Time: %d.%d s\n", 
                  currentTestElapsedTime / 1000, 
                  (currentTestElapsedTime % 1000) / 100);
    lastTimePrint = millis();
  }

  if (millis() - testStartTime >= TEST_DURATION_MS) {
    unsigned long actualTestDuration = millis() - testStartMillis;
    Serial.println("Test completed.");
    Serial.printf("Duration: %lu ms (%.1f s)\n", 
                  actualTestDuration, 
                  actualTestDuration / 1000.0);
    
    Serial.println("\n====================");
    Serial.println("Entering deep sleep...");
    delay(1000);
    
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    
    esp_sleep_enable_timer_wakeup(0);
    esp_deep_sleep_start();
  }
  
  delay(100);
}

void runTest() {
  Serial.println("\n-----------------------------------------");
  
  configureTestMode(currentMode);
  printCurrentMode();
  
  testStartTime = millis();
  testStartMillis = millis();
  currentTestElapsedTime = 0;
  Serial.println("Test started at: " + String(testStartMillis) + "ms");
}

void setupWiFi() {
  WiFi.mode(WIFI_MODE_STA);
  Serial.println("Wi-Fi initialized");
}

void connectToWiFi() {
  Serial.print("Connecting to ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  
  const int MAX_CONNECTION_ATTEMPTS = 60;
  int attempts = 0;
  
  while (WiFi.status() != WL_CONNECTED && attempts < MAX_CONNECTION_ATTEMPTS) {
    delay(500);
    Serial.print(".");
    attempts++;
    
    if (attempts % 10 == 0) {
      Serial.println("\nRetrying connection...");
      
      WiFi.disconnect(true);
      delay(1000);
      
      if (attempts == 10) {
        WiFi.begin(ssid, password, 1);
      } else if (attempts == 20) {
        WiFi.mode(WIFI_OFF);
        delay(1000);
        WiFi.mode(WIFI_STA);
        delay(1000);
        WiFi.begin(ssid, password);
      } else if (attempts == 30) {
        esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G);
        WiFi.begin(ssid, password);
      } else if (attempts == 40) {
        esp_wifi_set_max_tx_power(78);
        WiFi.begin(ssid, password);
      } else if (attempts == 50) {
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        delay(1000);
        WiFi.mode(WIFI_STA);
        esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B);
        WiFi.begin(ssid, password);
      }
    }
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWi-Fi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("RSSI: ");
    Serial.println(WiFi.RSSI());
    esp_wifi_set_max_tx_power(84);
    esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G);
  } else {
    Serial.println("\nFailed to connect after multiple attempts");
    ESP.restart();
  }
}

void configureTestMode(TestMode mode) {
  Serial.println("Test started");
  
  wifi_config_t conf;
  esp_wifi_get_config(WIFI_IF_STA, &conf);
  
  WiFi.disconnect(true);
  delay(100);
  
  if (FORCE_MAXIMUM_POWER) {
    esp_wifi_set_ps(WIFI_PS_NONE);
    setCpuFrequencyMhz(160);
    esp_wifi_set_inactive_time(WIFI_IF_STA, 0);
  }
  
  switch (mode) {
    case TX_802_11B:
      esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B);
      esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT20);
      esp_wifi_set_max_tx_power(84);
      esp_wifi_set_ps(WIFI_PS_NONE);
      connectToWiFi();
      break;
      
    case TX_802_11G:
      esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G);
      esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT20);
      esp_wifi_set_max_tx_power(84);
      esp_wifi_set_ps(WIFI_PS_NONE);
      connectToWiFi();
      break;
      
    case TX_802_11N_HT20:
      esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
      esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT20);
      esp_wifi_set_max_tx_power(84);
      esp_wifi_set_ps(WIFI_PS_NONE);
      connectToWiFi();
      break;
      
    case TX_802_11N_HT40:
      esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
      esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT40);
      esp_wifi_set_max_tx_power(84);
      esp_wifi_set_ps(WIFI_PS_NONE);
      connectToWiFi();
      break;
      
    case TX_802_11AX:
      esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N | WIFI_PROTOCOL_11AX);
      esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT20);
      esp_wifi_set_max_tx_power(84);
      esp_wifi_set_ps(WIFI_PS_NONE);
      connectToWiFi();
      break;
  }

  if (WiFi.status() == WL_CONNECTED) {
    delay(500);
    startTxTraffic();
    startCpuLoadTask();
  }
}

void printCurrentMode() {
  Serial.println("\nCurrent Test Mode:");
  
  switch (currentMode) {
    case TX_802_11B:
      Serial.println("TX: 802.11b @ 20.5 dBm (Expected: 382 mA)");
      break;
    case TX_802_11G:
      Serial.println("TX: 802.11g @ 19.0 dBm (Expected: 316 mA)");
      break;
    case TX_802_11N_HT20:
      Serial.println("TX: 802.11n HT20 @ 18.0 dBm (Expected: 295 mA)");
      break;
    case TX_802_11N_HT40:
      Serial.println("TX: 802.11n HT40 @ 17.5 dBm (Expected: 280 mA)");
      break;
    case TX_802_11AX:
      Serial.println("TX: 802.11ax @ 15.5 dBm (Expected: 251 mA)");
      break;
  }
}

void startTxTraffic() {
  Serial.println("Starting UDP traffic load");
  
  xTaskCreate(
    [](void* parameter) {
      WiFiUDP udp;
      IPAddress broadcastIP(255, 255, 255, 255);
      const int packetSize = 1024; 
      uint8_t buffer[1024];
      
      for (int i = 0; i < packetSize; i++) {
        buffer[i] = i & 0xFF;
      }
      
      while (true) {
        for (int burst = 0; burst < 5; burst++) {
          udp.beginPacket(broadcastIP, 65000);
          udp.write(buffer, packetSize);
          udp.endPacket();
          delay(1);
        }
        delay(2);
      }
    },
    "UDP_TX_Task",
    4096,
    NULL,
    1,
    NULL
  );
}

void startCpuLoadTask() {
  xTaskCreate(
    [](void* parameter) {
      volatile int result = 0;
      while (true) {
        for (int i = 0; i < 1000; i++) {
          result += i * i;
        }
        delay(5);
      }
    },
    "CPU_LOAD_Task",
    2048,
    NULL,
    1,
    NULL
  );
}