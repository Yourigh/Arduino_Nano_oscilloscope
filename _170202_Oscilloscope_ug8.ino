#include "U8glib.h"
#include <EEPROM.h>

// Variables you might want to play with
byte useThreshold = 1;                  // 0 = Off, 1 = Rising, 2 = Falling
byte theThreshold = 128;                // 0-255, Multiplied by voltageConst
unsigned int timePeriod = 200;          // 0-65535, us or ms per measurement (max 0.065s or 65.535s)
byte voltageRange = 1;                  // 1 = 0-5V, 2 = 0-2.5V, 3 = 0-1.25V
byte ledBacklight = 128;

boolean autoHScale = true;             // Automatic horizontal (time) scaling
boolean linesNotDots = true;            // Draw lines between data points


//adjustmet for ADC speed for arduino nano for prescale 16: main clock 16Mhz / 16 = 1MHz. ADC Conversion takes 13 clocks so 1MHz/13 = 77kHz, so period 13us. 
//Some time to store result and process if statement need to be added. (+3us)
//default, non-high speed prescale is 128 http://forum.arduino.cc/index.php?topic=6549.0
const byte high_speedADC_time_us = 16; 

const byte BTNup = 2;
const byte BTNdown = 6;
const byte BTNrange = 3;

// Variables that can probably be left alone
const byte vTextShift = 3;              // Vertical text shift (to vertically align info)
const byte numOfSamples = 100;          // Leave at 100 for 128x64 pixel display
unsigned int HQadcReadings[numOfSamples];
byte adcReadings[numOfSamples];
byte thresLocation = 0;                 // Threshold bar location
float voltageConst = 0.03457146;          // Scaling factor for converting 0-63 to V  //it was 0.052381;  when 3.3V max
float avgV = 0.0;    
float maxV = 0.0;
float minV = 0.0;
float ptopV = 0.0;
float theFreq = 0;

const byte theAnalogPin = 7;             // Data read pin

const byte lcdLED = 5;                   // LED Backlight
const byte lcdA0 = 8;                    // Data and command selections. L: command  H : data
const byte lcdRESET = 9;                 // Low reset
const byte lcdCS = 10;                    // SPI Chip Select (internally pulled up), active low
const byte lcdMOSI = 11;                 // SPI Data transmissionans
const byte lcdSCK = 13;                  // SPI Serial Clock

// HW SPI:
U8GLIB_DOGM128 u8g(lcdSCK, lcdMOSI, lcdCS, lcdA0);    // SPI Com: SCK = 13, MOSI = 11, CS = 10, A0 = 8

// High speed ADC code
// From: http://forum.arduino.cc/index.php?PHPSESSID=e21f9a71b887039092c91a516f9b0f36&topic=6549.15
#define FASTADC 1
// defines for setting and clearing register bits
#ifndef cbi
#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#endif
#ifndef sbi
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
#endif

