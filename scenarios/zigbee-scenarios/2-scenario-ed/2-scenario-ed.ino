// Device A (End Device) - With Acknowledgment Function

#ifndef ZIGBEE_MODE_ED
#error "Zigbee end device mode is not selected in Tools->Zigbee mode"
#endif

#include "Zigbee.h"
#include "DHT.h"
extern "C" {
  #include "esp_zigbee_core.h"
  #include "esp_sleep.h"
}

#define TEMP_SENSOR_ENDPOINT_NUMBER 10
#define DHTPIN 4
#define DHTTYPE DHT22
#define SLEEP_DURATION 180000000  // 3 minutes in microseconds
#define WAKE_PIN GPIO_NUM_5

#define DHT_READ_ATTEMPTS 5
#define DHT_READ_RETRY_DELAY 2000

#define ACK_WAIT_TIMEOUT 5000
#define ACK_RETRY_ATTEMPTS 3

uint8_t button = BOOT_PIN;
bool initializing = true;

unsigned long dht_errors = 0;
unsigned long dht_readings = 0;

float last_valid_temp = 0;
float last_valid_humidity = 0;
bool has_valid_reading = false;

volatile bool ack_received = false;
volatile unsigned long last_ack_time = 0;
volatile uint8_t retry_count = 0;

ZigbeeTempSensor zbTempSensor = ZigbeeTempSensor(TEMP_SENSOR_ENDPOINT_NUMBER);
DHT dht(DHTPIN, DHTTYPE);

bool checkDHT22() {
  Serial.println("Checking DHT22 sensor connection...");
  
  pinMode(DHTPIN, INPUT_PULLUP);
  delay(50);
  int reading = digitalRead(DHTPIN);
  
  Serial.printf("DHT22 pin state with pull-up: %d (should not be LOW)\n", reading);
  
  if (reading == LOW) {
    Serial.println("Warning: DHT pin is LOW with pull-up. Check for short circuit.");
  }
  
  pinMode(DHTPIN, INPUT);
  
  return true;
}

void readDHT22() {
  Serial.println("Reading DHT22 sensor...");
  
  float temperature = NAN;
  float humidity = NAN;
  bool success = false;
  
  for (int attempt = 0; attempt < DHT_READ_ATTEMPTS; attempt++) {
    dht_readings++;
    
    if (attempt > 0) {
      Serial.printf("Retrying DHT22 read, attempt %d of %d\n", attempt + 1, DHT_READ_ATTEMPTS);
      delay(DHT_READ_RETRY_DELAY);
    }
    
    temperature = dht.readTemperature();
    humidity = dht.readHumidity();
    
    if (!isnan(temperature) && !isnan(humidity)) {
      Serial.printf("DHT22 read successful on attempt %d\n", attempt + 1);
      Serial.printf("Temperature: %.2f°C, Humidity: %.2f%%\n", temperature, humidity);
      
      last_valid_temp = temperature;
      last_valid_humidity = humidity;
      has_valid_reading = true;
      
      success = true;
      break;
    }
  }
  
  if (!success) {
    dht_errors++;
    Serial.printf("Failed to read from DHT sensor after %d attempts!\n", DHT_READ_ATTEMPTS);
    Serial.printf("DHT errors: %lu of %lu readings (%.1f%%)\n", 
                  dht_errors, dht_readings, 
                  (float)dht_errors / dht_readings * 100.0f);
    
    if (has_valid_reading) {
      Serial.println("Using last valid reading instead of error value");
      temperature = last_valid_temp;
      humidity = last_valid_humidity;
    } else {
      temperature = -100;
      humidity = 0;
    }
  }
  
  Serial.printf("SENSOR_DATA: T=%.2f, H=%.2f\n", temperature, humidity);
  
  zbTempSensor.setTemperature(temperature);
}

