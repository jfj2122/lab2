/*
 * CU Vent Desing Project
 * Gabe Seymour & Jeffrey Jaquith
 * 
 * Program Arduino to compress ambu bag at given amplitude and frequency
 * Measure pressures to ensure bag has tight seal
 * Alarm user of any abnormal conditions
 * 
 * TODOs not listed 
 * - reset pin?
 * 
 */
#include <Wire.h>
#include <Adafruit_MotorShield.h>
#include <pt.h>
#include <LiquidCrystal.h>
#include <AccelStepper.h>

// Motorshield at 0x60 (default address)
Adafruit_MotorShield AFMS = Adafruit_MotorShield(0x60);

// Connecting motors to port 1 and 2 (left an right)
Adafruit_StepperMotor *motorL = AFMS.getStepper(200, 1);
Adafruit_StepperMotor *motorR = AFMS.getStepper(200, 2);

// wrappers for left and right motor movements ***** TODO - testing for single vs double and torque requirements
void forwardstepl() { motorL->onestep(FORWARD, SINGLE); }
void backwardstepl() { motorL->onestep(BACKWARD, SINGLE); }
void forwardstepr() { motorR->onestep(FORWARD, SINGLE); }
void backwardstepr() { motorR->onestep(BACKWARD, SINGLE): }

// wrap motors in AccelStepper object
AccelStepper stepperl(forwardstepl, backwardstepl);
AccelStepper stepperr(forwardstepr, backwardstepr);

// Set up digital and analog pins
// Analog Pins ****** TODO Need min and Max for analog sensors******
#define fsrL = A0      // Left FSR
#define fsrR = A1      // Right FSR
#define knob1 = A2     // anaolg pin off potentiometer1
#define knob2 = A3     // analog pin off potentiometer2

// Digital Pins 
#define audAlarm = 0 // Audion Alarm
#define alarmSil = 1 // Silence alarm
#define confirm_but = 3
#define emerOff = 4  // emergency off button
#define lsL = 5      // Left limit swtich
#define lsR = 6      // Right limit switch
LiquidCrystal lcd(7, 8, 9, 10, 11, 12);

// Global variables
int freq, mspeed;       // number of compressions per minute, rpm of motor
int bflag, eflag;       // flag for buttons, flag for emergency
int mflag;              // flag for motors
int period;             // Time between cycle initiation
int amplitude_steps;    // Actual distance the 
int zero_bag;           // Distance barely resting on bag
float ampl, diam;       // size of compressions, diameter of bag
long start_time         // start time per cycle

// Designate the threads 
//TimedAction moveMotors = TimedAction(period, actuate);
struct pt p_compress;     //thread for actuating motors
struct pt p_decomppress;  // thread for decopmressing motors
struct pt p_lcd;          // thread for operating LCD screen  

// Thread to actuate motors
// use a thread that runs every certain amount of time (to correspond to frequency)
static int actuate_copmress() {
  static unsigned long time_check;
  PT_BEGIN(pt);
  while(1){
    if (eflag == 1) break;
    time_check = millis();
    stepperl.moveTo(amplitude_steps);
    stepperr.moveTo(amplitude_steps);
    PT_WAIT_UNTIL(pt, millis() - time_check > period);
  }
  PT_END(pt);
}

// Thread to actuate motors
// runs after compression reaches full amplitude
static int actuate_decompress() {
  PT_BEGIN(pt);
  while(1) {
    PT_WAIT_UNTIL(pt, mflag == 1);
    if(eflag == 1) break;
    stepperr.moveTo(zero_bag);
    stepperl.moveTo(zero_bag);
    mflag = 0;
  }
  PT_END(pt);
}

// Thread lcd screen
// use a thread that waits for input for the lcd screen since
// it only needs to be updated when inputs change
static int lcd_screen(struct pt *pt) {
  PT_BEGIN(pt);
  while(1) {
    PT_WAIT_UNTIL(pt, bflag != 0);
    /*        TO DO
     * if statments, based on the indication changed or status
     * ie startup is flag 1, changed amplitude flag 3, alarm flag 5
     * after update set flag back to zero
     * - once confirm has been pushed load new values
     * - perform calculation on value adjusted same as setup
     * ----- need to investigate to see if setting LCD will block motors from being used
     */
  }
  PT_END(pt);
}

// Run when machine is started, zeros motors to limit switch then opens fully
void zeroMotors() {
  // ***** TODO using absoulte distance can travel on lead screw and bottom limit switch
}

// Finds relative zero for compression on the bag
long zeroOnBag() {
  // ***** TODO step in one at a time, zero in on bag until resistance felt at FSR
  // going to require clibaration of FSR
}

void setLCD() {
  // *****TODO - set the diameter, initial amplitude, initial frequency
  // once set prompt user to insert bag then press begin
}

void setup() {
  // set pins
  pinMode(fsrL, INPUT);
  pinMode(fsrR, INPUT);
  pinMode(knob1, INPUT);
  pinMode(knob2, INPUT);
  pinMode(emerOff, INPUT);
  pinMode(confirm_but, INPUT);
  pinMode(lsL, INPUT);
  pinMode(lsR, INPUT);
  pinMode(alarmSil, INPUT);
  pinMode(audAlarm, OUTPUT);

  // initiate (columns, rows)
  lcd.begin(20,4);
  
  // set speeds for zeroing motors
  stepperl.setSpeed(mspeed);
  stepperr.setSpeed(mspeed); // *** TODO - one constant speed or speed that adjusts?

  // call setup and zeriond functions - or we can just use them in the setup()
  zeroMotors(); 
  setLCD();
  zero_bag = zeroOnBag();
    
  // TODO ***** on board calculation
  // period from frequency
  // amplitude_steps from amplitude
  // speed adjustments
}

void loop() {
  // start multithreads
  actuate(&p_motors);
  lcd_screen(&p_lcd);
  stepperl.run();
  stepperr.run();
  /* **** TODO READ ALL INPUTS ****
   * The code above combined with the threading arrangment runs the motors non blocking
   * Read FSRs for pressure readings
   *  - IF pressure is out of range set Eflag = 2. Eflag 2 sounds alarm but keeps motors in operation, LCD will display error code
   *    The alarm can be silenced or the device can be killed with Emergency off - on emergency off motors will release
   *    
   *  - Reads pot-knobs as well as confirmation button to read for changes in settings, as POT moves LCD is updated, changes are not applied until 
   *    the confirmation button is pressed
   *    
   *    Psuedo Code
   *    press = FSR Calculations
   *    if (FSR !in range)
   *      elfag = 2
   *      DIGITAL_WRITE(sound_alarm_on)
   *      update LCD
   *    
   *    if (Digital_READ(alarm_silence)
   *      DIGITAL_WRITE(sound_alarm_off)
   *      
   *    if (emergency_off)
   *      elfag == 1
   *      stepperl.release();
   *      stepperr,release();
   *      
   *    if (POTL_NEW != POTL_OLD)
   *      POTL_OLD = POTL_NEW;
   *      update LCD
   *    
   *    if (POTR_NEW != POTR_OLD) 
   *      POTR_OLD = POTR_NEW;
   *      update LCD
   *      
   *    if (CONFIRMATION_BUTTON)
   *      Compute new rate and frequency 
   *      apply to global valirables for actuate_compress() method
   */

}
