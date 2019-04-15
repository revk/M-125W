// This is a wrapper for a number of common applications
// It provides the basic common aspects - connnection to WiFi and MQTT
//
// TODO :-
// This currently only sets hostname, ssid, etc from #define, to change to EEPROM
// Add functions for app to set these and store in EEPROM and other settings
// Sort hostname and app topic logic more cleanly
// Add fall back SSID
// Add fall back MQTT
// Add option for TLS MQTT

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

#define DEBUG_ESP_HTTP_UPDATE
#define DEBUG_ESP_PORT Serialx

#include <ESP8266WiFi.h>
#include <ESP8266httpUpdate.h>
#include <PubSubClient.h>

WiFiClient mqttclient;
PubSubClient mqtt(mqttclient);

static unsigned long mqttping = 0;
static unsigned long mqttretry = 0;
static unsigned long mqttbackoff = 100;

char chipid[7] = "?"; // Hex chip ID as string

static void upgrade(const byte *message, size_t len)
{
  revk_pub("stat", "upgrade", "Upgrade " __TIME__);
  WiFiClient client;
  t_httpUpdate_return ret = ESPhttpUpdate.update(client, FIRMWARE, 80, "/" APP ".ino." BOARD ".bin");
  if (!ESPhttpUpdate.update(client, FIRMWARE, 80, "/" APP ".ino." BOARD ".bin"))
  {
    revk_pub("error", "upgrade", "%s", ESPhttpUpdate.getLastErrorString().c_str());
    Serial.println(ESPhttpUpdate.getLastErrorString());
  }
}

extern void app_mqtt(const char *prefix, const char*suffix, const byte *message, size_t len);
void message(const char* topic, byte* payload, unsigned int len)
{
  char *p = strchr(topic, '/');
  if (!p)return;
  char *prefix = (char*)alloca(p - topic + 1);
  strncpy(prefix, topic, p - topic);
  prefix[p - topic] = 0;
  p = strchr(p + 1, '/');
  if (p)p++; else p = NULL;
  if (!strcmp(prefix, "cmnd") && p && !strcmp(p, "upgrade"))
  { // Do upgrade from web
    upgrade(payload, len);
    return; // Yeh, would not get here.
  }
  app_mqtt(prefix, p, payload, len);
}

void setup()
{
  snprintf(chipid, sizeof(chipid), "%06X", ESP.getChipId());
  WiFi.hostname(HOSTNAME);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFISSID, WIFIPASS);
  mqtt.setServer(MQTTHOST, MQTTPORT);
  mqtt.setCallback(message);
  app_setup();
}

void loop()
{
  // MQTT reconnnect
  if (!mqtt.loop() && mqttretry < millis())
  {
    if (mqtt.connect(chipid, MQTTUSER, MQTTPASS))
    { // Worked
      revk_pub("stat", "boot", "Running " __TIME__ );
      mqttbackoff = 1000;
      char sub[101];
      snprintf(sub, sizeof(sub), "+/" APP "-%s/#", chipid); // Specific device
      mqtt.subscribe(sub);
      mqtt.subscribe("+/" HOSTNAME "/#"); // The hostname
      mqtt.subscribe("+/" APP "/#"); // General group topic
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
  va_end(ap);
  char topic[101];
  snprintf(topic, sizeof(topic), "%s/" HOSTNAME "-%s/%s", prefix, chipid, suffix);
  mqtt.publish(topic, temp);
}
