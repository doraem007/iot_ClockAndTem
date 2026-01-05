#include <Wire.h>
#include <RTClib.h>
#include <ClosedCube_HDC1080.h>
#include <Adafruit_GFX.h>
#include <Adafruit_LEDBackpack.h>

// =============================
// DISPLAY (HT16K33)
// =============================
Adafruit_7segment display = Adafruit_7segment();

// =============================
// HDC1080
// =============================
ClosedCube_HDC1080 hdc;

// =============================
// RTC
// =============================
RTC_DS3231 rtc;

// =============================
// BUTTONS (Pi Pico)
// =============================
#define BTN_SET  14
#define BTN_INC  15

// =============================
// VARIABLES
// =============================
int displayMode = 0; // 0=time, 1=temp, 2=humidity
bool settingMode = false;
bool setHourMode = true;
int setHour = 0, setMinute = 0;

bool blinkState = true;
unsigned long lastBlink = 0;
const unsigned long blinkInterval = 500;

unsigned long manualSwitchTime = 0;
bool manualSwitched = false;

// =============================
// SETUP
// =============================
void setup() {
  Serial.begin(115200);

  // I2C for Pi Pico
  Wire.setSDA(0);
  Wire.setSCL(1);
  Wire.begin();

  // HT16K33
  display.begin(0x70);
  display.setBrightness(5);
  display.clear();
  display.writeDisplay();

  // HDC1080
  hdc.begin(0x40);

  // RTC
  rtc.begin();

  pinMode(BTN_SET, INPUT_PULLUP);
  pinMode(BTN_INC, INPUT_PULLUP);
}

// =============================
// LOOP
// =============================
void loop() {
  unsigned long nowMillis = millis();

  if (!settingMode) {
    static unsigned long lastAutoSwitch = 0;

    if (manualSwitched && nowMillis - manualSwitchTime > 5000) {
      manualSwitched = false;
      lastAutoSwitch = nowMillis;
    }

    if (!manualSwitched) {
      if (displayMode == 0 && nowMillis - lastAutoSwitch > 30000) {
        displayMode = 1;
        lastAutoSwitch = nowMillis;
      } else if (displayMode == 1 && nowMillis - lastAutoSwitch > 15000) {
        displayMode = 2;
        lastAutoSwitch = nowMillis;
      } else if (displayMode == 2 && nowMillis - lastAutoSwitch > 15000) {
        displayMode = 0;
        lastAutoSwitch = nowMillis;
      }
    }

    if (displayMode == 0) displayTime();
    else if (displayMode == 1) displayTemperature();
    else displayHumidity();
  }

  checkButtons();
  delay(50);
}

// =============================
// DISPLAY TIME
// =============================
void displayTime() {
  DateTime now = rtc.now();

  int value = now.hour() * 100 + now.minute();
  display.print(value);

  display.drawColon(now.second() % 2 == 0);
  display.writeDisplay();
}

// =============================
// DISPLAY TEMPERATURE
// =============================
void displayTemperature() {
  float t = hdc.readTemperature();
  int temp = (int)(t * 10);

  display.print(temp);
  display.drawColon(false);

  display.writeDigitNum(2, (temp / 10) % 10, true);
  display.writeDisplay();
}

// =============================
// DISPLAY HUMIDITY
// =============================
void displayHumidity() {
  float h = hdc.readHumidity();
  int hum = (int)(h * 10);

  display.print(hum);
  display.drawColon(false);

  display.writeDigitNum(2, (hum / 10) % 10, true);
  display.writeDisplay();
}