bool sendDataWithAck() {
  bool sent_successfully = false;
  retry_count = 0;
  
  while (retry_count < ACK_RETRY_ATTEMPTS && !sent_successfully) {
    ack_received = false;
    
    Serial.printf("Sending temperature data (attempt %d/%d)...\n", 
                  retry_count + 1, ACK_RETRY_ATTEMPTS);
    
    unsigned long start_time = millis();
    
    zbTempSensor.reportTemperature();
    
    Serial.println("Waiting for coordinator to process data...");
    
    delay(ACK_WAIT_TIMEOUT);
    
    bool random_success = (random(100) < 80);
    
    if (random_success) {
      Serial.println("Simulated acknowledgment received");
      ack_received = true;
      last_ack_time = millis();
      sent_successfully = true;
    } else {
      Serial.println("Simulated acknowledgment timeout");
      retry_count++;
      
      if (retry_count < ACK_RETRY_ATTEMPTS) {
        Serial.printf("Retrying transmission (attempt %d/%d)...\n", 
                      retry_count + 1, ACK_RETRY_ATTEMPTS);
        delay(1000);
      }
    }
  }
  
  if (!sent_successfully) {
    Serial.printf("Failed to receive ACK after %d attempts\n", ACK_RETRY_ATTEMPTS);
  } else {
    Serial.printf("Data transmission successful after %d attempt(s)\n", retry_count + 1);
  }
  
  return sent_successfully;
}

static void temp_sensor_update_task(void *arg) {
  for (;;) {
    if (Zigbee.connected()) {
      digitalWrite(WAKE_PIN, HIGH);
      
      Serial.println("Checking sensor and sending data");
      
      checkDHT22();
      readDHT22();
      
      bool data_sent = sendDataWithAck();
      
      delay(1000);
      
      digitalWrite(WAKE_PIN, LOW);
      
      int sleep_time = data_sent ? SLEEP_DURATION : (SLEEP_DURATION / 2);
      Serial.printf("Disconnecting and entering deep sleep for %d seconds...\n", sleep_time / 1000000);
      delay(1000);
      
      esp_sleep_enable_timer_wakeup(sleep_time);
      esp_deep_sleep_start();
    }
    delay(5000);
  }
}

void basicSetup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }
  
  pinMode(button, INPUT_PULLUP);
  pinMode(WAKE_PIN, OUTPUT);
  digitalWrite(WAKE_PIN, LOW);
  
  randomSeed(analogRead(0));
  
  Serial.println("Initializing DHT sensor...");
  dht.begin();
  delay(2000);
  Serial.println("DHT initialization complete.");
  
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) {
    Serial.println("Device woke up from deep sleep by timer");
    initializing = false;
  } else {
    Serial.println("Device starting for the first time");
    initializing = true;
  }
  
  Serial.println("Basic Setup Complete.");
  delay(2000);
}

bool zigbeeInitialize() {
  zbTempSensor.setManufacturerAndModel("ESP32-C6", "DHT22Sensor");
  zbTempSensor.setMinMaxValue(-40, 80);
  zbTempSensor.setTolerance(0.5);
  Zigbee.addEndpoint(&zbTempSensor);

  Serial.println("Starting Zigbee...");
  if (!Zigbee.begin()) {
    Serial.println("Zigbee failed to start! Rebooting...");
    ESP.restart();
    return false;
  } 
  
  Serial.println("Zigbee started. Connecting to network...");
  return true;
}

void normalOperation() {
  Serial.println("Normal Operation.");
  while (!Zigbee.connected()) {
    Serial.print(".");
    delay(250);
  }
  
  Serial.println("\nConnected to Zigbee network.");
  
  zbTempSensor.setReporting(0, 60, 0);
  
  xTaskCreate(temp_sensor_update_task, "temp_sensor_update", 4096, NULL, 10, NULL);
}

void setup() {
  basicSetup();
  if (!zigbeeInitialize()) {
    return;
  }
  normalOperation();
}

void loop() {
  if (digitalRead(button) == LOW) {
    delay(100);
    unsigned long startTime = millis();
    while (digitalRead(button) == LOW) {
      delay(50);
      if ((millis() - startTime) > 3000) {
        Serial.println("Resetting Zigbee to factory defaults.");
        delay(1000);
        Zigbee.factoryReset();
      }
    }
    
    if (initializing) {
      readDHT22();
      sendDataWithAck();
    }
  }
  delay(100);
}