void collectData(void) {
  unsigned int tempThres = 0;
  unsigned int i = 0;

  if (autoHScale == true) {
    // With automatic horizontal (time) scaling enabled,
    // scale quickly if the threshold location is far, then slow down
    if (thresLocation > 5*numOfSamples/8) {
      timePeriod = timePeriod + 10;
    } else if (thresLocation < 3*numOfSamples/8) {
      timePeriod = timePeriod - 10;
    } else if (thresLocation > numOfSamples/2) {
      timePeriod = timePeriod + 2;
    } else if (thresLocation < numOfSamples/2) {
      timePeriod = timePeriod - 2;
    }
  }
  // Enforce minimum time periods
  if (timePeriod < high_speedADC_time_us) { 
    timePeriod = high_speedADC_time_us;
  }
  
  // Adjust voltage constant to fit the voltage range
  if (voltageRange == 1) {
    voltageConst = 0.079365151; //  0-5V    was//0.0523810; // 0-3.30V //
  } else if (voltageRange == 2) {
    voltageConst = 0.0261905; //  0-2.5V was//0.0261905; // 0-1.65V //
  } else if (voltageRange == 3) {
    voltageConst = 0.01309526; //  0-1,25V was//0.0130952; //0-0.825V //
  }
  
  // If using threshold, wait until it has been reached
  if (voltageRange == 1) tempThres = theThreshold << 2;
  else if (voltageRange == 2) tempThres = theThreshold << 1;
  else if (voltageRange == 3) tempThres = theThreshold;
  if (useThreshold == 1) {
     i = 0; while ((analogRead(theAnalogPin)>tempThres) && (i<32768)) i++;
     i = 0; while ((analogRead(theAnalogPin)<tempThres) && (i<32768)) i++;
  }
  else if (useThreshold == 2) {
     i = 0; while ((analogRead(theAnalogPin)<tempThres) && (i<32768)) i++;
     i = 0; while ((analogRead(theAnalogPin)>tempThres) && (i<32768)) i++;
  }

  // Collect ADC readings
  for (i=0; i<numOfSamples; i++) {
    // Takes 35 us with high speed ADC setting
    HQadcReadings[i] = analogRead(theAnalogPin);
    if (timePeriod > high_speedADC_time_us) //was -35
      {
        delayMicroseconds(timePeriod - high_speedADC_time_us); //was -35
      }
      
  }
  for (i=0; i<numOfSamples; i++) {
    // Scale the readings to 0-63 and clip to 63 if they are out of range.
    if (voltageRange == 1) {
      if (HQadcReadings[i]>>4 < 0b111111) adcReadings[i] = HQadcReadings[i]>>4 & 0b111111;
      else adcReadings[i] = 0b111111;
    } else if (voltageRange == 2) {
      if (HQadcReadings[i]>>3 < 0b111111) adcReadings[i] = HQadcReadings[i]>>3 & 0b111111;
      else adcReadings[i] = 0b111111;
    } else if (voltageRange == 3) {
      if (HQadcReadings[i]>>2 < 0b111111) adcReadings[i] = HQadcReadings[i]>>2 & 0b111111;
      else adcReadings[i] = 0b111111;
    }
    // Invert for display
    adcReadings[i] = 63-adcReadings[i];
  }
  
  // Calculate and display frequency of signal using zero crossing
  if (useThreshold != 0) {
     if (useThreshold == 1) {
        thresLocation = 1;
        while ((adcReadings[thresLocation]<(63-(theThreshold>>2))) && (thresLocation<numOfSamples-1)) (thresLocation++);
        thresLocation++;
        while ((adcReadings[thresLocation]>(63-(theThreshold>>2))) && (thresLocation<numOfSamples-1)) (thresLocation++);
     }
     else if (useThreshold == 2) {
        thresLocation = 1;
        while ((adcReadings[thresLocation]>(63-(theThreshold>>2))) && (thresLocation<numOfSamples-1)) (thresLocation++);
        thresLocation++;
        while ((adcReadings[thresLocation]<(63-(theThreshold>>2))) && (thresLocation<numOfSamples-1)) (thresLocation++);
     }

     theFreq = (float) 1000/(thresLocation * timePeriod) * 1000;
  }
  
  // Average Voltage
  avgV = 0;
  for (i=0; i<numOfSamples; i++)
     avgV = avgV + adcReadings[i];
  avgV = (63-(avgV / numOfSamples)) * voltageConst;

  // Maximum Voltage
  maxV = 63;
  for (i=0; i<numOfSamples; i++)
     if (adcReadings[i]<maxV) maxV = adcReadings[i];
  maxV = (63-maxV) * voltageConst;

  // Minimum Voltage
  minV = 0;
  for (i=0; i<numOfSamples; i++)
     if (adcReadings[i]>minV) minV = adcReadings[i];
  minV = (63-minV) * voltageConst;

  // Peak-to-Peak Voltage
  ptopV = maxV - minV;
}

