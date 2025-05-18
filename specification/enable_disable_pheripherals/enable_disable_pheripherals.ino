#include <Arduino.h>
#include <WiFi.h>
#include "esp_wifi.h"
#include "driver/periph_ctrl.h"
#include "soc/periph_defs.h"
#include "esp_task_wdt.h"
#include "esp_pm.h"
#include "driver/rtc_io.h"
#include "soc/rtc.h"

#define LOAD_ITERATIONS 1000000

periph_module_t peripherals[] = {
    PERIPH_UART1_MODULE,
    PERIPH_I2C0_MODULE,
    PERIPH_I2S1_MODULE,
    PERIPH_TIMG0_MODULE,
    PERIPH_TIMG1_MODULE,
    PERIPH_LEDC_MODULE,
    PERIPH_RMT_MODULE,
    PERIPH_SPI_MODULE,
    PERIPH_SPI2_MODULE,
    PERIPH_PCNT_MODULE,
    PERIPH_SARADC_MODULE,
    PERIPH_SYSTIMER_MODULE
};

void setupWiFiModemSleep() {
    WiFi.mode(WIFI_STA);
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
}

void setupPowerManagement() {
    #ifdef CONFIG_PM_ENABLE
    esp_pm_config_esp32c6_t pm_config = {
        .max_freq_mhz = 160,
        .min_freq_mhz = 160,
        .light_sleep_enable = false
    };
    esp_pm_configure(&pm_config);
    #endif
    
    esp_wifi_set_ps(WIFI_PS_NONE);
    WiFi.mode(WIFI_AP_STA);
    
    #ifdef ESP_OK
    esp_wifi_set_max_tx_power(80);
    #endif
}

void enableAllPeripherals() {
    for (unsigned i = 0; i < sizeof(peripherals)/sizeof(peripherals[0]); i++) {
        periph_module_enable(peripherals[i]);
        delay(5);
    }
    
    #ifdef SOC_ADC_SUPPORTED
    analogReadResolution(12);
    
    pinMode(A0, INPUT);
    pinMode(A1, INPUT);
    pinMode(A2, INPUT);
    pinMode(A3, INPUT);
    
    for (int i = 0; i < 10; i++) {
        analogRead(A0);
        analogRead(A1);
        analogRead(A2);
        analogRead(A3);
    }
    #endif
    
    #ifdef WIRE_HAS_END
    Wire.begin();
    Wire.setClock(400000);
    #endif
    
    #ifdef SPI_HAS_BEGIN
    SPI.begin();
    #endif
    
    #ifdef LEDC_CHANNEL_0
    ledcSetup(0, 5000, 8);
    ledcSetup(1, 5000, 8);
    
    ledcAttachPin(12, 0);
    ledcAttachPin(13, 1);
    
    ledcWrite(0, 128);
    ledcWrite(1, 64);
    #endif
    
    delay(100);
}

void disableAllPeripherals() {
    for (unsigned i = 0; i < sizeof(peripherals)/sizeof(peripherals[0]); i++) {
        periph_module_disable(peripherals[i]);
        delay(5);
    }
    
    delay(100);
}

void intensiveCpuLoad() {
    volatile float result1 = 0.0, result2 = 0.0;
    volatile float a = 1.7345;
    volatile float b = 0.9734;
    volatile float c = 2.3451;
    volatile float d = 0.8712;
    
    const int MATRIX_SIZE = 100;
    volatile float** matrix = new volatile float*[MATRIX_SIZE];
    for (int i = 0; i < MATRIX_SIZE; i++) {
        matrix[i] = new volatile float[MATRIX_SIZE];
        for (int j = 0; j < MATRIX_SIZE; j++) {
            matrix[i][j] = sin(i * 0.01) * cos(j * 0.01);
        }
    }
    
    for (int i = 0; i < LOAD_ITERATIONS; i++) {
        result1 += sin(a) * cos(b) / sqrt(a * b + 0.01) + tan(a+b) * log(a*b + 1.5);
        result2 += sin(c) * cos(d) / sqrt(c * d + 0.01) + tan(c+d) * log(c*d + 1.5);
        
        a = a * 0.99991 + 0.00023;
        b = b * 0.99993 + 0.00017;
        c = c * 0.99995 + 0.00019;
        d = d * 0.99997 + 0.00013;
        
        if (i % 100 == 0) {
            for (int j = 0; j < 10; j++) {
                for (int k = 0; k < 10; k++) {
                    float sum = 0;
                    for (int l = 0; l < 10; l++) {
                        sum += matrix[j][l] * matrix[l][k];
                    }
                    matrix[j][k] = sum * 0.01 + result1;
                }
            }
        }
        
        if (i % 500 == 0) {
            int size = random(100, 1000);
            volatile float* temp = new volatile float[size];
            for (int j = 0; j < size; j++) {
                temp[j] = result1 * j + result2;
            }
            result1 += temp[random(0, size)];
            delete[] temp;
        }
        
        if (result1 > 1000000) result1 = 0;
        if (result2 > 1000000) result2 = 0;
    }
    
    for (int i = 0; i < MATRIX_SIZE; i++) {
        delete[] matrix[i];
    }
    delete[] matrix;
}

void measureAndPrint(const char* label, int frequency, bool enablePeripherals) {
    setCpuFrequencyMhz(frequency);
    delay(100);
    
    Serial.printf("\n=== %s @ %d MHz, Peripherals: %s ===\n",
                  label, getCpuFrequencyMhz(),
                  enablePeripherals ? "ON" : "OFF");
    
    if (enablePeripherals) {
        enableAllPeripherals();
    } else {
        disableAllPeripherals();
    }
    
    Serial.println("Starting in 3s...");
    Serial.flush();
    delay(3000);
    
    Serial.println("CPU test running");
    Serial.flush();
    delay(200);
    
    unsigned long startTime = millis();
    // intensiveCpuLoad();
    unsigned long runDuration = millis() - startTime;
    
    delay(200);
    Serial.printf("Test done: %lu ms\n", runDuration);
    
    Serial.println("Idle test in 2s...");
    Serial.flush();
    delay(2000);
    
    Serial.println("CPU idle for 5s");
    Serial.flush();
    
    delay(5000);
    
    Serial.println("=== Test End ===\n");
    delay(1000);
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    
    esp_task_wdt_deinit();
    
    setupPowerManagement();
    setupWiFiModemSleep();
    
    Serial.println();
    Serial.println("==============================================");
    Serial.println("ESP32-C6 Modem-sleep Test");
    Serial.println("==============================================");
    
    measureAndPrint("Modem-sleep", 80, true);
    
    Serial.println("Tests complete");
    Serial.println("Sleeping for 10s...");
    
    Serial.flush();
    delay(100);
    
    esp_sleep_enable_timer_wakeup(10 * 1000000);
    
    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER) {
        Serial.println("Woke from sleep");
    }
    
    Serial.println("Entering sleep...");
    Serial.flush();
    delay(100);
    
    esp_deep_sleep_start();
}

void loop() {
    delay(10000);
}