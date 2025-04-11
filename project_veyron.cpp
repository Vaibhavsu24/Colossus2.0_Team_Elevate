#include <Adafruit_Fingerprint.h>
#include <HardwareSerial.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "BluetoothSerial.h"

#define MAX_ATTEMPTS 5
#define LOCK_DURATION 60000

#define RELAY_PIN 23
#define BUZZER_PIN 25
#define trigger 26

HardwareSerial mySerial(2);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);
LiquidCrystal_I2C lcd(0x27, 16, 2);
BluetoothSerial SerialBT;

String bluetoothCode = "abcd";
String receivedCode = "";

bool accessGranted = false;
bool motorRunning = false;
int failedAttempts = 0;
unsigned long lockStartTime = 0;
bool systemLocked = false;
unsigned long lastLCDUpdate = 0;
bool showAltPrompt = false;

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);
  lcd.begin(16, 2);
  lcd.backlight();

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(trigger, OUTPUT);

  digitalWrite(RELAY_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);
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

void loop() {
  if (systemLocked) {
    handleLockState();
    return;
  }

  updateLCDPrompt();

  if (getFingerprintID()) {
    grantAccess("Fingerprint");
  }

  if (SerialBT.available()) {
    handleBluetoothInput();
  }

  if (Serial.available()) {
    handlePySerialCommands();
  }

  delay(50);
}

bool getFingerprintID() {
  int p = finger.getImage();
  if (p != FINGERPRINT_OK) return false;

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) return false;

  p = finger.fingerSearch();
  if (p == FINGERPRINT_OK) {
    Serial.println("Fingerprint matched");
    return true;
  }

  Serial.println("Fingerprint NOT matched");
  showError();
  return false;
}

void grantAccess(String method) {
  accessGranted = true;
  failedAttempts = 0;
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Access via:");
  lcd.setCursor(0, 1);
  lcd.print(method);

  Serial.println("Access Granted via " + method);

  if (!motorRunning) {
    startMotor();
  }
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
    digitalWrite(RELAY_PIN, HIGH);
    motorRunning = true;
    lcd.clear();
    lcd.print("Motor Running");
    Serial.println("Motor Started");
  }
}

void stopMotor() {
  if (motorRunning) {
    digitalWrite(RELAY_PIN, LOW);
    motorRunning = false;
    lcd.clear();
    lcd.print("Motor Stopped");
    Serial.println("Motor Stopped");
  }
}

void showError() {
  failedAttempts++;
  digitalWrite(BUZZER_PIN, HIGH);
  lcd.clear();
  lcd.print("Access Denied!");
  lcd.setCursor(0, 1);
  lcd.print("Attempt ");
  lcd.print(failedAttempts);
  digitalWrite(BUZZER_PIN, LOW);
  delay(2000);

  if (failedAttempts >= MAX_ATTEMPTS) {
    systemLocked = true;
    lockStartTime = millis();
    lcd.clear();
    lcd.print("System Locked!");
    Serial.println("LOCK_ALERT"); // Signal to Python to get location
    delay(1000);
  }
  lcd.clear();
}

void handleLockState() {
  unsigned long elapsed = millis() - lockStartTime;
  if (elapsed >= LOCK_DURATION) {
    systemLocked = false;
    failedAttempts = 0;
    lcd.clear();
    lcd.print("System Unlocked");
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
  lcd.setCursor(0, 0);
  lcd.print("Project Veyron");
  lcd.setCursor(0, 1);
  lcd.print("By Team Elevate");
  delay(1500);
  lcd.clear();
}