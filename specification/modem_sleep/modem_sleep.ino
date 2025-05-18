/**
 * ESP32-C6 Power Consumption Test
 * 
 * This sketch tests different power consumption modes of the ESP32-C6 including:
 * - Active mode (TX and RX)
 * - Modem-sleep mode at different CPU frequencies (80/160 MHz)
 * - Different peripheral states (enabled/disabled)
 */

#include <Arduino.h>
#include "esp_wifi.h"
#include "esp_sleep.h"
#include "esp_pm.h"
#include "driver/rtc_io.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_bt.h"
#include <WiFi.h>

#define ACTIVE_MODE_TEST_DURATION_MS 10000
#define MODEM_SLEEP_TEST_DURATION_MS 10000
#define DELAY_BETWEEN_TESTS_MS 3000

void setupWiFi();
void disableAllPeripherals();
void enableAllPeripherals();
void testActiveMode();
void testModemSleep(uint8_t cpuFreqMHz, bool peripheralsEnabled, bool cpuRunning);

void setup() {
  // // Initialize serial communication
  // Serial.begin(115200);
  // delay(1000); 
}

void loop() {
  // // Test 1: Active Mode - TX current consumption at 100% duty cycle
  // testActiveMode();
  
  // delay(DELAY_BETWEEN_TESTS_MS);
  
  // // Test 2: Modem-sleep Mode (CPU 160MHz, running, peripherals disabled)
  // testModemSleep(160, false, true);
  // delay(5000);
  
  // delay(DELAY_BETWEEN_TESTS_MS);
  
  // // Test 3: Modem-sleep Mode (CPU 160MHz, idle, peripherals disabled)
  // testModemSleep(160, false, false);
  // // delay(5000);
  
  // delay(DELAY_BETWEEN_TESTS_MS);
  
  // Test 4: Modem-sleep Mode (CPU 80MHz, running, peripherals disabled)
  testModemSleep(80, false, true);
  delay(5000);
  
  // delay(DELAY_BETWEEN_TESTS_MS);
  
  // // Test 5: Modem-sleep Mode (CPU 80MHz, idle, peripherals disabled)
  // testModemSleep(80, false, false);
  // delay(5000);
  
  // delay(DELAY_BETWEEN_TESTS_MS);
  
  // // Test 6: Modem-sleep Mode (CPU 160MHz, running, peripherals enabled)
  // testModemSleep(160, true, true);
  // delay(5000);
  
  // delay(DELAY_BETWEEN_TESTS_MS);
  
  // // Test 7: Modem-sleep Mode (CPU 160MHz, idle, peripherals enabled)
  // testModemSleep(160, true, false);
  // delay(5000);
  
  // delay(DELAY_BETWEEN_TESTS_MS);
  
  // // Test 8: Modem-sleep Mode (CPU 80MHz, running, peripherals enabled)
  // testModemSleep(80, true, true);
  // delay(5000);
  
  // delay(DELAY_BETWEEN_TESTS_MS);
  
  // // Test 9: Modem-sleep Mode (CPU 80MHz, idle, peripherals enabled)
  // testModemSleep(80, true, false);
  // delay(5000);
  
  // delay(DELAY_BETWEEN_TESTS_MS);
  
  // delay(10000);
  esp_sleep_enable_timer_wakeup(10 * 1000000); // 10 seconds in microseconds
  Serial.println("Going to sleep now");
  Serial.flush(); 
  esp_deep_sleep_start();
}

// Configure WiFi for testing
void setupWiFi() {
  WiFi.mode(WIFI_MODE_STA);
  WiFi.disconnect();
  delay(100);
}

// Disable all peripherals to minimize power consumption
void disableAllPeripherals() {
  esp_wifi_stop();
  esp_bt_controller_disable();
}

// Enable all peripherals
void enableAllPeripherals() {
  esp_wifi_start();
  esp_bt_controller_enable(ESP_BT_MODE_BLE);
}

// Test active mode with TX at 100% duty cycle
void testActiveMode() {
  setupWiFi();
  WiFi.mode(WIFI_MODE_STA);
  
  // Enable continuous transmission
  esp_wifi_set_ps(WIFI_PS_NONE);
  
  unsigned long startTime = millis();
  while (millis() - startTime < ACTIVE_MODE_TEST_DURATION_MS) {
    // Keep CPU busy to simulate 100% duty cycle
    delay(100);
    Serial.print(".");
  }
  
  Serial.println("\nActive mode test completed");
}

// Test modem-sleep mode with different configurations
void testModemSleep(uint8_t cpuFreqMHz, bool peripheralsEnabled, bool cpuRunning) {
  // Set CPU frequency
  setCpuFrequencyMhz(cpuFreqMHz);
  
  // Configure peripherals
  if (peripheralsEnabled) {
    enableAllPeripherals();
  } else {
    disableAllPeripherals();
  }
  
  // Configure WiFi power save mode (modem-sleep)
  WiFi.mode(WIFI_MODE_STA);
  esp_wifi_set_ps(WIFI_PS_MIN_MODEM); // Using MIN_MODEM for ESP32-C6
  
  // Wait for the specified duration
  unsigned long startTime = millis();
  while (millis() - startTime < MODEM_SLEEP_TEST_DURATION_MS) {
    if (cpuRunning) {
      // Keep CPU running
      // Perform some calculation to keep CPU active
      volatile long sum = 0;
      for (volatile int i = 0; i < 10000; i++) {
        sum += i;
      }
      Serial.print(".");
      delay(100);
    } else {
      delay(100);
      Serial.print(".");
    }
  }
  
  Serial.println("\nModem-sleep test completed");
  
  // Reset to default CPU frequency
  setCpuFrequencyMhz(160);
}