/*  PROJECT FUGU FIRMWARE V1.10  (DIY 1kW Open Source MPPT Solar Charge Controller)
 *  By: TechBuilder (Angelo Casimiro)
 *  FIRMWARE STATUS: Verified Stable Build Version
 *  (Contact me for the experimental beta versions)
 *  -----------------------------------------------------------------------------------------------------------
 *  DATE CREATED:  02/07/2021 
 *  DATE MODIFIED: 09/03/2022
 *  -----------------------------------------------------------------------------------------------------------
 *  CONTACTS:
 *  GitHub - www.github.com/AngeloCasi (New firmware releases will only be available on GitHub Link)
 *  Email - casithebuilder@gmail.com
 *  YouTube - www.youtube.com/TechBuilder
 *  Facebook - www.facebook.com/AngeloCasii
 *  -----------------------------------------------------------------------------------------------------------
 *  PROGRAM FEATURES:
 *  - MPPT Perturbed Algorithm With CC-CV
 *  - WiFi & Bluetooth BLE Blynk Phone App Telemetry
 *  - Selectable Charger/PSU Mode (can operate as a programmable buck converter)
 *  - Dual Core ESP32 Unlocked (using xTaskCreatePinnedToCore(); )
 *  - Precision ADC Tracking Auto ADS1115/ADS1015 Detection (16bit/12bit I2C ADC)
 *  - Automatic ACS712-30A Current Sensor Calibration
 *  - Equipped With Battery Disconnect & Input Disconnect Recovery Protection Protocol
 *  - LCD Menu Interface (with settings & 4 display layouts)
 *  - Flash Memory (non-volatile settings save function)
 *  - Settable PWM Resolution (8bit-16bit)
 *  - Settable PWM Switching Frequency (1.2kHz - 312kHz)
 *  -----------------------------------------------------------------------------------------------------------
 *  PROGRAM INSTRUCTIONS:
 *  1.) Watch YouTube video tutorial first before using
 *  2.) Install the required Arduino libraries for the ICs
 *  3.) Select Tools > ESP32 Dev Board Board 
 *  4.) Do not modify code unless you know what you are doing
 *  5.) The MPPT's synchronous buck converter topology is code dependent, messing with the algorithm
 *      and safety protection protocols can be extremely dangerous especially when dealing with HVDC.
 *  6.) Install Blynk Legacy to access the phone app telemetry feature
 *  7.) Input the Blynk authentication in this program token sent by Blynk to your email after registration
 *  8.) Input WiFi SSID and password in this program
 *  9.) When using WiFi only mode, change "disableFlashAutoLoad = 0" to = 1 (LCD and buttons not installed)
 *      this prevents the MPPT unit to load the Flash Memory saved settings and will load the Arduino variable
 *      declarations set below instead
 *  -----------------------------------------------------------------------------------------------------------
 *  GOOGLE DRIVE PROJECT LINK: coming soon
 *  INSTRUCTABLE TUTORIAL LINK: coming soon
 *  YOUTUBE TUTORIAL LINK: www.youtube.com/watch?v=ShXNJM6uHLM
 *  GITHUB UPDATED FUGU FIRMWARE LINK: github.com/AngeloCasi/FUGU-ARDUINO-MPPT-FIRMWARE
 *  -----------------------------------------------------------------------------------------------------------
 *  ACTIVE CHIPS USED IN FIRMWARE:
 *  - ESP32 WROOM32
 *  - ADS1115/ADS1015 I2C ADC
 *  - ACS712-30A Current Sensor IC
 *  - IR2104 MOSFET Driver
 *  - CH340C USB TO UART IC
 *  - 16X2 I2C Character LCD

 *  OTHER CHIPS USED IN PROJECT:
 *  - XL7005A 80V 0.4A Buck Regulator (2x)
 *  - AMS1115-3.3 LDO Linear Regulator 
 *  - AMS1115-5.0 LDO Linear Regulator  
 *  - CSD19505 N-ch MOSFETS (3x)
 *  - B1212 DC-DC Isolated Converter
 *  - SS310 Diodes
 */
