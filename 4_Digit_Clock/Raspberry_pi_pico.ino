#include <Wire.h>
#include <RTClib.h>
#include <ClosedCube_HDC1080.h>
#include <Adafruit_GFX.h>
#include <Adafruit_LEDBackpack.h>

// =============================
// I2C PINS (Pi Pico)
// =============================
#define I2C_SDA 20
#define I2C_SCL 21

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
// Bitmask (Alpha)
// =============================
#define SEG_C  (0x01 | 0x08 | 0x10 | 0x20) // 0x39
#define SEG_H  (0x02 | 0x04 | 0x10 | 0x20 | 0x40) // 0x76

// =============================
// BUTTONS (Pi Pico)
// =============================
#define BTN_INC 6
#define BTN_SET 7

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

unsigned long lastSensorRead = 0;
const unsigned long sensorInterval = 2000; // อ่านทุก 2 วินาที

float tempCache = 0.0;
float humCache  = 0.0;


// =============================
// SETUP
// =============================
void setup() {
  Serial.begin(115200);

  // =============================
  // I2C (Pi Pico)
  // =============================
  Wire.setSDA(I2C_SDA);
  Wire.setSCL(I2C_SCL);
  Wire.begin();

  // =============================
  // HT16K33
  // =============================
  display.begin(0x70, &Wire);
  display.setBrightness(5);
  display.clear();
  display.writeDisplay();

  // =============================
  // HDC1080
  // =============================
  Wire.begin();
  Wire.setClock(400000);
  hdc.begin(0x40);
  


  // =============================
  // RTC DS3231
  // =============================
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

  int h = now.hour();
  int m = now.minute();

  display.clear();

  // ชั่วโมง
  display.writeDigitNum(0, h / 10);   // 0 หรือ 1 หรือ 2
  display.writeDigitNum(1, h % 10);   // หลักหน่วย

  // นาที
  display.writeDigitNum(3, m / 10);   // 0–5
  display.writeDigitNum(4, m % 10);   // 0–9

  display.drawColon(now.second() % 2 == 0);
  display.writeDisplay();
}

// =============================
// DISPLAY TEMPERATURE
// =============================
void displayTemperature() {
  updateHDC1080();

  int t = (int)(tempCache * 10);

  display.clear();
  display.writeDigitRaw(0, SEG_C);
  display.writeDigitNum(1, (t / 100) % 10); 
  display.writeDigitNum(3, (t / 10) % 10);  
  display.writeDigitNum(4, t % 10);         
  display.writeDigitRaw(3, display.displaybuffer[3] | 0x80);
  display.drawColon(false);
  display.writeDisplay();
}


// =============================
// DISPLAY HUMIDITY
// =============================
void displayHumidity() {
  updateHDC1080();

  int h = (int)(humCache * 10);

  display.clear();
  display.writeDigitRaw(0, SEG_H);
  display.writeDigitNum(1, (h / 100) % 10);
  display.writeDigitNum(3, (h / 10) % 10);
  display.writeDigitNum(4, h % 10);
  display.writeDigitRaw(3, display.displaybuffer[3] | 0x80);
  display.drawColon(false);
  display.writeDisplay();
}


void updateHDC1080() {
  if (millis() - lastSensorRead < sensorInterval) return;

  tempCache = hdc.readTemperature();
  humCache  = hdc.readHumidity();

  // clamp ค่า RH กันเพี้ยน
  if (humCache < 0) humCache = 0;
  if (humCache > 100) humCache = 100;

  lastSensorRead = millis();

  // debug
  Serial.print("HDC1080 -> T=");
  Serial.print(tempCache);
  Serial.print(" C, RH=");
  Serial.print(humCache);
  Serial.println(" %");
}


void checkButtons() {
  unsigned long nowMillis = millis();

  // =============================
  // BTN_SET (13) → edge detect
  // =============================
  static bool lastSetState = HIGH;
  bool setState = digitalRead(BTN_SET);

  if (lastSetState == HIGH && setState == LOW) {
    if (!settingMode) {
      settingMode = true;
      setHourMode = true;

      DateTime now = rtc.now();
      setHour = now.hour();
      setMinute = now.minute();
      lastBlink = nowMillis;

      Serial.println("ENTER setting mode");
    } else if (setHourMode) {
      setHourMode = false;
    } else {
      DateTime now = rtc.now();
      rtc.adjust(DateTime(now.year(), now.month(), now.day(), setHour, setMinute, 0));

      settingMode = false;
      Serial.println("EXIT setting mode → Time saved");
    }
  }
  lastSetState = setState;

  // =============================
  // BTN_INC (12) → auto increment
  // =============================
  static bool lastIncState = HIGH;
  static unsigned long incLastTime = 0;
  const unsigned long incInterval = 300; // กดค้างเพิ่มทุก 300ms

  bool incState = digitalRead(BTN_INC);

  if (incState == LOW) { // ปุ่มถูกกด
    if (!settingMode) {
      // manual switch display
      if (lastIncState == HIGH) { // กดครั้งแรก
        displayMode = (displayMode + 1) % 3;
        manualSwitched = true;
        manualSwitchTime = nowMillis;
        Serial.print("Switch display to mode = ");
        Serial.println(displayMode);
      }
    } else {
      // กำหนดเวลา auto increment
      if (lastIncState == HIGH || nowMillis - incLastTime >= incInterval) {
        if (setHourMode) setHour = (setHour + 1) % 24;
        else setMinute = (setMinute + 1) % 60;
        incLastTime = nowMillis;
      }
    }
  }
  lastIncState = incState;

  // =============================
  // BLINK EFFECT
  // =============================
  if (settingMode && nowMillis - lastBlink >= blinkInterval) {
    blinkState = !blinkState;
    lastBlink = nowMillis;
  }

  // =============================
  // SHOW SETTING TIME
  // =============================
  if (!settingMode) return;

display.clear();

if (setHourMode) {
  if (blinkState) {
    display.writeDigitNum(0, setHour / 10);
    display.writeDigitNum(1, setHour % 10);
  }
  display.writeDigitNum(3, setMinute / 10);
  display.writeDigitNum(4, setMinute % 10);
} else {
  display.writeDigitNum(0, setHour / 10);
  display.writeDigitNum(1, setHour % 10);
  if (blinkState) {
    display.writeDigitNum(3, setMinute / 10);
    display.writeDigitNum(4, setMinute % 10);
  }
}

display.drawColon(true);
display.writeDisplay();


}
