#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_sleep.h>
#include <WiFiUdp.h>

const char* ssid = "test";
const char* password = "test";

const unsigned int RX_UDP_PORT = 4210;
WiFiUDP udpServer;

const int TEST_PIN = 5;
const int TEST_DURATION_MS = 15000;
const int DELAY_BETWEEN_TESTS_MS = 2000;
const bool FORCE_MAXIMUM_POWER = true;

unsigned long packetsReceived = 0;
unsigned long totalBytesReceived = 0;
unsigned long lastStatTime = 0;
const int STAT_INTERVAL_MS = 1000;

unsigned long testStartMillis = 0;
unsigned long currentTestElapsedTime = 0;

enum TestMode {
  RX_HT20,
  RX_HT40,
  RX_HE20,
  TEST_COUNT
};

TestMode currentMode = RX_HT20;
unsigned long testStartTime = 0;

void setupWiFi();
void configureTestMode(TestMode mode);
void printCurrentMode();
void setTestPin(bool state);
void runNextTest();
void startUdpServer();
void checkForUdpData();

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  if (wakeup_reason != ESP_SLEEP_WAKEUP_UNDEFINED) {
    Serial.println("Woke from sleep: " + String(wakeup_reason));
  }
  
  pinMode(TEST_PIN, OUTPUT);
  digitalWrite(TEST_PIN, LOW);
  
  Serial.println("ESP32-C6 Wi-Fi RX Current Test");
  Serial.println("===============================");
  Serial.println("Test PIN: GPIO" + String(TEST_PIN));
  Serial.println("Each test: " + String(TEST_DURATION_MS/1000) + "s");
  
  WiFi.setHostname("ESP32-C6-RX-Test");
  setupWiFi();
  runNextTest();
}

void loop() {
  currentTestElapsedTime = millis() - testStartMillis;
  
  checkForUdpData();
  
  if (millis() - lastStatTime >= STAT_INTERVAL_MS) {
    Serial.printf("RX: %d.%ds, Pkts: %lu, Bytes: %lu\n", 
                  currentTestElapsedTime / 1000, 
                  (currentTestElapsedTime % 1000) / 100,
                  packetsReceived,
                  totalBytesReceived);
    lastStatTime = millis();
  }

  if (millis() - testStartTime >= TEST_DURATION_MS) {
    setTestPin(LOW);
    
    unsigned long actualDuration = millis() - testStartMillis;
    Serial.println("Test done. PIN LOW.");
    Serial.printf("Duration: %lu ms (%.1fs)\n", actualDuration, actualDuration / 1000.0);
    Serial.printf("Total pkts: %lu, bytes: %lu\n", packetsReceived, totalBytesReceived);
    
    packetsReceived = 0;
    totalBytesReceived = 0;
    
    if (currentMode == TEST_COUNT - 1) {
      Serial.println("\n==========================");
      Serial.println("ALL TESTS COMPLETED");
      Serial.println("Restarting in 5s...");
      delay(5000);
      ESP.restart();
    } else {
      udpServer.stop();
      
      Serial.println("Waiting " + String(DELAY_BETWEEN_TESTS_MS/1000) + "s...");
      delay(DELAY_BETWEEN_TESTS_MS);
      
      currentMode = static_cast<TestMode>(currentMode + 1);
      runNextTest();
    }
  }
  
  delay(10);
}

void runNextTest() {
  Serial.println("\n--------------------------------");
  Serial.println("Test " + String(currentMode + 1) + "/" + String(TEST_COUNT));
  
  configureTestMode(currentMode);
  printCurrentMode();
  
  testStartTime = millis();
  testStartMillis = millis();
  currentTestElapsedTime = 0;
  lastStatTime = millis();
  
  Serial.println("Started: " + String(testStartMillis) + "ms");
  Serial.println("UDP port: " + String(RX_UDP_PORT));
}

void setupWiFi() {
  WiFi.mode(WIFI_MODE_STA);
  esp_wifi_set_max_tx_power(84);
  esp_wifi_set_ps(WIFI_PS_NONE);
  WiFi.setAutoReconnect(true);
}

void configureTestMode(TestMode mode) {
  setTestPin(HIGH);
  
  wifi_config_t conf;
  esp_wifi_get_config(WIFI_IF_STA, &conf);
  
  WiFi.disconnect(false);
  delay(500);
  
  if (FORCE_MAXIMUM_POWER) {
    esp_wifi_set_ps(WIFI_PS_NONE);
    setCpuFrequencyMhz(240);
    esp_wifi_set_inactive_time(WIFI_IF_STA, 0);
  }
  
  switch (mode) {
    case RX_HT20:
      esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
      esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT20);
      esp_wifi_set_ps(WIFI_PS_NONE);
      break;
      
    case RX_HT40:
      esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
      esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT40);
      esp_wifi_set_ps(WIFI_PS_NONE);
      break;
      
    case RX_HE20:
      esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N | WIFI_PROTOCOL_11AX);
      esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT20);
      esp_wifi_set_ps(WIFI_PS_NONE);
      break;
  }

  Serial.print("Connecting to ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  int maxAttempts = 30;
  
  while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
    delay(500);
    Serial.print(".");
    attempts++;
    
    if (attempts % 10 == 0) {
      Serial.println("\nRetrying...");
      WiFi.disconnect(true);
      delay(1000);
      WiFi.begin(ssid, password);
    }
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nFailed to connect. Restarting...");
    delay(1000);
    ESP.restart();
  }
  
  Serial.println("\nConnected");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  
  startUdpServer();
}

void printCurrentMode() {
  Serial.println("\nMode:");
  
  setTestPin(HIGH);
  
  switch (currentMode) {
    case RX_HT20:
      Serial.println("RX: 802.11n HT20 (Expected: 78 mA)");
      break;
    case RX_HT40:
      Serial.println("RX: 802.11n HT40 (Expected: 82 mA)");
      break;
    case RX_HE20:
      Serial.println("RX: 802.11ax HE20 (Expected: 78 mA)");
      break;
  }
}

void setTestPin(bool state) {
  digitalWrite(TEST_PIN, state);
}

void startUdpServer() {
  udpServer.begin(RX_UDP_PORT);
  Serial.printf("UDP listening on %d\n", RX_UDP_PORT);
}

void checkForUdpData() {
  int packetSize = udpServer.parsePacket();
  
  if (packetSize) {
    packetsReceived++;
    totalBytesReceived += packetSize;
    
    char buffer[packetSize];
    udpServer.read(buffer, packetSize);
  }
}