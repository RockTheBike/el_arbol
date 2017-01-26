/**** sLEDgeHammer
 * Arduino code to run the El Arbol lighting
 * ver. 2 (as of 4-17-2013
 * Written by:
 * Jake <jake@spaz.org>
 */

//#include <SoftPWM.h>

#define TESTMODE_KNOBVAL 100 // if knob is lower than this value, do testPins()
#define numLevels 15
int pin[numLevels] = {
  2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, A2, A4, A5};

#define numTree 8
byte tree[numTree] = {
  8,2,7,4,13,12,A4,10};

#define numBranches 8
byte branches[numBranches] = {
  6,9,A2,5,3,12,11,A5};

#define numFlickers 5
byte flickers[numFlickers] = {
  2,4,13,A4,12};

float levelVolt[numLevels] = {
  15.0, 25.0, 25.5, 26.0, 27.3, 28.2, 28.6, 29.2, 30.4, 30.8, 31.2}; // for testing

const int pwm = 0;
const int onoff = 1;
// type of each level (pwm or onoff)

// Arduino pin used as input to voltage sensor
const int voltpin = A0; // Voltage Sensor Input
const int ampspin = A3; // amperage sensor input
#define KNOBPIN A1 // rotary knob control input
const float knobfactor = 0.50;  // factor by which the knob affects voltish

#define VOLTCOEFF 13.179
const float ampscoeff = 20.25;  // ADC value divided by this equals amps
#define AMPSZERO 509.0  // ADC value corresponding to 0 amps

//MAXIMUM VOLTAGE TO GIVE LEDS
//const float maxvolt = 24 -- always run LEDs in 24V series configuration.
const float maxvolt = 24.0;

// vars to store temp values
int adcvalue = 0;
float voltage = 0;
float voltish = 0;  // voltage compensated by knob affect
float amperage = 0;

// on/off state of each level (for use in status output)
int state[numLevels];

const int AVG_CYCLES = 50; // average measured voltage over this many samples
const int DISPLAY_INTERVAL_MS = 500; // when auto-display is on, display every this many milli-seconds
const int VICTORYTIME=500;

int readCount = 0; // for determining how many sample cycle occur per display interval

// vars for current PWM duty cycle
int pwmValue;
//int pwmvalue24V;
boolean updatePwm = false;

// timing variables
unsigned long time = 0;
unsigned long timeRead = 0;
unsigned long timeDisplay = 0;
unsigned long timeWeStartedCheckingForVictory = 0;

// minimum time to keep each level on for
unsigned long onTime = 100;

unsigned long lastTime = 0;  // last time we did whatever it is

// current display level
int level = -1;

// var for looping through arrays
int i = 0;
int x = 0;
int y = 0;


void setup() {
  Serial.begin(57600);

  // Initialize Software PWM
  //  SoftPWMBegin();

  Serial.println("El Arbol 2.0");

  pinMode(voltpin,INPUT);

  // init LED pins
  for(i = 0; i < numLevels; i++) {
    pinMode(pin[i],OUTPUT);
    digitalWrite(pin[i], HIGH);  // default state is all lights on
  }
  randomSeed(analogRead(5));
}

#define under18 0
#define risingAbove18 1
#define risingAbove22 2
#define above25 3
#define fallingBelow25 4
int whatState = -1; // which of the above states are we in
unsigned long pattern;
unsigned long randomTime;

boolean victory = false;  // have we won?

