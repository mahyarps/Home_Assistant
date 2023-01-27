#include <ESP8266WiFi.h>
#include <PubSubClient.h>

// Loading Passwords: (place your passwords inside passwords.h file)
#include "passwords.h"
char ssid[] = WIFI_SSID;
char pass[] = WIFI_PASS;

#define MSG_BUFFER_SIZE  (50)
char msg[MSG_BUFFER_SIZE];

const int analogPin = A0; //Analoge pin moisture
const int moistureD = D7;  //Digital pin moisture
const int moistureP = D6;  //Power of Moisture Sensor 
const int waterLevelP = D0; // Powere of Water Level Sensor
const int pumpP = D2; // Pump
const int ledP = D1; // LED

// Set const variables
const char broker[] = "mqtt-broker";
const int  port     = 1883; // Change the port accordingly
const char pubtopic[]  = "flowerpot/green/1/status"; // Set topics for publishing mqtt messages
const char subtopic[]  = "flowerpot/green/1/cmd"; // Set topics to subscribe to

// Tunning: ( Change variables according to your usage )
int pumpDelay = 500; // Time to pump water
int waterLevelLowerThreshold = 60; // Threshold to turn on the LED
int waterLevelUpperThreshold = 220; // Threshold to turn on the LED
int moistureThreshod = 550; // will be removed after tuning the moisture digital pin

//Global Vriables
int moistureA_value = 0;
int moistureD_value = 0;
int waterLevel_value = 0;
unsigned long prevMillis = 0; // Last time ran the loop
const long interval = 3600000; // Interval to run the loop

// Saved Stats:
int prev_waterLevel_value = 0;
int prev_moistureA_value = 0;
const int send_values_threshhold = 10; // Set how often send the data using mqtt
bool first_it = true; // First itteration

WiFiClient wifi;
PubSubClient client(wifi);

// Setup WIFI connection:
void setup_wifi() {

  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);

  Serial.print("Connecting ");
  // Try to reconnect to the wifi:
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(". ");
  }
  Serial.println();

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

// MQTT callback:
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
    digitalWrite(BUILTIN_LED, LOW);   // Turn the LED on (Note: LOW is the voltage level
    // but actually the LED is on; this is because it is active low on the ESP-01)
  } else {
    digitalWrite(BUILTIN_LED, HIGH);  // Turn the LED off by making the voltage HIGH
  }
}

// Reconnect to MQTT:
void reconnect() {
  Serial.println("Attempting MQTT connection...");
  // Create a client ID
  String clientId = "flowerplot1";
  // Attempt to connect
  if (client.connect(clientId.c_str())) {
    Serial.println("connected");
    // Once connected, publish an announcement...
    client.publish(pubtopic, "connected");
    // ... and resubscribe
    client.subscribe(subtopic);
  } else {
    Serial.print("failed, rc=");
    Serial.print(client.state());
    Serial.println(" try again in 5 seconds");
    // Wait 5 seconds before retrying
    delay(5000);
  }
}

// Check the moisture level and water the plants if needed:
void status() {
  //Measure Moisture
  digitalWrite(moistureP, HIGH);
  delay(2000);
  moistureA_value = analogRead(analogPin);
  moistureD_value = digitalRead(moistureD);
  Serial.println("Moisture Sensor Values:");
  Serial.println(moistureA_value);
  Serial.println(moistureD_value);
  if (abs(moistureA_value -  prev_moistureA_value) > send_values_threshhold) {
    snprintf (msg, MSG_BUFFER_SIZE, "Moisture/%ld", moistureA_value);
    client.publish(pubtopic, msg);
    prev_moistureA_value = moistureA_value;
  }
  digitalWrite(moistureP, LOW);

  Serial.print(moistureA_value);
  Serial.print("    ");
  Serial.println(moistureThreshod);
  if (moistureA_value > moistureThreshod){
    // Pump water
    Serial.println("Pump Started");
    client.publish(pubtopic, "Pump/ON");
    digitalWrite(pumpP, HIGH);
    delay(pumpDelay);
    client.publish(pubtopic, "Pump/OFF");
    Serial.println("Pump Stopped");
    digitalWrite(pumpP, LOW);
  }
  
  Serial.println("------------------------------------------------"); 
}

// Setup pins, wifi and mqtt:
void setup() {
  Serial.begin(115200);
  
  pinMode(moistureP, OUTPUT);
  pinMode(waterLevelP, OUTPUT);
  pinMode(pumpP, OUTPUT);
  pinMode(ledP, OUTPUT);
  pinMode(moistureD, INPUT);
  digitalWrite(moistureP, LOW);
  digitalWrite(waterLevelP, LOW);
  digitalWrite(pumpP, LOW);
  digitalWrite(ledP, LOW);

  // attempt to connect to Wifi:
  setup_wifi();

  // Setup mqtt client
  client.setServer(broker, port);
  client.setCallback(callback);
  
  status();
}

// Main loop:
void loop() {

  // Reconnect if the client is not connected to MQTT:
  while (!client.connected()) {
    reconnect();
  }

  client.loop();

  unsigned long currentMillis = millis();
  Serial.println(currentMillis);
  if ( currentMillis - prevMillis >= interval || first_it){
    if (first_it){
      first_it = false;
    }
    prevMillis = currentMillis;
    status();
  }
  
  //Measure Water Level
  digitalWrite(waterLevelP, HIGH);
  delay(2000);
  waterLevel_value = analogRead(analogPin);
  if (abs(waterLevel_value - prev_waterLevel_value) > send_values_threshhold){
    snprintf (msg, MSG_BUFFER_SIZE, "Water Level/%ld", waterLevel_value);
    client.publish(pubtopic, msg);
    prev_waterLevel_value = waterLevel_value;
  }
  Serial.println("Water Level Sensor Values:");
  Serial.println(waterLevel_value);
  digitalWrite(waterLevelP, LOW);
  if (waterLevel_value < waterLevelLowerThreshold || waterLevel_value > waterLevelUpperThreshold){
    if ( digitalRead(ledP) == LOW){
      Serial.println("LED Turned ON");
      client.publish(pubtopic, "LED/ON");
      digitalWrite(ledP, HIGH);
    }
  } else if (digitalRead(ledP) == HIGH) {
    Serial.println("LED Turned OFF");
    client.publish(pubtopic, "LED/OFF");
    digitalWrite(ledP, LOW);
  }
}
