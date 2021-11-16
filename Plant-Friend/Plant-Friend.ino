/*
    Get date and time - uses the ezTime library at https://github.com/ropg/ezTime -
    and then show data from a DHT22 on a web page served by the Huzzah and
    push data to an MQTT server - uses library from https://pubsubclient.knolleary.net

    Zixu Cheng (Cade)
    CASA0014 - Plant Friend
    Nov 2021
*/

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ezTime.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <DHT_U.h>
#include <ArduinoQueue.h>

#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321

// Sensors - DHT22 and Nails
uint8_t DHTPin = 12;        // on Pin 2 of the Huzzah
uint8_t soilPin = 0;      // ADC or A0 pin on Huzzah
float Temperature;        // Declare variable Temperature
float Humidity;           // Declare variable Humidity
int Moisture = 1; // initial value just in case web page is loaded before readMoisture called
int sensorVCC = 13;       // set sensorVCC on pin 13
int blueLED = 2;          // set LED on pin 2

// new variable for Plant Friend
int needSunlight = 1;     // initial value of needSunlight
int needWatering = 1;     // initial value of needWatering
int score = 100;          // initia value of score of plant status
float avg_score = 0;        // initia value of average score
ArduinoQueue<int> q_score(60);  //declare a queue to preserve the lateset data like a slidewindow
ArduinoQueue<int> q_score_temp(60);   //declare a temporary queue to help calculate the average score

DHT dht(DHTPin, DHTTYPE);   // Initialize DHT sensor.


// Wifi and MQTT
#include "arduino_secrets.h" 
/*
**** please enter your sensitive data in the Secret tab/arduino_secrets.h
**** using format below

#define SECRET_SSID "ssid name"
#define SECRET_PASS "ssid password"
#define SECRET_MQTTUSER "user name - eg student"
#define SECRET_MQTTPASS "password";
 */

//const char* ssid     = SECRET_SSID;
//const char* password = SECRET_PASS;
//const char* mqttuser = SECRET_MQTTUSER;
//const char* mqttpass = SECRET_MQTTPASS;

// initialise and web and mqtt server configuration 

ESP8266WebServer server(80);
const char* mqtt_server = "mqtt.cetools.org";
WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;

// Date and time
Timezone GB;


void setup() {
  // Set up LED to be controllable via broker
  // Initialize the BUILTIN_LED pin as an output
  // Turn the LED off by making the voltage HIGH
  pinMode(BUILTIN_LED, OUTPUT);     
  digitalWrite(BUILTIN_LED, HIGH);  

  // Set up the outputs to control the soil sensor
  // switch and the blue LED for status indicator
  pinMode(sensorVCC, OUTPUT); 
  digitalWrite(sensorVCC, LOW);
  pinMode(blueLED, OUTPUT); 
  digitalWrite(blueLED, HIGH);

  // open serial connection for debug info
  Serial.begin(115200);
  delay(100);

  // start DHT sensor
  pinMode(DHTPin, INPUT);
  dht.begin();

  // run initialisation functions
  startWifi();
  startWebserver();
  syncDate();

  // start MQTT server
  client.setServer(mqtt_server, 1884);
  client.setCallback(callback);

}

void loop() {
  // handler for receiving requests to webserver
  server.handleClient();

  if (minuteChanged()) {
    readMoisture();
    sendMQTT();
    Serial.println(GB.dateTime("H:i:s")); // UTC.dateTime("l, d-M-y H:i:s.v T")
  }
  
  client.loop();
}

void readMoisture(){
  
  // power the sensor
  digitalWrite(sensorVCC, HIGH);
  digitalWrite(blueLED, LOW);
  delay(100);
  // read the value from the sensor:
  Moisture = analogRead(soilPin);         
  //Moisture = map(analogRead(soilPin), 0,320, 0, 100);    // note: if mapping work out max value by dipping in water     
  //stop power 
  digitalWrite(sensorVCC, LOW);  
  digitalWrite(blueLED, HIGH);
  delay(100);
  Serial.print("Wet ");
  Serial.println(Moisture);   // read the value from the nails
}

