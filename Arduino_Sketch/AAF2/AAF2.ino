//------------------------------------------------------------------
// Ascom-Arduino Focusser
// Dave Wells
// Thanks for code snippets & inspiration:
//  o  Gina (Stargazers Lounge) for the stepper control basics
//  o  russellhq  (Stargazers Lounge) for the 1-wire code and info.
//------------------------------------------------------------------

//------ Change Log ------------------------------------------------
//  Version    Date        Change
//  0.0.2      26/05/2014  Initial - copied from Windows version
//  2.0.0      22/06/2014  Renumbered to match Ascom Driver Numbering
//  2.0.1      09/08/2014  Initial position set to 1000
//  2.0.2      16/08/2014  Halt function implemented with H# command
//                         New I# command to set an initial position
//
//  version numbering brought into line with driver numbering
//  2.2.0      03/10/2014  Implemented Temperature Sensing C# command
//------------------------------------------------------------------

// Include necessary libraries
#include <OneWire.h>                          //DS18B20 temp sensor
#include <DallasTemperature.h>                //DS18B20 temp sensor  
#define ONE_WIRE_BUS 6                        //DS18B20 DATA wire connected to Digital Pin 6

const String programName = "Arduino Focuser";
const String programVersion = "2.1.0";

const int     motorPins[4] = {7,8,9,10};       // Declare pins to drive motor control board
const int     motorSpeedLo = 16000;            // Motor step delay for Lo speed (uS)
const int     motorSpeedHi = 2000;             // Motor step delay for Hi speed (uS)
const int     motorSpeedDefault = 16000;       // Default motor step speed (uS)(failsafe operation)
const int     speedThreshold = 25;             // motor speed Hi if steps to go is higher than this
int           motorSpeed = motorSpeedDefault;  // current delay for motor step speed (uS)
DeviceAddress tempSensor;                      // Temperature sensor
double        currentTemperature;              // current temperature
boolean       tempSensorPresent = false;       // Is there a temperature sensor installed?

// Default initial positions if not set using the Innnn# command by Ascom Driver
int           currentPosition = 1000;             // current position
int           targetPosition = 1000;              // target position

// Initialise the temp sensor
OneWire oneWire(ONE_WIRE_BUS);         // Setup a oneWire instance to communicate with any OneWire devices
DallasTemperature sensors(&oneWire);   // Pass our oneWire reference to Dallas Temperature.
  
// lookup table to drive motor control board                                
const int stepPattern[8] = {B01000, B01100, B00100, B00110, B00010, B00011, B00001, B01001};

// For ASCOM connection
String inputString = "";         // string to hold incoming data
boolean stringComplete = false;  // whether the string is complete
boolean isMoving = false;        // is the motor currently moving

//------------------------------------------------------------------
// Move stepper anticlockwise 
//------------------------------------------------------------------
void anticlockwise()
{
  for(int i = 0; i < 8; i++)
  {
        setOutput(i);
        delayMicroseconds(motorSpeed);
  }
  clearOutput();
}
//------------------------------------------------------------------

//------------------------------------------------------------------
// Move stepper clockwise 
//------------------------------------------------------------------

void clockwise()
{
  for(int i = 7; i >= 0; i--)
  {
        setOutput(i);
        delayMicroseconds(motorSpeed);
  }
  clearOutput();
}
//------------------------------------------------------------------

//------------------------------------------------------------------
// Set output pins for stepper
//------------------------------------------------------------------
void setOutput(int out)
{
  for (int i=0; i<4; i++) {
    digitalWrite(motorPins[i], bitRead(stepPattern[out], i));
  }
}
//------------------------------------------------------------------

//------------------------------------------------------------------
// Clear output pins for stepper
// To ensure they are not left in an on state after movement
//------------------------------------------------------------------
void clearOutput()
{
  for (int i=0; i<4; i++) {
    digitalWrite(motorPins[i], 0);
  }
}
//------------------------------------------------------------------

//------------------------------------------------------------------
// Get Temperature 
//------------------------------------------------------------------
double GetTemperature()
{
  sensors.requestTemperatures();                    // Get temperatures
  double tempC = sensors.getTempC(tempSensor);      // Get Temperature from our (single) Sensor
  
  if (tempC == -127.00) {
    // error getting temperature, don't change current temperature
  }
  else {
    currentTemperature = tempC;
  }
  return currentTemperature;
}
//------------------------------------------------------------------

