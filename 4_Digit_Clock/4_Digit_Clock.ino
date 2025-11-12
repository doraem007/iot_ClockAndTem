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
bool setHourMode = true; // true = ตั้งชั่วโมง, false = ตั้งนาที
int setHour = 0, setMinute = 0;
bool blinkState = true;
unsigned long lastBlink = 0;
const unsigned long blinkInterval = 500;

// INC button state
bool incHeld = false;
unsigned long incPressStart = 0;

// --- Setup ---
void setup() {
  Serial.begin(115200);
  Serial.println("=== Starting Clock ===");

  // Display
  Display.init();
  Display.set(BRIGHT_TYPICAL);
  Display.clearDisplay();
  Serial.println("TM1637 initialized");

  // HDC1080
  Wire.begin(21, 22); // SDA, SCL
  hdc.begin(0x40);
  Serial.println("HDC1080 initialized");

  // RTC
  if (!rtc.begin()) {
    Serial.println("ERROR: RTC not found!");
    while (1);
  } else {
    Serial.println("RTC initialized");
  }

  // Buttons
  pinMode(BTN_SET, INPUT_PULLUP);
  pinMode(BTN_INC, INPUT_PULLUP);
  Serial.println("Buttons initialized");

  // Read initial RTC time
  DateTime now = rtc.now();
  setHour = now.hour();
  setMinute = now.minute();
  Serial.print("Initial time from RTC: ");
  Serial.print(setHour);
  Serial.print(":");
  Serial.println(setMinute);
}

// --- Loop ---
void loop() {
  unsigned long currentMillis = millis();

  if (!settingMode) {
    static unsigned long lastSwitch = 0;

    switch(displayMode) {
      case 0: // Time
        if (currentMillis - lastSwitch >= 30000) { // 30 sec
          displayMode = 1;
          lastSwitch = currentMillis;
          Serial.println("Switch display to Temperature");
        }
        displayTime();
        break;
      case 1: // Temperature
        if (currentMillis - lastSwitch >= 15000) { // 15 sec
          displayMode = 2;
          lastSwitch = currentMillis;
          Serial.println("Switch display to Humidity");
        }
        displayTemperature();
        break;
      case 2: // Humidity
        if (currentMillis - lastSwitch >= 15000) { // 15 sec
          displayMode = 0;
          lastSwitch = currentMillis;
          Serial.println("Switch display to Time");
        }
        displayHumidity();
        break;
    }
  }

  checkButtons();
  delay(100);
}

// --- Display Functions ---
void displayTime() {
  DateTime now = rtc.now();
  int hour = now.hour();
  int minute = now.minute();

  // Sanitize
  if (hour < 0 || hour > 23) hour = 0;
  if (minute < 0 || minute > 59) minute = 0;

  Display.display(0, hour / 10);
  Display.display(1, hour % 10);
  Display.display(2, minute / 10);
  Display.display(3, minute % 10);
  Display.point(POINT_ON);

  Serial.print("Displaying Time: ");
  Serial.print(hour);
  Serial.print(":");
  Serial.println(minute);
}

void displayTemperature() {
  float temp = hdc.readTemperature();
  
  // จำกัดค่า
  if (temp < -9.9) temp = -9.9;   // เพราะมีแค่ 3 หลัก + จุด
  if (temp > 99.9) temp = 99.9;

  // ตัดทศนิยม 1 ตำแหน่ง
  int t_int = (int)temp;                      
  int t_dec = (int)((temp - t_int) * 10);     

  if(t_dec < 0) t_dec = -t_dec; // กรณี negative

  Display.display(0, 12);
  
  Display.display(1, (t_int / 10) % 10);
  Display.display(2, t_int % 10);
  Display.display(3, t_dec);
  Display.point(POINT_OFF);

  Serial.print("Displaying Temperature: ");
  Serial.print(temp, 1);
  Serial.println(" °C");
}

void displayHumidity() {
  float hum = hdc.readHumidity();

  // จำกัดค่า
  if (hum < 0.0) hum = 0.0;
  if (hum > 99.9) hum = 99.9;

  int h_int = (int)hum;                 // ตัวเลขเต็ม
  int h_dec = (int)((hum - h_int) * 10); // ตัวเลขทศนิยม

  if (h_dec < 0) h_dec = -h_dec;        // ป้องกันค่าลบ

  Display.display(0, 16);
  Display.display(1, h_int / 10);
  Display.display(2, h_int % 10);
  Display.display(3, h_dec);
  Display.point(POINT_OFF);

  Serial.print("Displaying Humidity: ");
  Serial.print(hum, 1);
  Serial.println(" %");
}


// --- Button Functions ---
void checkButtons() {
  unsigned long currentMillis = millis();

  // --- SET button ---
  if (digitalRead(BTN_SET) == LOW) {
    delay(200); // debounce
    if (!settingMode) {
      // เริ่มตั้งค่า
      settingMode = true;
      setHourMode = true;  // เริ่มด้วยชั่วโมง
      DateTime now = rtc.now();
      setHour = now.hour();
      setMinute = now.minute();
      lastBlink = currentMillis;
      blinkState = true;
      Serial.println("Entered SETTING mode, adjust Hour");
    } else if (setHourMode) {
      // เปลี่ยนไปตั้งนาที
      setHourMode = false;
      Serial.println("Adjust Minute");
    } else {
      // ออกจาก setting mode -> ปรับ RTC
      DateTime now = rtc.now();
      rtc.adjust(DateTime(now.year(), now.month(), now.day(), setHour, setMinute, 0));
      settingMode = false;
      Serial.print("Exiting SETTING mode, Time set to: ");
      Serial.print(setHour);
      Serial.print(":");
      Serial.println(setMinute);
    }
    delay(300); // debounce
  }

  if (!settingMode) return;

  // --- INC button ---
  if (digitalRead(BTN_INC) == LOW) {
    if (!incHeld) incPressStart = currentMillis;
    incHeld = true;

    if (currentMillis - incPressStart >= 500) { // long press
      if (setHourMode) setHour = (setHour + 1) % 24;
      else setMinute = (setMinute + 1) % 60;
      incPressStart = currentMillis - 300;
    } else { // short press
      if (setHourMode) setHour = (setHour + 1) % 24;
      else setMinute = (setMinute + 1) % 60;
    }

    Serial.print("INC button pressed, ");
    if (setHourMode) Serial.print("Hour = "); else Serial.print("Minute = ");
    Serial.println(setHourMode ? setHour : setMinute);

  } else incHeld = false;

  // --- Blink effect ---
  if (currentMillis - lastBlink >= blinkInterval) {
    blinkState = !blinkState;
    lastBlink = currentMillis;
  }

  // --- Display setting time ---
  if (setHourMode) {
    // ชั่วโมงกระพริบ
    if (blinkState) {
      Display.display(0, setHour / 10);
      Display.display(1, setHour % 10);
    } else {
      Display.display(0, CHAR_BLANK);
      Display.display(1, CHAR_BLANK);
    }
    Display.display(2, setMinute / 10);
    Display.display(3, setMinute % 10);
  } else {
    // นาที กระพริบ
    Display.display(0, setHour / 10);
    Display.display(1, setHour % 10);
    if (blinkState) {
      Display.display(2, setMinute / 10);
      Display.display(3, setMinute % 10);
    } else {
      Display.display(2, CHAR_BLANK);
      Display.display(3, CHAR_BLANK);
    }
  }
  Display.point(POINT_ON);
}
