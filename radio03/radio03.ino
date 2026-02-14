// Created by Rūdolfs Rutkovskis January 2026

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>

#define TEA5767_ADDR 0x60

#define BTN_UP 2
#define BTN_DN 3
#define BTN_ENC 4
#define EEPROM_ADDR_FREQ 0  // uses bytes 0–1
int  encoderPosCount = 0;
int  lastEncoderPosCount = 0;
int pinALast;  
int aVal;

LiquidCrystal_I2C lcd(0x27, 16, 2);

float frequency = 104.3;
const float step = 0.1;   // 100 kHz steps
bool muted = false;
bool frequencySaved = true;
bool frequencyUpdated = true;

void setFrequency(float freqMHz);
byte readSignalLevel();
void saveFrequency(float freq);
float loadFrequency();
void showDisplay();

unsigned long lastSaveUpdate = 0;
const unsigned long saveInterval = 30000; // ms

unsigned long lastFrequencyUpdated = 0;
const unsigned long frequencyUpdateInterval = 250; // ms

struct RadioStation {
  float freq;
  const char* name;
};

RadioStation stations[] = {
  {88.6,  "RADIO ROKS"},
  {89.2,  "Radio SWH Rock"},
  {90.0,  "Radio SWH GOLD"},
  {90.7,  "Latvijas Radio 1"},
  {91.5,  "Latvijas Radio 2"},
  {93.1,  "Latvijas Radio 5"},
  {93.9,  "Baltkom Radio"},
  {94.5,  "RETRO FM LATVIJA"},
  {94.9,  "Relax FM Latvija"},
  {95.8,  "Latvijas Radio 6"},
  {96.2,  "EHR Plus"},
  {96.8,  "EHR Superhits"},
  {97.3,  "Radio Marija Latvija"},
  {98.3,  "TOP RADIO"},
  {99.0,  "Radio 9"},
  {99.5,  "LOUNGE FM"},
  {100.0, "Radio SWH LV"},
  {100.5, "BBC World Service"},
  {101.0, "EHR LatviesHiti"},
  {101.8, "Kristigais Radio"},
  {102.3, "RADIO SKONTO +"},
  {102.7, "Radio Mix FM"},
  {103.2, "AUTORADIO LV"},
  {103.7, "Latvijas Radio 3"},
  {104.3, "EiropasHituRadio"},
  {105.2, "Radio SWH"},
  {105.7, "Radio SWH Plus"},
  {106.2, "STAR FM"},
  {106.8, "Radio TEV"},
  {107.2, "RADIO SKONTO"}
};

const int stationCount = sizeof(stations) / sizeof(stations[0]);


void setup() {
  Wire.begin();

  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DN, INPUT_PULLUP);
  pinMode(BTN_ENC, INPUT_PULLUP);
  pinALast = digitalRead(BTN_UP);//Read Pin A

  lcd.init();
  lcd.backlight();

  frequency = loadFrequency();
  setFrequency(frequency);   // uses your proven config
  delay(250);
  showDisplay();
}

