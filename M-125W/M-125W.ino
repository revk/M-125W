// Marsden Scales
// Reporting via MQTT

//#define MYDEBUG

#include <ESP8266RevK.h>
#include <ESP8266HTTPClient.h>

ESP8266RevK revk(__FILE__, "Build: " __DATE__ " " __TIME__);

#ifdef ARDUINO_ESP8266_NODEMCU
#define USE_SPI
#endif

// My settings
char cloudhost[129] = "weigh.me.uk";
char cloudpass[33] = "test";
byte cloudtls[20] = {0xA9, 0x4F, 0x79, 0xCE, 0x80, 0xD7, 0xA2, 0x88, 0x84, 0xFE, 0x62, 0xF6, 0xCC, 0xD8, 0x49, 0xCA, 0x0E, 0xBA, 0xC1, 0x9C};

#define SEND  1 // Send button
#define RST 2 // SPI
#define SS 16 // SPI

// RC522 connnections
// GPIO2  RST
// GPIO13 MOSI
// GPIO12 MISO
// GPIO14 SCK (CLK)
// GPIO16 SDA (SS)

#ifdef USE_SPI
#include <SPI.h>
#include <MFRC522.h>
MFRC522 rfid(16, 2); // Instance of the class
#endif

void pressend();
void app_wrap(char*topic, uint8_t*message, unsigned int len);
void app_mqtt(const char *prefix, const char*suffix, const byte *message, size_t len);


boolean app_setting(const char *setting, const byte *value, size_t len)
{ // Called for settings retrieved from EEPROM
  if (!strcasecmp(setting, "cloudhost") && len < sizeof(cloudhost)) {
    memcpy(cloudhost, value, len);
    cloudhost[len] = 0;
  } else if (!strcasecmp(setting, "cloudpass") && len < sizeof(cloudpass)) {
    memcpy(cloudpass, value, len);
    cloudpass[len] = 0;
  } else if (!strcasecmp(setting, "cloudtls") && len == sizeof(cloudtls))memcpy(cloudtls, value, len); // Exact length required
  else
    return false; // Unknown setting
  return true; // Done
}

boolean app_cmnd(const char*suffix, const byte *message, size_t len)
{ // Called for incoming MQTT messages
  if (!strcasecmp(suffix, "send"))
  {
    presssend();
    return true;
  }
  return false;
}

void setup()
{
#ifdef MYDEBUG
  rst_info *myResetInfo = ESP.getResetInfoPtr();
  Serial.printf("App started %s (%d)\n", ESP.getResetReason().c_str(), myResetInfo->reason);
#else
  Serial.begin(9600); // Marsden talks at 9600 Baud
  digitalWrite(SEND, HIGH);
  pinMode(SEND, INPUT);
#endif
#ifdef USE_SPI
  SPI.begin(); // Init SPI bus
  rfid.PCD_Init(); // Init MFRC522
#endif
}

#define MAX_LINE 100
char line[MAX_LINE + 1];
int linep = 0;

unsigned sendbutton = 0; // Signed to allow for wrap
void presssend()
{
  if (!(sendbutton = millis() + 250))sendbutton++; // Dont allow 0 to happen
#ifdef MYDEBUG
  Serial.println("Send low");
#else
  digitalWrite(SEND, LOW);
  pinMode(SEND, OUTPUT);
#endif
}

void report(byte *id, char *weight)
{
  if (weight && id)
    revk.pub("stat", "idweight", "%02X%02X%02X%02X %s", id[0], id[1], id[2], id[3], weight);
  else if (weight)
    revk.pub("stat", "weight", "%s", weight);
  else if (id)
    revk.pub("stat", "id", "%02X%02X%02X%02X", id[0], id[1], id[2], id[3]);
  if (!*cloudhost)return;
  // Post
  char url[500];
  int m = sizeof(url) - 1, p = 0;
  if (p < m)p += snprintf(url + p, m - p, "https://%s/weighin.cgi?version=%s", cloudhost, __DATE__ " " __TIME__);
  if (p < m)p += snprintf(url + p, m - p, "&scales=%06X", ESP.getChipId());
  if (p < m && cloudpass)p += snprintf(url + p, m - p, "&auth=%s", cloudpass); // Assume no special characters
  if (p < m && weight)p += snprintf(url + p, m - p, "&weight=%s", weight);
  if (p < m && id)p += snprintf(url + p, m - p, "&id=%02X%02X%02X%02X", id[0], id[1], id[2], id[3]);
  url[p] = 0;
  for (p = 0; url[p]; p++)if (url[p] == ' ')url[p] = '+';
  // Note, always https
  WiFiClientSecure client;
  client.setFingerprint(cloudtls);
  HTTPClient https;
  if (https.begin(client, url)) {
    int ret = https.GET();
    https.end();
    if (ret == 426)revk.ota(); // Upgrade required: New firmware required
    if (ret / 100 != 2)
      revk.error("https", "HTTP failed %d", ret);
  } else revk.error("https", "Failed");
}

void loop()
{
  revk.loop();
  if (sendbutton && (int) (sendbutton - millis()) < 0)
  { // Send button done
    sendbutton = 0;
#ifdef MYDEBUG
    Serial.println("Send high");
#else
    pinMode(SEND, INPUT);
    digitalWrite(SEND, HIGH);
#endif
  }
  static long carddone = 0;
  static byte cardid[4] = {};
  if (carddone && (int)(carddone - millis()) < 0)
  { // Card read timed out
    report(cardid, NULL);
    carddone = 0;
  }
  while (Serial.available() > 0) {
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
          if (carddone)report(cardid, p);
          else report(NULL, p);
          carddone = 0;
        }
      }
      linep = 0;
    }
  }
#ifdef USE_SPI
  static long cardcheck = 0;
  if ((int)(cardcheck - millis()) < 0)
  {
    cardcheck = millis() + 100;
    if (rfid.PICC_IsNewCardPresent())
    {
      if (rfid.PICC_ReadCardSerial())
      {
        presssend();
        MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
        memcpy(cardid, rfid.uid.uidByte, 4);
        if (!(carddone = millis() + 10000))carddone++;
      }
    }
  }
#endif
}
