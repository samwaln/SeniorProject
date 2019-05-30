
// ======================== LIBS ==============================
#include <avr/power.h>
#include <avr/sleep.h>
#include <Adafruit_NeoPixel.h> //https://github.com/adafruit/Adafruit_NeoPixel

// ======================== PINS ==============================
//for the indicator light its a neopixel
#define INDICATOR_PIN 8
#define SOFT_SWITCH_PIN 3

//accelerometer
#define x A3
#define y A4
#define z A5

// ======================== VARS ==============================
Adafruit_NeoPixel indicator = Adafruit_NeoPixel(1, INDICATOR_PIN);
uint8_t colors[3][3] = {{1, 1, 1}, {1, 1, 1}, {1, 0, 0}}; //color multiplier per state, 1,1,1 is white, 1,0,0, is red, etc. Something like {{1, 1, 1}, {0, 0, 1}, {1, 0, 0}}; is nice for debug to see it go into userSleepState 1
uint8_t colorState = 255;
uint8_t colorPulseIncrement = -1;
volatile bool cpuSleepFlag = true;
/*
The way this works is we keep a running sum of acceleration magnitude for all three axis. 
We take this sum over a window of time. If the sum < threshold, then the user will move into userSleepState of 1.
if the user is in userSleepState1 for consecutive windows for consecutiveThresholdTime, then we move into userSleepState of 2
we then notify the user with a red light. if the user does not surpase the threshold before the userReallyAsleepDelay is reached, then the user is considered asleep.
There are many ways to handle this, consider this a starting point.
*/
unsigned long nextReadTime, windowTime, cpuAwoken, userReallyAsleepStart, indicatorPulseTime;
int indicatorPulseDelay = 50; //bigger this is, the slower it will pulse.
int readDelay = 5; //read accelerometer every x ms
int windowDelay = 1500; //compute displacement every window of time
unsigned long userReallyAsleepDelay = 60000UL; // 1 minute in milliseconds, change as needed
int threshold = 5000; //amount of movement that sleep is less than. this is a sum magnitude so the lower the window, the lower this should be. This can be converted to fn of window time. Change as needed
int consecutiveThresholdTime = 60; // in seconds, change as needed
int userSleepState = 0; //0 awake, 1 possible asleep, 2 asleep
bool userReallyAsleep = false; //flag is set after sleep is 'confirmed'
bool newWindow = true; //when a new window is hit
//accelerometer values
int pxVal = 0;
int pyVal = 0;
int pzVal = 0;
int movementSum = 0; //running total of accelerometer magnitude
int consecutivePossibleSleeps = 0; //consecutive windows where possibly asleep.
int color1 = 0;
int color2 = 0;
int color3 = 0;

// ======================== SETUP ==============================
void setup() {
  Serial.begin(9600);
  Serial.println(F("Starting setup"));
  //warning: If you connect an external voltage reference to the AREF pin, you must do the following before calling analogRead().
  analogReference(EXTERNAL); //aref reference voltage
  //setup pins
  pinMode(SOFT_SWITCH_PIN, INPUT);
  //setup the timers
  nextReadTime = millis();
  windowTime = nextReadTime;
  //indicator light
  indicator.begin();
  indicator.setBrightness(50); //pick a good brightness the brighter, the more power.
  indicator.setPixelColor(0,0,0,255);
  //start with cpu sleep
  cpuSleepNow();
  Serial.println("setup done");
}
//========================= LOOP ===============================
void loop() {
  //put your main code here, to run repeatedly: 
  //everthing broken up into seperate functions to keep things simple.
  if(millis() - indicatorPulseTime > indicatorPulseDelay){
    indicatorPulseTime = millis();
    indicatorHandler();
  }
  softSwitchHandler();
  sleepHandler();
  
  //new calculation window every windowDelay
  if (millis() - windowTime > windowDelay) {
    windowTime = millis();
    newWindow = true;
  }
  
  //read accelerometer every readDelay amount of time
  if (millis() - nextReadTime > readDelay) {
    nextReadTime = millis();
    accelerometerHandler();
  }
}

// ======================== SOFT SWITCH ==============================
void softSwitchHandler(){
  if(digitalRead(SOFT_SWITCH_PIN) == LOW && millis()-cpuAwoken > 1000){
    //its held down. 
    cpuSleepNow();
  }
}

// ======================== ACCELEROMETER ==============================
void accelerometerHandler() {
  //read the accelerometer
  int xVal = analogRead(x);
  int yVal = analogRead(y);
  int zVal = analogRead(z);
  //  Serial.print("x: ");
  //  Serial.print(xVal);
  //  Serial.print(" y: ");
  //  Serial.print(yVal);
  //  Serial.print(" z: ");
  //  Serial.print(zVal);
  //if its a new calculation window
  if (newWindow) {
    //if the displacement is less than the threshold, then the user may be asleep.
    if (movementSum < threshold) {
      if(userSleepState !=2){
        userSleepState = 1;  
        Serial.println("Sleep State = 1");
        color1=0;
        color2=255;
        color3=0;
        indicatorHandler();
        
      }
    } else{
      Serial.println("Sleep State = 0");
      userSleepState = 0;
      color1=0;
      color2=255;
      color3=0;
      indicatorHandler();
    }

    if (userSleepState >= 1) {
      consecutivePossibleSleeps++;
      if (consecutivePossibleSleeps > (consecutiveThresholdTime/2 * ((float)1000/windowDelay))){
        userSleepState = 1;
        color1=255;
        color2=255;
        color3=0;
        indicatorHandler();
      }
      if (consecutivePossibleSleeps > (consecutiveThresholdTime * ((float)1000/windowDelay))){
        userSleepState = 2;
        Serial.println("Sleep State = 2");
        color1=255;
        color2=0;
        color3=0;
        indicatorHandler();
      }
    } else {
      consecutivePossibleSleeps = 0;
    }
    pxVal = xVal;
    pyVal = yVal;
    pzVal = zVal;
    movementSum = 0;
    newWindow = false;
  } else {
    //compute magnitude changes
    movementSum += abs(xVal - pxVal) + abs(yVal - pyVal) + abs(zVal - pzVal);
  }
}
// ======================== USER SLEEP ==============================
void sleepHandler(){
  if(userSleepState == 2){
    if(millis() - userReallyAsleepStart > userReallyAsleepDelay){
      userReallyAsleep = true;
    }
  }else{
    userReallyAsleepStart = millis();
  }
}

// ======================== INDICATOR ==============================
void indicatorHandler(){
  //pulse the indicator for whichever color it is. can change what the pulse looks like here. 
  colorState+=colorPulseIncrement;
  if(colorState < 1){
    colorPulseIncrement = 1;
  }else if(colorState == 254){
    colorPulseIncrement = -1;
  }else if(colorState >255){
    colorPulseIncrement = -1;
  }
  indicator.setPixelColor(0,color1, color2, color3);
  indicator.show();
}

// ======================== CPU SLEEP ==============================
void cpuSleepNow() {
    Serial.print("Sleep");  
    set_sleep_mode(SLEEP_MODE_PWR_DOWN); 
    sleep_enable();
    delay(100);
    //turn stuff off
    indicator.setPixelColor(0,0,0,0);
    indicator.show();
    attachInterrupt(digitalPinToInterrupt(3),pinInterrupt, FALLING);
    sleep_mode();
    sleep_disable();
    detachInterrupt(digitalPinToInterrupt(3));
}

void pinInterrupt()  
{  
    cpuAwoken = millis();
    reSetup();
}  

//when the microcontroller is woken up, lets reset values
void reSetup(){
  userReallyAsleep = false;
  userSleepState = 0;
}
