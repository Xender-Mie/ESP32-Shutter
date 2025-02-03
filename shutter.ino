//Include all the needed libraries:
#include <WiFi.h> //This library allow the ESP32 device to connect at a WiFi network.
#include <PubSubClient.h> //This library allow the device to sub and publish on MQTT topics.
#include <BH1750.h>
#include <Stepper.h>
#include <DHT.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>

//------------------- Used Constants :-------------------//
//The current disposition of the pins are made for the linked schema via this article: https://lastminuteengineers.com/esp32-pinout-reference/
//You can modify them to correspond to your need.

//Pins configuration:
#define SDA_PIN 21
#define SCL_PIN 22
#define DHT_PIN 32
#define DHT_TYPE DHT11
#define TRIG_PIN 25
#define ECHO_PIN 33
#define BUZZER_PIN 19
#define MOTOR_PIN1 27
#define MOTOR_PIN2 14
#define MOTOR_PIN3 13
#define MOTOR_PIN4 26
#define PIN_DHT11 5
#define DHTTYPE DHT11
DHT dht(PIN_DHT11, DHTTYPE);

//OLED screen configuration:
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

//Definie the step's number per rotation:
#define STEP_REVOLUTION 2048

//Sowtfare constants:
#define TimeStroke 10000 //Needed time by the shutter to open or clse himself.
#define periodAction 5000
#define LightThreshold 300 //Threshold for opening/closing the shutters.
#define periodMotor  10000

//-------------------------------------------------------//

//Initialization of the sensors and the actuators:
BH1750 LuxMeter;
Stepper motor(STEP_REVOLUTION, MOTOR_PIN1, MOTOR_PIN2, MOTOR_PIN3, MOTOR_PIN4);

//Globales variables:
long TimeCourse=0; //How much time of the stroke the shutter has done when he recieve an another instruction.
long LastTimeAction = 0;
long LastTimeMotor = 0;
bool ShutterOpen = false;

//WiFi  network you want to connect:
const char* ssid = "";//Enter the SSID of your wifi point.
const char* password = "";//Enter the associated passsword.

//This field content the IP adress or the host name of the MQTT server:
const char* mqtt_server = ""; //Fill with yours.

WiFiClient espClientSHUTTER; //Initialize the WiFi client library (WiFi connection).
PubSubClient clientSHUTTER(espClientSHUTTER); //Creat an object MQTT client (MQTT connection). 
//Now, our MQTT client is call "client". We must now creat a connection to a broker.
long lastMsg = 0;

//This procedure connect the card to WiFi network using the parametres in the previous variables:
void setup_wifi() {
  delay(10);
  //We start by connecting to a WiFi network:
  Serial.println(); //Print an empty line/break line in the serial monitor.
  Serial.print("Connecting to ");
  Serial.println(ssid); 

  WiFi.begin(ssid, password); //Start the Wi-Fi connexion with the given datas.

  //This loop wait 500 ms util the ESP32 connected to the Wi-Fi network. Le connection statut is given by "WiFi.status()".
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

//The procedure "reconnect()" is used to guarantee the MQTT connection between the ESP32 and the MQTT broker. 
//It is called in the main loop and repeat until the connection is stable.
void reconnect() {
  //Loop until we're reconnected:
  while (!clientSHUTTER.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
	  //If the connection is already etablished, the procedure do nothing. 
	  //If the connection is not etablished yet, the code try to connect with "client.connect("espClientSHUTTER")".
    //If the connection success, the code print "connected" on the serial monitor, subscribe to the topic "shutter/control" with "client.subscribe("shutter/control")".
    if (clientSHUTTER.connect("espClientSHUTTER")) {
      Serial.println("connected");
      //Subscribe:
      clientSHUTTER.subscribe("shutter/control");
    } 
	//If the connection failed, the code print "failed, rc=", follows the connection state with "client.state()" on the serial monitor.
  //The procedure loop until the connection is successfully etablished.
	else {
      Serial.print("failed, rc=");
      Serial.print(clientSHUTTER.state());
      Serial.println(" try again in 5 seconds");
      //We wait 5 seconds before retry:
      delay(5000);
    }
  }
}

//The procedure callback is used for treating the MQTT messages recieved by the ESP32 and associated then with fonctionalities.
//It is called when a new message is recieve on Topics the ESP32 is sub.
void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic); //Print Topic's name on which the message was published.
  Serial.print(". Message: ");
  
  //The received message is transmitted to the function as an array of "message" bytes with a length of "length". 
  //To work with this message in the rest of the code, we must first convert it into a string.
  String messageTemp; //Set a temporary string variable to store the received message.
  
  //Loop for each elements in the arry bytes "message" and add then to "messageTemp".
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
	//For each iteration, the currrent element from the bytes array "message" is converted in char with "(char)message[i]" and printed into the serial monitor "Serial.print((char)message[i])".
	//Then, this element is added a the end of the "messageTemp" string with "messageTemp += (char)message[i];
  }
  Serial.println();

