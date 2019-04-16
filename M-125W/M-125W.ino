// Marsden Scales
// Reporting via MQTT

#include "info.h"
#include <ESP8266RevK.h>

void app_wrap(char*topic, uint8_t*message, unsigned int len);
void app_mqtt(const char *prefix, const char*suffix, const byte *message, size_t len);
ESP8266RevK revk("M-125W",app_wrap,app_mqtt);

void app_wrap(char*topic, uint8_t*message, unsigned int len)
{
  revk.message(topic,message,len);
}

void app_mqtt(const char *prefix, const char*suffix, const byte *message, size_t len)
{
  if (!suffix)return;
  if(strcmp(prefix, "cmnd"))return;
  if (!strcmp(suffix, "send"))
  {
    revk.pub("error", suffix, "Can't do send button yet");
    // TODO press SEND button
  } else revk.pub("error", suffix, "Unknown command");
}

void setup()
{
  Serial.begin(9600); // Marsden talks at 9600 Baud
}

#define MAX_LINE 100
char line[MAX_LINE + 1];
int linep = 0;

void loop()
{
  revk.loop();
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
          while (*p == ' ')p++;          revk.pub("stat", "weight", "%s", p);
        }
      }
      linep = 0;
    }
  }
}
