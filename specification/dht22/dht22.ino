#include "DHT.h"

#define DHT_POWER_PIN  5    // GPIO pin to power the DHT22
#define DHT_DATA_PIN   4    // GPIO pin for DHT22 data
#define DHT_TYPE       DHT22


#define DEEP_SLEEP_TIME  10
DHT dht(DHT_DATA_PIN, DHT_TYPE);

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\nDHT22 Test");
  
  // Configure pins
  pinMode(DHT_POWER_PIN, OUTPUT);
  digitalWrite(DHT_POWER_PIN, LOW);
  pinMode(DHT_DATA_PIN, INPUT_PULLUP);
  digitalWrite(DHT_POWER_PIN, HIGH);
  delay(3000);
  dht.begin();
  
  Serial.print("DHT data line state: ");
  Serial.println(digitalRead(DHT_DATA_PIN) ? "HIGH" : "LOW");
}

void loop() {
  Serial.println("\n--- Starting measurement cycle ---");
  
  digitalWrite(DHT_POWER_PIN, LOW);
  delay(1000);
  
  // Turn sensor ON
  digitalWrite(DHT_POWER_PIN, HIGH);
  Serial.println("DHT22 powered ON");
  
  // DHT22 requires at least 2s after power-up
  delay(3000);
  
  Serial.print("Data line before reading: ");
  Serial.println(digitalRead(DHT_DATA_PIN) ? "HIGH" : "LOW");
  
  // Read temperature and humidity
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();
  
  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("Failed to read from DHT sensor!");
    Serial.print("Data line after failed reading: ");
    Serial.println(digitalRead(DHT_DATA_PIN) ? "HIGH" : "LOW");
    digitalWrite(DHT_POWER_PIN, LOW);
    Serial.println("DHT22 powered OFF due to read failure");
  } else {
    Serial.print("Humidity: ");
    Serial.print(humidity);
    Serial.print("% | Temperature: ");
    Serial.print(temperature);
    Serial.println("°C");
    delay(1000);
    
    // Turn sensor OFF
    digitalWrite(DHT_POWER_PIN, LOW);
    Serial.println("DHT22 powered OFF after successful reading");
  }
  
  Serial.println("Waiting for next measurement cycle...");
  delay(5000);
}