/* Code Jack Kelleher 25/11/2018
 * Elements relating to weather API adapted from https://www.hackster.io/officine/getting-weather-data-655720
 * Elements relating to MPR121 adapted from the MPR121 example "SimpleTouch"
 * Interrupt and Blink Without Delay-style codes extrapolated from a large variety of web-based examples.
 * Commented code is not used in the current prototype due to structural limitations but would be implemented in the full version.
 * This sketch will change pressure and temp to those at a given location, but not clock time. This is simply because I did not build the
 * override mechanism into my current version of the clock, so unless you spin the seconds at several thousand RPM there's no way to 
 * change the time in a sensible manner. 
 */

/*PINS IN USE: 
Motor control transistor: D7
MPR121 interrupt: D6
Hall Effect minute sensor: D4
Servos: D1,2,3
MPR121 comms: D11,12
*/

/*COMPONENTS:
 * - MOSFET for motor control
 * - MPR121 for touch sensing
 * - Hall Effect sensor for timing
 * - 3 servos for controlling the birds
 * - 1 motor, 10RPM at 6V
 * - MKR1000 for WiFi connectivity and overall control
 */

#include <MPR121.h>
#include <Wire.h>

//#include <Time.h>
//#include <TimeLib.h>

#include <ArduinoJson.h>
#include <SPI.h>
#include <WiFi101.h>

#include <Servo.h>

//Define the number of touch buttons for location choice
#define numElectrodes 5

//Initialise WiFi variables
char ssid[] = "4GEE_WiFi_6929_2.4GHz"; //  your network SSID (name)
char pass[] = "ageing-oclock-6929";//  your network PASSWORD ()

//open weather map api key
String apiKey= "5a2d94f06315a65bf491c6785e5495eb";
//Set the location for weather; default is London
String location = "London,uk";
volatile int locCode = 0;
int status = WL_IDLE_STATUS;
char server[] = "api.openweathermap.org";
WiFiClient client;

//Initialise a two-dimensional array storing possible locations
const char* locations[] = {"London,uk", "Newport,us", "Toulon,fr", "Split, hr", "Oslo, no"};

//Initialise clock motor control pin
//int mPin = 7;

//Initialise MPR121 interrupt pin on Arduino
int touchIrq = 6;

//Set a default temp and pressure
int currentTemp = 20;
int pressure = 1010;

//Set an original temp and pressure
int oldTemp = 20;
int oldPress = 1010;

volatile boolean weatherTrigger = false;

//Set a weather-checking routine
long lastWeatherCheck = 0;
long weatherCheckInterval = 5000;//600000; //Check the weather every ten minutes.

//Servo timing check
long lastServoMove = 0;
long servoInterval = 30;

//Define the hall effect sensor input pins + time-related variables
//#define minSense 4
//volatile int minCount = 0;
//long lastTimeCheck = 0;
//long timeCheckInterval = 600000; //Check the time every ten minutes.

//Introduce the servos
Servo pressIncline;
Servo pressRise;
Servo tempRise;
int posIncline = 90;
int posTemp = 90;
int posPress = 90;

//Global time counter
long currentMillis = millis();

