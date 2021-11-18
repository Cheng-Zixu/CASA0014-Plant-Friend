# Plant Friend

Plant Friend is a plant monitor project made by Zixu Cheng (Cade) in CASA0014. It appears like a cartoon friend monitoring your plant 24/7, scoring its status, and updating the status information in real-time.

**Advantage of Plant Friend:** 

The scoring of plants can better help you monitor the status of plants!

![./imgs/Plant-Friend.jpg]()

## 0. Overview

Plant Friend is made of various IoT sensors and components in CE IoT kits including Arduino board, Temperature and humidity sensor and Triode. It monitors the plant, sends data to the MQTT server [3], and stores data in the database in Raspberry Pi.

### Data

The data provided by Plant Friend includes, all of them are **real-time**:

* Temperature ---- The temperature of the room where your plant lives.
* Humidity ---- The humidity of the room where your plant lives.
* Moisture ---- The moisture of the soil where your plant grows.
* needSunlight ---- Indicating that your plant needs more sunlight (Yes: 1/ No: -1).
* needWatering ---- Indicating that your plant needs watering (Yes: 1/ No: -1).
* Plant_score ---- Evaluate your plant status score now.
* Plant_avg_score_hour ---- Evaluate your plant status score in the past an hour (the average score of your plant in the past an hour).

All these data could be subscribe on MQTT Server `mqtt.cetools.org` with port number `1883` and the subscription address is `student/CASA0014/plant/Plant-Friend-Cade/#`.

### Scorecard

When Temperature < 25,  needSunlight = 1 else -1;

When Moisture < 100,  needWatering = 1 else -1;

The evaluation criteria of the "Plant_score" is based on the following scorecard: 

| **needSunlight** | **needWatering** | **Score** |
| :--------------: | :--------------: | :-------: |
|        1         |        1         |    25     |
|        -1        |        1         |    50     |
|        1         |        -1        |    75     |
|        -1        |        -1        |    100    |

The "Plant_avg_score_hour" uses the latest data in the last hour and calculates the average score when updated data.

## 1. requirements

### 1.1 Sensors and components

To build a Plant Friend, we need the sensors and components as follows:

* 1 Arduino Feather Huzzah ESP8266 WIFI [5];
* 1 Temperature and humidity sensor DHT22 [6];
* 1 Triode BC547 [7];
* 2 100Ω/1 200Ω/2 10kΩ Resistors;
* 2 nails;

### 1.2 Servers and Softwares

To make Plant Friend work well, we need a MQTT server to broadcast the data of the plant monitor and a server like a Raspberry Pi to install the InfluxDB database to receive and preserve the data.

#### Setup a server

