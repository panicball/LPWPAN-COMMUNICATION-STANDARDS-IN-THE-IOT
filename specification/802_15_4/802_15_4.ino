/**
 * Tests 802.15.4 in different modes
 */

#include "esp_ieee802154.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_sleep.h"

static const char* TAG = "POWER_TEST";

// For CPU loading
volatile uint32_t dummy_counter = 0;

// Test config struct
typedef struct {
  const char* mode_name;
  int8_t power;  // dBm (-128 = RX)
} test_config_t;

// Tests 
test_config_t test_configs[] = {
  {"TX @ 20.0 dBm",  20},
  {"TX @ 12.0 dBm",  12},
  {"TX @ 0.0 dBm",   0},
  {"TX @ -24.0 dBm", -24},
  {"RX mode",        -128}
};

#define CONFIG_COUNT (sizeof(test_configs) / sizeof(test_config_t))

uint8_t packet[] = {
  0x61, 0x88,                         // FCF
  0x00,                               // Seq num
  0xcd, 0xab,                         // PAN ID (0xabcd)
  0x00, 0x00,                         // Dest addr
  0x00, 0x00,                         // Src addr  
  'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd', '!',  // Data
  0x00, 0x00                          // FCS (hw fills)
};

// Forward declarations
void setup_radio(void);
void run_test(test_config_t config);
void do_tx(int8_t power);
void do_rx(void);
void burn_cpu(void);

void setup() {
  Serial.begin(115200);
  // Let serial connect
  delay(1000);  
  
  Serial.println("\n\nESP32-C6 Power Tester");
  Serial.println("====================");
  
  // Full speed ahead!
  setCpuFrequencyMhz(160);
  Serial.printf("CPU @ %d MHz\n", getCpuFrequencyMhz());
  
  // Init radio
  setup_radio();
  
  // Print test info
  Serial.println("\nTest configs:");
  Serial.println("------------");
  
  for (int i = 0; i < CONFIG_COUNT; i++) {
    Serial.printf("%s\n", test_configs[i].mode_name);
  }
}

void loop() {
  static bool done = false;
  
  if (!done) {
    Serial.println("\nStarting tests...");
    
    // Run all tests
    for (int i = 0; i < CONFIG_COUNT; i++) {
      Serial.printf("\n[TEST %d/%d] %s\n", i+1, CONFIG_COUNT, 
                    test_configs[i].mode_name);
      run_test(test_configs[i]);
      
      // Chill for a bit
      delay(5000);
    }
    
    done = true;
    
    Serial.println("\nAll done! Sleeping in 5s...");
    delay(5000);
    
    // Go to sleep
    esp_sleep_enable_timer_wakeup(10000000); // 10s
    Serial.println("Zzzzz... Reset to flash new code.");
    Serial.flush();
    
    esp_deep_sleep_start();
  }
  
  // Just in case deep sleep fails
  Serial.println("Sleep failed! Waiting...");
  delay(10000);
}

void setup_radio(void) {
  // Basic radio setup
  esp_ieee802154_enable();
  esp_ieee802154_set_channel(11);  // Use ch11
  
  uint16_t pan_id = 0xabcd;
  esp_ieee802154_set_panid(pan_id);
  
  uint16_t addr = 0x0000;
  esp_ieee802154_set_short_address(addr);
  
  Serial.println("Radio ready!");
}

void run_test(test_config_t config) {
  Serial.printf("Starting %s test\n", config.mode_name);
  Serial.println("CONNECT METER NOW!");
  
  // Countdown
  for (int i = 5; i > 0; i--) {
    Serial.printf("%d...\n", i);
    delay(1000);
  }
  
  Serial.printf("GO! %s TEST RUNNING\n", config.mode_name);
  
  if (config.power == -128) {
    // RX test
    do_rx();
  } else {
    // TX test
    do_tx(config.power);
  }
  
  Serial.println("Test done. Write down your reading!");
}

void do_tx(int8_t tx_power) {
  // Set TX power  
  esp_ieee802154_set_txpower(tx_power);
  
  Serial.printf("TX @ %d dBm for 10s...\n", tx_power);
  
  uint64_t start = esp_timer_get_time();
  uint64_t end = start + 10000000; // 10s
  
  while (esp_timer_get_time() < end) {
    // Send packet
    esp_ieee802154_transmit(packet, sizeof(packet));
    
    burn_cpu();
    
    delayMicroseconds(100);
  }
}

void do_rx(void) {
  // Listen mode
  esp_ieee802154_receive();
  
  Serial.println("Receiving for 10s...");
  
  uint64_t start = esp_timer_get_time();
  uint64_t end = start + 10000000; // 10s
  
  while (esp_timer_get_time() < end) {
    burn_cpu();
    delay(1);
  }
  
  // Done receiving
  esp_ieee802154_sleep();
}

void burn_cpu(void) {
  float x = 0;
  for (int i = 0; i < 1000; i++) {
    x += sin(i) * cos(i);
    x = sqrt(x * x);
  }
  dummy_counter++;
}