//================================ MPPT FIRMWARE LCD MENU INFO =====================================//
// The lines below are for the Firmware Version info displayed on the MPPT's LCD Menu Interface     //
//==================================================================================================//
String 
firmwareInfo      = "v3.6.0  ",
firmwareDate      = "13-08-22",
firmwareContactR1 = "   INDO  MPPT   ",
firmwareContactR2 = "                ";

//====================== ARDUINO LIBRARIES (ESP32 Compatible Libraries) ============================//
// You will have to download and install the following libraries below in order to program the MPPT //
// unit. Visit TechBuilder's YouTube channel for the "MPPT" tutorial.                               //
//============================================================================================= ====//
#include <EEPROM.h>                 //SYSTEM PARAMETER  - EEPROM Library (By: Arduino)
#include <Wire.h>                   //SYSTEM PARAMETER  - WIRE Library (By: Arduino)
#include <SPI.h>                    //SYSTEM PARAMETER  - SPI Library (By: Arduino)
#include <hd44780.h>
#include <hd44780ioClass/hd44780_I2Cexp.h>
#include <Adafruit_ADS1X15.h>       //SYSTEM PARAMETER  - ADS1115/ADS1015 ADC Library (By: Adafruit)
#include "driver/mcpwm.h"           //SYSTEM PARAMETER  - MCPWM Peripheral API
#include "soc/mcpwm_reg.h"
#include "soc/mcpwm_struct.h"

hd44780_I2Cexp lcd(0x27, I2Cexp_BOARD_DFROBOT);
Adafruit_ADS1015 ads;               //SYSTEM PARAMETER  - ADS1015 ADC Library (By: Adafruit) Kindly delete this line if you are using ADS1115
//Adafruit_ADS1115 ads;               //SYSTEM PARAMETER  - ADS1115 ADC Library (By: Adafruit) Kindly uncomment this if you are using ADS1115

//====================================== USER PARAMETERS ===========================================//
// The parameters below are the default parameters used when the MPPT charger settings have not     //
// been set or saved through the LCD menu interface or mobile phone WiFi app. Some parameters here  //
// would allow you to override or unlock features for advanced users (settings not on the LCD menu) //
//==================================================================================================//
#define relay_PV        26          //SYSTEM PARAMETER - Relay Solar Panel
#define relay_BAT       25          //SYSTEM PARAMETER - Relay Battery
#define buck_EN         32          //SYSTEM PARAMETER - Buck MOSFET Driver Enable Pin
#define LED             2           //SYSTEM PARAMETER - LED Indicator GPIO Pin
#define FAN             12          //SYSTEM PARAMETER - Fan GPIO Pin
#define ADC_ALERT       34          //SYSTEM PARAMETER - Fan GPIO Pin
#define TempSensor      35          //SYSTEM PARAMETER - Temperature Sensor GPIO Pin
#define buttonLeft      18          //SYSTEM PARAMETER - Button Up
#define buttonRight     19          //SYSTEM PARAMETER - Button Down
#define buttonBack      23          //SYSTEM PARAMETER - Button Enter
#define buttonSelect    17          //SYSTEM PARAMETER - Button Set
#define GPIO_PWM0A_OUT  33
#define GPIO_PWM0B_OUT  14
#define LVD_pin         15