void handleSerial(void) {
  char inByte;
  char dataByte;
  boolean exitLoop = false;
  do {
    // Clear out buffer
    do {
      inByte = Serial.read();
    } while (Serial.available() > 0);
  
    Serial.print("\nArduino LCD Oscilloscope\n");
    Serial.print(" 1 - Change threshold usage (currently: ");
      if (useThreshold == 0) Serial.print("Off)\n");
      else if (useThreshold == 1) Serial.print("Rise)\n");
      else if (useThreshold == 2) Serial.print("Fall)\n");
    Serial.print(" 2 - Change threshold value (x/255 of range) (currently: ");
      Serial.print(theThreshold, DEC); Serial.print(")\n");
    Serial.print(" 3 - Change sampling period (currently: ");
      Serial.print(timePeriod, DEC); Serial.print(")\n");
    Serial.print(" 4 - Change voltage range (currently: ");
      if (voltageRange == 1) Serial.print("0-5V)\n");
      else if (voltageRange == 2) Serial.print("0-2.5V)\n");
      else if (voltageRange == 3) Serial.print("0-1.25V)\n");
    Serial.print(" 5 - Toggle auto horizontal (time) scaling (currently: ");
      if (autoHScale == true) Serial.print("On)\n");
      else if (autoHScale == false) Serial.print("Off)\n");
    Serial.print(" 6 - Toggle line/dot display (currently: ");
      if (linesNotDots == true) Serial.print("Lines)\n");
      else if (linesNotDots == false) Serial.print("Dots)\n");
    Serial.print(" 8 - Exit\n");
    
    // Wait for input/response, refresh display while in menu
    do {
      collectData();
      // Picture Display Loop
      u8g.firstPage();  
      do { draw(); } while( u8g.nextPage() );
    } while (Serial.available() == 0);
    inByte = Serial.read();
    
    if (inByte == '1') {
      Serial.print("Change threshold usage\n");
      Serial.print(" 0 - Off\n");
      Serial.print(" 1 - Rise\n");
      Serial.print(" 2 - Fall\n");
      do { } while (Serial.available() == 0);
      dataByte = Serial.read();
      if (dataByte == '0') useThreshold = 0;
      else if (dataByte == '1') useThreshold = 1;
      else if (dataByte == '2') useThreshold = 2;
    } else if (inByte == '2') {
      Serial.print("Change threshold value (thresholds for 0-5V,0-2.5V,0-1.25V ranges)\n");
      Serial.print(" 0 - 32  (0.63V, 0.31V, 0.16V)\n");
      Serial.print(" 1 - 80  (1.57V, 0.78V, 0.39V)\n");
      Serial.print(" 2 - 128 (2.51V, 1.25V, 0.63V)\n");
      Serial.print(" 3 - 176 (3.45V, 1.73V, 0.86V)\n");
      Serial.print(" 4 - 224 (4.39V, 2.20V, 1.10V)\n");
      do { } while (Serial.available() == 0);
      dataByte = Serial.read();
      if (dataByte == '0') theThreshold = 32;
      else if (dataByte == '1') theThreshold = 80;
      else if (dataByte == '2') theThreshold = 128;
      else if (dataByte == '3') theThreshold = 176;
      else if (dataByte == '4') theThreshold = 224;
    } else if (inByte == '3') {
      Serial.print("Change sampling frequency\n");
      Serial.print(" 0 - 62.5 kHz (16  us/sample)\n");
      Serial.print(" 1 - 33.3 kHz (30  us/sample)\n");
      Serial.print(" 2 - 16.7 kHz (60  us/sample)\n");
      Serial.print(" 3 - 8.33 kHz (120 us/sample)\n");
      Serial.print(" 4 - 3.33 kHz (300 us/sample)\n");
      Serial.print(" 5 - 1.67 kHz (600 us/sample)\n");
      Serial.print(" 6 - 1.00 kHz (1   ms/sample)\n");
      Serial.print(" 7 - 0.50 kHz (2   ms/sample)\n");
      Serial.print(" 8 - 0.25 kHz (4   ms/sample)\n");
      do { } while (Serial.available() == 0);
      dataByte = Serial.read();
      if (dataByte == '0') timePeriod = 16;
      else if (dataByte == '1') timePeriod = 30;
      else if (dataByte == '2') timePeriod = 60;
      else if (dataByte == '3') timePeriod = 120;
      else if (dataByte == '4') timePeriod = 300;
      else if (dataByte == '5') timePeriod = 600;
      else if (dataByte == '6') timePeriod = 1000;
      else if (dataByte == '7') timePeriod = 2000;
      else if (dataByte == '8') timePeriod = 4000;
    } else if (inByte == '4') {
      Serial.print("Change voltage range\n");
      Serial.print(" 1 - 0-5.00V\n");
      Serial.print(" 2 - 0-2.50V\n");
      Serial.print(" 3 - 0-1.25V\n");
      do { } while (Serial.available() == 0);
      dataByte = Serial.read();
      if (dataByte == '1') voltageRange = 1;
      else if (dataByte == '2') voltageRange = 2;
      else if (dataByte == '3') voltageRange = 3;
    } else if (inByte == '5') {
      Serial.print("Toggle auto horizontal (time) scaling\n");
      Serial.print(" 0 - Off\n");
      Serial.print(" 1 - On\n");
      do { } while (Serial.available() == 0);
      dataByte = Serial.read();
      if (dataByte == '0') autoHScale = false;
      else if (dataByte == '1') autoHScale = true;
    } else if (inByte == '6') {
      Serial.print("Toggle line/dot display\n");
      Serial.print(" 0 - Lines\n");
      Serial.print(" 1 - Dots\n");
      do { } while (Serial.available() == 0);
      dataByte = Serial.read();
      if (dataByte == '0') linesNotDots = true;
      else if (dataByte == '1') linesNotDots = false;
    } else if (inByte == '8') {
      Serial.print("Bye!\n");
      exitLoop = true;
    }
  } while (exitLoop == false);
}