//If a MQTT message (messageTemp variable) is recieve on the "shutter/control" topic, we verify the content.
  if (String(topic) == "shutter/control") {
    if (messageTemp == "down") {
      closeShutter(TimeCourse);
    } else if (messageTemp == "up") {
      openShutter(TimeCourse);
    }
  }

}

void setup() {
  pinMode(PIN_DHT11, INPUT);

  Serial.begin(9600);
  dht.begin();
  delay(1000);

  setup_wifi(); //This line call the setup_wifi() procedure to configur Wi-Fi connection on the ESP32.
  clientSHUTTER.setServer(mqtt_server, 1883); //Define the MQTT server (broker) used for the communication. The port is 1883, which is the standard port for MQTT.
  clientSHUTTER.setCallback(callback); //define the "callback" procedure as the procedure to call when a MQTT message is recieve.

  Serial.println("Sarting system...");

  //I2C initialization:
  Wire.begin(SDA_PIN, SCL_PIN);

  //BH1750 initialization:
  if (!LuxMeter.begin()) {
    Serial.println("Error: BH1750 not detected");
    while (true);
  }
  Serial.println("BH1750 successfully setup");

  //DHT initialization:
  dht.begin();
  Serial.println("DHT setup");

  //OLED screen initialization:
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("Error: fail to setup the screen");
    while (true);
  }
  Serial.println("OLED setup");

  //Motor initialization:
  motor.setSpeed(10);
  Serial.println("motor setup");

  //Pins initialization:
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  //print the setup message on the OLED screen:
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Systeme setup");
  display.display();
  delay(1000);
  display.clearDisplay();

  //Print the remaining free memory:
  Serial.print("Free memory: ");
  Serial.println(ESP.getFreeHeap());
}

float distanceShutter(){
  long duration, distance;
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);
    duration = pulseIn(ECHO_PIN, HIGH, 30000);
    if (duration == 0) {
      Serial.println("Error reading Ultrasound");
      printError("Error reading Ultrasound");
      distance = -1;
    } else {
      distance = (duration / 2) / 29.1;
    }
    return distance;
}

long openShutter(long begin) { //We get the time already passed by the shutter to know when to stop it.
  if (ShutterOpen==true) { //We verify that the shutter is not already open.
    printError("The shutter is already open."); //If it so we inform user that they can't open it more.
    Serial.println("The shutter is already open.");
    return 0; //Since the shutter is closed, the time needed to open it is shortened.
  }
  Serial.println("Opening the shutter...");
  long now=millis(); //We get the time when the loop begin.
  while (TimeCourse<TimeStroke) { //We verify that the shutter end his stroke with the time he take to open himself.
    motor.step(STEP_REVOLUTION / 2);
    TimeCourse=millis()-now; //To know how many time the motor turn we take the current time minus the loop begin one.
  }
  ShutterOpen = true;
  Serial.println("Shutter open.");
  beep(100); //One unic beep of 100ms signify that the shutter is open.
  return 0; //Once the shutter is open, the needed time to close it is the one define by TimeStroke.
}

