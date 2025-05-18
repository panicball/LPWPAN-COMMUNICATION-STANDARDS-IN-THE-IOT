/*
  ESP32-C6 Low-Power Mode Test

   ESP32-C6  low-power tests:
  - Light Sleep
  - Deep Sleep
  - Power Off (by externally pulling CHIP_PU low)
*/

// modes
//#define MODE_LIGHT_SLEEP
#define MODE_DEEP_SLEEP
//#define MODE_POWER_OFF  // This implies external hardware pulling CHIP_PU low

#include <Arduino.h>

void setupGPIOsForLowPower() {
  // Set all GPIOs to a state that draws minimal current (usually INPUT)
  for (int pin = 0; pin < 48; pin++) {
    pinMode(pin, INPUT); 
  }
}

void disablePeripherals() {
  // Turn off Wi-Fi / BT 
  // WiFi.mode(WIFI_OFF);
  // btStop();
}

void setup() {
  Serial.begin(115200);
  delay(100);

  // 1. Disable any peripherals that draw current
  disablePeripherals();

  // 2. Configure GPIOs to reduce leakage
  setupGPIOsForLowPower();
  delay(100);

#ifdef MODE_LIGHT_SLEEP
  Serial.println("==== LIGHT SLEEP MODE TEST ====");

  esp_sleep_enable_timer_wakeup(5ULL * 1000000ULL);

  // Light sleep
  Serial.println("Entering Light Sleep for 5 seconds...");
  // Flush Serial buffer
  Serial.flush();
  // Start light sleep
  esp_light_sleep_start();

  Serial.println("Woke up from Light Sleep.");

#elif defined(MODE_DEEP_SLEEP)
  Serial.println("==== DEEP SLEEP MODE TEST ====");

  esp_sleep_enable_timer_wakeup(10ULL * 1000000ULL);

  Serial.println("Entering Deep Sleep for 10 seconds...");
  Serial.flush();
  // Enter deep sleep
  esp_deep_sleep_start();


#elif defined(MODE_POWER_OFF)
  Serial.println("==== POWER OFF TEST ====");
  Serial.println("You must externally pull CHIP_PU low. Software alone can't do that.");
  Serial.println("Disconnect or switch off the supply after this message for measurement.");

#endif
}

void loop() {
}
