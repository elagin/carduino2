#include <Wire.h>
#include <LiquidCrystal.h>
#include <myBmp0855v.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <MsTimer2.h> 

#define PinA  18 // Odometer
#define TURBO_LED_PIN 9
bool turbo_led_state = false;
int pulse = 0;  // Odometer tick count
long prev_pulse = 0;
bool prev = false; // Prev state pulse count
bool isStart = true;

// voltage
#define POWER_SENSORS 2
#define VoltagePin A0;

struct powerData {
  float adj;
  float cur;
  float max;
  float min;
};
powerData powerSens[POWER_SENSORS];

// termometer
#define ONE_WIRE_BUS 10 // termometer bus
#define TEMPERATURE_PRECISION 10
#define MAX_DS1820_SENSORS 2
short sensorCount = 0;
short numberOfDevices = 0;

LiquidCrystal lcd(12, 11, 5, 4, 3, 2);
unsigned char screenSizeX=20;
unsigned char screenSizeY=4;

const unsigned char OSS = 0;  // Oversampling Setting

char buf[40];
char bufFloat[40];
char bufFloat2[40];

int min_dalas_temperature = 0;
int max_dalas_temperature = 0;
int dalas_temperature = 0;

OneWire oneWire(ONE_WIRE_BUS);  // Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
DallasTemperature sensors(&oneWire);    // Pass our oneWire reference to Dallas Temperature.
//DeviceAddress insideThermometer, outsideThermometer;    // arrays to hold device addresses
DeviceAddress tempDeviceAddress;

bool turboTimerActive = false;

void getVoltage(bool isRestart=false)
{
  for(int i=0; i < POWER_SENSORS; i++) {
    int value = analogRead(i);
    //        if(i==1)
    {
      sprintf(buf, "analogRead(%d): %d", i, value);
      Serial.println(buf);
    }
    if(value > 1023) {
      Serial.println("MAX!!");
      //            lcd.print("MAX!!");
      // alarm();
    } 
    else
    {
      float vout = (value * 5.0) / 1024.0;
//      Serial.println(vout);
//      sprintf(buf, "pin: %f: %f v.", i, powerSens[1].cur);
      Serial.print("PIN ");
      Serial.print(i);
      Serial.print(" : ");
      Serial.print(vout);            
      Serial.print("v. ");

      powerSens[i].cur = vout / powerSens[i].adj;
      if(powerSens[i].cur > powerSens[i].max) {
        powerSens[i].max = powerSens[i].cur;
      }
      if(powerSens[i].cur > 0 && (powerSens[i].cur < powerSens[i].min || isRestart) ) {
        powerSens[i].min = powerSens[i].cur;
      }
      Serial.print(" / ");
      Serial.println(powerSens[i].cur);
      Serial.print("v.");
    }
  }
//  sprintf(buf, "Sens %d: %d", powerSens[0].cur, powerSens[1].cur);
//  Serial.println(buf);
  Serial.println("============");
}

void setup()
{
  Serial.begin(9600);
  Wire.begin();

  pinMode(PinA, INPUT); 
  digitalWrite(PinA, HIGH);     // подключить подтягивающий резистор
  attachInterrupt(5, int_on, RISING);  // настроить прерывание interrupt 0 на pin 2
  attachInterrupt(4, turbo_rising, FALLING);  // настроить прерывание interrupt 4 на D19
  MsTimer2::set(2000, stop_engine); // 500ms период  // Настроить турботаймер
  
  lcd.begin(screenSizeX, screenSizeY);
  lcd.setCursor(0,0);
// 9.15 - 7.69
  powerSens[0].adj = 21500.0 / (42000.0 + 21500.0);
  powerSens[1].adj = 1;

  for(int i=0; i < POWER_SENSORS; i++) {
    pinMode(i, INPUT);
    Serial.print("pinMode ");
    Serial.print(i);
    Serial.println("to imput");
  }  

  barometer.Calibration();
  //  18b20_init();
  t_init();  

  getVoltage(true);
}

void t_init()
{
  Wire.begin();
  sensors.begin();
  oneWire.reset_search();
  // Loop through each device, print out address

  // Grab a count of devices on the wire
  numberOfDevices = sensors.getDeviceCount();
  lcd.print("Found ");
  lcd.print(numberOfDevices, DEC);
  lcd.print(" devices.");
  delay(500);
  for(int i=0;i<numberOfDevices; i++)
  {
    // Search the wire for address
    if(sensors.getAddress(tempDeviceAddress, i))
    {
/*      Serial.print("Found device ");
      Serial.print(i, DEC);
      Serial.print(" with address: ");
      //		printAddress(tempDeviceAddress);
      Serial.println();

      Serial.print("Setting resolution to ");
      Serial.println(TEMPERATURE_PRECISION, DEC);
*/
      // set the resolution to TEMPERATURE_PRECISION bit (Each Dallas/Maxim device is capable of several different resolutions)
      sensors.setResolution(tempDeviceAddress, TEMPERATURE_PRECISION);
/*
      Serial.print("Resolution actually set to: ");
      Serial.print(sensors.getResolution(tempDeviceAddress), DEC); 
      Serial.println();*/
    }
    else{
      lcd.print("Found ghost device at ");
      lcd.print(i, DEC);
      lcd.print(" Check power and cabling");
    }
  }
}

