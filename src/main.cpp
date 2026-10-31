#include <Arduino.h>
#include <math.h>
#include <Preferences.h>
#include <string.h>
#include <Wire.h>

namespace {
constexpr uint8_t kAs5600Address = 0x36;
constexpr uint8_t kRawAngleRegister = 0x0C;
constexpr int kSdaPin = 17;
constexpr int kSclPin = 18;
constexpr int kDirPin = 10;
constexpr int kStatusPin0 = 4;
constexpr int kStatusPin1 = 5;
constexpr uint16_t kAngleMask = 0x0FFF;
constexpr uint16_t kAngleCounts = 4096;
constexpr float kDegreesPerCount = 360.0f / 4096.0f;
constexpr float kAngleChangeThresholdDegrees = 0.3f;
constexpr float kZeroDeadbandLowDegrees = -0.08f;
constexpr float kZeroDeadbandHighDegrees = 0.02f;
constexpr uint32_t kReportIntervalMs = 5000;
Preferences preferences;
uint16_t zeroOffset = 0;
char commandBuffer[16];
size_t commandLength = 0;
bool hasReportedValues = false;
float lastReportedAngle = 0.0f;
uint8_t lastReportedPinStates = 0;
uint32_t lastReportMillis = 0;

bool readRawAngle(uint16_t &angle) {
  Wire.beginTransmission(kAs5600Address);
  Wire.write(kRawAngleRegister);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  if (Wire.requestFrom(kAs5600Address, static_cast<uint8_t>(2)) != 2) {
    return false;
  }

  const uint8_t highByte = Wire.read();
  const uint8_t lowByte = Wire.read();
  angle = ((static_cast<uint16_t>(highByte) << 8) | lowByte) & kAngleMask;
  return true;
}

void handleSerialCommands() {
  while (Serial.available() > 0) {
    const char incoming = static_cast<char>(Serial.read());
    if (incoming == '\r') {
      continue;
    }
    if (incoming == '\n') {
      commandBuffer[commandLength] = '\0';
      if (strcmp(commandBuffer, "zero") == 0) {
        uint16_t currentAngle = 0;
        if (!readRawAngle(currentAngle)) {
          Serial.println("Cannot zero: AS5600 read failed.");
        } else if (preferences.putUShort("zero", currentAngle) == sizeof(currentAngle)) {
          zeroOffset = currentAngle;
          hasReportedValues = false;
          Serial.print("Zero set to raw angle ");
          Serial.println(zeroOffset);
        } else {
          Serial.println("Cannot save zero offset to NVRAM.");
        }
      } else if (commandLength > 0) {
        Serial.println("Unknown command. Send 'zero' to set the current position as zero.");
      }
      commandLength = 0;
      continue;
    }

    if (commandLength < sizeof(commandBuffer) - 1) {
      commandBuffer[commandLength++] = incoming;
    } else {
      commandLength = 0;
      Serial.println("Command too long.");
    }
  }
}
}  // namespace

void setup() {
  Serial.begin(115200);
  pinMode(kDirPin, OUTPUT);
  digitalWrite(kDirPin, LOW);
  pinMode(kStatusPin0, INPUT_PULLUP);
  pinMode(kStatusPin1, INPUT_PULLUP);
  preferences.begin("as5600", false);
  zeroOffset = preferences.getUShort("zero", 0);
  Wire.begin(kSdaPin, kSclPin);
  Serial.println("AS5600 angle reader starting (SDA=17, SCL=18)");
  Serial.println("Send 'zero' followed by Enter to zero the current position.");
}

void loop() {
  handleSerialCommands();

  uint16_t rawAngle = 0;
  if (readRawAngle(rawAngle)) {
    const uint16_t relativeAngle =
        (rawAngle + kAngleCounts - zeroOffset) & kAngleMask;
    const int16_t signedAngle = relativeAngle > (kAngleCounts / 2)
                                    ? static_cast<int16_t>(relativeAngle) - kAngleCounts
                                    : static_cast<int16_t>(relativeAngle);
    const float angleDegrees = signedAngle * kDegreesPerCount;
    const float reportedAngleDegrees =
        (angleDegrees > kZeroDeadbandLowDegrees &&
         angleDegrees < kZeroDeadbandHighDegrees)
            ? 0.0f
            : angleDegrees;
    const uint8_t pinStates = (digitalRead(kStatusPin0) ? 1U : 0U) |
                              (digitalRead(kStatusPin1) ? 2U : 0U);
    float angleChange = reportedAngleDegrees - lastReportedAngle;
    if (angleChange > 180.0f) {
      angleChange -= 360.0f;
    } else if (angleChange < -180.0f) {
      angleChange += 360.0f;
    }
    if (!hasReportedValues ||
        fabsf(angleChange) >= kAngleChangeThresholdDegrees ||
        pinStates != lastReportedPinStates ||
        millis() - lastReportMillis >= kReportIntervalMs) {
      Serial.print("Angle: ");
      Serial.print(reportedAngleDegrees, 2);
      Serial.print(" deg | GPIO4/5: ");
      Serial.print(static_cast<unsigned int>(pinStates));
      Serial.print(" (0b");
      if (pinStates < 2) {
        Serial.print('0');
      }
      Serial.print(pinStates, BIN);
      Serial.println(')');
      lastReportedAngle = reportedAngleDegrees;
      lastReportedPinStates = pinStates;
      lastReportMillis = millis();
      hasReportedValues = true;
    }
  } else {
    Serial.println("AS5600 read failed; check wiring and power.");
  }

  delay(100);
}