//====================================== USER PARAMETERS ==========================================//
// The parameters below are the default parameters used when the MPPT charger settings have not    //
// been set or saved through the LCD menu interface or mobile phone WiFi app. Some parameters here //
// would allow you to override or unlock features for advanced users (settings not on the LCD menu)//
//=================================================================================================//
bool                                  
MPPT_Mode               = 1,           //   USER PARAMETER - Enable MPPT algorithm, when disabled charger uses CC-CV algorithm 
output_Mode             = 1,           //   USER PARAMETER - 0 = PSU MODE, 1 = Charger Mode  
disableFlashAutoLoad    = 0,           //   USER PARAMETER - Forces the MPPT to not use flash saved settings, enabling this "1" defaults to programmed firmware settings.
enablePPWM              = 1,           //   USER PARAMETER - Enables Predictive PWM, this accelerates regulation speed (only applicable for battery charging application)
enableFan               = 1,           //   USER PARAMETER - Enable Cooling Fan
enableBluetooth         = 1,           //   USER PARAMETER - Enable Bluetooth Connection
enableLCD               = 1,           //   USER PARAMETER - Enable LCD display
enableLCDBacklight      = 1,           //   USER PARAMETER - Enable LCD display's backlight
overrideFan             = 0,           //   USER PARAMETER - Fan always on
LVD                     = 1,           //   USER PARAMETER - Low Voltage Discharge
enableDynamicCooling    = 0;           //   USER PARAMETER - Enable for PWM cooling control 
int
serialMonitoringMode    = 0,           //  USER PARAMETER - Selects serial telemetry data feed (0 - Disable Serial, 1 - Display All Data, 2 - Display Essential, 3 - Number only)
pwmResolution           = 11,          //  USER PARAMETER - PWM Bit Resolution 
pwmFrequency            = 40000,       //  USER PARAMETER - PWM Switching Frequency - Hz (For Buck)
temperatureFan          = 60,          //  USER PARAMETER - Temperature threshold for fan to turn on
temperatureMax          = 90,          //  USER PARAMETER - Overtemperature, System Shudown When Exceeded (deg C)
telemCounterReset       = 0,           //  USER PARAMETER - Reset Telem Data Every (0 = Never, 1 = Day, 2 = Week, 3 = Month, 4 = Year) 
errorTimeLimit          = 1000,        //  USER PARAMETER - Time interval for reseting error counter (milliseconds)  
errorTimeLimitPWM       = 500,         //  USER PARAMETER - Time interval for reseting error counter (milliseconds)  
errorCountLimit         = 5,           //  USER PARAMETER - Maximum number of errors  
millisRoutineInterval   = 250,         //  USER PARAMETER - Time Interval Refresh Rate For Routine Functions (ms)
millisSerialInterval    = 1,           //  USER PARAMETER - Time Interval Refresh Rate For USB Serial Datafeed (ms)
millisLCDInterval       = 1000,        //  USER PARAMETER - Time Interval Refresh Rate For LCD Display (ms)
millisLCDBackLInterval  = 2000,        //  USER PARAMETER - Time Interval Refresh Rate For WiFi Telemetry (ms)
backlightSleepMode      = 0,           //  USER PARAMETER - 0 = Never, 1 = 10secs, 2 = 5mins, 3 = 1hr, 4 = 6 hrs, 5 = 12hrs, 6 = 1 day, 7 = 3 days, 8 = 1wk, 9 = 1month
baudRate                = 115200;      //  USER PARAMETER - USB Serial Baud Rate (bps)

float 
voltageBatteryMax       = 60.0000,     //   USER PARAMETER - Maximum Battery Charging Voltage (Output V)
voltageBatteryMin       = 10.0000,     //   USER PARAMETER - Minimum Battery Charging Voltage (Output V)
LVDBatteryMax           = 80.0000,     //   USER PARAMETER - Maximum Battery Voltage Reconnect (Output V)
LVDBatteryMin           = 22.0000,     //   USER PARAMETER - Minimum Battery Voltage Disconnect (Output V)
currentCharging         = 30.0000,     //   USER PARAMETER - Maximum Charging Current (A - Output)
electricalPrice         = 9.5000;      //   USER PARAMETER - Input electrical price per kWh (Dollar/kWh,Euro/kWh,Peso/kWh)