void printVoltage(int x, int y)
{
  for(int i=0; i < POWER_SENSORS; i++) {
    lcd.setCursor(x,y+i);
    char bufFloatVin[40];
    char bufFloatMin[40];
    char bufFloatMax[40];
    dtostrf(powerSens[i].cur, 2, 2, bufFloatVin);
    dtostrf(powerSens[i].min, 2, 2, bufFloatMin);
    dtostrf(powerSens[i].max, 2, 2, bufFloatMax);
    sprintf(buf, "%s/%s/%s ", bufFloatVin, bufFloatMin, bufFloatMax);
    lcd.print(buf);
  Serial.print(i);
  Serial.print(" : ");  
  Serial.println(powerSens[i].cur);
    //        Serial.println(buf);
  }
  Serial.println("======================");    
}


void displayValues(int x, int y, int & min, int & curr, int & max)
{
  lcd.setCursor(x,y);
  sprintf(buf, "[%d/%d/%d]", min, curr, max);    
  lcd.print(buf);
  Serial.println(buf);
}

void display()
{
  lcd.clear();  
  displayValues(0, 0, barometer.min_temperature, barometer.temperature, barometer.max_temperature);
  displayValues(10, 0, min_dalas_temperature, dalas_temperature, max_dalas_temperature);
  displayValues(0, 1, barometer.min_pressure, barometer.pressure, barometer.max_pressure);    
  printVoltage(0, 2);
}

void get_bmp_data()
{
  normalValues(barometer.GetTemperature()/10, barometer.min_temperature, barometer.temperature, barometer.max_temperature);
  normalValues(barometer.GetPressure()/133.3, barometer.min_pressure, barometer.pressure, barometer.max_pressure);
}

void loop()
{
  get_bmp_data();
  get_dalas_temp();
  getVoltage();
  printVoltage(0,3);
  display();
//  delay(5000);
  delay(1000);  
}

// обработка прерывания
void int_on()
{
  if(prev)
  {
    if(isStart)
    {
      isStart = !isStart;
    }
    else
    {
      pulse++;
    }
    //    wait = !wait;
  }
  prev = !prev;
}

void turbo_rising()
{
  if(turbo_prev)
  {
    
    turbo_led_on != turbo_led_on; // Отключаем флаг
//#define TURBO_LED_PIN 9
//bool turbo_led_on = false;    
  }
  else
  {
  }
  detachInterrupt(4);
  if(turboTimerActive)
  {
    MsTimer2::stop();  //выключить таймер 
  }
  MsTimer2::start();  //включить таймер 
  attachInterrupt(4, turbo_off, RISING);  // настроить прерывание interrupt 0 на pin 2    
}

void stop_engine() //обработчик прерывания
{
  digitalWrite(13, (output==HIGH) ? output=LOW : output=HIGH);
//  digitalWrite(13, (output==HIGH) ? output=LOW : output=HIGH);
}


void get_dalas_temp()
{
  sensors.requestTemperatures();
  // Loop through each device, print out temperature data
  for(int i=0;i<numberOfDevices; i++)
  {
    // Search the wire for address
    if(sensors.getAddress(tempDeviceAddress, i))
    {
      // It responds almost immediately. Let's print out the data
      short tempC = sensors.getTempC(tempDeviceAddress);
/*      Serial.println("================");
      Serial.print("Temp C: ");
      Serial.println(tempC);*/
      if(tempC < 79 && tempC > -100)
      {
        normalValues(tempC, min_dalas_temperature, dalas_temperature, max_dalas_temperature);                  
//        Serial.println("Temp OK");
      }
    } 
    //else ghost device! Check your power requirements and cabling
  }
}

void normalValues(const int current, int & min, int & normal, int & max)
{
  static short valuesCount = 3;
  if(0 == valuesCount)
  {
    if(max < current)
    {
      max = current;
    }
    if(min > current)
    {
      min = current;
    }
  }
  else
  {
    max = current;
    min = current;
    valuesCount--;    
  } 
  normal = current;  
}

