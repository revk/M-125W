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
#ifndef MQTTUSER
#define MQTTHOST        ""
#endif
#ifndef MQTTPASS
#define MQTTPASS        ""
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
#include <Adafruit_MQTT.h>
#include <Adafruit_MQTT_Client.h>

WiFiClient mqttclient;
Adafruit_MQTT_Client mqtt(&mqttclient, MQTTHOST, 1883);
Adafruit_MQTT_Subscribe subupgrade = Adafruit_MQTT_Subscribe(&mqtt, "cmnd/" HOSTNAME "/upgrade");
Adafruit_MQTT_Publish status = Adafruit_MQTT_Publish(&mqtt, "stat/" HOSTNAME "/status");

unsigned long mqttping = 0;
unsigned long mqttretry = 0;

void upgrade(char *msg, uint16_t len)
{
  status.publish("Upgrade " __TIME__);
  ESPhttpUpdate.rebootOnUpdate(true);
  WiFiClient client;
  ESPhttpUpdate.update(client, FIRMWARE, 80, "/" HOSTNAME ".ino." BOARD ".bin");
}

void setup()
{
  Serial.begin(9600);
  WiFi.hostname(HOSTNAME);
  WiFi.begin(WIFISSID, WIFIPASS);
  subupgrade.setCallback(upgrade);
  mqtt.subscribe(&subupgrade);
  app_setup();
}

void loop()
{
  if (!mqtt.connected() && mqttretry < millis()) {
    mqtt.disconnect();
    if (!mqtt.connect())status.publish("Running " __TIME__);
    mqttretry = millis() + 1000;
  }
  mqtt.processPackets(1);
  if (mqttping < millis()) {
    mqttping = millis() + 10000;
    mqtt.ping();
  }
  app_loop();
}

void revk_pub(const char *prefix, const char *suffix, const char *fmt, ...)
{
  char temp[256];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(temp, sizeof(temp), fmt, ap);
  status.publish(temp); // TODO
}
