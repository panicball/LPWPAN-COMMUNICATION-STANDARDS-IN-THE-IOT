#ifndef ZIGBEE_MODE_ED
#error "Zigbee end device mode is not selected in Tools->Zigbee mode"
#endif

#include "Zigbee.h"

#define TEMP_SENSOR_ENDPOINT_NUMBER 10

#define uS_TO_S_FACTOR 1000000ULL
#define TIME_TO_SLEEP  55         

ZigbeeTempSensor zbTempSensor = ZigbeeTempSensor(TEMP_SENSOR_ENDPOINT_NUMBER);

/************************ Temp sensor *****************************/
void meausureAndSleep() {
  float temperature = temperatureRead();
  float humidity = temperature;
  zbTempSensor.setTemperature(temperature);
  zbTempSensor.setHumidity(humidity);

  zbTempSensor.report();
  Serial.printf("Reported temperature: %.2f°C, Humidity: %.2f%%\r\n", temperature, humidity);

  delay(100);
  Serial.println("Going to sleep now");
  esp_deep_sleep_start();
}

/********************* Arduino functions **************************/
void setup() {
  Serial.begin(115200);

  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  zbTempSensor.setManufacturerAndModel("Espressif", "SleepyZigbeeTempSensor");
  zbTempSensor.setMinMaxValue(10, 50);

  zbTempSensor.setTolerance(1);
  zbTempSensor.addHumiditySensor(0, 100, 1);
  Zigbee.addEndpoint(&zbTempSensor);

  esp_zb_cfg_t zigbeeConfig = ZIGBEE_DEFAULT_ED_CONFIG();
  zigbeeConfig.nwk_cfg.zed_cfg.keep_alive = 10000;

  Zigbee.setTimeout(10000);

  if (!Zigbee.begin(&zigbeeConfig, false)) {
    Serial.println("Zigbee failed to start!");
    Serial.println("Rebooting...");
    ESP.restart();
  }
  Serial.println("Connecting to network");
  while (!Zigbee.connected()) {
    Serial.print(".");
    delay(100);
  }
  Serial.println();
  Serial.println("Successfully connected to Zigbee network");
  delay(1000);
}

void loop() {
  meausureAndSleep();
}