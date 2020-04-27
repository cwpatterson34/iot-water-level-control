// Author:          Chris Patterson B00660536
// Project:         EEE802 - MEng Final Year Project - IOT Water Level Control System
// Date Created:    16/03/2020
// Modified:        00/05/2020
// Description:     Code for IOT Wi-Fi Water Level Control System

  //                 Libraries

#include <SPI.h>                          // allows communication with SPI devices
#include <WiFi101.h>                      // allows use of the WiFi Shield 1010 and the MKR1000 WiFi capabilities
#include <SD.h>                           // allows use of the external SD card module

String deviceID = "SYS001";               // System ID name 'SYS001', to allow for multiple systems to be used at once

  //                Wifi Global Variables

// Laptop hotspot
// char ssid[] = "CP Dev Network";              // Laptop network SSID (name)
// char pass[] = "?d413S72";                    // Laptop network password (use for WPA)

// Home Wi-Fi
 char ssid[] = "BTHub6-782G";              // Home network SSID (name)
 char pass[] = "GN6F4FxKD4xd";             // Home network password (use for WPA)

// mobile phone hotspot
// char ssid[] = "AndroidAP";                // Mobile network SSID (name)
// char pass[] = "xjfj2047";                 // Mobile network password (use for WPA)

  //                Pushing Box API Variables

const char pbURL[] = "api.pushingbox.com";           // server to contact for the push notification 
const String pbDevIDStates  = "v0644D26C9FE0F40";    // Scenario dev ID for Pushing Box to send States Data to google sheets 
const String pbDevIDTimings = "vBED47EB950D4859";    // Scenario dev ID for Pushing Box to send Timings Data to google sheets 

int status = WL_IDLE_STATUS;
WiFiClient client;                                // WiFi client library is initialised

  //                Global Variables

// LED Pins
int WifiGreenLED = 1;                     // RGB Wi-Fi LED - Red/Green Wi-Fi status indicator
int WifiRedLED  = 2;

int RedLed = 3;                           // LED status indicators for UVC lamp, Water Pump and Water Level
int UVCLampLed = 5;
int WaterPumpLed = 11;

// Actuator Pins
int UVCLamp = A1;                         // UVC Lamp 
int WaterPump = A2;                       // Water Pump 


// Water Level Sensors and Man Override Switch   
int SourceTank = 14;                      // Switches to simulate Water Level Float Sensors and manOride 
int High2ndTank = 13;
int Low2ndTank = 12;
int manOride = 6;

// SD Card variables
const int chipSelect = 4;
String dataString = "";                   // Datastring's to write data to SD Card
String dataString2 = "";
unsigned long now;

  //                Timing Variables

unsigned long UVCLampTimeStart = 0;     // In milliseconds,Allows to calculate the time the UVC Lamp is active
unsigned long UVCLampTimeStop = 0;
unsigned long UVCLampTimeTotal = 0;

unsigned long WaterPumpTimeStart = 0;   // In milliseconds,Allows to calculate the time the Water Pump is active
unsigned long WaterPumpTimeStop = 0;
unsigned long WaterPumpTimeTotal = 0;

float setLampWarmUpTime = 1;            // In minutes, warm up time for the UV Lamp before the Pump is
                                        // turned on. Default is 1 minute.
                                        
float setIntervalTime = 1;              // In minutes, Interval time where the status of the system is
                                        // checked while the 2nd Tank is being filled up. Default is 1 minute.

float setDownTime = 0.01;               // In minutes,'Down' time where the system is on standby mode
                                        // after the 2nd Tank is filled. Default is 6 hours.

float setMaxTime = 1.5;                 // In minutes, Max filling time before the Pump and UV Lamp are
                                        // shut down to address situations where the 2nd Tank never fills up due to
                                        // leakage or other faults. Default is 1.5 hours.

unsigned long startMillis;  
unsigned long currentMillis;

long lampWarmUpTime = setLampWarmUpTime * 60000;          // Convert to ms
float intervalTime  = setIntervalTime * 60000;            // Convert to ms
long downTime       = setDownTime * 3600000;              // Convert to ms
long maxTime        = setMaxTime * 3600000;               // Convert to ms
int cycleMax        = setMaxTime * 60 / setIntervalTime;  // Set max cycles to maxTime/intervalTime
                                                       