void loop() {

static bool lastEncBtn = HIGH;
static unsigned long pressStartTime = 0;

bool encBtn = digitalRead(BTN_ENC);

// Button pressed
if (lastEncBtn == HIGH && encBtn == LOW) {
  pressStartTime = millis();
}

// Button released
if (lastEncBtn == LOW && encBtn == HIGH) {
  unsigned long pressDuration = millis() - pressStartTime;

  int stationIndex;

  if (pressDuration > 700) {
    // LONG PRESS → previous station
    stationIndex = findPreviousStationIndex(frequency);
  } else {
    // SHORT PRESS → next station
    stationIndex = findNextStationIndex(frequency);
  }

  frequency = stations[stationIndex].freq;

  muted = true;
  setFrequency(frequency);
  showDisplay();

  delay(80);  // short mute pop protection

  muted = false;
  setFrequency(frequency);

  lastSaveUpdate = millis();
  frequencySaved = false;
}

lastEncBtn = encBtn;



  aVal = digitalRead(BTN_UP);
   if (aVal != pinALast) {
     if (digitalRead(BTN_DN) != aVal) { 
       encoderPosCount ++;
      } 
     else {
       encoderPosCount--;
      }
    } 
    
   if (lastEncoderPosCount-encoderPosCount==-2){
     frequencyUpdated = false;
     lastFrequencyUpdated = millis();
     frequencySaved = false;
     lastEncoderPosCount=encoderPosCount;
     frequency=frequency+0.1;
     if (frequency > 108.0) frequency = 87.5;
     muted = true; setFrequency(frequency);
     showDisplay();
    }
   if (lastEncoderPosCount-encoderPosCount==2){
     frequencyUpdated = false;
     lastFrequencyUpdated = millis();
     frequencySaved = false;
     lastEncoderPosCount=encoderPosCount;
     frequency=frequency-0.1;
     if (frequency < 87.5) frequency = 108.0;
     muted = true; setFrequency(frequency);
     showDisplay();
    }
   pinALast = aVal;
   
    // this is for faster scrolling thru fm frequency
   if (millis() - lastFrequencyUpdated > frequencyUpdateInterval && !frequencyUpdated) {
     muted = false; setFrequency(frequency);
     frequencyUpdated = true;
    }

  // periodic save
  if (millis() - lastSaveUpdate > saveInterval && !frequencySaved) {
    saveFrequency(frequency);
    frequencySaved = true;
  }
}


int findNextStationIndex(float currentFreq) {
  for (int i = 0; i < stationCount; i++) {
    if (stations[i].freq > currentFreq + 0.05) {
      return i;
    }
  }
  return 0; // wrap around
}

int findPreviousStationIndex(float currentFreq) {
  for (int i = stationCount - 1; i >= 0; i--) {
    if (stations[i].freq < currentFreq - 0.05) {
      return i;
    }
  }
  return stationCount - 1; // wrap to last
}


void showDisplay() {
  int signalLevel = map(readSignalLevel(), 0, 15, 0, 100);

  lcd.setCursor(0, 0);
  lcd.print("FM ");
  lcd.print(frequency, 1);
  lcd.print(" MHz   ");

  lcd.setCursor(0, 1);

  // find matching station name
  for (int i = 0; i < stationCount; i++) {
    if (abs(stations[i].freq - frequency) < 0.05) {
      lcd.print(stations[i].name);
      lcd.print("                ");
      return;
    }
  }

  lcd.print("SIG: ");
  lcd.print(signalLevel);
  lcd.print("%        ");
}

//
// ===== =====
//
void setFrequency(float freqMHz) {
  unsigned int pll = (unsigned int)((freqMHz * 1000000 + 225000) / 8192);

  Wire.beginTransmission(TEA5767_ADDR);
  Wire.write(((pll >> 8) & 0x3F) | (muted ? 0x80 : 0x00)); // High byte (mute off)
  Wire.write(pll & 0xFF);        // Low byte
  Wire.write(0xB0);              // Stereo, high-side LO injection
  Wire.write(0x10);              // Soft mute off
  Wire.write(0x00);              // No PLL reference
  Wire.endTransmission();

  delay(120); // allow tuner to lock
}

byte readSignalLevel() {
  Wire.requestFrom(TEA5767_ADDR, 5);

  if (Wire.available() == 5) {
    Wire.read(); // byte 1
    Wire.read(); // byte 2
    Wire.read(); // byte 3
    byte b4 = Wire.read(); // IF counter + signal
    Wire.read(); // byte 5

    return (b4 >> 4) & 0x0F; // 0–15
  }
  return 0;
}
//------------------
void saveFrequency(float freq) {
  int value = (int)(freq * 10); // 104.3 → 1043
  EEPROM.put(EEPROM_ADDR_FREQ, value);
}
//----------------------
float loadFrequency() {
  int value;
  EEPROM.get(EEPROM_ADDR_FREQ, value);

  if (value < 875 || value > 1080) {
    return 104.3; // default if EEPROM empty
  }
  return value / 10.0;
}