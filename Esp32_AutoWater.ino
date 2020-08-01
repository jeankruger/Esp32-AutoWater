#include <Wire.h>
#include "SH1106Wire.h"
#include "RTClib.h"
#include <ESP32Servo.h>

struct Time {
  int hour;
  int minute;
  int second;
};

struct TimeInterval {
  Time start;
  Time end;
};

const int PIN_BTN_1 = 25;
const int PIN_SDA = 26;
const int PIN_SCL = 27;
const int PIN_SERVO = 33;
const int PIN_RELAY = 32;

const int STATE_AUTO = 0;
const int STATE_CLOSE = 1;
const int STATE_OPEN = 2;

const TimeInterval AUTO_OPEN_INTERVALS[] = {
  { { 8, 29, 30 }, { 8, 35, 30 } },
  { { 20, 29, 30 }, { 20, 35, 30 } }
};

const long RELAY_POWER_DELAY = 1000L;

SH1106Wire display(0x3c, PIN_SDA, PIN_SCL); 
RTC_DS3231 rtc;  
Servo servo; 

boolean isBtn1Pressed = false;
int currentState = STATE_AUTO;
boolean isOpened = false;
unsigned long relayPoweredUntil = 0L;

void setup() {
  pinMode(PIN_BTN_1, INPUT);
  pinMode(PIN_RELAY, OUTPUT);
  
  display.init();

  Wire.begin(PIN_SDA, PIN_SCL);
  rtc.begin();

  servo.attach(PIN_SERVO);

  /*rtc.adjust(DateTime(2020, 7, 18, 17, 15, 00));*/

  relayPoweredUntil = millis() + RELAY_POWER_DELAY;
  refreshRelayPower();
  refreshServoPosition();
}

void loop() {
  DateTime now = rtc.now();
  
  if (digitalRead(PIN_BTN_1) != isBtn1Pressed) {
    isBtn1Pressed = !isBtn1Pressed;
    if (isBtn1Pressed) {
      switchState();
    }
  }

  if (computeIsOpened(now) != isOpened) {
    isOpened = !isOpened;
    relayPoweredUntil = millis() + RELAY_POWER_DELAY;
    refreshServoPosition();
  }

  refreshRelayPower();

  String date = toPaddedString(now.day()) + '/' + toPaddedString(now.month()) + '/' + toPaddedString(now.year());
  String time = toPaddedString(now.hour()) + ':' + toPaddedString(now.minute()) + ':' + toPaddedString(now.second());

  String state = "";
  if (currentState == STATE_AUTO) {
    state = "Auto > ";
  }
  if (isOpened) {
    state += "Ouvert";
  } else {
    state += "Fermé";
  }

  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_10);
  display.drawString(64, 0, date);
  display.setFont(ArialMT_Plain_24);
  display.drawString(64, 15, time);
  display.setFont(ArialMT_Plain_16);
  display.drawString(64, 44, state);
  display.display();

  delay(100);
}

void switchState() {
  currentState = (currentState + 1) % (STATE_OPEN + 1);
}

void refreshRelayPower() {
  if (relayPoweredUntil > millis()) {
    digitalWrite(PIN_RELAY, HIGH);
  } else {
    digitalWrite(PIN_RELAY, LOW);
  }
}

void refreshServoPosition() {
  if (isOpened) {
     servo.write(110);
  } else {
     servo.write(0);
  }
}

boolean computeIsOpened(DateTime &now) {
  boolean isOpened;
  switch (currentState) {
    case STATE_AUTO:
      isOpened = computeIsOpenedAuto(now);
      break;
    case STATE_CLOSE:
      isOpened = false;
      break;
    case STATE_OPEN:
      isOpened = true;
      break;
  }
  return isOpened;
}

boolean computeIsOpenedAuto(DateTime &now) {
  boolean isOpened = false;
  Time timeNow = toTime(now);
  int intervalsCount = (sizeof(AUTO_OPEN_INTERVALS) / sizeof(AUTO_OPEN_INTERVALS[0]));
  for (int i = 0; i < intervalsCount; i++) {
    if (intervalContains(AUTO_OPEN_INTERVALS[i], timeNow)) {
      isOpened = true;
      break;
    }
  }
  return isOpened;
}

String toPaddedString(int number) {
  String str = String(number, DEC);
  while (str.length() < 2) str = "0" + str;
  return str;
}

Time toTime(DateTime &dateTime) {
  return { dateTime.hour(), dateTime.minute(), dateTime.second() };
}

boolean intervalContains(TimeInterval interval, Time time) {
  int nowInSec = toSeconds(time);
  int startInSec = toSeconds(interval.start);
  int endInSec = toSeconds(interval.end);
  if (endInSec > startInSec) {
    return nowInSec >= startInSec && nowInSec < endInSec;
  } else {
    // midnight crossing
    return nowInSec >= startInSec || nowInSec < endInSec;
  }
}

int toSeconds(Time time) {
  return time.hour * 3600 + time.minute * 60 + time.second;
}
