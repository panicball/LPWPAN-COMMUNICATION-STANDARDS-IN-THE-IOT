// Device A (End Device) - Temperature measurement and sending

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
#define SLEEP_DURATION 180000000
#define WAKE_PIN GPIO_NUM_5

#define DHT_READ_ATTEMPTS 5
#define DHT_READ_RETRY_DELAY 2000

bool initializing = true;

unsigned long dht_errors = 0;
unsigned long dht_readings = 0;

float last_valid_temp = 0;
float last_valid_humidity = 0;
bool has_valid_reading = false;

float sensor_min_temp = -40.0f;
float sensor_max_temp = 80.0f;
float sensor_tolerance = 0.5f;

ZigbeeTempSensor zbTempSensor = ZigbeeTempSensor(TEMP_SENSOR_ENDPOINT_NUMBER);
DHT dht(DHTPIN, DHTTYPE);

void requestSensorSettings() {
  Serial.println("Requesting sensor settings via serial simulation...");
  
  Serial.printf("Current Sensor Config: min %.2f°C, max %.2f°C, tol %.2f°C\n", 
                sensor_min_temp, sensor_max_temp, sensor_tolerance);
}

void updateSensorConfiguration(float min_temp, float max_temp, float tolerance) {
  Serial.printf("Updating Sensor Config: min %.2f°C, max %.2f°C, tol %.2f°C\n",
                min_temp, max_temp, tolerance);
  
  sensor_min_temp = min_temp;
  sensor_max_temp = max_temp;
  sensor_tolerance = tolerance;
  
  zbTempSensor.setMinMaxValue(sensor_min_temp, sensor_max_temp);
  zbTempSensor.setTolerance(sensor_tolerance);
}

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
    
    Serial.println("Troubleshooting tips:");
    Serial.println("1. Check DHT22 power connection");
    Serial.println("2. Verify data pin connection and pull-up resistor");
    Serial.println("3. Try external power source");
    Serial.println("4. Ensure cable length is short");
    
    if (has_valid_reading) {
      Serial.println("Using last valid reading");
      temperature = last_valid_temp;
      humidity = last_valid_humidity;
    } else {
      temperature = -100;
      humidity = 0;
    }
  }
  
  Serial.printf("Sending temperature: %.2f°C\n", temperature);
  zbTempSensor.setTemperature(temperature);
  zbTempSensor.reportTemperature();
}

static void temp_sensor_update_task(void *arg) {
  for (;;) {
    if (Zigbee.connected()) {
      digitalWrite(WAKE_PIN, HIGH);
      
      Serial.println("Checking sensor and sending data");
      
      checkDHT22();
      readDHT22();
      
      requestSensorSettings();
      
      delay(1000);
      
      digitalWrite(WAKE_PIN, LOW);
      
      Serial.printf("Disconnecting from network and entering deep sleep for %d seconds...\n", SLEEP_DURATION / 1000000);
      delay(1000);
      
      esp_sleep_enable_timer_wakeup(SLEEP_DURATION);
      esp_deep_sleep_start();
    }
    delay(5000);
  }
}

void basicSetup() {
  Serial.begin(115200);
  while (!Serial) delay(10);
  
  pinMode(WAKE_PIN, OUTPUT);
  digitalWrite(WAKE_PIN, LOW);
  
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
  zbTempSensor.setMinMaxValue(sensor_min_temp, sensor_max_temp);
  zbTempSensor.setTolerance(sensor_tolerance);
  Zigbee.addEndpoint(&zbTempSensor);

  Serial.println("Starting Zigbee...");
  if (!Zigbee.begin()) {
    Serial.println("Zigbee failed to start! Rebooting...");
    ESP.restart();
    return false;
  } 
  
  Serial.println("Zigbee started. Searching for coordinator...");
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
  
  requestSensorSettings();
  
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
  delay(100);
}