// Setting all the default values for variables to 0
int readHigh2ndTank = 0;
int readLow2ndTank = 0;
int readSourceTank = 0;
int readWaterPump = 0;
int readUVCLamp = 0;
int readManOride = 0;
int OrideStat = 0;

/////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////      Set-Up
void setup() {
  Serial.begin(9600);                                   // initiate the serial monitor
  while(!Serial);

 //                LED Set-Up
  pinMode(UVCLampLed,OUTPUT);                              // initialising LED indicators as outputs
  pinMode(WaterPumpLed,OUTPUT);
  pinMode(RedLed,OUTPUT);
  pinMode(WifiGreenLED,OUTPUT);
  pinMode(WifiRedLED,OUTPUT);

  pinMode(UVCLamp,OUTPUT);                              // initialising Lamp & Pump pins as outputs
  pinMode(WaterPump,OUTPUT);

  //                Water Level simulation Set-Up
  pinMode(SourceTank,INPUT_PULLUP);               // initialising Water Level Sensors and manOride as inputs
  pinMode(High2ndTank,INPUT_PULLUP); 
  pinMode(Low2ndTank,INPUT_PULLUP);
  pinMode(manOride,INPUT_PULLUP);
  
  attachInterrupt( digitalPinToInterrupt( manOride ), manualOverride, CHANGE );   // Interrupt pin for manOride
  
 //                SD Card & Wi-Fi Set-Up
  Serial.print("Initializing SD card...");
  if (!SD.begin(chipSelect)) {                    // Initialising SD Card
    Serial.println("Card failed, or not present");
  }
  else{
    Serial.println("card initialized.");
  }
  while (status != WL_CONNECTED)                    // attempt to connect to WiFi network
  {
    Serial.print("\nWi-Fi Connection Initialising...");
    status = WiFi.begin(ssid, pass);                // System connects to the network with the defines SSID and password
    digitalWrite(WifiRedLED , HIGH);                // RGB LED will light up red while the WiFi is initialising
    digitalWrite(WifiGreenLED , LOW);
    delay(5000);          // 5 second delay between retries
  }
  Serial.print("\nWi-Fi Conection Initialisation Complete");
  Serial.print("\nNetwork SSID: ");           // SSID printed to serial monitor
  Serial.println(WiFi.SSID());
  Serial.print("Local IP Address: ");         // local network IP printed to the serial monitor
  IPAddress addressIP = WiFi.localIP();
  Serial.print(addressIP);
  digitalWrite(WifiRedLED , LOW);
  digitalWrite(WifiGreenLED , HIGH);          // RGB LED switched to green now that the system is connected to the WiFi
  now = millis();
  manualOverride();                           // Calling manual override function to read state of switch
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////      Main Loop

void loop() {


 if ( OrideStat == 0 )                   // if the reading of manOride is 0 
  {
    delay ( 1000 );

    readHigh2ndTank     = digitalRead( High2ndTank );    // read states of all sensors and actuators
    readLow2ndTank     = digitalRead( Low2ndTank );
    readSourceTank = digitalRead( SourceTank );
    readWaterPump   = digitalRead( WaterPump );
    readUVCLamp     = digitalRead( UVCLamp );

    Serial.println( " " );                // write to serial monitor
    Serial.println( " " );
    Serial.print( "Manual Override: " );               
    Serial.println( digitalRead( manOride ) );
    Serial.print( "Source Tank is: " );

    if ( readSourceTank == 0 ){        // if SourceTank is empty:
      Serial.println( "Empty" );       // print 'empty' to serial monitor
    }
    else if ( readSourceTank == 1 ){          // if SourceTank is full:
      Serial.println( "Filled" );             // print 'filled' to serial monitor
    }

    Serial.println( readLow2ndTank );
    Serial.println( readHigh2ndTank );
    Serial.print( "2nd Tank is: " );

    if ( ( readLow2ndTank == 0 ) && ( readHigh2ndTank == 0 ) ){    // if Secondary tank is empty:
      Serial.println( "Empty" );                                   // print 'empty' to serial monitor
    }
    
    else if ( ( readLow2ndTank == 1 ) && ( readHigh2ndTank == 1 ) ){    // if Secondary tank is full:
      Serial.println( "Filled" );                                       // print 'filled' to serial monitor
    }
    
    else if ( ( readLow2ndTank == 1 ) && ( readHigh2ndTank == 0 ) ){    // if Secondary tank is Half full:
      Serial.println( "Half-filled" );                                  // print 'half-filled' to serial monitor
    }

    else if ( ( readLow2ndTank == 0 ) && ( readHigh2ndTank == 1 ) ){    // If High sensor reads a 1 and low sensor reads a 0
      Serial.println( "Potential fault. Check sensors!" );              // Print potential fault in serial moniotr
    }

    Serial.print( "UV Lamp Status: " );
    Serial.println( readUVCLamp );             // Read UVC Lamp to print data to serial monitor

    Serial.print( "Pump Status: " );
    Serial.println( readWaterPump );           // Read Water Pump to print data to serial monitor

    if ( readHigh2ndTank == 0 ){
      Serial.println( "Case 1..." );
      Serial.println( "2nd Tank water level below MAX" );
      delay( 500 );

      if ( ( readSourceTank == 0 ) && ( readLow2ndTank == 0 ) && ( readWaterPump == 0 ) ){      // if both tanks are empty
        Serial.println( "Case 1a..." );
        Serial.println( "SOURCE TANK EMPTY!" );

        shutDown();          // Calls Shutdown function

        for ( int ii = 0; ii <= 3; ii++ ){    // for loop to notify SourceTank is empty, flash red Led when filling is in operation
          delay( 10 );
          digitalWrite( RedLed, HIGH );       // Blink red Led on and off
          delay( 10 );
          digitalWrite( RedLed, LOW );
        }
        return;
      }

      else if ( ( readWaterPump == 0 ) && ( readLow2ndTank == 0 ) && ( readSourceTank == 1 ) ){      // if Secondary tank is empty & SourceTank full
      
        tankRefill();          // Calls tank refill function

        
        return;
      }

      else if ( ( readWaterPump == 1 ) && ( readLow2ndTank == 0 ) && ( readSourceTank == 0 ) ){      // if both tanks are empty, call the shutdown function
        Serial.println( "Case 1c..." );
        Serial.println( "SOURCE TANK EMPTY!" );
        shutDown();
        return;
      }

      else if ( ( readWaterPump == 1 ) && ( readLow2ndTank == 0 ) && ( readSourceTank == 1 ) ){
        if ( digitalRead( High2ndTank ) == 1 ){      // if 2nd Tank is filled, call the shutdown function and start timer for timeout function
          Serial.println( "Case 1d(i)..." );
          Serial.println( "2nd Tank Filled." );
          shutDown();
          startMillis = millis();
          timeOut();
          return;
        }

        else if ( digitalRead( High2ndTank ) == 0 ){
          for ( int jj = 0; jj < cycleMax; jj++ ){           // for loop used to ensure the system doesnt reach its cycle max 
            delay( intervalTime );
            Serial.print( "Time elapsed: " );
            Serial.print( intervalTime / 60 / 1000 * (jj + 1) );
            Serial.println( " minutes" );
            
            if ( digitalRead( High2ndTank ) == 1  ){         // if 2nd Tank is filled, call the shutdown function
              Serial.println( "Case 1d(ii)..." );
              Serial.println( "2nd Tank Filled." );
              shutDown();
              startMillis = millis();
              timeOut();
              return;
            }

            else if ( digitalRead( SourceTank ) == 0 ){      // if SourceTank is empty, call the shutdown function
              Serial.println( "Case 1d(iii)..." );
              Serial.println( "NO WATER IN SOURCE TANK!" );
              shutDown();
              return;
            }

            else if ( jj == (cycleMax - 1) ){                // if cycle max has been reached, system needs to be
              Serial.println( "Case 1d(iv)..." );            // reset to return to normal operation
              Serial.println( "Time limit reached." );
              shutDown();
              Serial.println( "PLEASE RESET SYSTEM..." );
              digitalWrite( RedLed, HIGH );                  // Red Led will turn on for indication to reset the system
              while ( 1 );
            }
          }
        }
      }
    }
  }
  
  if (millis() >= now+1000){           // length inbetween each transmission in milliseconds
      now = millis();
      
      readStates();                    // Calls the readStates function to read the current state of the sensors and actuators
      WriteStatestoSD();               // Calls the writeStatestoSD function to write the states of sensors and actuators to the SD Card
      
      readTimings();                    // Calls the readTimings function to read the active timings of any actuator
      WriteTimingstoSD();               // Calls the writeTimingstoSD function to write the active timings of any actuator to the SD Card
      
      if (client.connect(pbURL, 80)) {    // If it can connect to the Pushing Box Url it will upload the States data to Google Sheets
        wifiStatesUpload();        
        
      }
      else {
        wifiReconnect();                    // If it cannot connect to the Pushing Box Url it will attempt to reconnect to the wifi       
        if (client.connect(pbURL, 80)) {    // If it can reconnect to the wifi, it will upload the States data to Google Sheets
          wifiStatesUpload();
                 
        }
      }

      if (client.connect(pbURL, 80)) {      // If it can connect to the Pushing Box Url it will upload the Timings data to Google Sheets
     
        wifiTimingsUpload();                // Calls wifi upload function
        //resetTimings();
      }
      else {
       // NVIC_SystemReset();                 // debugging - Power off the MKR1000, open serial monitor for reconnection
        wifiReconnect();                    // If it cannot connect to the Pushing Box Url it will attempt to reconnect to the wifi 
        if (client.connect(pbURL, 80)) {    // If it can reconnect to the wifi, it will upload the Timings data to Google Sheets
               
          wifiTimingsUpload();              // Calls wifi upload function
          //resetTimings();
      }
    } 
  }
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////      Functions to be called in the Main Loop

  //                Writing the States to the SD card
/* This function opens a data file within the SD Card by the name of 'States.txt', If the data file is
 *  open then the string of data will be uploaded to that text file, after the writing of the data, the
 *  text file will close, the same data is printed to the serial monitor */
void WriteStatestoSD() {
    File StatesData = SD.open("States.txt", FILE_WRITE);
    if (StatesData) {
      StatesData.println(dataString);      // Prints States data to the text file
      StatesData.close();                  // Closes text file
      // print to the serial port too:
      Serial.println(dataString);
    }
    // if the file isn't open, pop up an error:
    else {
      Serial.println("error opening States.txt");
    }
  }

  //                Writing the Timings to the SD card
  /* This function opens a data file within the SD Card by the name of 'Timings.txt', If the data file is
 *  open then the string of data will be uploaded to that text file, after the writing of the data, the
 *  text file will close, the same data is printed to the serial monitor */
void WriteTimingstoSD() {
    File TimingsData = SD.open("Timings.txt", FILE_WRITE);
    if (TimingsData) {
      TimingsData.println(dataString2);     // Prints Timings data to the text file
      TimingsData.close();                  // Closes text file
      // print to the serial port too:
      Serial.println(dataString2);
    }
    // if the file isn't open, pop up an error:
    else {
      Serial.println("error opening Timings.txt");
    }
  }

  //                Reconnection to the Wi-Fi
  /* This function is called when there has bee a loss of internet connection, this function then attempts
 *  to reconnect the the wifi network by using the defined ssid and password, there is a 5 second delay
 *  between reconnection attempts and it will loop until a successful connection is made. The Wifi RGB Led
 will light up red while the wifi is attempting to reconnect and turn green when a connection has been made*/
void wifiReconnect() {
  Serial.print("\nWi-Fi Connection Error");
  //status = WL_IDLE_STATUS;  
           
  while (status != WL_CONNECTED) {
    Serial.println("\nWi-Fi Connection Initialising...");
    // Serial.println(WiFi.status());
    status = WiFi.begin(ssid, pass);        // System connects to the network with the defined SSID and password
    digitalWrite(WifiRedLED , HIGH);        // RGB LED will light up red while the WiFi is initialising
    digitalWrite(WifiGreenLED , LOW);
    delay(5000);          // 5 second delay between retries
  }
  Serial.print("\nSuccessfully Reconnected to Wi-Fi\n");
  digitalWrite(WifiRedLED , LOW);
  digitalWrite(WifiGreenLED , HIGH);
}

  //                Shutdown feature to stop active actuators
/* This function when called is use to stop and shutdown the water pump and UVC Lamp */
void shutDown()
{
  Serial.println( "Stop Lamp and Pump." );
  digitalWrite( WaterPump, LOW );           // Turns off Pump and Pump Led indicator
  digitalWrite( WaterPumpLed, LOW );
  
  digitalWrite( UVCLamp, LOW );           // Turns off Lamp and Lamp Led indicator
  digitalWrite( UVCLampLed, LOW ); 
}

  //                Standby mode timeout
 /* Function to allow for a timeout using the millis concept for timing after the 
 Secondary Tank has been refilled*/ 
void timeOut()
{
  Serial.println( "System on standby mode after 2nd Tank is filled..." );
  while(1)
  {
    currentMillis = millis();
    Serial.print( currentMillis/1000 );         // convert current time to seconds
    Serial.println( "s" );
    OrideStat = digitalRead( manOride );
    if ( OrideStat == 1)
    {
      return;
    }
    delay( 1000 );
    if ( currentMillis - startMillis >= downTime )
    {
      return;
    }
  }
}

  //                Function to read the manOride switch
void manualOverride(){
  OrideStat = digitalRead(manOride);
}

  //                Read states into a datastring
/* This function is called to read the states of all sensors and actuators
 and add them into a datastring before uploading the datastring to the SD Card */
void readStates(){
  dataString = "";
      
      dataString += String(digitalRead(SourceTank));    // Stores a state value in the string to be sent to the SD Card
      dataString += ",";
      dataString += String(digitalRead(High2ndTank));
      dataString += ",";
      dataString += String(digitalRead(Low2ndTank));
      dataString += ",";
      dataString += String(digitalRead(UVCLamp));
      dataString += ",";
      dataString += String(digitalRead(WaterPump));
      dataString += ",";
      dataString += String(digitalRead(manOride));
 
}

  //                Read timings into a datastring
/* This function is called to calculate any timings of any active actuators and
add the data into a datastring before uploading the datastring to the SD Card */
void readTimings(){
  dataString2 = "";
  UVCLampTimeTotal = UVCLampTimeStop-UVCLampTimeStart;      // Sum to calculate total time then adding the result to the datastring
  dataString2 += String(UVCLampTimeTotal);
  dataString2 += ",";
  WaterPumpTimeTotal = WaterPumpTimeStop-WaterPumpTimeStart;     // Sum to calculate total time then adding the result to the datastring
  dataString2 += String(WaterPumpTimeTotal);
}

  //                Refill the Secondary Tank
/* This function is called upon when the Secondard Tank is empty and requires refilling, this includes
initialising the UVC Lamp and Water Pump used to pump water from the SourceTank into the Secondary Tank.*/
void tankRefill(){
  ////////////////////////////////////////// initialise refill
        Serial.println( "Case 1b..." );
        Serial.println( "Start UV Lamp." );
        digitalWrite( UVCLamp, HIGH );               // UVC Lamp and Led status indicator on
        digitalWrite( UVCLampLed, HIGH );
        UVCLampTimeStart = millis();                 // Start timing on UVC Lamp
        Serial.println( "Warming Up UV Lamp." );
        delay( lampWarmUpTime );
        Serial.println( "Start Pump." );
        digitalWrite( WaterPump, HIGH );               // Water Pump and Led status indicator on
        digitalWrite( WaterPumpLed, HIGH );
        WaterPumpTimeStart = millis();                 // Start timing on water pump
        
  ////////////////////////////////////////// tank refill
      if ( digitalRead( High2ndTank ) == 1 ){          // if Secondary Tank is filled
          Serial.println( "Case 1d(i)..." );
          Serial.println( "2nd Tank Filled." );
          shutDown();                                  // Call shutdown function to stop UVC Lamp and Water Pump
          UVCLampTimeStop = millis();                  // assign value as millis to time Lamp and Pump Stopped
          WaterPumpTimeStop = millis();
          startMillis = millis();
          timeOut();                                   // Call timeout function after Secondary Tank is refilled
          return;
        }
        else if ( digitalRead( High2ndTank ) == 0 ){        // if Secondary Tank is not full
          unsigned long timer = millis();                   
          while ( millis() <= (WaterPumpTimeStart + maxTime)){   // while loop runs while the timer hasnt timed out
                                                                // only breaks while loop if one of the sensors is triggered
            
            if (millis() >= timer + 10000){                 // prints time every 10 seconds
              Serial.print( "Time elapsed: " );
              Serial.print( (millis()-WaterPumpTimeStart) / 1000);
              Serial.println( " seconds" );
              timer = millis();
            }
            
            if ( digitalRead( High2ndTank ) == 1  ){       
              Serial.println( "Case 1d(ii)..." );
              Serial.println( "2nd Tank Filled." );
              shutDown();                                  // Call shutdown function to stop UVC Lamp and Water Pump
              UVCLampTimeStop = millis();                  // assign value as millis to time Lamp and Pump Stopped
              WaterPumpTimeStop = millis();
              startMillis = millis();
              timeOut();
              
              return;
            }

            else if ( digitalRead( SourceTank ) == 0 ){
              Serial.println( "Case 1d(iii)..." );
              Serial.println( "NO WATER IN SOURCE TANK!" );
              shutDown();
              UVCLampTimeStop = millis();                  // assign value as millis to time Lamp and Pump Stopped
              WaterPumpTimeStop = millis();
              return;
            }
          }
          Serial.println( "Case 1d(iv)..." );
          Serial.println( "Time limit reached." );
          shutDown();
          UVCLampTimeStop = millis();                  // assign value as millis to time Lamp and Pump Stopped
          WaterPumpTimeStop = millis();
          Serial.println( "PLEASE RESET SYSTEM..." );
          digitalWrite( RedLed, HIGH );
          while ( 1 );
        }
}

  //                Wifi upload of states to Google Sheets
/* This function uploads the states of the sensors and actuators to Google Sheets via the use of the Pushing
 *  Box API, using the client.print function, the data is sent to the 'States' sheet on the Google Sheets doc */
void wifiStatesUpload(){
  Serial.print("\n\nStates of Water Level Sensors & Actuators...");
        client.print("GET /pushingbox?devid=" + pbDevIDStates          // Sends data to pushingbox via use of the devid
                      + "&SourceTank="    + (String)digitalRead(SourceTank)
                      + "&Highin2ndTank="    + (String)digitalRead(High2ndTank)
                      + "&Lowin2ndTank="    + (String)digitalRead(Low2ndTank)
                      + "&UVCLamp="    + (String)digitalRead(UVCLamp)
                      + "&WaterPump="    + (String)digitalRead(WaterPump)
                      + "&manOride="    + (String)digitalRead(manOride));
                    
        client.println(" HTTP/1.1");
        client.print("Host: ");
        client.println(pbURL);
        client.println("User-Agent: MKR1000/1.0");
        //for MKR1000, unlike esp8266, do not close connection
        client.println();
        Serial.println("\nData Upload Complete");                // Prints all states of sensors and actuators to serial monitor
        
          Serial.print("States of Water Level Sensors & Actuators:\t" + (String)digitalRead(SourceTank)
                                                                      + (String)digitalRead(High2ndTank)
                                                                      + (String)digitalRead(Low2ndTank)
                                                                      + (String)digitalRead(UVCLamp)
                                                                      + (String)digitalRead(WaterPump)
                                                                      + (String)digitalRead(manOride));
}

  //                Wifi upload of Timings to Google Sheets
/* This function uploads the timings of any active actuators to Google Sheets via the use of the Pushing
 *  Box API, using the client.print function, the data is sent to the 'Timings' sheet on the Google Sheets doc*/
void wifiTimingsUpload(){
  Serial.print("\n\nActive Timing of Actuators...");
        client.print("GET /pushingbox?devid=" + pbDevIDTimings          // Sends data to pushingbox via use of the devid
                      + "&UVCLamp="       + (String)UVCLampTimeTotal
                      + "&WaterPump="     + (String)WaterPumpTimeTotal);
                      
        client.println(" HTTP/1.1");
        client.print("Host: ");
        client.println(pbURL);
        client.println("User-Agent: MKR1000/1.0");
        //for MKR1000, unlike esp8266, do not close connection
        client.println();
        Serial.println("\nData Upload Complete");       // Prints all active timings of actuators to serial monitor
        
          Serial.print("Active Timing of Actuators:\t" + (String)UVCLampTimeTotal
                                                       + (String)WaterPumpTimeTotal);
}

  //                Reset the UVC Lamp and Water pump Timing calculation variables to 0
void resetTimings(){
  UVCLampTimeTotal = 0;
  UVCLampTimeStart = 0;
  UVCLampTimeStop = 0;
  WaterPumpTimeTotal = 0;
  WaterPumpTimeStart = 0;
  WaterPumpTimeStop = 0;
  
}