void loop() {
  while (analogRead(KNOBPIN) < TESTMODE_KNOBVAL) testPins();
  time = millis();
  getvoltage();  // gets voltage AND amperage AND VOLTISH
  readCount++;

  if ((voltage > 18) && (whatState < risingAbove18)) {
    whatState = risingAbove18;
    allDark();  // go dark for 3 seconds
    doTheSweep();  // the whole animation
    allOn();  // go back to just all lit up
  }

  if ((voltage > 22) && (whatState == risingAbove18)) {
    whatState = risingAbove22;
    doTheSweep(); // do it immediately and it will happen every 20 seconds
    lastTime = time;  // it just got done
  }

  if ((whatState == risingAbove22) && (time - lastTime > 20000)) { // 20 seconds between 3-second sweeps
    doTheSweep();  // takes three seconds
    lastTime = time;  // it just got done
  }

  if (voltage  > 25) {
    whatState = above25;
    randomLights(); // takes 10 seconds
    lastTime = time;  // it just got done
    randomTime = 1500 + random(8500); // random amount of time 1.5 to 10 seconds
  }

  if ((voltage < 25) && (whatState == above25)) {
    whatState = fallingBelow25; // we are now falling
    lightning();
    lastTime = time;  // it just got done
    randomTime = 750 + random(4250); // random amount of time 0.75 to 5 seconds
  }

  if ((whatState == fallingBelow25) && (time - lastTime > randomTime)) {
    lightning();
    lastTime = time;  // it just got done
    randomTime = 750 + random(4250); // random amount of time 0.75 to 5 seconds
  }

  if ((whatState == above25) && (time - lastTime > randomTime)) {
    if (randomTime & 1) {  // randomly choose between two behaviors
      randomLights(); // takes 10 seconds
    }
    else {
      doTheSweep();
    }
    lastTime = time;  // it just got done
    randomTime = 1500 + random(8500); // random amount of time 1.5 to 10 seconds
  }



  if (voltage < 18) {
    whatState = under18;
    allOn();
  }


  //  if (analogRead(KNOBPIN) < 341) {
  //  for (i=0; i < numLevels; i++) digitalWrite(pin[i],LOW);
  //  if ((pattern & 15) < 12) digitalWrite(pin[(pattern & 15)],HIGH);
  //  } else
  //  if (analogRead(KNOBPIN) < 682) {
  //  } else {
  //    for (i=0; i < numLevels; i++) digitalWrite(pin[i],HIGH);
  //  }

  if(time - timeDisplay > DISPLAY_INTERVAL_MS){
    timeDisplay = time;
    printDisplay();
    readCount = 0;
  }
}

void testPins() {
  Serial.println("testPins() pin number:");
  for(i = 0; i < numLevels; i++) {
    digitalWrite(pin[i],HIGH);
    Serial.println(pin[i]);
    delay(1000);
    if (i == 0) delay(1000); // wait an extra second for the first one
    digitalWrite(pin[i],LOW);
  }
}

void lightning(){
  /* lightning is three little buzz buzz buzz of one of the five, then another,
   and another.  flickering is one second for each buzz.  pause between them
   0.5 seconds, 1 second pause between different branches buzzing  */
  for(i = 0; i < numLevels; i++) {
    digitalWrite(pin[i], HIGH);  // all lights ON
  }
  delay(3000); // All lights on. Wait 3 seconds.
  x = 1; // Flicker 2 like lightning for about 1-2 seconds
  for (i=0; i < 6; i++) {
    lastTime = millis();
    randomTime = 1000 + random(1000); // Flicker 2 like lightning for about 1-2 seconds
    while (millis() - lastTime < randomTime) {
      digitalWrite(flickers[x],LOW);
      delay(random(100)+100); // random amount of time 100-200 ms
      digitalWrite(flickers[x],HIGH);
      delay(random(100)+100); // random amount of time 100-200 ms
    }
    x = random(6); // Repeat but change which one is flickering each time.
  }
}

#define allDarkTime 3000 // how long allDark lasts
void allDark(){
  for(i = 0; i < numLevels; i++) {
    digitalWrite(pin[i], LOW);  // all lights off
  }
  delay(allDarkTime);
}

void allOn(){
  for(i = 0; i < numLevels; i++) {
    digitalWrite(pin[i], HIGH);  // all lights ON
  }
}

