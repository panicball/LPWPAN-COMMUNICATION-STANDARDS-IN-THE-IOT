#include <Arduino.h>
#include <esp_sleep.h>
#include <WiFi.h>
#include <esp_pm.h>
#include <driver/rtc_io.h>

#define TEST_MODE 1  // 1=180μA mode, 2=35μA mode
#define TRIGGER_PIN 4

void setup() {
  Serial.begin(115200);
  delay(1000);
 
  pinMode(TRIGGER_PIN, OUTPUT);
  digitalWrite(TRIGGER_PIN, HIGH);
 
  Serial.println("ESP32-C6 Low-Power Test");
  
  if (TEST_MODE == 1) {
    Serial.println("Mode: Light-sleep (180μA)");
  } else {
    Serial.println("Mode: Deep light-sleep (35μA)");
  }
 
  Serial.println("Sleeping in 5s...");
  delay(5000);
 
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
 
  #ifdef BT_ENABLED
  btStop();
  #endif
  
  esp_sleep_enable_timer_wakeup(10 * 1000000);
  
  if (TEST_MODE == 1) {
    Serial.println("Sleeping (180μA)");
    Serial.flush();
    digitalWrite(TRIGGER_PIN, LOW);
    esp_light_sleep_start();
  } else {
    Serial.println("Killing peripherals");
   
    #if CONFIG_IDF_TARGET_ESP32C6
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_AUTO);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RC_FAST, ESP_PD_OPTION_AUTO);
    
    int pins[] = {0, 1, 2, 3, 5};
    
    for (int i = 0; i < 5; i++) {
      if (pins[i] != TRIGGER_PIN) {
        gpio_num_t gpio = static_cast<gpio_num_t>(pins[i]);
        if (rtc_gpio_is_valid_gpio(gpio)) {
          rtc_gpio_isolate(gpio);
        }
      }
    }
    
    esp_pm_config_t pm_config = {
      .max_freq_mhz = 160,
      .min_freq_mhz = 80,
      .light_sleep_enable = true
    };
    esp_pm_configure(&pm_config);
    #endif
    
    Serial.println("Sleeping (35μA)");
    Serial.flush();
    digitalWrite(TRIGGER_PIN, LOW);
    esp_light_sleep_start();
  }
 
  digitalWrite(TRIGGER_PIN, HIGH);
 
  Serial.println("Woke up");
}

void loop() {
  Serial.println("Active for 3s");
  delay(3000);
 
  if (TEST_MODE == 1) {
    Serial.println("Back to sleep (180μA)");
    Serial.flush();
    digitalWrite(TRIGGER_PIN, LOW);
    esp_light_sleep_start();
  } else {
    Serial.println("Disabling peripherals");
    
    #if CONFIG_IDF_TARGET_ESP32C6
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_AUTO);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RC_FAST, ESP_PD_OPTION_AUTO);
    
    int pins[] = {0, 1, 2, 3, 5};
    
    for (int i = 0; i < 5; i++) {
      if (pins[i] != TRIGGER_PIN) {
        gpio_num_t gpio = static_cast<gpio_num_t>(pins[i]);
        if (rtc_gpio_is_valid_gpio(gpio)) {
          rtc_gpio_isolate(gpio);
        }
      }
    }
    #else
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);
    esp_sleep_pd_config(ESP_PD_DOMAIN_XTAL, ESP_PD_OPTION_OFF);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RC_FAST, ESP_PD_OPTION_OFF);
    #endif
   
    Serial.println("Back to sleep (35μA)");
    Serial.flush();
    digitalWrite(TRIGGER_PIN, LOW);
    esp_light_sleep_start();
  }
 
  digitalWrite(TRIGGER_PIN, HIGH);
  Serial.println("Woke up");
}