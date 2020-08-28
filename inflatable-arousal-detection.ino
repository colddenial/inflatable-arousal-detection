#include <Wire.h>
#include "Adafruit_MPRLS.h"
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266mDNS.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include "AppleMidi.h"
#include <WebSocketsClient.h>
#include <ArduinoJson.h>

wl_status_t wl_status;
ESP8266WiFiMulti WiFiMulti;
WebSocketsClient webSocket;

#define RESET_PIN  -1
#define EOC_PIN    -1
Adafruit_MPRLS mpr = Adafruit_MPRLS(RESET_PIN, EOC_PIN);

float pressure_hPa;
int pressure_hPa_avg;
float pressureArray[100];
float smoothArray[10];
int pressureArrayIndex = 0;
long lastQuarterSecond = 0;
long lastSecond = 0;
int sendValue = 0;
int sendValue2 = 0;
const char * hostname = "inflatable";

const int ledMaxPIN = 2;
const int toyControlPIN = 12;

int lowerPowerLevel = 200;
int upperPowerLevel = 768;
int triggerValue = 120;
int powerLevel = 0;

APPLEMIDI_CREATE_INSTANCE(WiFiUDP, AppleMIDI);

// Store pressure value in the pressureArray
void pushPressureValue(float value)
{
  pressureArray[pressureArrayIndex++] = value;
  if (pressureArrayIndex > 99) pressureArrayIndex = 0;
}

float average(float * array, int len)
{
  float sum = 0;
  for (int i = 0 ; i < len ; i++)
    sum += array [i] ;
  return sum / len ;
}

void everyQuarterSecond()
{
  // Deplete sendValue accordingly
  if (sendValue > 0) sendValue -= 2;
  if (sendValue < 0) sendValue = 0;
  if (sendValue2 > 0) sendValue2 -= 2;
  if (sendValue2 < 0) sendValue2 = 0;

  // Find the average of the last 100 samples
  pressure_hPa_avg = (int) average(pressureArray, 100);

  int diff1 = abs( (pressure_hPa_avg - ((int) pressure_hPa)) * 0.65);
  int diff2 = abs( (pressure_hPa_avg - ((int) pressure_hPa)) * 0.75);

  // Diffs are to large its likely a sensor glych
  if (diff1 > 20) diff1 = 20;
  if (diff2 > 20) diff2 = 20;

  // increase depleting values with pressure differences
  sendValue += diff1;
  sendValue2 += diff2;

  // Make sure we stay in range for midi transmission
  if (sendValue > 127) sendValue = 127;
  if (sendValue2 > 127) sendValue2 = 127;

  if (triggerValue == 0)
  {
    analogWrite(toyControlPIN, map(powerLevel, 0, 127, 0, 768));
  } else if (sendValue > triggerValue) {
    digitalWrite(ledMaxPIN, HIGH);
    analogWrite(toyControlPIN, 0);
  } else {
    digitalWrite(ledMaxPIN, LOW);
    analogWrite(toyControlPIN, map(sendValue, 127, 0, lowerPowerLevel, upperPowerLevel));
  }

  //Serial.println(sendValue);
  // Send off to midi channels
  AppleMIDI.sendControlChange(20, sendValue, (byte)11);
  AppleMIDI.sendControlChange(21, sendValue2, (byte)11);
  sendDataToRouteput();
}

void sendDataToRouteput()
{
  String out;
  DynamicJsonDocument jsonBuffer(1024);
  const JsonObject& routeput = jsonBuffer.createNestedObject("__routeput");
  routeput["channel"] = "inflatable";
  const JsonObject& setChannelProperty = routeput.createNestedObject("setChannelProperty");
  setChannelProperty["inflatable"] = "";
  jsonBuffer["hPa"] = pressure_hPa;
  jsonBuffer["hPaAverage"] = pressure_hPa_avg;
  jsonBuffer["arousalA"] = sendValue;
  jsonBuffer["arousalB"] = sendValue2;
  serializeJson(jsonBuffer, out);
  webSocket.sendTXT(out);
}

void setup()
{
  Serial.begin(115200);
  pinMode(ledMaxPIN, OUTPUT);
  pinMode(toyControlPIN, OUTPUT);
  digitalWrite(toyControlPIN, LOW);
  if (! mpr.begin()) {
    Serial.println("Failed to communicate with MPRLS sensor, check wiring?");
    while (1) {
      delay(10);
    }
  }
  Serial.println("Found MPRLS sensor");
  WiFi.hostname( hostname );
  WiFiMulti.addAP("XiN1", "house777");
  WiFiMulti.addAP("XiG1", "house777");
  wl_status = WiFiMulti.run();
  while (wl_status != WL_CONNECTED) {
    delay(100);
    wl_status = WiFiMulti.run();
  }
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);
  webSocket.begin("173.255.230.80", 6144, "/channel/inflatable/");
  AppleMIDI.begin(hostname);
  AppleMIDI.OnReceiveControlChange(OnAppleMidiControlChange);
  tryMDNS();
}

void tryMDNS()
{
  if (MDNS.begin(hostname, WiFi.localIP()))
  {
    MDNS.addService("apple-midi", "udp", 5004);
  }
}

void OnAppleMidiControlChange(byte channel, byte number, byte value)
{
  if (channel == 11)
  {
    if (number == 1)
    {
      powerLevel = value;
      upperPowerLevel =  map(powerLevel, 0, 127, 200, 768);
    } else if (number == 20) {
      triggerValue = value;
    }
  }
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {

}

void loop() {
  pressure_hPa = mpr.readPressure();
  while (pressure_hPa < 1000)
  {
    delay(10);
    pressure_hPa = mpr.readPressure();
  }
  pushPressureValue(pressure_hPa);
  wl_status = WiFiMulti.run();
  if (wl_status == WL_CONNECTED)
  {
    MDNS.update();
    AppleMIDI.run();
    webSocket.loop();
    long ts = millis();
    if (ts - lastQuarterSecond >= 250)
    {
      everyQuarterSecond();
      lastQuarterSecond = ts;
    }
  }
  delay(10);
}