void startWifi() {
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  // check to see if connected and wait until you are
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void syncDate() {
  // get real date and time
  waitForSync();
  Serial.println("UTC: " + UTC.dateTime());
  GB.setLocation("Europe/London");
  Serial.println("London time: " + GB.dateTime());

}
void startWebserver() {
  // when connected and IP address obtained start HTTP server
  server.on("/", handle_OnConnect);
  server.onNotFound(handle_NotFound);
  server.begin();
  Serial.println("HTTP server started");
}

void sendMQTT() {

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  Temperature = dht.readTemperature(); // Gets the values of the temperature
  snprintf (msg, 50, "%.1f", Temperature);
  Serial.print("Publish message for t: ");
  Serial.println(msg);
  client.publish("student/CASA0014/plant/Plant-Friend-Cade/temperature", msg);

  Humidity = dht.readHumidity(); // Gets the values of the humidity
  snprintf (msg, 50, "%.1f", Humidity);
  Serial.print("Publish message for h: ");
  Serial.println(msg);
  client.publish("student/CASA0014/plant/Plant-Friend-Cade/humidity", msg);

  //Moisture = analogRead(soilPin);   // moisture read by readMoisture function
  snprintf (msg, 50, "%.0i", Moisture);
  Serial.print("Publish message for m: ");
  Serial.println(msg);
  client.publish("student/CASA0014/plant/Plant-Friend-Cade/moisture", msg);

  // To indicate whether needing more sunlight
  needSunlight = (Temperature < 25) ? 1 : -1;

  snprintf (msg, 50, "%.0i", needSunlight);
  Serial.print("Publish message for needSunlight: ");
  Serial.println(msg);
  Serial.print("The plant need more Sunlight!");
  client.publish("student/CASA0014/plant/Plant-Friend-Cade/needSunlight", msg);

  // To indicate whether needing watering
  needWatering = (Moisture < 100) ? 1 : -1;
  
  snprintf (msg, 50, "%.0i", needWatering);
  Serial.print("Publish message for needWatering: ");
  Serial.println(msg);
  Serial.print("The plant need to be watering!");
  client.publish("student/CASA0014/plant/Plant-Friend-Cade/needWatering", msg);

  if (needSunlight == 1 and needWatering == 1){
    score = 25;
  } else if (needWatering == 1) {
    score = 50;
  } else if (needSunlight == 1) {
    score = 75;
  } else {
    score = 100;
  }
  snprintf (msg, 50, "%.0i", score);
  Serial.print("Publish message for plant score: ");
  Serial.println(msg);
  client.publish("student/CASA0014/plant/Plant-Friend-Cade/plant_score", msg);

  // preserve the latest data
  if (!q_score.isFull()) {
    q_score.enqueue(score);
  } else {
    q_score.dequeue();
    q_score.enqueue(score);
  }
  
  // update the queue status 
  q_score_temp = q_score;
  int temp = 0;
  int len = q_score_temp.itemCount();

  // calculate the average score via the help of q_score_temp
  while (!q_score_temp.isEmpty()){
    temp += q_score_temp.getHead();
    q_score_temp.dequeue();
  }
  
  avg_score = temp / len;
  snprintf (msg, 50, "%.2f", avgscore);
  Serial.print("Publish message for avg score in an hour: ");
  Serial.println(msg);
  client.publish("student/CASA0014/plant/Plant-Friend-Cade/plant_avg_score_hour", msg); 
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Switch on the LED if an 1 was received as first character
  if ((char)payload[0] == '1') {
    digitalWrite(BUILTIN_LED, LOW);   // Turn the LED on (Note that LOW is the voltage level
    // but actually the LED is on; this is because it is active low on the ESP-01)
  } else {
    digitalWrite(BUILTIN_LED, HIGH);  // Turn the LED off by making the voltage HIGH
  }

}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    
    // Attempt to connect with clientID, username and password
    if (client.connect(clientId.c_str(), mqttuser, mqttpass)) {
      Serial.println("connected");
      // ... and resubscribe
      client.subscribe("student/CASA0014/plant/Plant-Friend-Cade/inTopic");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void handle_OnConnect() {
  Temperature = dht.readTemperature(); // Gets the values of the temperature
  Humidity = dht.readHumidity(); // Gets the values of the humidity
  server.send(200, "text/html", SendHTML(Temperature, Humidity, Moisture));
}

void handle_NotFound() {
  server.send(404, "text/plain", "Not found");
}

String SendHTML(float Temperaturestat, float Humiditystat, int Moisturestat) {
  String ptr = "<!DOCTYPE html> <html>\n";
  ptr += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  ptr += "<title>ESP8266 DHT22 Report</title>\n";
  ptr += "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
  ptr += "body{margin-top: 50px;} h1 {color: #444444;margin: 50px auto 30px;}\n";
  ptr += "p {font-size: 24px;color: #444444;margin-bottom: 10px;}\n";
  ptr += "</style>\n";
  ptr += "</head>\n";
  ptr += "<body>\n";
  ptr += "<div id=\"webpage\">\n";
  ptr += "<h1>ESP8266 Huzzah DHT22 Report</h1>\n";

  ptr += "<p>Temperature: ";
  ptr += (int)Temperaturestat;
  ptr += " C</p>";
  ptr += "<p>Humidity: ";
  ptr += (int)Humiditystat;
  ptr += "%</p>";
  ptr += "<p>Moisture: ";
  ptr += Moisturestat;
  ptr += "</p>";
  ptr += "<p>Sampled on: ";
  ptr += GB.dateTime("l,");
  ptr += "<br>";
  ptr += GB.dateTime("d-M-y H:i:s T");
  ptr += "</p>";

  ptr += "</div>\n";
  ptr += "</body>\n";
  ptr += "</html>\n";
  return ptr;
}
