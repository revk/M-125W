// Marsden Scales
// ESP-01 based for simple reporting of weight on SEND button
// Option to force send using cmnd/app/host/send
// ESP-12F based for use with MFRC522 to send on card read
// reporting via MQTT (note, this expected to be local and so non TLS)
// reporting via https to a server (CA by Let's Encrypt)

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

#define SENDRETRY 1000  // Re-press send if no weight yet
#define CARDWAIT 20000  // Wait for weight after getting card
//#define TAGWAIT 5000  // Wait for tag read

#include <ESPRevK.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266TrueRandom.h>

ESPRevK revk(__FILE__, __DATE__ " " __TIME__,NULL);

#ifdef ARDUINO_ESP8266_NODEMCU
#define USE_SPI
#define USE_PN532 // Use PN532 rather than MFRC522
#endif

#define app_settings \
  s(cloudhost); \
  s(cloudpass); \

#define s(n) const char *n=NULL
  app_settings
#undef s

#define SEND  1 // Send button (TX pin)
#define RST 2 // SPI
#define SS 16 // SPI

#ifdef USE_SPI
#include <SPI.h>
#ifdef USE_PN532
#include <PN532_SPI.h>
#include "PN532RevK.h"
  PN532_SPI pn532spi(SPI, SS);
  PN532RevK nfc(pn532spi);
  uint32_t pn532ver = 0;
#ifdef TAGWAIT
#include "emulatetag.h"
#include "NdefMessage.h"
  EmulateTag tag(pn532spi);
#endif
#else
#include <MFRC522.h>
  MFRC522 nfc(SS, RST); // Instance of the class
#endif
#endif

  void pressend();
  int report(const char *id, const char *weight);

  const char * app_setting(const char *tag, const byte *value, size_t len)
  { // Called for settings retrieved from EEPROM
#define s(n) do{const char *t=PSTR(#n);if(!strcmp_P(tag,t)){n=(const char *)value;return t;}}while(0)
    app_settings
#undef s
    return NULL; // Done
  }

  boolean app_command(const char*tag, const byte *message, size_t len)
  { // Called for incoming MQTT messages
    if (!strcasecmp_P(tag, PSTR("send")))
    {
      presssend();
      return true;
    }
    if (!strcasecmp_P(tag, PSTR("report")))
    {
      report(NULL, NULL);
      return true;
    }
    return false;
  }

  void setup()
  {
#ifdef REVKDEBUG
    debug("Started " __FILE__);
#else
    Serial.begin(9600); // Marsden talks at 9600 Baud
    digitalWrite(SEND, HIGH);
    pinMode(SEND, INPUT_PULLUP);
#endif
#ifdef USE_SPI
#ifdef USE_PN532
    pn532ver = nfc.begin();
#else
    nfc.PCD_Init(); // Init MFRC522
#endif
#endif
  }

