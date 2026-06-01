#include <Arduino.h>
#include <Wire.h>

#define I2C_SDA 8
#define I2C_SCL 7
#define TOUCH_RST 11

void setup() {
  Serial.begin(115200);
  delay(3000);
  
  Serial.println("\n=== Touch Probe ===");
  
  pinMode(TOUCH_RST, OUTPUT);
  for (int i = 0; i < 3; i++) {
    digitalWrite(TOUCH_RST, 1);
    delay(200);
    digitalWrite(TOUCH_RST, 0);
    delay(200);
    digitalWrite(TOUCH_RST, 1);
    delay(200);
    Serial.printf("Reset cycle %d done\n", i);
  }

  Wire.begin(I2C_SDA, I2C_SCL, 400000);
  delay(500);

  Serial.println("\nI2C Scanning...");
  for (uint8_t addr = 1; addr < 128; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("Found 0x%02X\n", addr);
      for (int r = 0; r <= 0xA7; r += 1) {
        Wire.beginTransmission(addr);
        Wire.write(r);
        if (Wire.endTransmission(false) == 0) {
          uint8_t buf[6] = {0};
          Wire.requestFrom(addr, (uint8_t)6);
          for (int j = 0; j < 6 && Wire.available(); j++) buf[j] = Wire.read();
          if (buf[0] != 0 || buf[1] != 0 || buf[2] != 0 || buf[3] != 0) {
            Serial.printf("  reg0x%02X: %02x %02x %02x %02x %02x %02x\n",
              r, buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]);
          }
        }
        delay(5);
      }
    }
  }
  
  Serial.println("=== Done ===");
}

void loop() { delay(5000); }