void doTheSweep(){
  for (i = 0; i < numTree; i++) { // Send light up the tree with a 100ms delay between each segment.
    digitalWrite(tree[i], HIGH);
    delay(100);
  }
  for (i = 0; i < numBranches; i++) { // Light the branches up with a 100 mS delay between each
    digitalWrite(branches[i], HIGH);
    delay(100);
  }
  delay(100);
  for (i = 0; i < numBranches; i++) { // 3. Now darken one branch at a time in the same order
    digitalWrite(branches[i], LOW);  // darken
    delay(100);
    digitalWrite(branches[i], HIGH); // turn it back on
  }
  delay(100);
  // Repeat step 3.
  for (i = 0; i < numBranches; i++) { // 3. Now darken one branch at a time in the same order
    digitalWrite(branches[i], 0);  // darken
    delay(100);
    digitalWrite(branches[i], 255); // turn it back on
  }
  delay(100);
  for (i = (numBranches-1); i >= 0; i--) {  // reverse order - start with last and go backwards
    digitalWrite(branches[i], 0);  // darken
    delay(100);
  }
  delay(100);
  for (i = (numTree-1); i >= 0; i--) {  // reverse order - start with last and go backwards
    digitalWrite(tree[i], 0);  // darken
    delay(100);
  }
  delay(100);
}

#define randomLightsTime 10000 // how long this routine will run
void randomLights(){
  time = millis();  // this line is not necessary, it was already done in the main loop
  while (millis() - time < randomLightsTime) {  // repeat until time is up
    pattern = random(2048); // up to 11 bits
    for (i=0; i < numLevels; i++) if (pattern & 2^i) {
      digitalWrite(pin[i],255);
    }
    else {
      digitalWrite(pin[i],0);
    }
    delay(pattern*20); // 10-20 times slower
  }
}

void getvoltage(){  // gets voltage AND amperage
  adcvalue = analogRead(voltpin);
  voltage = average(adc2volts(adcvalue), voltage);

#define VOLTKNEE 20

  if (voltage <= VOLTKNEE) { // VOLTKNEE IS WHERE WE START AFFECTING THINGS
    voltish = voltage;
  }
  else {
    voltish = VOLTKNEE + ((voltage - VOLTKNEE)* (1.0 + (float(analogRead(KNOBPIN) - 512)/512.0*knobfactor)));
    // KNOBPIN can read between 0 and 1023, -511 to 511 times knobfactor
    // means voltage * (1 + -knobfactor to +knobfactor)
    // if knobfactor is 0.50 then voltish can be 50 to 150% of voltage
  }
  adcvalue = analogRead(ampspin);
  amperage = average(adc2amps(adcvalue), amperage);
}

float average(float val, float avg){
  if(avg == 0)
    avg = val;
  return (val + (avg * (AVG_CYCLES - 1))) / AVG_CYCLES;
}

static int volts2adc(float v){
  /* voltage calculations
   *
   * Vout = Vin * R2/(R1+R2), where R1 = 100k, R2 = 10K
   * 30V * 10k/110k = 2.72V // at ADC input, for a 55V max input range
   *
   * Val = Vout / 5V max * 1024 adc val max (2^10 = 1024 max vaue for a 10bit ADC)
   * 2.727/5 * 1024 = 558.4896
   */
  //int led3volts0 = 559;

  /* 24v
   * 24v * 10k/110k = 2.181818181818182
   * 2.1818/5 * 1024 = 446.836363636363636
   */
  //int led2volts4 = 447;

  //adc = v * 10/110/5 * 1024 == v * 18.618181818181818;

  return v * 18.618181818181818;
}

float adc2volts(float adc){
  // v = adc * 110/10 * 5 / 1024 == adc * 0.0537109375;
  //  return adc * 0.0537109375; // 55 / 1024 = 0.0537109375;
  return adc / VOLTCOEFF;
}

float adc2amps(float adc){
  return (adc - AMPSZERO) / ampscoeff;
}


void printDisplay(){
  Serial.print("volts: ");
  Serial.print(voltage);
  Serial.print("  voltish: ");
  Serial.print(voltish);
  Serial.print(", amperage: ");
  Serial.print(amperage);
  Serial.print(" (");
  Serial.print(analogRead(ampspin));
  Serial.print("), Levels ");
  for(i = 0; i < numLevels; i++) {
    Serial.print(i);
    Serial.print("=");
    Serial.print(state[i]);
    Serial.print(",");
  }
  //  Serial.print("readCount: ");
  //  Serial.print(readCount);
  Serial.println("");
}















