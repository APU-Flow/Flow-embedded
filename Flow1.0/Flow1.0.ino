#include <ESP8266WiFi.h>

#define AP_SSID "APU-MYDEVICES"

#define SERVER "data.sparkfun.com"
#define PORT 80
#define PUBLIC_KEY "QGrYjaqxVXS0QVJwXKRp"
#define PRIVATE_KEY "JqrW0jkxpAUwekon4lR5"



//Flow Meter Setup
byte statusLed    = D3;

byte sensorInterrupt = D6;  // 0 = digital pin 2
byte sensorPin       = D6;

// The hall-effect flow sensor outputs approximately 4.5 pulses per second per
// litre/minute of flow.
float calibrationFactor = 4.5;

volatile byte pulseCount;  

float flowRate;
unsigned int flowMilliLitres;
unsigned long totalMilliLitres;

unsigned long oldTime;



//Flow amount variables

unsigned long t0;
unsigned long t1;
unsigned long oldt1 = 0;
bool flowing = 0;
const int timeout = 2*60*1000; //timeout in order to see grouping for one single usage event
unsigned long duration;

void setup() {

  //WiFI Settings
  Serial.begin(38400);
  wifiConnect();



  //Flow Meter Settings
  // Initialize a serial connection for reporting values to the host
  Serial.begin(38400);
   
  // Set up the status LED line as an output
  pinMode(statusLed, OUTPUT);
  digitalWrite(statusLed, HIGH);  // We have an active-low LED attached
  
  pinMode(sensorPin, INPUT);
  digitalWrite(sensorPin, HIGH);

  pulseCount        = 0;
  flowRate          = 0.0;
  flowMilliLitres   = 0;
  totalMilliLitres  = 0;
  oldTime           = 0;

  // The Hall-effect sensor is connected to pin 2 which uses interrupt 0.
  // Configured to trigger on a FALLING state change (transition from HIGH
  // state to LOW state)
  attachInterrupt(sensorInterrupt, pulseCounter, FALLING);
}

void loop() 
{
  // put your main code here, to run repeatedly:
   if((millis() - oldTime) > 1000)    // Only process counters once per second
  { 
    // Disable the interrupt while calculating flow rate and sending the value to
    // the host
    detachInterrupt(sensorInterrupt);
        
    // Because this loop may not complete in exactly 1 second intervals we calculate
    // the number of milliseconds that have passed since the last execution and use
    // that to scale the output. We also apply the calibrationFactor to scale the output
    // based on the number of pulses per second per units of measure (litres/minute in
    // this case) coming from the sensor.
    flowRate = ((1000.0 / (millis() - oldTime)) * pulseCount) / calibrationFactor;
    
    // Note the time this processing pass was executed. Note that because we've
    // disabled interrupts the millis() function won't actually be incrementing right
    // at this point, but it will still return the value it was set to just before
    // interrupts went away.
    oldTime = millis();
    
    // Divide the flow rate in litres/minute by 60 to determine how many litres have
    // passed through the sensor in this 1 second interval, then multiply by 1000 to
    // convert to millilitres.
    flowMilliLitres = (flowRate / 60) * 1000;
    
    // Add the millilitres passed in this second to the cumulative total
    totalMilliLitres += flowMilliLitres;
      
    unsigned int frac;
    
    // Print the flow rate for this second in litres / minute
    Serial.print("Flow rate: ");
    Serial.print(int(flowRate));  // Print the integer part of the variable
    Serial.print(".");             // Print the decimal point
    // Determine the fractional part. The 10 multiplier gives us 1 decimal place.
    frac = (flowRate - int(flowRate)) * 10;
    Serial.print(frac, DEC) ;      // Print the fractional part of the variable
    Serial.print("L/min");
    // Print the number of litres flowed in this second
    Serial.print("  Current Liquid Flowing: ");             // Output separator
    Serial.print(flowMilliLitres);
    Serial.print("mL/Sec");

    // Print the cumulative total of litres flowed since starting
    Serial.print("  Output Liquid Quantity: ");             // Output separator
    Serial.print(totalMilliLitres);
    Serial.println("mL"); 

    // Reset the pulse counter so we can start incrementing again
    pulseCount = 0;
    
    // Enable the interrupt again now that we've finished sending output
    attachInterrupt(sensorInterrupt, pulseCounter, FALLING);
  }

  //New reporting math
  if(flowMilliLitres != 0) //was just if(flowMilliLitres)
  {
    t1 = millis();
  }

  //flow is stopped
  if(flowMilliLitres == 0 && millis() > t1 + 2*60*1000 && totalMilliLitres != 0) //2 minutes - 2 minutes = timeout, set as variable later
  {
    duration = t1-t0;
    if(duration > timeout)
    {
      duration =- timeout;
    }
    else
    {
      duration = 1;
    }
    sendData(t0, totalMilliLitres, duration);
    totalMilliLitres = 0;
    flowing = 0;
    Serial.println("Reporting..");
    oldt1 = t1;
  }

  //flow has started
  if(flowing == 0 && totalMilliLitres > 0)
  {
    t0 = millis();
    flowing = 1;
  }
}

void pulseCounter()
{
  // Increment the pulse counter
  pulseCount++;
}

void wifiConnect()
{
  Serial.println("");
  Serial.println("");
  WiFi.begin(AP_SSID);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("Wifi connected");
  
}

void sendData(unsigned long startTime, unsigned long totalMilliLitres, unsigned long duration)
{
  WiFiClient client;
   
   while(!client.connect(SERVER, PORT)) {
    Serial.println("connection failed");
    wifiConnect(); 
  }

  
  IPAddress server(138,68,56,236);
  String postData = "{";
  postData.concat("\"email\":\"kbeard13@apu.edu\"");
  postData.concat(",\"meterId\":1");
  postData.concat(",\"startTime\":");
  postData.concat(startTime);
  postData.concat(",\"duration\":");
  postData.concat(duration);
  postData.concat(",\"totalVolume\":");
  postData.concat(totalMilliLitres);
  postData.concat("}");
  Serial.println(postData);

  if (client.connect(server, 3000)) {
    client.println("POST /usageEvent HTTP/1.1");
    client.println("Host: 138.68.56.236");
    client.println("User-Agent: Arduino/1.0");
    client.println("Connection: close");
    client.println("Content-Type: application/json");
    client.print("Content-Length: ");
    client.println(postData.length());
    client.println();
    client.println(postData);
  }
/*
  String url = "";
  url += "GET /input/";
  url += PUBLIC_KEY;
  url += "?private_key=";
  url += PRIVATE_KEY;
 
  url += "&amount=";
  url += String(totalMilliLitres);

  url += "&duration=";
  url += String(duration);
  

  url += " HTTP/1.1";
    
  client.println(url);
  client.print("Host: ");
  client.println(SERVER);
  client.println("Connection: close");
  client.println();

  Serial.println(url);
  */
  delay(100);
  while(client.available())
  {
    String line = client.readStringUntil('\r');
    Serial.print(line);
  }
  
  Serial.println();
  Serial.println("Connection closed");
}