//------------------------------------------------------------------
// ASCOM Serial Commands
//------------------------------------------------------------------
void serialCommand(String command) {

  switch(command.charAt(0)) {
  case '#':  // Confirm Connection
    Serial.print("OK!#");
    break;
  case 'T': // Set Target Position
    {
      int hashpos = command.indexOf('#');    // position of hash in string
      String targetPosS = command.substring(1,hashpos);
      int targetPosI = targetPosS.toInt();
      targetPosition = targetPosI;
      Serial.print("T" + targetPosS + ":OK#");
      break;
    }
  case 'C': // Get Temperature
    {
      Serial.print("C");
      Serial.print(GetTemperature(), 2);
      Serial.print(":OK#");
      break;
    }
  case 'I': // Set Initial Position. Sets Position without any movement
    {
      int hashpos = command.indexOf('#');    // position of hash in string
      String initPosS = command.substring(1,hashpos);
      int initPosI = initPosS.toInt();
      currentPosition = initPosI;
      targetPosition = initPosI;
      Serial.print("I" + initPosS + ":OK#");
      break;
    }
  case 'P': // Get Current Position
    {
      String currentPositionS = String(currentPosition);
      Serial.print("P" + currentPositionS + ":OK#");
      break;
    }
  case 'H': // Halt
    {
      targetPosition = currentPosition;
      String currentPositionS = String(currentPosition);
      Serial.print("H" + currentPositionS + ":OK#");
      break;
    }
  case 'M': // Is motor moving
    {
      if (isMoving) {
        Serial.print("M1:OK#");
      } else {
        Serial.print("M0:OK#");
      }
      break;
    }
  case 'V': // Get Version and abilities
    {
      String tempInstalled = (tempSensorPresent == true ? " inc. Temp Sensor" : "");
      Serial.print( programName + " V" + programVersion + tempInstalled + "#");
      break;
    }
  default:
    {
      motorSpeed = motorSpeedDefault;
      Serial.print("ERR#");
      break;
    }
  }
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// Setup
//------------------------------------------------------------------
void setup() 
{
  // Declare the stepper motor pins as outputs
  for (int i=0; i<4; i++) {
    pinMode(motorPins[i], OUTPUT);
  }
  
  clearOutput();
      
  // initialize serial for ASCOM
  Serial.begin(9600);
  // reserve 200 bytes for the ASCOM driver inputString:
  inputString.reserve(200);

  // Set initial position to 1000. Quick fix to avoid negative positions.
  currentPosition = 1000;
  targetPosition = 1000;
  
  // OneWire Libary setup
  oneWire.reset_search();                    // Reset search
  oneWire.search(tempSensor);                // Search for temp sensor and assign address to tempSensor

  // DallasTemperature Library setup
  sensors.begin();                           // Initialise 1-wire bus
  
  // Check if the temperature sensor is installed
  if (sensors.getDeviceCount() == 0)
  {
    tempSensorPresent = false;
  }
  else
  {
    // temperature sensor installed - set it up and get initial value
    tempSensorPresent = true;
    sensors.setResolution(tempSensor, 10);     // Set the resolution to 12 bit (9bit=0.50C; 10bit=0.25C; 11bit=0.125C; 12bit=0.0625C)
    sensors.requestTemperatures();             // Get the Temperatures
    currentTemperature = GetTemperature();
  }
}
//------------------------------------------------------------------

//------------------------------------------------------------------
// Main Loop
//------------------------------------------------------------------
void loop()
{
  // process the command string when a hash arrives:
  if (stringComplete) {
    serialCommand(inputString); 
    
    // clear the command string:
    inputString = "";
    stringComplete = false;
  }
  
  // Move the position by a single step if target is different to current position
  if (targetPosition != currentPosition) {
    isMoving = true;
    
    // Adjust speed according to distance yet to travel
    if (abs(currentPosition - targetPosition) > speedThreshold) {
      motorSpeed = motorSpeedHi;
    } else {
      motorSpeed = motorSpeedLo;
    }
    
    // Going Anticlockwise to lower position
    if (targetPosition < currentPosition) {
      anticlockwise();
      currentPosition--;
    }
    
    // Going Clockwise to higher position
    if (targetPosition > currentPosition) {
      clockwise();
      currentPosition++;
    }
  } else {
    isMoving = false;
  }

}
//------------------------------------------------------------------

//------------------------------------------------------------------
// SerialEvent occurs whenever new data comes in the serial RX. 
//------------------------------------------------------------------
void serialEvent() {
  while (Serial.available()) {
    // get the new byte:
    char inChar = (char)Serial.read(); 
    // add it to the inputString:
    inputString += inChar;
    // if incoming character is hash, set flag so main loop can action it
    if (inChar == '#') {
      stringComplete = true;
    } 
  }
}
//------------------------------------------------------------------

