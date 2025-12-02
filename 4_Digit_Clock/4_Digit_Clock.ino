#include <TM1637.h>
#include <Wire.h>
#include <RTClib.h>
#include <ClosedCube_HDC1080.h>

// --- Display Setup ---
#define CLK 23
#define DIO 17
TM1637 Display(CLK, DIO);
#define CHAR_BLANK 127

// --- HDC1080 ---
ClosedCube_HDC1080 hdc;

// --- RTC ---
RTC_DS3231 rtc;

// --- Buttons ---
#define BTN_SET  13
#define BTN_INC  12

// --- Variables ---
int displayMode = 0; // 0=time, 1=temp, 2=humidity

bool settingMode = false;
bool setHourMode = true;
int setHour = 0, setMinute = 0;

bool blinkState = true;
unsigned long lastBlink = 0;
const unsigned long blinkInterval = 500;

// For long press INC
bool incHeld = false;
unsigned long incPressStart = 0;

// Manual display switching
unsigned long manualSwitchTime = 0;
bool manualSwitched = false;  // If manual switch is active

// =============================
// SETUP
// =============================
void setup() {
  Serial.begin(115200);
  Serial.println("=== Starting Clock ===");

  // Display
  Display.init();
  Display.set(1); // brightness 0–7
  Display.clearDisplay();

  // HDC1080
  Wire.begin(21, 22);
  hdc.begin(0x40);
  
  // RTC
  if (!rtc.begin()) {
    Serial.println("ERROR: RTC not found!");
    while (1);
  }

  // Buttons
  pinMode(BTN_SET, INPUT_PULLUP);
  pinMode(BTN_INC, INPUT_PULLUP);
}

// =============================
// MAIN LOOP
// =============================
void loop() {
  unsigned long nowMillis = millis();

  // =========================
  // NOT SETTING MODE
  // =========================
  if (!settingMode) {
    static unsigned long lastAutoSwitch = 0;

    // Manual switch active for 5 sec
    if (manualSwitched) {
      if (nowMillis - manualSwitchTime >= 5000) {
        manualSwitched = false;
        lastAutoSwitch = nowMillis;
        Serial.println("Manual switch expired → resume auto mode");
      }
    }

    // Auto rotate only when not manually switched
    if (!manualSwitched) {
      if (displayMode == 0 && nowMillis - lastAutoSwitch >= 30000) {
        displayMode = 1;
        lastAutoSwitch = nowMillis;
      } else if (displayMode == 1 && nowMillis - lastAutoSwitch >= 15000) {
        displayMode = 2;
        lastAutoSwitch = nowMillis;
      } else if (displayMode == 2 && nowMillis - lastAutoSwitch >= 15000) {
        displayMode = 0;
        lastAutoSwitch = nowMillis;
      }
    }

    // Display current mode
    if (displayMode == 0) displayTime();
    else if (displayMode == 1) displayTemperature();
    else displayHumidity();
  }

  // Check buttons
  checkButtons();

  delay(80);
}

// =============================
// DISPLAY TIME
// =============================
void displayTime() {
  DateTime now = rtc.now();
  int hour = now.hour();
  int minute = now.minute();

  Display.display(0, hour / 10);
  Display.display(1, hour % 10);
  Display.display(2, minute / 10);
  Display.display(3, minute % 10);

  // Second even → dot on, odd → off
  Display.point(now.second() % 2 == 0 ? POINT_ON : POINT_OFF);
}

// =============================
// DISPLAY TEMPERATURE
// =============================
void displayTemperature() {
  float temp = hdc.readTemperature();

  if (temp < -9.9) temp = -9.9;
  if (temp > 99.9) temp = 99.9;

  int t_int = (int)temp;
  int t_dec = abs((int)((temp - t_int) * 10));

  Display.display(0, 12);  // T
  Display.display(1, (t_int / 10) % 10);
  Display.display(2, t_int % 10);
  Display.display(3, t_dec);

  Display.point(POINT_OFF);
}

// =============================
// DISPLAY HUMIDITY
// =============================
void displayHumidity() {
  float hum = hdc.readHumidity();

  if (hum < 0.0) hum = 0.0;
  if (hum > 99.9) hum = 99.9;

  int h_int = (int)hum;
  int h_dec = abs((int)((hum - h_int) * 10));

  Display.display(0, 16);  // H
  Display.display(1, h_int / 10);
  Display.display(2, h_int % 10);
  Display.display(3, h_dec);

  Display.point(POINT_OFF);
}
// =============================
// BUTTON HANDLING (ปรับใหม่)
// =============================
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

  if (setHourMode) {
    Display.display(0, blinkState ? setHour / 10 : CHAR_BLANK);
    Display.display(1, blinkState ? setHour % 10 : CHAR_BLANK);
    Display.display(2, setMinute / 10);
    Display.display(3, setMinute % 10);
  } else {
    Display.display(0, setHour / 10);
    Display.display(1, setHour % 10);
    Display.display(2, blinkState ? setMinute / 10 : CHAR_BLANK);
    Display.display(3, blinkState ? setMinute % 10 : CHAR_BLANK);
  }

  Display.point(POINT_ON);
}