void setup() {
  //Initialize serial and wait for port to open:
  Serial.begin(9600);
  while(!Serial);
  Serial.println("Hello world.");
  
  // Initialise MPR121
  if(!MPR121.begin(0x5A)){ 
    Serial.println("error setting up MPR121");  
    switch(MPR121.getError()){
      case NO_ERROR:
        Serial.println("MPR121: no error");
        break;  
      case ADDRESS_UNKNOWN:
        Serial.println("MPR121: incorrect address");
        break;
      case READBACK_FAIL:
        Serial.println("MPR121: readback failure");
        break;
      case OVERCURRENT_FLAG:
        Serial.println("MPR121: overcurrent on REXT pin");
        break;      
      case OUT_OF_RANGE:
        Serial.println("MPR121: electrode out of range");
        break;
      case NOT_INITED:
        Serial.println("MPR121: not initialised");
        break;
      default:
        Serial.println("MPR121: unknown error");
        break;      
    }
  }
  
  // Set up the MPR121
  MPR121.setInterruptPin(4);
  MPR121.setTouchThreshold(40);
  MPR121.setReleaseThreshold(20);  
  MPR121.updateTouchData();

  pressIncline.attach(3);
  tempRise.attach(2);
  pressRise.attach(1); //CHECK PIN - MAYBE IMPLEMENT FEATHERWING

  //Set motor pin
  //pinMode(mPin,OUTPUT);

  //Run motor at half-speed by PWM, from a 12V power supply (10RPM). This allows to occasionally accelerate the motor for timekeeping.
  //SEE NOTE ON checkTime() FXN FOR WHY I KNOW THIS IS A TERRIBLE IDEA.
  //analogWrite(mPin, 127);

  //Set servo position to midpoint for all servos
  Serial.println("Setting position of servo");
  pressIncline.write(posIncline);
  delay(15);
  pressRise.write(posPress);
  delay(15);
  tempRise.write(posTemp);
  delay(15);

  //Set time tracking pins
  //pinMode(minSense, INPUT_PULLUP);
  //attachInterrupt(digitalPinToInterrupt(minSense), minCountFxn, RISING);

  //Set the MPR121 tracking pin
  pinMode(touchIrq, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(touchIrq), setLoc, FALLING);

  // attempt to connect to Wifi network:
  /*while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);

    status = WiFi.begin(ssid, pass);
    
    // wait 10 seconds for connection:
    delay(1000);
  }
  Serial.println("Connected to wifi");*/
  Serial.println("I got this far!");
  getWeather();
  setWeather();

  //Now the birds are at the default height and inclination. Assuming the clock has been set to start at the right time, it will be ready to go! 
  //Setting the clock time is a project for the future. 
}

void loop() {
  //Serial.println("Hey!");
  //delay(1000);
  //Check the time periodically and correct if it's wrong
  /*if (currentMillis - lastTimeCheck >= timeCheckInterval) {
    lastTimeCheck = currentMillis;
    checkTime();
  }*/
  if (weatherTrigger == true) {
    getWeather();
    setWeather();
    weatherTrigger = false;
  }
  
  //Check the weather every ten minutes and update the birds.
  if (currentMillis - lastWeatherCheck >= weatherCheckInterval) {
    Serial.println("Time to check the weather!"); 
    lastWeatherCheck = currentMillis;
    getWeather();
    setWeather();
  }  
}

//This function checks the time against the number of minutes counted and corrects the clock if necessary.
//NOTE: I am aware that overvolting the motor is a terrible idea. This part of the code is purely for kicks. In a full setup,
//I would use a 6V 30RPM motor, and set the standard PWM to 84; this would require more frequent timing corrections since 85
//is not quite a third of 256. I could also use a stepper. Using a 12V supply is also a nuisance for powering my servos.
/*void checkTime() {
  time_t currentTime = WiFi.getTime();
  time_t hourNow = hour(currentTime);
  time_t minuteNow = minute(currentTime);

  //If the time is not lining up, double or halve the speed of the clock until it does.
  if (minuteNow > minCount) {
    int delayCorrect = minuteNow - minCount * 60000; 
    analogWrite(mPin, 255);
    delay(delayCorrect);
    analogWrite(mPin, 127);
  } else if (minuteNow < minCount) {
    int delayCorrect = minCount - minuteNow * 60000;
    analogWrite(mPin, 64);
    delay(delayCorrect);
    analogWrite(mPin, 127);
  }

  //Checking the specific hour or minute is more complex - would require identifying each marker in turn by a specific signal, rather than an indistinct magnet at each increment. 
  //This function will keep the clock on time to the minute, assuming it is started on a zero minute of an hour, and will disregard any hour discrepancy. 
}*/


