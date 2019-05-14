// Marsden Scales
// ESP-01 based for simple reporting of weight on SEND button
// Option to force send using cmnd/app/host/send
// ESP-12F based for use with MFRC522 to send on card read
// Reporting via MQTT (note, this expected to be local and so non TLS)
// Reporting via https to a server (CA by Let's Encrypt)

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

#define USE_PN532 // Use PN532 ratehr than MFRC522

#define SENDRETRY 1000  // Re-press send if no weight yet
#define CARDWAIT 20000  // Wait for weight after getting card

#include <ESP8266RevK.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266TrueRandom.h>

ESP8266RevK revk(__FILE__, __DATE__ " " __TIME__);

#ifdef ARDUINO_ESP8266_NODEMCU
#define USE_SPI
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
#include "PN532.h"
  PN532_SPI pn532spi(SPI, SS);
  PN532 nfc(pn532spi);
#else
#include <MFRC522.h>
  MFRC522 nfc(SS, RST); // Instance of the class
#endif
#endif

  void pressend();

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
    return false;
  }

  void setup()
  {
#ifdef REVKDEBUG
    Serial.println("Started " __FILE__);
#else
    Serial.begin(9600); // Marsden talks at 9600 Baud
    digitalWrite(SEND, HIGH);
    pinMode(SEND, INPUT);
#endif
#ifdef USE_SPI
    SPI.begin(); // Init SPI bus
#ifdef USE_PN532
    nfc.begin();
    SPI.setFrequency(100000);
    if (!nfc.getFirmwareVersion())
      debug("Failed PN532");
    nfc.setPassiveActivationRetries(1);
    nfc.SAMConfig();
#else
    nfc.PCD_Init(); // Init MFRC522
#endif
#endif
  }

#define MAX_LINE 100
  char line[MAX_LINE + 1];
  int linep = 0;

  unsigned sendbutton = 0; // Signed to allow for wrap
  void presssend()
  {
    sendbutton = (millis() + 250 ? : 1);
#ifdef REVKDEBUG
    Serial.println("Send low");
#else
    digitalWrite(SEND, LOW);
    pinMode(SEND, OUTPUT);
#endif
  }

  int report(char *id, char *weight)
  {
    if (!cloudpass)
    { // We need a password - make one - store in flash - so we use same every time
      int i;
      char pass[33];
      for (i = 0; i <  sizeof(pass) - 1; i++)pass[i] = 'A' + ESP8266TrueRandom.random(26);
      pass[i] = 0; // End
      revk.setting(F("cloudpass"), pass); // save
    }
    if (weight && id)
      revk.event(F("idweight"), F("%02X%02X%02X%02X %s"), id[0], id[1], id[2], id[3], weight);
    else if (weight)
      revk.event(F("weight"), F("%s"), weight);
    if (!cloudhost)cloudhost = "weigh.me.uk";
    // Post
    char url[500];
    int m = sizeof(url) - 1, p = 0;
    if (p < m)p += snprintf_P(url + p, m - p, PSTR("/weighin.cgi?version=%s"), revk.appver);
    if (p < m)p += snprintf_P(url + p, m - p, PSTR("&scales=%06X"), ESP.getChipId());
    if (p < m && cloudpass)p += snprintf_P(url + p, m - p, PSTR("&auth=%s"), cloudpass); // Assume no special characters
    if (p < m && weight)p += snprintf_P(url + p, m - p, PSTR("&weight=%s"), weight);
    if (p < m && id)p += snprintf_P(url + p, m - p, PSTR("&id=%s"), id);
    url[p] = 0;
    for (p = 0; url[p]; p++)if (url[p] == ' ')url[p] = '+';
    // Note, always https
    WiFiClientSecure client;
    revk.clientTLS(client);
    HTTPClient https;
    https.begin(client, cloudhost, 443, url, true);
    int ret = https.GET();
    https.end();
    if (ret == HTTP_CODE_UPGRADE_REQUIRED)revk.ota(); // Upgrade required: New firmware required
    if (ret > 0 && ret != HTTP_CODE_OK && ret != HTTP_CODE_NO_CONTENT)
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
    static long sendretry = 0;
    static long carddone = 0;
    static char tid[15];
    if (carddone && (int)(carddone - now) < 0)
    { // Card read timed out
      report(tid, NULL);
      carddone = 0;
      sendretry = 0;
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
          if (*p != '0')
          { // Expect some weight - i.e. >=1 kg >=1 stone >=1 lb
            if (carddone)
              report(tid, p);
            else
              report(NULL, p);
            carddone = 0;
            sendretry = 0;
          }
        }
      }
    }
#ifdef USE_SPI
    static long cardcheck = 0;
    if (revk.mqttconnected && !carddone && (int)(cardcheck - now) < 0)
    {
      cardcheck = now + 100;
      byte uid[7], uidlen = 0;
#ifdef USE_PN532
      if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidlen))
#else
      if (nfc.PICC_IsNewCardPresent() && nfc.PICC_ReadCardSerial())
#endif
      {
#ifndef USE_PN532
        if (nfc.uid.size <= sizeof(uid))
          memcpy(uid, nfc.uid.uidByte, uidlen = nfc.uid.size);
#endif
        int n;
        for (n = 0; n < uidlen && n * 2 < sizeof(tid); n++)sprintf_P(tid + n * 2, PSTR("%02X"), uid[n]);
        debugf("Id %s", tid);
        revk.event(F("id"), F("%s"), tid);
        presssend();
        sendretry = (now + SENDRETRY ? : 1);
        carddone = (now + CARDWAIT ? : 1);
      }
    }
#endif
  }
