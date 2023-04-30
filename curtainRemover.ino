#ifdef ESP32
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>
#endif
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>   // Universal Telegram Bot Library written by Brian Lough: https://github.com/witnessmenow/Universal-Arduino-Telegram-Bot
#include <ArduinoJson.h>
#include <Arduino.h>
#include <driver/adc.h>

// Replace with your network credentials
const char* ssid = "OPPO Reno8 Z 5G";
const char* password = "34567890";

// Initialize Telegram BOT
#define BOTTOKEN "6210319406:AAFynpvUxIu9sAQfGvWgT_ztw_btxJc2VVQ"  // your Bot Token (Get from Botfather)

// Use @myidbot to find out the chat ID of an individual or a group
// Also note that you need to click "start" on a bot before it can
// message you
#define CHAT_ID "6099254515"

#ifdef ESP8266
  X509List cert(TELEGRAM_CERTIFICATE_ROOT);
#endif

// Motor Control Pins
#define MOTOR_PIN_1_P32 32
#define MOTOR_PIN_2_P33 33

// Microphone Pin
#define MIC_PIN_P34 34

// Clap Threshold (adjust as needed)
#define CLAPTHRESHOLD 50
#define BUFFER_SIZE 16  // Change this to adjust the buffer size

// Telegram Bot setup var
WiFiClientSecure client;
UniversalTelegramBot bot(BOTTOKEN, client);

// Checks for new messages every 1 second.
int botRequestDelay = 1000;
unsigned long lastTimeBotRan;

// Mic
int micValue = 0;
int averageValue = 0;

// Buffering
int buffer[BUFFER_SIZE];  // Create a buffer of the specified size
int bufferIndex = 0;  // Index of the next value to be added to the buffer
int bufferSum = 0;  // Running sum of the buffer values

// Motor Control Variables
bool rotateRight = true;
bool motorStopped = true;
bool lState = 1;
bool rState = 0;
bool rotToRight = true;
bool commandFound = false;
int movingTimeMS = 2000;
int startRotate = 0;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  #ifdef ESP8266
    configTime(0, 0, "pool.ntp.org");      // get UTC time via NTP
    client.setTrustAnchors(&cert); // Add root certificate for api.telegram.org
  #endif
  
  // Connect to Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  #ifdef ESP32
    client.setCACert(TELEGRAM_CERTIFICATE_ROOT); // Add root certificate for api.telegram.org
  #endif
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }
  // Print ESP32 Local IP Address
  Serial.println(WiFi.localIP());

  pinMode(MOTOR_PIN_1_P32, OUTPUT);
  pinMode(MOTOR_PIN_2_P33, OUTPUT);
//  Serial.begin(9600);
  
  // Initialize ADC (analog-to-digital converter) for microphone pin
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_11db);
  
  for (int i = 0; i < BUFFER_SIZE; i++) {
    buffer[i] = 0;
  }
}

// Will buffer mic_value so it not too swing
int buffering(int micValue){
  bufferSum -= buffer[bufferIndex];  // Subtract the oldest value from the sum
  bufferSum += micValue;  // Add the new value to the sum
  buffer[bufferIndex] = micValue;  // Add the new value to the buffer
  bufferIndex = (bufferIndex + 1) % BUFFER_SIZE;  // Increment the buffer index and wrap around if necessary

  // Calculate the average value from the buffer
  averageValue = bufferSum / (2 * BUFFER_SIZE);

  return averageValue;
}

// Handle what happens when you receive new messages
void handleNewMessages(int numNewMessages) {
  Serial.println("handleNewMessages");
  Serial.println(String(numNewMessages));

  for (int i=0; i<numNewMessages; i++) {
    // Chat id of the requester
    String chat_id = String(bot.messages[i].chat_id);
    if (chat_id != CHAT_ID){
      bot.sendMessage(chat_id, "Unauthorized user", "");
      continue;
    }
    
    // Print the received message
    String text = bot.messages[i].text;
    Serial.println(text);

    String from_name = bot.messages[i].from_name;

    if (text == "/start") {
      String welcome = "Welcome, " + from_name + ".\n";
      welcome += "Use the following commands to control your outputs.\n\n";
      welcome += "/led_on to turn GPIO ON \n";
      welcome += "/led_off to turn GPIO OFF \n";
      welcome += "/state to request current GPIO state \n";
      bot.sendMessage(chat_id, welcome, "");
    }

    if (text == "/rotate_right") {
      if ( ! motorStopped ) 
      bot.sendMessage(chat_id, "LED state set to ON", "");
      rotToRight = true;
    }
  }
}

// Motor will do their job here ( rotate L/R or stop )
void moterJob(bool clapDetected){
   if( (clapDetected && motorStopped) || commandFound){
    if(rotToRight){
      lState = HIGH;
      rState = LOW;
      rotToRight = false;
    }
    else{
      lState = LOW;
      rState = HIGH;
      rotToRight = true;
    }
    motorStopped = false;
    startRotate = millis(); 
  }

  if(!motorStopped){
    if(millis() - startRotate >= movingTimeMS){
      motorStopped = true;
      lState = LOW;
      rState = LOW;
    }
    digitalWrite(MOTOR_PIN_1_P32,lState);
    digitalWrite(MOTOR_PIN_2_P33,rState);
  }
}

bool detectSound(int micValue){
  int averageValue = buffering(micValue);
  bool clapDetected = averageValue > CLAPTHRESHOLD && micValue <= 600;
  return clapDetected;
}

void loop() {
  handleNewMessages();
  micValue = analogRead(MIC_PIN_P34);
  bool clapDetected = detectSound(micValue);
  Serial.print(clapDetected);
  Serial.print(" ");
  Serial.print(averageValue);
  Serial.print(" ");
  Serial.println(micValue);
  moterJob(clapDetected);
  delay(10);
}
