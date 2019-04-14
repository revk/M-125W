// Simple ESP8266 code for use with Marsden M-125 scales. Connect GND, 3.3V and serial. Press "SEND" to send weight.
// Tested on ESP-01 AND ESP-12F

#include <ESP8266httpUpdate.h>
#include <ESP8266WiFi.h>
#include <Adafruit_MQTT.h>
#include <Adafruit_MQTT_Client.h>

#define NAME        "M-125W"
#define MQTTHOST    "mqtt.iot"    // Set your host name
#define UPGRADEHOST "mqtt.iot"    // Set hostname for getting code upgrade
#define SSID        "IoT"         // Set your WiFi SSID
#define WPA2        "password"    // Set your WIFi password

#ifdef ARDUINO_ESP8266_NODEMCU
#define BOARD "ESP12F"
#else
#define BOARD "ESP01"
#endif

WiFiClient client;
Adafruit_MQTT_Client mqtt(&client, MQTTHOST, 1883);
Adafruit_MQTT_Subscribe subupgrade = Adafruit_MQTT_Subscribe(&mqtt, "cmnd/" NAME "/upgrade");
Adafruit_MQTT_Publish status = Adafruit_MQTT_Publish(&mqtt, "stat/" NAME "/status");
Adafruit_MQTT_Publish weight = Adafruit_MQTT_Publish(&mqtt, "stat/" NAME "/weight");

unsigned long mqttping = 0;
unsigned long mqttretry = 0;

void upgrade(char *msg, uint16_t len)
{
  status.publish("Upgrade " __TIME__);
  ESPhttpUpdate.rebootOnUpdate(true);
  WiFiClient client;
  ESPhttpUpdate.update(client, UPGRADEHOST, 80, "/" NAME "-" BOARD ".bin");
}

void setup() {
  Serial.begin(9600);
  WiFi.hostname(NAME);
  WiFi.begin(SSID, WPA2);
  subupgrade.setCallback(upgrade);
  mqtt.subscribe(&subupgrade);
}

#define MAX_LINE 100
char line[MAX_LINE + 1];
int linep = 0;

void loop() {
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
  if (Serial.available() > 0) {
    char c = Serial.read();
    if (c >= ' ')
    {
      if (linep < MAX_LINE)line[linep++] = c;
    } else
    { // end of line
      line[linep] = 0;
      if (linep)
      {
        if (!strncmp(line, "NET WEIGHT", 10))
        {
          char *p = line + 10;
          while (*p == ' ')p++;
          weight.publish(p);
        }
      }
      linep = 0;
    }
  }
}