//================================== CALIBRATION PARAMETERS =======================================//
// The parameters below can be tweaked for designing your own MPPT charge controllers. Only modify //
// the values below if you know what you are doing. The values below have been pre-calibrated for  //
// MPPT charge controllers designed by TechBuilder (Angelo S. Casimiro)                            //
//=================================================================================================//
bool
ADS1015_Mode            = 1;          //  CALIB PARAMETER - Use 1 for ADS1015 ADC model use 0 for ADS1115 ADC model
int
ADC_GainSelect          = 1,          //  CALIB PARAMETER - ADC Gain Selection (0→±6.144V 3mV/bit, 1→±4.096V 2mV/bit, 2→±2.048V 1mV/bit)
avgCountVS              = 3,          //  CALIB PARAMETER - Voltage Sensor Average Sampling Count (Recommended: 3)
avgCountCS              = 2,          //  CALIB PARAMETER - Current Sensor Average Sampling Count (Recommended: 4)
avgCountTS              = 200;        //  CALIB PARAMETER - Temperature Sensor Average Sampling Count
float
calVoltageBattery       = 0.0000,     //  CALIB PARAMETER -
calVoltageSolar         = 0.0000,     //  CALIB PARAMETER -
inVoltageDivRatio       = 75.0000,    //  CALIB PARAMETER - Input voltage divider sensor ratio (change this value to calibrate voltage sensor)
outVoltageDivRatio      = 40.2156,    //  CALIB PARAMETER - Output voltage divider sensor ratio (change this value to calibrate voltage sensor)
vOutSystemMax           = 80.0000,    //  CALIB PARAMETER - 
vInSystemMax            = 150.0000,   //  CALIB PARAMETER - 
cOutSystemMax           = 60.0000,    //  CALIB PARAMETER - 
ntcResistance           = 7000.00,   //  CALIB PARAMETER - NTC temp sensor's resistance. Change to 10000.00 if you are using a 10k NTC
voltageDropout          = 1.0000,     //  CALIB PARAMETER - Buck regulator's dropout voltage (DOV is present due to Max Duty Cycle Limit)
voltageBatteryThresh    = 1.5000,     //  CALIB PARAMETER - Power cuts-off when this voltage is reached (Output V)
currentInAbsolute       = 51.0000,    //  CALIB PARAMETER - Maximum Input Current The System Can Handle (A - Input)
currentOutAbsolute      = 67.0000,    //  CALIB PARAMETER - Maximum Output Current The System Can Handle (A - Input)
PPWM_margin             = 99.5000,    //  CALIB PARAMETER - Minimum Operating Duty Cycle for Predictive PWM (%)
PWM_MaxDC               = 100.0000,    //  CALIB PARAMETER - Maximum Operating Duty Cycle (%) 90%-97% is good
efficiencyRate          = 1.0000,     //  CALIB PARAMETER - Theroretical Buck Efficiency (% decimal)
duty_PWM                = 0.0000,
InCurrentMidPoint       = 2.5250,     //  CALIB PARAMETER - Current Sensor Input Midpoint (V)
OutCurrentMidPoint      = 2.5250,     //  CALIB PARAMETER - Current Sensor Output Midpoint (V)
currentSens             = 0.0000,     //  CALIB PARAMETER - Current Sensor Sensitivity (V/A)
InCurrentSensV          = 0.0165,     //  CALIB PARAMETER - Current Sensor Sensitivity (mV/A) 0.0660 for ACS712 30A; 0.0165 for HLSR-50A
OutCurrentSensV         = 0.0200,     //  CALIB PARAMETER - Current Sensor Sensitivity (mV/A) 0.0200 for ACS758 100A
vInSystemMin            = 8.0000,     //  CALIB PARAMETER - 
vOutSystemMin           = 0.0000;     //  CALIB PARAMETER - 