#define MAX_LINE 100
  char line[MAX_LINE + 1];
  int linep = 0;

  unsigned int sendbutton = 0; // Signed to allow for wrap
  unsigned int sendlast = 0; // Last send button
  void presssend()
  {
    sendlast = millis();
    sendbutton = (sendlast + 250 ? : 1);
#ifdef REVKDEBUG
    Serial.println("Send low");
#else
    digitalWrite(SEND, LOW);
    pinMode(SEND, OUTPUT);
#endif
  }

  int report(const char *id, const char *weight)
  {
    revk.mqttcloseTLS(F("Post data")); // Clash on memory space for TLS?
    if (!cloudpass)
    { // We need a password - make one - store in flash - so we use same every time
      debug("Cloud pass needed");
      int i;
      char pass[33];
      for (i = 0; i <  sizeof(pass) - 1; i++)pass[i] = 'A' + ESP8266TrueRandom.random(26);
      pass[i] = 0; // End
      revk.setting(F("cloudpass"), pass); // save
    }
    debug("report");
    if (weight && id)
      revk.event(F("idweight"), F("%s %s"), id, weight);
    else if (weight)
      revk.event(F("weight"), F("%s"), weight);
    if (!cloudhost)cloudhost = "weigh.me.uk";
    // Post
    char url[250];
    int m = sizeof(url) - 1, p = 0;
    if (p < m)p += snprintf_P(url + p, m - p, PSTR("/weighin.cgi?version=%s"), revk.appver);
    if (p < m)p += snprintf_P(url + p, m - p, PSTR("&scales=%06X"), ESP.getChipId());
    if (p < m && cloudpass)p += snprintf_P(url + p, m - p, PSTR("&auth=%s"), cloudpass); // Assume no special characters
    if (p < m && weight)p += snprintf_P(url + p, m - p, PSTR("&weight=%s"), weight);
    if (p < m && id)p += snprintf_P(url + p, m - p, PSTR("&id=%s"), id);
    url[p] = 0;
    for (p = 0; url[p]; p++)if (url[p] == ' ')url[p] = '+';
    debugf("URL %s", url);
    // Note, always https
    WiFiClientSecure client;
    revk.clientTLS(client);
    HTTPClient https;
    debugf("Connect %s", cloudhost);
    https.begin(client, cloudhost, 443, url, true);
    int ret = https.GET();
    https.end();
    if (ret == HTTP_CODE_UPGRADE_REQUIRED)
      revk.ota(); // Upgrade required: New firmware required
    else if (ret == HTTP_CODE_LOCKED)
    {
      revk.setting(F("mqtthost"), cloudhost); // Kick mqtt in to life on same server as fallback for config
      revk.setting(F("mqttuser"));
      revk.setting(F("mqttpass"));
      revk.setting(F("mqttota1"));
      revk.setting(F("mqttport"));
    }
    else if (ret > 0 && ret != HTTP_CODE_OK && ret != HTTP_CODE_NO_CONTENT)
      revk.error(F("https"), F("Failed %d from %s"), ret, cloudhost);
    else if (ret < 0)
    {
      char err[100];
      client.getLastSSLError(err, sizeof(err));
      revk.error(F("https"), F("Failed %s: %s from %s"), https.errorToString(ret).c_str(), err,  cloudhost);
    }
    return ret;
  }

  void loop()
  {
    revk.loop();
    long now = millis();
    static long sendretry = 0;
    static long carddone = 0;
    static char tid[15];
    static long tagdone = 0;
    static long periodic = 30000;
    if (!sendbutton && !carddone && !tagdone && !sendretry && (int)(periodic - now) < 0)
    { // Perfiodic send
#ifdef USE_PN532
      if (!pn532ver && !(pn532ver = nfc.begin()))
        revk.error(F("PM532 failed"));
#endif
      periodic = now + 86400000;
      report(NULL, NULL);
    }
    if (sendbutton && (int) (sendbutton - now) < 0)
    { // Send button done
      sendbutton = 0;
#ifdef REVKDEBUG
      Serial.println(F("Send high"));
#else
      pinMode(SEND, INPUT_PULLUP);
      digitalWrite(SEND, HIGH);
#endif
    }
#ifdef TAGWAIT
#ifdef USE_PN532
    if (tagdone)
    {
      if ((int)(tagdone - now) < 0)
      { // Tag time out
        nfc.begin(); // Normal -- TODO not working as expected
        tagdone = 0;
      } else
        tag.emulate();
    }
#endif
#endif
    if (carddone && (int)(carddone - now) < 0)
    { // Card read timed out
      report(tid, NULL);
      carddone = 0;
      sendretry = 0;
      *tid = 0;
    }
    if (sendretry && (int)(sendretry - now) < 0)
    {
      presssend();
      sendretry = (now + SENDRETRY ? : 1);
    }
    while (Serial.available() > 0)
    { // Get serial
      char c = Serial.read();
      if (c >= ' ')
      { // line
        if (linep < MAX_LINE)line[linep++] = c;
      } else if (linep)
      { // end of line
        line[linep] = 0;
        linep = 0; // new line
        if (!strncmp_P(line, PSTR("NET WEIGHT"), 10))
        { // Time to send
          char *p = line + 10;
          while (*p == ' ')p++;
          if (*p == '0')
          {
            if (!carddone && (int)(now - sendlast) > SENDRETRY * 2)
            { // Unsolicited, got 0, so set up retry
              sendretry = (now + SENDRETRY ? : 1);
              carddone = (now + CARDWAIT ? : 1);
            }
          }
          else
          { // Expect some weight - i.e. >=1 kg >=1 stone >=1 lb
            if (carddone && *tid)
              report(tid, p);
            else if ((int)(now - sendlast) > SENDRETRY * 2)
            {
              report(NULL, p); // Unsolicited send
#ifdef TAGWAIT
#ifdef USE_PN532
              tagdone = (now + TAGWAIT ? : 1);
              static uint8_t ndefBuf[120];
              {
                NdefMessage message = NdefMessage();
                snprintf_P((char*)ndefBuf, sizeof(ndefBuf), PSTR("shortcuts://run-shortcut?name=Log%%20Weight&value=%s"), p);
                message.addUriRecord((char*)ndefBuf);
                int messageSize = message.getEncodedSize();
                if (messageSize < sizeof(ndefBuf))
                {
                  message.encode(ndefBuf);
                  tag.setNdefFile(ndefBuf, messageSize);
                  //tag.setUid(ESP.getChipId ());
                  tag.init();
                }
              }
#endif
#endif
              *tid = 0;
            }
            carddone = 0;
            sendretry = 0;
          }
        }
      }
    }
#ifdef USE_SPI
    static long cardcheck = 0;
    if (revk.wificonnected && !carddone && (int)(cardcheck - now) < 0)
    {
      cardcheck = now + 100;
#ifdef USE_PN532
      String id1;
      if (nfc.getID(id1))
      {
        strncpy(tid, id1.c_str(), sizeof(tid));
        debugf("Id %s", tid);
        revk.event(F("id"), F("%s"), tid);
        presssend();
        sendretry = (now + SENDRETRY ? : 1);
        carddone = (now + CARDWAIT ? : 1);
      }
#else
      if (nfc.PICC_IsNewCardPresent() && nfc.PICC_ReadCardSerial())
      {
        int n;
        for (n = 0; n < nfc.uid.size && n * 2 < sizeof(tid); n++)sprintf_P(tid + n * 2, PSTR("%02X"), nfc.uid.uidByte[n]);
        debugf("Id %s", tid);
        revk.event(F("id"), F("%s"), tid);
        presssend();
        sendretry = (now + SENDRETRY ? : 1);
        carddone = (now + CARDWAIT ? : 1);
      }
#endif
    }
#endif
  }
