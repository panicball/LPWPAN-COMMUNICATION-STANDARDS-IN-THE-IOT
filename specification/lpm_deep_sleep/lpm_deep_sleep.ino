#include <Arduino.h>
#include <esp_sleep.h>
#include <WiFi.h>
#include <driver/adc.h>
#include "esp_adc_cal.h"


void setup() {
  Serial.begin(115200);
  delay(1000);

  // Serial.println("ESP32 Deep-Sleep Current Measurement");
  // Serial.println("Deep-sleep (7μA) - Only RTC timer and LP memory powered");
  
  delay(5000);
  
  // Disable WiFi
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  
  // Disable Bluetooth
  btStop();
  
  // Disable ADC
  // esprv_int_disable();
  
  // Configure all possible power domains to be powered down
  // esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
  // esp_sleep_pd_config(ESP_PD_DOMAIN_XTAL, ESP_PD_OPTION_AUTO);
  // esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);  // Keep LP memory on
  // esp_sleep_pd_config(ESP_PD_DOMAIN_RC_FAST, ESP_PD_OPTION_ON);
  
  // Set wake-up timer (10 seconds)
  esp_sleep_enable_timer_wakeup(10ULL * 1000000ULL);
  
  Serial.println("Entering Deep-sleep (7μA mode) now");
  Serial.flush(); // Make sure all serial data is sent
  
  // Enter deep sleep mode
  esp_deep_sleep_start();
}

void loop() {
}