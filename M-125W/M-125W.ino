// Marsden Scales
// Reporting via MQTT

#define MYDEBUG

#include <ESP8266RevK.h>
#include <ESP8266HTTPClient.h>

#ifdef ARDUINO_ESP8266_NODEMCU
#define USE_SPI
#endif

#define CLOUD "weigh.me.uk"
// TODO https
const uint8_t fingerprint[20] = {0x5A, 0xCF, 0xFE, 0xF0, 0xF1, 0xA6, 0xF4, 0x5F, 0xD2, 0x11, 0x11, 0xC6, 0x1D, 0x2F, 0x0E, 0xBC, 0x39, 0x8D, 0x50, 0xE0};

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
ESP8266RevK revk(__FILE__);

boolean app_setting(const char *setting, const byte *value, size_t len)
{ // Called for settings retrieved from EEPROM
  return false; // Unknown setting
}

boolean app_cmnd(const char*suffix, const byte *message, size_t len)
{ // Called for incoming MQTT messages
  if (!strcmp(suffix, "send"))
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
  pinMode(SEND, OUTPUT);
#endif
#ifdef USE_SPI
  SPI.begin(); // Init SPI bus
  rfid.PCD_Init(); // Init MFRC522
#endif
}

#define MAX_LINE 100
char line[MAX_LINE + 1];
int linep = 0;

unsigned long carddone = 0;
byte cardid[4] = {};

unsigned long sendbutton = 0;
void presssend()
{
  sendbutton = millis() + 250;
#ifdef MYDEBUG
  Serial.println("Send low");
#else
  digitalWrite(SEND, LOW);
#endif
}

void report(byte *id, char *weight)
{
  char url[200];
  // TODO for testing - needs proper authentication adding!
  // TODO HTTPS
  if (id && weight)
  {
    revk.pub("stat", "idweight", "%02X%02X%02X%02X %s", id[0], id[1], id[2], id[3], weight);
    snprintf(url, sizeof(url), "http://%s/weighin.cgi?scales=%06X&id=%02X%02X%02X%02X&weight=%s", CLOUD, ESP.getChipId(),  id[0], id[1], id[2], id[3], weight);
  }
  else if (id)
  {
    revk.pub("stat", "id", "%02X%02X%02X%02X", id[0], id[1], id[2], id[3]);
    snprintf(url, sizeof(url), "http://%s/weighin.cgi?scales=%06X&id=%02X%02X%02X%02X", CLOUD, ESP.getChipId(), id[0], id[1], id[2], id[3]);
  }
  else if (weight)
  {
    revk.pub("stat", "weight", "%s", weight);
    snprintf(url, sizeof(url), "http://%s/weighin.cgi?scales=%06X&weight=%s", CLOUD, ESP.getChipId(), weight);
  }
  WiFiClient      client;
  HTTPClient http;
  http.begin(client, url);
  http.GET();
  http.end();
}

void loop()
{
  revk.loop();
  if (sendbutton && sendbutton < millis())
  { // Send button done
    sendbutton = 0;
#ifdef MYDEBUG
    Serial.println("Send high");
#else
    digitalWrite(SEND, HIGH);
#endif
  }
  if (carddone && carddone < millis())
  { // Card read timed out
    report(cardid, NULL);
    carddone = 0;
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
          if (carddone)report(cardid, p);
          else report(NULL, p);
          carddone = 0;
        }
      }
      linep = 0;
    }
  }
#ifdef USE_SPI
  if (rfid.PICC_IsNewCardPresent())
  {
    if (rfid.PICC_ReadCardSerial())
    {
      presssend();
      MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
      memcpy(cardid, rfid.uid.uidByte, 4);
      carddone = millis() + 5000;
    }
  }
#endif
}
