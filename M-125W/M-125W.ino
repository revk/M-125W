// Marsden Scales
// Reporting via MQTT

#include "revk.h"

void app_mqtt(const char *topic, char *message, size_t len)
{
}

void app_setup()
{

}

#define MAX_LINE 100
char line[MAX_LINE + 1];
int linep = 0;

void app_loop()
{
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
          revk_pub("stat", "weight", p);
        }
      }
      linep = 0;
    }
  }
}
