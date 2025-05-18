#include <Arduino.h>
#include <esp_sleep.h>
#include <WiFi.h>

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("Entering Light-sleep in 3 seconds...");
  delay(3000);

  // Disable Wi-Fi to reduce current consumption
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  esp_sleep_enable_timer_wakeup(5ULL * 1000000ULL);

  Serial.println("Entering Light-sleep now");
  esp_light_sleep_start();
  delay(1000);

  Serial.println("Woke up from Light-sleep");
}

void loop() {
  delay(2000);
  Serial.println("Going back to Light-sleep...");
  esp_light_sleep_start();
  Serial.println("Woke up again!");
}
