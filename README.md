# Inflatable Arousal Detection #

This is my attempt at detecting arousal using discreet pressure changes in an inflatable dildo using an pressure sensor and microcontroller. Like most of my projects, i use midi control change messages to represent analog output.

Parts:

ADAFRUIT MPRLS PORTED PRESSURE SENSOR BREAKOUT - 0 TO 25 PSI
https://www.adafruit.com/product/3965

ADAFRUIT HUZZAH ESP8266 BREAKOUT
https://www.adafruit.com/product/2471

### Usage ###
This program will output MIDI Control changes via Apple's RTP Midi protocol. Right now it uses control changes 20 and 21 (on channel 11) which currently represent different sensitivities.


Wifi Settings can be added to the code (search for)
```
WiFiMulti.addAP("...", "...");
```

Once the device is active and connected to your network it should show up via MDNS as "inflatable" I highly recommend using rtpMIDI from http://www.tobias-erichsen.de/software/rtpmidi.html to form a connection.

Once you've created the midi device lovense toys can be controlled using:
https://github.com/colddenial/midi-lovense-bridge

Creating a rule using "FULL INVERTED" will lower the power of the toy as arousal increases.