// This is a wrapper for a number of common applications
// It provides the basic common aspects - connnection to WiFi and MQTT
//
// TODO :-
// This currently only sets hostname, ssid, etc from #define, to change to EEPROM
// Add functions for app to set these and store in EEPROM and other settings
// Add options for some Serial debug
// Add fall back SSID
// Add fall back MQTT
// Add option for TLS MQTT
// Changed to another MQTT library, and add subscribe, etc.

// There are a number of default / key settings which can be overridden in the info.h file
//
#include "revk.h"
#include "info.h"

#ifndef APP
#define APPE            "RevK-Test"
#endif
#ifndef HOSTNAME
#define HOSTNAME        "RevK"
#endif
#ifndef MQTTHOST
#define MQTTHOST        "mqtt.iot"
#endif
#ifndef MQTTPORT
#define MQTTPORT        1883
#endif
#ifndef MQTTUSER
#define MQTTUSER        NULL
#endif
#ifndef MQTTPASS
#define MQTTPASS        NULL
#endif
#ifndef FIRMWARE
#define FIRMWARE        "mqtt.iot"
#endif
#ifndef WIFISSID
#define WIFISSID        "IoT"
#endif
#ifndef WIFIPASS
#define WIFIPASS        "unsecure"
#endif

#ifdef ARDUINO_ESP8266_NODEMCU
#define BOARD "nodemcu"
#else
#define BOARD "generic"
#endif

#include <ESP8266httpUpdate.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

WiFiClient mqttclient;
PubSubClient mqtt(mqttclient);

unsigned long mqttping = 0;
unsigned long mqttretry = 0;
unsigned long mqttbackoff = 100;
char chipid[7];

void upgrade(char *msg, uint16_t len)
{
  revk_pub("stat", "upgrade", "Upgrade " __TIME__);
  ESPhttpUpdate.rebootOnUpdate(true);
  WiFiClient client;
  ESPhttpUpdate.update(client, FIRMWARE, 80, "/" HOSTNAME ".ino." BOARD ".bin");
}

void setup()
{
  snprintf(chipid, sizeof(chipid), "%06X", ESP.getChipId());
  Serial.begin(9600);
  WiFi.hostname(HOSTNAME);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFISSID, WIFIPASS);
  mqtt.setServer(MQTTHOST, MQTTPORT);
  app_setup();
}

void loop()
{
  // MQTT reconnnect
  if (!mqtt.loop() && mqttretry < millis())
  {
    Serial.printf("MQTT connect %d %d %d\n",mqttretry,mqttbackoff,millis());
    if (mqtt.connect(chipid, MQTTUSER, MQTTPASS))
    { // Worked
      revk_pub("stat", "boot", "Running " __TIME__ " %s", chipid);
      mqttbackoff = 1000;
    }
    else if (mqttbackoff < 300000) mqttbackoff *= 2; // Failed, back off
    mqttretry = millis() + mqttbackoff;
  }
  app_loop();
}

void revk_pub(const char *prefix, const char *suffix, const char *fmt, ...)
{
  char temp[256];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(temp, sizeof(temp), fmt, ap);
  // TODO
}