//===================================== SYSTEM PARAMETERS =========================================//
// Do not change parameter values in this section. The values below are variables used by system   //
// processes. Changing the values can damage the MPPT hardware. Kindly leave it as is! However,    //
// you can access these variables to acquire data needed for your mods.                            //
//=================================================================================================//
bool
buckEnable            = 0,           // SYSTEM PARAMETER - Buck Enable Status
fanStatus             = 0,           // SYSTEM PARAMETER - Fan activity status (1 = On, 0 = Off)
bypassEnable          = 0,           // SYSTEM PARAMETER - 
chargingPause         = 0,           // SYSTEM PARAMETER - 
ReadSensor            = 1,           // SYSTEM PARAMETER - 
lowPowerMode          = 0,           // SYSTEM PARAMETER - 
buttonRightStatus     = 0,           // SYSTEM PARAMETER -
buttonLeftStatus      = 0,           // SYSTEM PARAMETER - 
buttonBackStatus      = 0,           // SYSTEM PARAMETER - 
buttonSelectStatus    = 0,           // SYSTEM PARAMETER -
buttonRightCommand    = 0,           // SYSTEM PARAMETER - 
buttonLeftCommand     = 0,           // SYSTEM PARAMETER - 
buttonBackCommand     = 0,           // SYSTEM PARAMETER - 
buttonSelectCommand   = 0,           // SYSTEM PARAMETER -
settingMode           = 0,           // SYSTEM PARAMETER -
setMenuPage           = 0,           // SYSTEM PARAMETER -
boolTemp              = 0,           // SYSTEM PARAMETER -
flashMemLoad          = 0,           // SYSTEM PARAMETER -  
confirmationMenu      = 0,           // SYSTEM PARAMETER -
BNC                   = 0,           // SYSTEM PARAMETER -  
REC                   = 0,           // SYSTEM PARAMETER - 
FLV                   = 0,           // SYSTEM PARAMETER - 
IUV                   = 0,           // SYSTEM PARAMETER - 
IOV                   = 0,           // SYSTEM PARAMETER - 
IOC                   = 0,           // SYSTEM PARAMETER - 
OUV                   = 0,           // SYSTEM PARAMETER - 
OOV                   = 0,           // SYSTEM PARAMETER - 
OOC                   = 0,           // SYSTEM PARAMETER - 
OTE                   = 0,           // SYSTEM PARAMETER - 
IUC                   = 0;
int
inputSource           = 0,           // SYSTEM PARAMETER - 0 = MPPT has no power source, 1 = MPPT is using solar as source, 2 = MPPTis using battery as power source
avgStoreTS            = 0,           // SYSTEM PARAMETER - Temperature Sensor uses non invasive averaging, this is used an accumulator for mean averaging
temperature           = 0,           // SYSTEM PARAMETER -
sampleStoreTS         = 0,           // SYSTEM PARAMETER - TS AVG nth Sample
pwmMax                = 0,           // SYSTEM PARAMETER -
pwmMaxLimited         = 0,           // SYSTEM PARAMETER -
pwmChannel            = 0,           // SYSTEM PARAMETER -
batteryPercent        = 0,           // SYSTEM PARAMETER -
errorCount            = 0,           // SYSTEM PARAMETER -
menuPage              = 0,           // SYSTEM PARAMETER -
subMenuPage           = 0,           // SYSTEM PARAMETER -
ERR                   = 0,           // SYSTEM PARAMETER - 
conv1                 = 0,           // SYSTEM PARAMETER -
conv2                 = 0,           // SYSTEM PARAMETER -
PPWM                  = 0.0000,      // SYSTEM PARAMETER -
duty_modulo           = 90,
duty_index            = 1,
intTemp               = 0;           // SYSTEM PARAMETER -
float
VSI                   = 0.0000,      // SYSTEM PARAMETER - Raw input voltage sensor ADC voltage
VSO                   = 0.0000,      // SYSTEM PARAMETER - Raw output voltage sensor ADC voltage
CSI                   = 0.0000,      // SYSTEM PARAMETER - Raw current sensor ADC voltage
CSI_converted         = 0.0000,      // SYSTEM PARAMETER - Actual current sensor ADC voltage
CSO                   = 0.0000,      // SYSTEM PARAMETER - Raw current sensor ADC voltage
CSO_converted         = 0.0000,      // SYSTEM PARAMETER - Actual current sensor ADC voltage 
TS                    = 0.0000,      // SYSTEM PARAMETER - Raw temperature sensor ADC value
powerInput            = 0.0000,      // SYSTEM PARAMETER - Input power (solar power) in Watts
powerInputPrev        = 0.0000,      // SYSTEM PARAMETER - Previously stored input power variable for MPPT algorithm (Watts)
powerOutput           = 0.0000,      // SYSTEM PARAMETER - Output power (battery or charing power in Watts)
energySavings         = 0.0000,      // SYSTEM PARAMETER - Energy savings in fiat currency (Peso, USD, Euros or etc...)
voltageInput          = 0.0000,      // SYSTEM PARAMETER - Input voltage (solar voltage)
voltageInputPrev      = 0.0000,      // SYSTEM PARAMETER - Previously stored input voltage variable for MPPT algorithm
voltageInputPrev1     = 0.0000,      // SYSTEM PARAMETER - Previously stored input voltage variable for MPPT algorithm
voltageOutput         = 0.0000,      // SYSTEM PARAMETER - Input voltage (battery voltage)
currentInput          = 0.0000,      // SYSTEM PARAMETER - Output power (battery or charing voltage)
currentOutput         = 0.0000,      // SYSTEM PARAMETER - Output current (battery or charing current in Amperes)
TSlog                 = 0.0000,      // SYSTEM PARAMETER - Part of NTC thermistor thermal sensing code
ADC_BitReso           = 0.0000,      // SYSTEM PARAMETER - System detects the approriate bit resolution factor for ADS1015/ADS1115 ADC
daysRunning           = 0.0000,      // SYSTEM PARAMETER - Stores the total number of days the MPPT device has been running since last powered
Wh                    = 0.0000,      // SYSTEM PARAMETER - Stores the accumulated energy harvested (Watt-Hours)
kWh                   = 0.0000,      // SYSTEM PARAMETER - Stores the accumulated energy harvested (Kiliowatt-Hours)
MWh                   = 0.0000,      // SYSTEM PARAMETER - Stores the accumulated energy harvested (Megawatt-Hours)
loopTime              = 0.0000,      // SYSTEM PARAMETER -
outputDeviation       = 0.0000,      // SYSTEM PARAMETER - Output Voltage Deviation (%)
buckEfficiency        = 0.0000,      // SYSTEM PARAMETER - Measure buck converter power conversion efficiency (only applicable to my dual current sensor version)
PWM                   = 0.0000,      // SYSTEM PARAMETER -
floatTemp             = 0.0000,
voltagePvSystem       = 0.0000;
unsigned long 
currentErrorMillis    = 0,           //SYSTEM PARAMETER -
currentErrorMillisPWM = 0,           //SYSTEM PARAMETER -
currentButtonMillis   = 0,           //SYSTEM PARAMETER -
currentSerialMillis   = 0,           //SYSTEM PARAMETER -
currentRoutineMillis  = 0,           //SYSTEM PARAMETER -
currentRoutineMillisPWM= 0,          //SYSTEM PARAMETER -
currentLCDMillis      = 0,           //SYSTEM PARAMETER - 
currentLCDBackLMillis = 0,           //SYSTEM PARAMETER - 
currentMenuSetMillis  = 0,           //SYSTEM PARAMETER - 
currentMenuMillis     = 0,           //SYSTEM PARAMETER - 
currentMillisBackflow = 0,           //SYSTEM PARAMETER - 
prevMenuMillis        = 0,           //SYSTEM PARAMETER - 
prevButtonMillis      = 0,           //SYSTEM PARAMETER -
prevSerialMillis      = 0,           //SYSTEM PARAMETER -
prevRoutineMillis     = 0,           //SYSTEM PARAMETER -
prevErrorMillis       = 0,           //SYSTEM PARAMETER -
prevErrorMillisPWM    = 0,           //SYSTEM PARAMETER -
prevLCDMillis         = 0,           //SYSTEM PARAMETER -
prevLCDBackLMillis    = 0,           //SYSTEM PARAMETER -
timeOn                = 0,           //SYSTEM PARAMETER -
loopTimeStart         = 0,           //SYSTEM PARAMETER - Used for the loop cycle stop watch, records the loop start time
loopTimeEnd           = 0,           //SYSTEM PARAMETER - Used for the loop cycle stop watch, records the loop end time
secondsElapsed        = 0,           //SYSTEM PARAMETER - 
DelayStart            = 0;           //SYSTEM PARAMETER - 