1. login your server via SSH or other ways.

   * If you don't have a server, you can setup a Raspberry Pi [8] as your server .

     some useful tutorial for setting up a Raspberry Pi:

     * [Raspberry Pi website](https://www.raspberrypi.org/downloads/) ;
     * [Official tutorial](https://projects.raspberrypi.org/en/projects/raspberry-pi-setting-up) ;
     * [This online tutorial is easier for beginers](https://www.tomshardware.com/uk/reviews/raspberry-pi-headless-setup-how-to,6028.html) ;
     * [CE Workshop-02 tutorial](https://workshops.cetools.org/codelabs/CASA0014-2-Plant-Monitor/index.html?index=..%2F..casa0014#9)

   * If you already have a server, just login your server.

2. install InfluxDB [9] on the server.

   Start your Terminal and input these commands to install and configure InfluxDB:

​		(1) Add the InfluxDB key

```
wget -qO- https://repos.influxdata.com/influxdb.key | sudo apt-key add -
```

​		(2) Add the repository to the sources list

```
echo "deb https://repos.influxdata.com/debian buster stable" | sudo tee /etc/apt/sources.list.d/influxdb.list
```

​		(3) Update the package list

```
sudo apt update
```

​		(4) Install InfluxDB

```
sudo apt install influxdb
```

#### MQTT configuration

To connect to a MQTT server, we need to configure some parameters for Wi-Fi and MQTT connections  in an Arduino file.

##### Add information directly in Arduino file (Not recommeded):

Add the codes below in your Plant Friend Arduino file. That's not recommended because your secret information could be exposed.

```
const char* ssid     = SECRET_SSID; // wifi connection id
const char* password = SECRET_PASS; // wifi password
const char* mqttuser = SECRET_MQTTUSER;	// mqtt user
const char* mqttpass = SECRET_MQTTPASS;	// mqtt password

const char* mqtt_server = "enter mqtt url";
```

##### Using a secret file (Recommended):

Create a new head file named "arduino_secrets.h" and add your secret information in it using the following format:

```
#define SECRET_SSID "ssid name"
#define SECRET_PASS "ssid password"
#define SECRET_MQTTUSER "user name - eg student"
#define SECRET_MQTTPASS "password";
```

Import the secret head file in your main Arduino file like:

```
#include "arduino_secrets.h" 
```

## 2. How it works

### 2.1 Circuit

The circuit of Plant Friend is like the following configuration[1]:

<img src=".\imgs\circuit.png" style="zoom: 25%;" />

#### Monitor the Temperature and Humidity

Plant Friend uses a DHT22 sensor to monitor the temperature and humidity of the room where your plant lives.

#### Monitor the soil moisture

Plant Friend uses a BC547 triode and two nails to quantify soil moisture through the current values between the two nails.

### 2.2 Programme

#### 2.2.1 Declare variables [2]

```
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
```

#### 2.2.2 Function

##### setup

To setup the arduino board, start DHT22 sensors, run initialise functions and start MQTT servers.

##### loop

To make Plant Friend work periodically to collect, analyse and broadcast data.

##### readMoisture

To fetch the soil moisture via current values.

##### startWifi

To start Wifi connection for MQTT and webserver service.

##### syncDate

To get real date and time.

##### startWebserver

To start HTTP server when connected to Wi-Fi and IP address obtained

##### sendMQTT (The highlight of Plant Friend)

To indicate whether needing more sunlight:

```
// To indicate whether needing more sunlight
needSunlight = (Temperature < 25) ? 1 : -1;

snprintf (msg, 50, "%.0i", needSunlight);
Serial.print("Publish message for needSunlight: ");
Serial.println(msg);
client.publish("student/CASA0014/plant/Plant-Friend-Cade/needSunlight", msg);
```

  To indicate whether needing watering:

```
// To indicate whether needing watering
needWatering = (Moisture < 100) ? 1 : -1;

snprintf (msg, 50, "%.0i", needWatering);
Serial.print("Publish message for needWatering: ");
Serial.println(msg);
client.publish("student/CASA0014/plant/Plant-Friend-Cade/needWatering", msg);
```

To evaluate your plant status score now:

```
// To evaluate your plant status score now
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
```

To preserve the latest data and calculate the average score:

```
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
```

##### callback

Switch on the LED if messages received.

##### reconnect

Reconnect to MQTT server automatically when disconnected.

##### handle_OnConnect

Broadcast data via HTTP server.

##### handle_NotFound

Return 404 pages when webserver fails.

##### SendHTML

Show data on HTTP web page.

### 2.3 Subscribe data on MQTT

The data provided by Plant Friend can be subscribed on MQTT server. Everyone could subscribe the information on `mqtt.cetools.org` with port number `1883`. The subscription address is `student/CASA0014/plant/Plant-Friend-Cade/#`.

![./imgs/MQTT.png]()

### 2.4 Data visualisation on Grafana [4]

You can visit `http://stud-pi-ucfnzc1:3000/` to check your plant data information through some visulisation like line graph.

![./imgs/Grafana.png]()

## 3. Further work

* Modify the publish time interval;
  * Maybe deprecate the minuteChanged() function and use delay() instead.

* Modify the queue size to adapt the changeable interval;
  * Make the size as a variable.
* Debug and code more concisely;

## 4. Reference

[1] CE Workshops CASA0014 Plant-Monitor, Duncan Wilson. https://workshops.cetools.org/codelabs/CASA0014-2-Plant-Monitor/index.html?index=..%2F..casa0014#

[2] ArduinoQueue, Einar Arnason. https://github.com/EinarArnason/ArduinoQueue

[3] MQTT Explorer. http://mqtt-explorer.com/

[4] Grafana Official documents. https://grafana.com/docs/

[5] Adafruit Feather HUZZAH ESP8266 Overview, lady ada. https://learn.adafruit.com/adafruit-feather-huzzah-esp8266

[6] DHT22 Description. https://www.adafruit.com/product/385

[7] BC547 Transistor Pinout Configuration, https://components101.com/transistors/bc547-transistor-pinout-datasheet

[8] Raspberry Pi Documentation, https://www.raspberrypi.com/documentation/computers/getting-started.html

[9] Get started with the InfluxData Platform, https://docs.influxdata.com/platform/getting-started/

