// Firmware Configuration from Blynk 
#define BLYNK_TEMPLATE_ID "TMPL6dhkoRm1L"
#define BLYNK_TEMPLATE_NAME "PET Feeder"
#define BLYNK_AUTH_TOKEN "CzjiOhwWTvw9BhLfXBoBAENSr2RiaUA3"

#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <RTClib.h>
#include <LiquidCrystal_I2C.h>
#include <HX711.h>
#include <ESP32Servo.h>

// Wi-Fi Credentials
char ssid[] = "Love Never Fails";
char pass[] = "17072016";

// RTC
RTC_DS3231 rtc;

// LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Load Cell
#define HX711_DT 35
#define HX711_SCK 15
HX711 scale;

// Ultrasonic Sensor
#define TRIG_PIN 14
#define ECHO_PIN 25

// Servo Motor
Servo gateServo;
#define SERVO_PIN 4

// Buzzer
#define BUZZER_PIN 32

// Thresholds
const int PLATE_WEIGHT_THRESHOLD = 150; // Weight threshold in grams
const float HOPPER_THRESHOLD = 30.0;   // Hopper level threshold in %

BlynkTimer timer; // Blynk Timer for periodic tasks

// Servo Timing
#define SERVO_DELAY 3000      // Dispensing time
#define MOTOR_COOLDOWN 60000  // delay time after servo operation (to avoid repetetive rotation at the same minute)
unsigned long servoStartTime = 0; // Start time for servo operation
bool isServoActive = false; 
unsigned long lastBuzzTime = 0; 
bool buzzerEnabled = true; 

void setup() {
    Serial.begin(115200);

    Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

    // RTC
    if (!rtc.begin()) {
        Serial.println("Couldn't find RTC");
        while (1);
    }
    if (rtc.lostPower()) {
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__))); // Set RTC to compile time
    }

    //  LCD
    lcd.init();
    lcd.backlight();
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Pet Feeder Ready");

    //  Load Cell
    scale.begin(HX711_DT, HX711_SCK);
    // Repair the problem of readin fluctuation 
    for (int i = 0; i < 10; i++) {
        scale.tare();
        delay(100);
    }
    scale.set_scale(2280.f);

    //  Servo
    gateServo.attach(SERVO_PIN);

    //  Buzzer
    pinMode(BUZZER_PIN, OUTPUT);

    //  Ultrasonic Sensor
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);

    // Sending data to Mobile App
    timer.setInterval(1000L, sendHopperAndPlateDataToBlynk); 
}

float readHopperLevel() {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    long duration = pulseIn(ECHO_PIN, HIGH);
    float distance = duration * 0.034 / 2; // cm

    // full hopper is at 10cm, empty is at 30cm
    float level = map(distance, 30, 10, 0, 100); // %
    return constrain(level, 0, 100);
}

void operateServo() {
    if (!isServoActive) {
        gateServo.write(90);  
        servoStartTime = millis(); 
        isServoActive = true; 
    }
}

void buzzAlert() {
    if (buzzerEnabled) {
        digitalWrite(BUZZER_PIN, HIGH);
        delay(2000); // Buzzer on for 2 seconds
        digitalWrite(BUZZER_PIN, LOW);
        lastBuzzTime = millis(); 
        buzzerEnabled = false;    
    }
}

void sendHopperAndPlateDataToBlynk() {
    float hopperLevel = readHopperLevel();
    float plateWeight = scale.get_units();

    Blynk.virtualWrite(V1, hopperLevel); // Hopper level to App
    Blynk.virtualWrite(V2, plateWeight); // Plate weight to App

    // LCD
    lcd.setCursor(0, 0);
    lcd.print("Hopper: ");
    lcd.print(hopperLevel);
    lcd.print("%   ");
    lcd.setCursor(0, 1);
    lcd.print("Weight: ");
    lcd.print(plateWeight);
    lcd.print("g  ");

    // Enable the buzzer again
    if (!buzzerEnabled && (millis() - lastBuzzTime >= 3600000)) { 
        buzzerEnabled = true; // 
    }

    // Check hopper level
    if (hopperLevel < HOPPER_THRESHOLD) {
        // Notify the App
        static unsigned long lastNotifyTime = 0;
        if (millis() - lastNotifyTime >= 43200000) { // 12 hours
            Blynk.logEvent("hopper_low", "Hopper level is low!");
            lastNotifyTime = millis(); 
        }
        buzzAlert(); 
    }
}

// Remote feeding usin Blunk App
BLYNK_WRITE(V0) {
    int buttonState = param.asInt(); // Read button value
    if (buttonState == 1) {
        operateServo(); // Operate servo when button is pressed
    }
}

void loop() {
    Blynk.run();
    timer.run();

    if (isServoActive) {
        if (millis() - servoStartTime >= SERVO_DELAY) {
            gateServo.write(0); 
            isServoActive = false; 
        }
    }

    // Getting time
    DateTime now = rtc.now();
    int hour = now.hour();
    int minute = now.minute();

    // feeding times
    if ((hour == 7 || hour == 13 || hour == 19) && minute == 00) {
        float plateWeight = scale.get_units();
        if (plateWeight < PLATE_WEIGHT_THRESHOLD) {
            operateServo();  
        }
    }
}