//========================= CUSTOM CHARACTER ===========================//
byte
Panel_Char[]    = {0x1F,0x15,0x1F,0x15,0x1F,0x15,0x1F,0x00},
Battery_Char[]  = {0x0E,0x1B,0x11,0x11,0x1F,0x1F,0x1F,0x00},
Left_Char[]     = {0x02,0x04,0x08,0x1F,0x08,0x04,0x02,0x00},
Right_Char[]    = {0x08,0x04,0x02,0x1F,0x02,0x04,0x08,0x00},
Strip_Char[]    = {0x00,0x00,0x00,0x1F,0x00,0x00,0x00,0x00};

//====================================== MAIN PROGRAM =============================================//
// The codes below contain all the system processes for the MPPT firmware. Most of them are called //
// from the 8 .ino tabs. The codes are too long, Arduino tabs helped me a lot in organizing them.  //
// The firmware runs on two cores of the Arduino ESP32 as seen on the two separate pairs of void   //
// setups and loops. The xTaskCreatePinnedToCore() freeRTOS function allows you to access the      //
// unused ESP32 core through Arduino. Yes it does multicore processes simultaneously!              // 
//=================================================================================================//

//==================== CORE1: SETUP ====================//
void setup() { 
  
  //SERIAL INITIALIZATION          
  Serial.begin(baudRate);                                   //Set serial baud rate
  Serial.println("> Serial Initialized");                   //Startup message
  
  //CREATE CHARACTERS
//  lcd.createChar(0, Panel_Char);
//  lcd.createChar(1, Battery_Char);  
//  lcd.createChar(2, Left_Char);
//  lcd.createChar(3, Right_Char);
//  lcd.createChar(4, Strip_Char);

  //GPIO PIN INITIALIZATION
  pinMode(relay_PV,OUTPUT);
  pinMode(relay_BAT,OUTPUT);
  pinMode(buck_EN,OUTPUT);
  pinMode(LED,OUTPUT); 
  pinMode(FAN,OUTPUT);
  pinMode(LVD_pin,OUTPUT);
  pinMode(TempSensor,INPUT); 
  pinMode(ADC_ALERT,INPUT);
  pinMode(buttonLeft,INPUT); 
  pinMode(buttonRight,INPUT); 
  pinMode(buttonBack,INPUT); 
  pinMode(buttonSelect,INPUT); 

  mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, GPIO_PWM0A_OUT);
  mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0B, GPIO_PWM0B_OUT);

  mcpwm_group_set_resolution(MCPWM_UNIT_0, 90000000);
  mcpwm_timer_set_resolution(MCPWM_UNIT_0, MCPWM_TIMER_0, 90000000);

  mcpwm_config_t pwm_config;
  pwm_config.frequency    = pwmFrequency;
  pwm_config.cmpr_a       = 0.0;
  pwm_config.cmpr_b       = 0.0;
  pwm_config.counter_mode = MCPWM_UP_COUNTER;
  pwm_config.duty_mode    = MCPWM_DUTY_MODE_0;
  mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &pwm_config);

  mcpwm_deadtime_enable(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_ACTIVE_HIGH_COMPLIMENT_MODE, 3, 3);

  //ADC INITIALIZATION
  ADC_SetGain();                                             //Sets ADC Gain & Range
  ads.begin();                                               //Initialize ADC

  //GPIO INITIALIZATION                          
  buck_Disable();
  
  //INITIALIZE AND LIOAD FLASH MEMORY DATA
  EEPROM.begin(512);
  Serial.println("> FLASH MEMORY: STORAGE INITIALIZED");  //Startup message 
  initializeFlashAutoload();                              //Load stored settings from flash memory       
  Serial.println("> FLASH MEMORY: SAVED DATA LOADED");    //Startup message 

  //LCD INITIALIZATION
  if(enableLCD==1){
    lcd.begin(16, 4);
    //CREATE CHARACTERS
    lcd.createChar(0, Panel_Char);
    lcd.createChar(1, Battery_Char);
    lcd.createChar(2, Left_Char);
    lcd.createChar(3, Right_Char);
    lcd.createChar(4, Strip_Char);
    lcd.setBacklight(HIGH);
    lcd.setCursor(0,1);
    lcd.print("MPPT INITIALIZED");
    delay(1000);
    lcd.setCursor(1,2);
    lcd.print("--INDO--MPPT--");
    delay(1500);
    lcd.clear();
  }

  //SETUP FINISHED
}
//====================== CORE1: LOOP ======================//
void loop() {
  Read_Sensors();         //TAB#2 - Sensor data measurement and computation
  Device_Protection();    //TAB#3 - Fault detection algorithm  
  System_Processes();     //TAB#4 - Routine system processes 
  Charging_Algorithm();   //TAB#5 - Battery Charging Algorithm
  LCD_Menu();             //TAB#6 - Low Power Algorithm
  Serial_Monitor();       //TAB#7 - (USB & Serial Monitoring)
}