long closeShutter(long begin) { //We get the time already passed by the shutter to know when to stop it.
  if (ShutterOpen==false) { //We verify that the shutter is not already close.
    printError("The shutter is already close."); //If it so, we inform user that they can't close it more.
    Serial.println("The shutter is already close.");
    return 0; //Since the shutter is closed, the time needed to open it is shortened.
  }
  Serial.println("Closing the shutter...");
  long now=millis(); //We get the time when the loop begin.
  while (TimeCourse<TimeStroke) { //We verify that the shutter end his stroke with the time he take to close himself.
    if (distanceShutter()<8) { //We continually verify that there is no obstacle on the stroke.
      printError("Obstacle detected.");
      return TimeStroke; //If it so, we return the tim that is already passed to know start it after.
    }
    motor.step(-STEP_REVOLUTION / 2);
    TimeCourse=millis()-now; //To know how many time the motor turn we take the current time minus the loop begin one.
  }
  ShutterOpen = false;
  Serial.println("Shutter close.");
  beep(100); //Two beep of 100ms signify that the shutter is open.
  beep(100);
  return 0; //Once the shutter is open, the needed time to close it is the one define by TimeStroke.
}

void beep(unsigned char delayms) { //Creating function
  analogWrite(BUZZER_PIN, 200); //Setting pin to high
  delay(delayms); //Delaying
  analogWrite(BUZZER_PIN, 0); //Setting pin to LOW
  delay(delayms); //Delaying
}

void printDatas(float lux, long distance, float temperature, float humidity) {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("Luminosity: ");
  display.println(lux);
  display.print("Distance: ");
  display.println(distance);
  display.print("Temp: ");
  display.print(temperature);
  display.println(" C");
  display.print("Humidity: ");
  display.print(humidity);
  display.println(" %");
  display.display();
}

void printError(String message) {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("Error: ");
  display.println(message);
  display.display();
}

void loop() {
  //The first task of the main loop is to verify if the MQTT client is connected. 
	//If is not, we call reconnect() to fix it:
  if (!clientSHUTTER.connected()) {
    reconnect();
  }
  
  clientSHUTTER.loop(); //The procedure client.loop() is called to treat the enterrings MQTT messages.
				                //Maintains connection with MQTT server by checking if new messages have arrived and sending the pending messages.

  //The last part verify the passed time since the last published message and send the following one only after 2 seconds.
  long now = millis(); //Creat a variable "now" to store the number of milliseconds passed since the program started.
  if (now - lastMsg > 2000) { //Verify if the passed time since the last published message is less tha 2000ms.
		lastMsg = now; //If it so, update the variable "lastMsg" with the current value of "now".
		
		//Main code:
    //BH1750 reading:
    float lux = LuxMeter.readLightLevel();
    if (lux < 0) {
      Serial.println("Error reading luminosity");
      printError("Error reading Luminosity");
      lux = 0.0; //Default value.
    }

    //Ultrasounn reading:
    long distance=distanceShutter();

    //DHT reading:
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();
    if (isnan(temperature) || isnan(humidity)) {
      Serial.println("Error reading DHT");
      printError("Erreur reading DHT");
      temperature = 0.0;
      humidity = 0.0;
    }

    //print the datas:
    printDatas(lux, distance, temperature, humidity);

    //Send the datas on the NodeRed:
    char tempString[8];
    dtostrf(humidity, 1, 2, tempString);
    clientSHUTTER.publish("shutter/humidity", tempString);
    dtostrf(temperature, 1, 2, tempString);
    clientSHUTTER.publish("shutter/temperature", tempString);
    dtostrf(lux, 1, 2, tempString);
    clientSHUTTER.publish("shutter/luminosity", tempString);

    //Control the shutter based on the luminosity et la temperature:
    if (millis() - LastTimeAction > periodAction) {
      if (lux > LightThreshold && distance > 10 && !ShutterOpen) {
        TimeCourse=openShutter(TimeCourse);
      } else if ( ((lux <= LightThreshold || temperature>=25) && distance > 10) && ShutterOpen) {
        TimeCourse=closeShutter(TimeCourse);
      } else if (distance<10) {
        beep(50);
        printError("Obstacle detected.");
        delay(120);
      }
      LastTimeAction = millis();
    }
    
  }
}