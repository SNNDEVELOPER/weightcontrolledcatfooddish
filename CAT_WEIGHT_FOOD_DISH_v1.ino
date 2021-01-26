/*
 * A sketch using the Arduino library HX711_ADC with logic for 
   controlling an motorized food dish with an H-Bridge
   Shaun Nelson - 2021
*/

#include <HX711_ADC.h>
#include <EEPROM.h>

#define LED 13

int openMotorPin = 11;          // PIN USED TO SEND OPEN SIGNAL TO H-BRIDGE
int closeMotorPin = 10;         // PIN USED TO SEND CLOSED SIGNAL TO H-BRIDGE
const int MinCatWeight = 1100;  // CHANGE FOR YOUR SPECIFIC MIN WEIGHT
const int MaxCatWeight = 1450;  // CHANGE FOR YOUR SPECIFIC MAX WEIGHT
long interval = 300;            // MODIFY TO INCREASE OR DECREASE INTERVAL
long previousMillis = 0;
enum States { Closed, Open, Opening, Closing };
States State = Closed;
unsigned long OpenStartedTime;
unsigned long CloseStartedTime;

//pins:
const int HX711_dout = 4; //mcu > HX711 dout pin
const int HX711_sck = 5; //mcu > HX711 sck pin

//HX711 constructor:
HX711_ADC LoadCell(HX711_dout, HX711_sck);

const int calVal_calVal_eepromAdress = 0;
unsigned long t = 0;

void setup() {
  Serial.begin(57600); delay(10);
  Serial.println();
  Serial.println("Starting...");

  pinMode(openMotorPin, OUTPUT);
  pinMode(closeMotorPin, OUTPUT);


  float calibrationValue; // calibration value
  calibrationValue = 113.0; //696.0; // uncomment this if you want to set this value in the sketch
#if defined(ESP8266) || defined(ESP32)
  //EEPROM.begin(512); // uncomment this if you use ESP8266 and want to fetch this value from eeprom
#endif
  //EEPROM.get(calVal_eepromAdress, calibrationValue); // uncomment this if you want to fetch this value from eeprom

  LoadCell.begin();
  unsigned long stabilizingtime = 2000; // tare preciscion can be improved by adding a few seconds of stabilizing time
  boolean _tare = true; //set this to false if you don't want tare to be performed in the next step
  LoadCell.start(stabilizingtime, _tare);
  if (LoadCell.getTareTimeoutFlag()) {
    Serial.println("Timeout, check MCU>HX711 wiring and pin designations");
  }
  else {
    LoadCell.setCalFactor(calibrationValue); // set calibration factor (float)
    Serial.println("Startup is complete");
  }
  while (!LoadCell.update());
  Serial.print("Calibration value: ");
  Serial.println(LoadCell.getCalFactor());
  Serial.print("HX711 measured conversion time ms: ");
  Serial.println(LoadCell.getConversionTime());
  Serial.print("HX711 measured sampling rate HZ: ");
  Serial.println(LoadCell.getSPS());
  Serial.print("HX711 measured settlingtime ms: ");
  Serial.println(LoadCell.getSettlingTime());
  Serial.println("Note that the settling time may increase significantly if you use delay() in your sketch!");
  if (LoadCell.getSPS() < 7) {
    Serial.println("!!Sampling rate is lower than specification, check MCU>HX711 wiring and pin designations");
  }
  else if (LoadCell.getSPS() > 100) {
    Serial.println("!!Sampling rate is higher than specification, check MCU>HX711 wiring and pin designations");
  }
}

void loop() {
  static boolean newDataReady = 0;
  const int serialPrintInterval = 500; //increase value to slow down serial print activity

  // check for new data/start next conversion:
  if (LoadCell.update()) newDataReady = true;

  // get smoothed value from the dataset:
  if (newDataReady) {
    if (millis() > t + serialPrintInterval) {
      float i = LoadCell.getData();
      Serial.print("Load_cell output val: ");

      // LOGIC TO OPEN/CLOSE LID BASED ON CAT WEIGHT
      unsigned long currentMillis = millis();

      switch (State)
      {
        case Closed:
          {
            int ScaleWeight = i;
            if (ScaleWeight >= MinCatWeight && ScaleWeight <= MaxCatWeight)
            {
              OpenStartedTime = millis();
              digitalWrite(openMotorPin, HIGH);
              State = Opening;
            }
          }
          break;
        case Opening:
          if (millis() - OpenStartedTime > interval)
          {
            digitalWrite(openMotorPin, LOW);
            State = Open;
          }
          break;
        case Open:
          if (i < MinCatWeight)
          {
            CloseStartedTime = millis();
            digitalWrite(closeMotorPin, HIGH);
            State = Closing;
          }
          break;
        case Closing:
          if (millis() - CloseStartedTime > interval)
          {
            digitalWrite(closeMotorPin, LOW);
            State = Closed;
          }
          break;
        default:
          Serial.println("Invalid state");
          break;
      }

      Serial.println(i);
      newDataReady = 0;
      t = millis();
    }


  }

  // receive command from serial terminal, send 't' to initiate tare operation:
  if (Serial.available() > 0) {
    char inByte = Serial.read();
    if (inByte == 't') LoadCell.tareNoDelay();
  }

  // check if last tare operation is complete:
  if (LoadCell.getTareStatus() == true) {
    Serial.println("Tare complete");
  }

}

