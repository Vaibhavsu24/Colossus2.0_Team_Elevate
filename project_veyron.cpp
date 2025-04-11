#include <Adafruit_Fingerprint.h>
#include <HardwareSerial.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "BluetoothSerial.h"

// ===== Constants and Pins =====
#define MAX_ATTEMPTS 5
#define LOCK_DURATION 60000
#define RELAY_PIN 23
#define BUZZER_PIN 33
#define LED_PIN 4
#define trigger 26

// ===== Object Instantiations =====
HardwareSerial mySerial(2);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);
LiquidCrystal_I2C lcd(0x27, 16, 2);
BluetoothSerial SerialBT;

// ===== Variables =====
String bluetoothCode = "abcd";
String receivedCode = "";
bool accessGranted = false;
bool motorRunning = false;
int failedAttempts = 0;
unsigned long lockStartTime = 0;
bool systemLocked = false;
unsigned long lastLCDUpdate = 0;
bool showAltPrompt = false;
bool justGrantedAccess = false;

// ===== Function Declarations =====
bool getFingerprintID();
void grantAccess(String method);
void handleBluetoothInput();
void handlePySerialCommands();
void startMotor();
void stopMotor();
void showError();
void handleLockState();
void updateLCDPrompt();
void showWelcome();
void animateText(int col, int row, String text, int delayTime);

// ===== SETUP =====
void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);
  lcd.begin(16, 2);
  lcd.backlight();

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(trigger, OUTPUT);

  digitalWrite(RELAY_PIN, 0); // Relay OFF at boot (active LOW relay)
  digitalWrite(RELAY_PIN,0);
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(LED_PIN, LOW);
  digitalWrite(trigger, LOW);
  motorRunning = false;

  lcd.clear();
  showWelcome();

  mySerial.begin(57600, SERIAL_8N1, 16, 17);
  finger.begin(57600);

  if (!finger.verifyPassword()) {
    lcd.clear();
    lcd.print("Sensor Error!");
    digitalWrite(BUZZER_PIN, HIGH);
    while (1);
  }

  SerialBT.begin("ProjectVeyron");
  Serial.println("Bluetooth ready");
}

// ===== LOOP =====
void loop() {
  if (systemLocked) {
    handleLockState();
    return;
  }

  if (!justGrantedAccess) updateLCDPrompt();

  if (getFingerprintID()) {
    grantAccess("Fingerprint");
  } else if (SerialBT.available()) {
    handleBluetoothInput();
  } else if (Serial.available()) {
    handlePySerialCommands();
  }

  delay(50);
}

// ===== FUNCTION DEFINITIONS =====
bool getFingerprintID() {
  int p = finger.getImage();
  if (p != FINGERPRINT_OK) return false;

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) {
    Serial.println("Image conversion failed");
    showError();
    return false;
  }

  p = finger.fingerSearch();
  if (p != FINGERPRINT_OK) {
    Serial.println("No matching fingerprint found");
    showError();
    return false;
  }

  Serial.println("Fingerprint matched!");
  return true;
}

void grantAccess(String method) {
  accessGranted = true;
  failedAttempts = 0;
  justGrantedAccess = true;

  lcd.clear();
  animateText(0, 0, "Access via:", 50);
  animateText(0, 1, method, 50);
  Serial.println("Access Granted via " + method);

  tone(BUZZER_PIN, 1000, 200);
  delay(300);
  noTone(BUZZER_PIN);

  delay(1000);
  lcd.clear();
  animateText(0, 0, "Welcome", 50);
  animateText(0, 1, "Onboard!", 50);

  startMotor();
}

void handleBluetoothInput() {
  char incoming = SerialBT.read();
  receivedCode += incoming;

  if (receivedCode.length() == 4) {
    if (receivedCode == bluetoothCode) {
      grantAccess("Bluetooth");
    } else {
      showError();
    }
    receivedCode = "";
  }

  if (receivedCode.length() > 4) {
    receivedCode = "";
  }
}

void handlePySerialCommands() {
  char command = Serial.read();
  if (command == '1') {
    stopMotor();
  } else if (command == '0') {
    startMotor();
  }
}

void startMotor() {
  if (!motorRunning) {
    digitalWrite(RELAY_PIN, HIGH);  // Active LOW: turn motor ON
    motorRunning = true;
    lcd.clear();
    animateText(0, 0, "Motor", 50);
    animateText(0, 1, "Running", 50);
    Serial.println("Motor Started");
  }
}

void stopMotor() {
  if (motorRunning) {
    digitalWrite(RELAY_PIN, LOW);  // Active LOW: turn motor OFF
    motorRunning = false;
    lcd.clear();
    animateText(0, 0, "Motor", 50);
    animateText(0, 1, "Stopped", 50);
    Serial.println("Motor Stopped");
  }
}

void showError() {
  failedAttempts++;

  for (int i = 0; i < 2; i++) {
    digitalWrite(LED_PIN, HIGH);
    tone(BUZZER_PIN, 1000);
    delay(200);
    digitalWrite(LED_PIN, LOW);
    noTone(BUZZER_PIN);
    delay(200);
  }

  lcd.clear();
  animateText(0, 0, "Access Denied!", 50);
  lcd.setCursor(0, 1);
  lcd.print("Attempt ");
  lcd.print(failedAttempts);
  delay(2000);

  if (failedAttempts == 4) {
    Serial.write('X');
  }

  if (failedAttempts >= MAX_ATTEMPTS) {
    systemLocked = true;
    lockStartTime = millis();
    lcd.clear();
    animateText(0, 0, "System Locked!", 50);

    for (int i = 0; i < 5; i++) {
      digitalWrite(LED_PIN, HIGH);
      digitalWrite(BUZZER_PIN, HIGH);
      delay(300);
      digitalWrite(LED_PIN, LOW);
      digitalWrite(BUZZER_PIN, LOW);
      delay(300);
    }
  }

  lcd.clear();
}

void handleLockState() {
  unsigned long elapsed = millis() - lockStartTime;
  if (elapsed >= LOCK_DURATION) {
    systemLocked = false;
    failedAttempts = 0;
    digitalWrite(BUZZER_PIN, LOW);
    lcd.clear();
    animateText(0, 0, "System", 50);
    animateText(0, 1, "Unlocked", 50);
    delay(1500);
    lcd.clear();
  } else {
    int remaining = (LOCK_DURATION - elapsed) / 1000;
    lcd.setCursor(0, 0);
    lcd.print("Too many tries ");
    lcd.setCursor(0, 1);
    lcd.print("Wait: ");
    lcd.print(remaining);
    lcd.print(" sec");
    delay(1000);
  }
}

void updateLCDPrompt() {
  if (millis() - lastLCDUpdate > 2000) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(showAltPrompt ? "Place Finger" : "Use Bluetooth");
    lcd.setCursor(0, 1);
    lcd.print("to Unlock");
    showAltPrompt = !showAltPrompt;
    lastLCDUpdate = millis();
  }
}

void showWelcome() {
  animateText(0, 0, "Project Veyron", 100);
  animateText(0, 1, "By Team Elevate", 100);
  delay(1500);
  lcd.clear();
}

void animateText(int col, int row, String text, int delayTime) {
  lcd.setCursor(col, row);
  for (int i = 0; i < text.length(); i++) {
    lcd.print(text[i]);
    delay(delayTime);
  }
}
