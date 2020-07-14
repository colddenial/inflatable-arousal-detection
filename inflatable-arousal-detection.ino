#include <Wire.h>
#include "Adafruit_MPRLS.h"
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266mDNS.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include "AppleMidi.h"

wl_status_t wl_status;
ESP8266WiFiMulti WiFiMulti;

#define RESET_PIN  -1
#define EOC_PIN    -1
Adafruit_MPRLS mpr = Adafruit_MPRLS(RESET_PIN, EOC_PIN);

float pressure_hPa;
float pressureArray[100];
float smoothArray[10];
int pressureArrayIndex = 0;
long lastQuarterSecond = 0;
long lastSecond = 0;
int sendValue = 0;
int sendValue2 = 0;
const char * hostname = "inflatable";


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
  int avg = (int) average(pressureArray, 100);

  int diff1 = abs( (avg - ((int) pressure_hPa)) * 0.75 );
  int diff2 = abs( (avg - ((int) pressure_hPa)) );

  // Diffs are to large its likely a sensor glych
  if (diff1 > 20) diff1 = 20;
  if (diff2 > 20) diff2 = 20;

  // increase depleting values with pressure differences
  sendValue += diff1;
  sendValue2 += diff2;

  // Make sure we stay in range for midi transmission
  if (sendValue > 127) sendValue = 127;
  if (sendValue2 > 127) sendValue2 = 127;

  // Send off to midi channels
  AppleMIDI.sendControlChange(20, sendValue, (byte)11);
  AppleMIDI.sendControlChange(21, sendValue2, (byte)11);
}

void setup() {
  Serial.begin(115200);
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
  AppleMIDI.begin(hostname);
  tryMDNS();
}

void tryMDNS()
{
  if (MDNS.begin(hostname, WiFi.localIP()))
  {
    MDNS.addService("apple-midi", "udp", 5004);
  }
}


void loop() {
  pressure_hPa = mpr.readPressure();
  while(pressure_hPa < 1000)
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
    long ts = millis();
    if (ts - lastQuarterSecond >= 250)
    {
      everyQuarterSecond();
      lastQuarterSecond = ts;
    }
  }
  delay(10);
}