//This function gets the weather from openweathermap.org's free API. 
void getWeather() {

  //USE THIS LINE WHEN MPR121 IS IN USE
  //location = locations[locCode];

  Serial.println("\nStarting connection to server...");
  // if you get a connection, report back via serial:
  if (client.connect(server, 80)) {
    Serial.println("connected to server");
    // Make a HTTP request:
    client.print("GET /data/2.5/weather?");
    client.print("q="+location);
    client.print("&APPID="+apiKey);
    client.println("&units=metric");
    client.println("Host: api.openweathermap.org");
    client.println("Connection: close");
    client.println();
  } else {
    Serial.println("unable to connect");
  }

  delay(1000);
  String line = "";

  while (client.connected()) {
    line = client.readStringUntil('\n');
    
    //Serial.println(line);
    Serial.println("parsingValues");

    //create a json buffer where to store the json data
    StaticJsonBuffer<5000> jsonBuffer;

    JsonObject& root = jsonBuffer.parseObject(line);
    if (!root.success()) {
      Serial.println("parseObject() failed");
      return;
    }

  //get the data from the json tree
  currentTemp = root["main"]["temp"];
  pressure = root["main"]["pressure"];
  //root.printTo(conditions);
  Serial.println("Current temp is: ");
  Serial.println(currentTemp);
  Serial.println("Pressure is: ");
  Serial.println(pressure);
  }
}

//This function sets the three servos to their correct positions based on the range of temp and pressure allowed.
void setWeather() {
  Serial.println("Setting weather...");
  //Don't bother if nothing's changed!
  if (currentTemp != oldTemp) { 
    Serial.println("Temperature has changed.");
    
    //Keep temp in sensible range...
    if (currentTemp > 30) {
      currentTemp = 30;
    } else if (currentTemp < -10) {
      currentTemp = -10;
    }

    //Calculate the servo movement conversion factor; servo angle is between 50 and 120, temp is -10 to 30
    int tempMove = ((70/40) * (currentTemp + 10)) + 50;
    //Move the bird to the correct position, slowly
    servoSeek(tempRise, tempMove, posTemp);
    //tempRise.write(tempMove);

    //Set the old temperature to be the latest reading in preparation for the next check.
    oldTemp = currentTemp;
  }

  //Now do the same for pressure
  if (pressure != oldPress) {
    Serial.println("Pressure has changed.");

    //Keep pressure in a sensible range too.
    if (pressure > 1040) {
    pressure = 1040;
    } else if (pressure < 970) {
      pressure = 970;
    }

    //Set the bird's pitch to show whether pressure inclination is rising, falling, or steady
    if (pressure > oldPress) {
      pressIncline.write(110); // CHECK THIS NUMBER
    } else if (pressure < oldPress) {
      pressIncline.write(70); //AND THIS ONE TOO
    } else {
      pressIncline.write(90);
    }

    //Calculate pressure movement conversion
    int pressMove = ((60/70) * (pressure - 900));
    servoSeek(pressRise, pressMove, posPress);
    oldPress = pressure;
  }

  Serial.println("Weather set.");
}

//This function is a twist on Blink Without Delay which will move the servos slowly to their position, instead of rushing there.
void servoSeek(Servo servo, int writeAngle, int pos) {
  do {
      if (currentMillis - lastServoMove >= servoInterval) {
        pos ++;
        servo.write(pos);
    }
  } while (pos <= writeAngle);
}

//This is the interrupt function that will keep track of how many minute markers have passed by the clock hand, and reset the number to 0 every hour.
/*void minCountFxn() {
 minCount ++;
 if (minCount == 60) {
  minCount = 0;
  }
}*/
 

 //This is the interrupt function that will change the location for weather to the user's selection.
void setLoc() {
  MPR121.updateTouchData();
  for(int i=0; i<numElectrodes; i++){
    if(MPR121.isNewTouch(i)){
      locCode = i;
      weatherTrigger = true;
    }
  }
}
