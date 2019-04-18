// Marsden Scales
// ESP-01 based for simple reporting of weight on SEND button
// Option to force send using cmnd/app/host/send
// ESP-12F based for use with MFRC522 to send on card read
// Reporting via MQTT (note, this expected to be local and so non TLS)
// Reporting via https to a server (CA by Let's Encrypt)
//

// Wiring (recommend a 4 pin header, see https://youtu.be/l1VAymhwtVM for details)
// GND/3V3 to GND/VDD pads in middle (non display side of PCB)
// RX to ping 4 of SO8 labelled U2 (display side of PCB)
// TX via diode, low side to ESP, to SEND switch the non grounded side (display side of PCB)

// Witing for ESP-12F
// As above to the M-125
// RC522 connnections (in addition to GND/3V3)
// GPIO2  RST
// GPIO13 MOSI
// GPIO12 MISO
// GPIO14 SCK (CLK)
// GPIO16 SDA (SS)

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

#define SEND  1 // Send button (TX pin)
#define RST 2 // SPI
#define SS 16 // SPI

#ifdef USE_SPI
#include <SPI.h>
#include <MFRC522.h>
MFRC522 rfid(16, 2); // Instance of the class
#endif

void pressend();

boolean app_setting(const char *setting, const byte *value, size_t len)
{ // Called for settings retrieved from EEPROM
  if (!strcasecmp(setting, "cloudhost") && len < sizeof(cloudhost)) {
    memcpy(cloudhost, value, len);
    cloudhost[len] = 0;
  } else if (!strcasecmp(setting, "cloudpass") && len < sizeof(cloudpass)) {
    memcpy(cloudpass, value, len);
    cloudpass[len] = 0;
  }
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
  WiFiClientSecure client = revk.leclient();
  HTTPClient https;
  if (https.begin(client, url)) {
    int ret = https.GET();
    https.end();
    if (ret == 426)revk.ota(); // Upgrade required: New firmware required
    if (ret / 100 != 2)
      revk.error("https", "Failed %d from %s", ret, cloudhost);
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
    cardcheck = millis() + 10;
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