void draw(void) {
  int i;
  char buffer[16];
  
  u8g.setFont(u8g_font_micro);
  
  // Draw static text
  u8g.drawStr(0, 5+vTextShift, "Av");
  u8g.drawStr(0, 11+vTextShift, "Mx");
  u8g.drawStr(0, 17+vTextShift, "Mn");
  u8g.drawStr(0, 23+vTextShift, "PP");
  u8g.drawStr(0, 29+vTextShift, "Th");
  u8g.drawStr(24, 35+vTextShift, "V");
  u8g.drawStr(0, 41+vTextShift, "Tm");
  u8g.drawStr(4, 47+vTextShift, "ms/div");
  u8g.drawStr(20, 53+vTextShift, "Hz");
  u8g.drawStr(0, 59+vTextShift, "R");
  
  // Draw dynamic text
  if (autoHScale == true) u8g.drawStr(124, 5, "A");
  dtostrf(avgV, 3, 2, buffer);
  u8g.drawStr(12, 5+vTextShift, buffer);
  dtostrf(maxV, 3, 2, buffer);
  u8g.drawStr(12, 11+vTextShift, buffer);
  dtostrf(minV, 3, 2, buffer);
  u8g.drawStr(12, 17+vTextShift, buffer);
  dtostrf(ptopV, 3, 2, buffer);
  u8g.drawStr(12, 23+vTextShift, buffer);
  dtostrf(theFreq, 5, 0, buffer);
  u8g.drawStr(0, 53+vTextShift, buffer);
  if (useThreshold == 0) {
    u8g.drawStr(12, 29+vTextShift, "Off");
  } else if (useThreshold == 1) {
    u8g.drawStr(12, 29+vTextShift, "Rise");
    dtostrf((float) (theThreshold>>2) * voltageConst, 3, 2, buffer);
  } else if (useThreshold == 2) {
    u8g.drawStr(12, 29+vTextShift, "Fall");
    dtostrf((float) (theThreshold>>2) * voltageConst, 3, 2, buffer);
  }
  u8g.drawStr(8, 35+vTextShift, buffer);
  // Correctly format the text so that there are always 4 characters
  if (timePeriod < 400) {
    dtostrf((float) timePeriod/1000 * 25, 3, 2, buffer);
  } else if (timePeriod < 4000) {
    dtostrf((float) timePeriod/1000 * 25, 3, 1, buffer);
  } else if (timePeriod < 40000) {
    dtostrf((float) timePeriod/1000 * 25, 3, 0, buffer);
  } else { // Out of range
    dtostrf((float) 0.00, 3, 2, buffer);
  }
  u8g.drawStr(12, 41+vTextShift, buffer);
  if (voltageRange == 1) {
    u8g.drawStr(4, 59+vTextShift, "0-5.00");
  } else if (voltageRange == 2) {
    u8g.drawStr(4, 59+vTextShift, "0-2.50");
  } else if (voltageRange == 3) {
    u8g.drawStr(4, 59+vTextShift, "0-1.25");
  }
  
  // Display graph lines
  u8g.drawLine((128-numOfSamples),0,(128-numOfSamples),63);
  if (useThreshold != 0)
     for (i=29; i<127; i+=3)
        u8g.drawPixel(i,63-(theThreshold>>2));
  for (i=0; i<63; i+=5) {
     u8g.drawPixel(53,i);
     u8g.drawPixel(78,i);
     u8g.drawPixel(103,i);
     u8g.drawPixel(127,i);
  }
  // Threshold bar
  for (i=0; i<63; i+=3)
     u8g.drawPixel(thresLocation+(128-numOfSamples),i);
  // Draw ADC readings
  if (linesNotDots == true) {
    for (i=1; i<numOfSamples; i++) // Draw using lines
      u8g.drawLine(i+(128-numOfSamples)-1,adcReadings[i-1],i+(128-numOfSamples),adcReadings[i]);
  } else {
    for (i=2; i<numOfSamples; i++) // Draw using points
      u8g.drawPixel(i+(128-numOfSamples),adcReadings[i]);
  }
}


//ISR for button  //button pressed when FALSE
/*void BTN_ISR()
{
  
  Serial.print("   Mode button pressed");
}*/

void setup() {

  //RESET procedure 170202
    pinMode(lcdRESET, OUTPUT);
  digitalWrite(lcdRESET, LOW);
  delay(10);
  digitalWrite(lcdRESET, HIGH);
  delay(10);
  pinMode (BTNup,INPUT);  
  pinMode (BTNup,INPUT_PULLUP); 
  pinMode (BTNdown,INPUT);  
  pinMode (BTNdown,INPUT_PULLUP); 
  pinMode (BTNrange,INPUT);  
  pinMode (BTNrange,INPUT_PULLUP); 
  //attachInterrupt(digitalPinToInterrupt(BTNrange), BTN_ISR, FALLING);  // ISR for button
  u8g.begin();
  Serial.begin(9600);
  
  // Turn on LED backlight
  analogWrite(lcdLED, ledBacklight);
  
  #if FASTADC
    // set prescale to 16
    sbi(ADCSRA,ADPS2) ;
    cbi(ADCSRA,ADPS1) ;
    cbi(ADCSRA,ADPS0) ;
  #endif
  delay(100);
}

void loop() {
  collectData();
  // Picture Display Loop
  u8g.firstPage();  
  do { draw(); } while( u8g.nextPage() );

  // If user sends any serial data, show menu
  if (Serial.available() > 0) {
    handleSerial();
  }

  // rebuild the picture after some delay
  delay(100);

  //turn off autoset 8s after start
  if (millis()>8000) 
    autoHScale = false; 

  if (digitalRead(BTNrange)== 0){
    if (++voltageRange == 4)
      voltageRange = 1;
  }
  if (digitalRead(BTNup)==0)
    if (timePeriod >= 4000) timePeriod = 4000;
      else if (timePeriod >= 2000) timePeriod = 4000;
      else if (timePeriod >= 1000) timePeriod = 2000;
      else if (timePeriod >= 600) timePeriod = 1000;
      else if (timePeriod >= 300) timePeriod = 600;
      else if (timePeriod >= 120) timePeriod = 300;
      else if (timePeriod >= 60) timePeriod = 120;
      else if (timePeriod >= 30) timePeriod = 60;
      else if (timePeriod >= 16) timePeriod = 30;
  if (digitalRead(BTNdown)==0)
    if (timePeriod <= 16) timePeriod = 16;
      else if (timePeriod <= 30) timePeriod = 16;
      else if (timePeriod <= 60) timePeriod = 30;
      else if (timePeriod <= 120) timePeriod = 60;
      else if (timePeriod <= 300) timePeriod = 120;
      else if (timePeriod <= 600) timePeriod = 300;
      else if (timePeriod <= 1000) timePeriod = 600;
      else if (timePeriod <= 2000) timePeriod = 1000;
      else if (timePeriod <= 4000) timePeriod = 2000;   
}

