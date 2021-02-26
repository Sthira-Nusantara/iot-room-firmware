#include <ESP8266WiFi.h>
#include <FastLED.h>
#include <PubSubClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <ESP8266httpUpdate.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>


// Relay Toggler Initialize
#define RELAY 16     //D0
#define IR 13        //D7
#define RELAYLAMP 12 //D6

#define NUM_LEDS 20
#define DATA_PIN 2

const String FirmwareVer = {"1.1"};
#define URL_fw_Version "https://raw.githubusercontent.com/Sthira-Nusantara/iot-room-firmware/master/version.txt"
#define URL_fw_Bin "https://raw.githubusercontent.com/Sthira-Nusantara/iot-room-firmware/master/firmware.bin"

String URL_register = "https://api.rupira.com/v2/iot/request";
String URL_authenticate = "https://api.rupira.com/v2/iot/authenticate";


const char *mqtt_server = "mqtt.rupira.com";
String MacAdd = String(WiFi.macAddress());

const char *ssid = "IOT_STHIRA";
const char *password = "IotSthiraNusantara@2712";


// ----- Basic Init
String COMPANY = "sthira";
String DEVICE = "rfid";
String UNUM = MacAdd;

// ----- Prefix init
String prefix = COMPANY + "/" + DEVICE + "/" + UNUM;

String toggle = prefix + "/toggle";
String openSubs = prefix + "/toggle/+";
String failureSubs = prefix + "/bellRing";
String successSubs = prefix + "/opendoor";
String testSubs = prefix + "/test";

// Door Condition
int locked = 0;
int updateLocked = 0;

unsigned long previousMillis = 0;
const long interval = 10000;

WiFiUDP ntpUDP;

// NTP UTC GMT+7 (Offset) (7 * 3600s) = 25200
const long utcOffsetInSeconds = 25200;

char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

WiFiClient espClient;

PubSubClient mqttClient(espClient);


WiFiClientSecure deviceClient;

CRGB leds[NUM_LEDS];

void changeColor(CRGB color)
{
  for (int i = 0; i <= NUM_LEDS; i++)
  {
    leds[i] = color;
  }
  FastLED.show();
}

void blinkingColor(CRGB color, int times, int ms)
{
  for (int i = 0; i <= times; i++)
  {
    changeColor(color);
    delay(ms);
    changeColor(CRGB::Black);
    delay(ms);
  }

  changeColor(color);
}

void registerDevice() {

  deviceClient.setInsecure();
  HTTPClient https;

  https.begin(deviceClient, URL_register.c_str()); //HTTP
  https.addHeader("Content-Type", "application/json");

  Serial.println("Try to register device");
  int httpCode = https.POST("{\"UNUM\": \"" + MacAdd + "\",\"requestCategory\":\"Access\"}");

  if (httpCode > 0) {
    // HTTP header has been send and Server response header has been handled
    Serial.printf("[HTTP] POST... code: %d\n", httpCode);

    // file found at server
    if (httpCode == HTTP_CODE_OK)
    {
      const String& payload = https.getString();
      Serial.println("received payload:\n<<");
      Serial.println(payload);
      Serial.println(">>");
    }
    else
    {
      const String& payload = https.getString();
      Serial.println("received payload:\n<<");
      Serial.println(payload);
      Serial.println(">>");
    }
  } else {
    Serial.printf("[HTTP] POST... failed, error: %s\n", https.errorToString(httpCode).c_str());
  }

  https.end();

}

void FirmwareUpdate()
{
  Serial.println("Preparing update firmware");

  if ((WiFi.status() == WL_CONNECTED)) {

    deviceClient.setInsecure();

    HTTPClient https;

    Serial.print("[HTTPS] begin...\n");
    if (https.begin(deviceClient, URL_fw_Version)) {  // HTTPS
//      https.addHeader("Authorization", "Bearer affd6c0995d4b701ce6e67b2531eb368177f3e7f");


      Serial.print("[HTTPS] GET...\n");
      // start connection and send HTTP header
      int httpCode = https.GET();

      // httpCode will be negative on error
      if (httpCode > 0) {
        // HTTP header has been send and Server response header has been handled
        Serial.printf("[HTTPS] GET... code: %d\n", httpCode);

        // file found at server
        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
          String payload = https.getString();
          payload.trim();

          Serial.print("Latest Version: ");
          Serial.println(payload);

          if (payload.equals(FirmwareVer) )
          {
            Serial.println("Device already on latest firmware version");
          }
          else
          {
            Serial.println("New firmware detected");
            ESPhttpUpdate.setLedPin(LED_BUILTIN, LOW);

            Serial.println("Device ready to update");

            t_httpUpdate_return ret = ESPhttpUpdate.update(deviceClient, URL_fw_Bin);

            switch (ret) {
              case HTTP_UPDATE_FAILED:
                Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
                break;

              case HTTP_UPDATE_NO_UPDATES:
                Serial.println("HTTP_UPDATE_NO_UPDATES");
                break;

              case HTTP_UPDATE_OK:
                Serial.println("Update Done well");
                Serial.println("HTTP_UPDATE_OK");
                break;
            }
          }

        }
      } else {
        Serial.printf("[HTTPS] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
      }

      https.end();
    } else {
      Serial.printf("[HTTPS] Unable to connect\n");
    }
  }
}


String scanNetwork(const char *ssid)
{
  Serial.println("scan start");

  // WiFi.scanNetworks will return the number of networks found
  int n = WiFi.scanNetworks();
  Serial.println("scan done");
  if (n == 0)
  {
    Serial.println("no networks found");
  }
  else
  {
    Serial.print(n);
    Serial.println(" networks found");
    int myVal = WiFi.RSSI(0);
    String BSSIDvalue;
    for (int i = 0; i < n; ++i)
    {
      Serial.print(WiFi.SSID(i));
      Serial.print(" - ");
      Serial.print(WiFi.BSSIDstr(i));
      Serial.print("(");
      Serial.print(WiFi.RSSI(i));
      Serial.print(") - ");
      Serial.println(WiFi.channel(i));
      if (WiFi.SSID(i) == ssid)
      {
        if (WiFi.RSSI(i) > myVal)
        {
          myVal = WiFi.RSSI(i);
          BSSIDvalue = WiFi.BSSIDstr(i);
        }
      }
    }

    Serial.println();
    Serial.println(BSSIDvalue);

    return BSSIDvalue;
  }

  return "";
}

int getChannel(String bssid)
{
  Serial.println("Find BSSID channel");

  // WiFi.scanNetworks will return the number of networks found
  int n = WiFi.scanNetworks();
  Serial.println("scan network for channel done");
  if (n == 0)
  {
    Serial.println("no networks found");
  }
  else
  {
    for (int i = 0; i < n; ++i)
    {
      if (WiFi.BSSIDstr(i) == bssid)
      {
        return WiFi.channel(i);
      }
    }
  }
  return 0;
}

void setup_wifi()
{

  WiFi.mode(WIFI_STA);
  delay(10);
  // We start by connecting to a WiFi network

  String BSSIDnetwork = scanNetwork(ssid);
  int chan = getChannel(BSSIDnetwork);

  int n = BSSIDnetwork.length();

  char char_array[n + 1];

  strcpy(char_array, BSSIDnetwork.c_str());

  uint16_t num = strtoul(char_array, nullptr, 16);

  uint8_t bssid[sizeof(num)];
  memcpy(bssid, &num, sizeof(num));

  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  Serial.print("Connecting to BSSID ");
  Serial.println(BSSIDnetwork);
  Serial.print("Connecting to Channel ");
  Serial.println(chan);

  WiFi.begin(ssid, password, chan, bssid);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    changeColor(CRGB::Red);
    Serial.print(".");
    delay(500);
    changeColor(CRGB::Black);
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++)
  {
    Serial.println((char)payload[i]);
  }

  if (String(topic).indexOf(testSubs) >= 0)
  {
    if (payload[0] == '1')
    {
      blinkingColor(CRGB::Blue, 3, 250);
    }
  }

  if (String(topic).indexOf(toggle) >= 0)
  {
    if (locked == HIGH)
    {
      if (payload[0] == '1')
      {
        deviceClient.setInsecure();
        HTTPClient https;

        https.begin(deviceClient, URL_authenticate.c_str()); //HTTP
        https.addHeader("Content-Type", "application/json");

        Serial.println("Authenticating...");
        int httpCode = https.POST("{\"request\": \"" + String(topic) + "\"}");

        if (httpCode > 0) {
          // HTTP header has been send and Server response header has been handled
          Serial.printf("[HTTP] POST... code: %d\n", httpCode);

          // file found at server
          if (httpCode == HTTP_CODE_OK)
          {
            const String& payload = https.getString();
            Serial.println("received payload:\n<<");
            Serial.println(payload);
            Serial.println(">>");


            DynamicJsonDocument doc(1024);

            deserializeJson(doc, payload);

            const char* hasil = doc["data"];

            if (String(hasil) == "true")
            {
              // Open Door
              digitalWrite(RELAY, HIGH);
              Serial.println("Door Open");
              changeColor(CRGB::Green);

              delay(5000);

              // Close Door
              digitalWrite(RELAY, LOW);
              Serial.println("Door Close");
              changeColor(CRGB::Red);

              delay(1000);
            }
          }
          else
          {
            const String& payload = https.getString();
            Serial.println("error payload:\n<<");
            Serial.println(payload);
            Serial.println(">>");

            Serial.println("Sorry, you are not authenticated");

            blinkingColor(CRGB::Red, 3, 250);

            delay(1000);

          }
        } else {
          Serial.printf("[HTTP] POST... failed, error: %s\n", https.errorToString(httpCode).c_str());
        }

        https.end();
      }
    }
    else
    {
      Serial.println("Sorry, There is someone in this room");
      blinkingColor(CRGB::Red, 3, 250);

      delay(1000);
    }
  }

  Serial.println();
  //  return;
}


boolean reconnect()
{
  changeColor(CRGB::Magenta);
  // Loop until we're reconnected
  Serial.print("Attempting MQTT connection...");
  // Create a random client ID
  String clientId = "ESP8266Client-";
  clientId += String(random(0xffff), HEX);
  // Attempt to connect
  if (mqttClient.connect(clientId.c_str()))
  {
    Serial.println("connected");
    mqttClient.publish("connected", MacAdd.c_str());

    //      Your Subs
    mqttClient.subscribe(successSubs.c_str());
    mqttClient.subscribe(failureSubs.c_str());
    mqttClient.subscribe(openSubs.c_str());
    mqttClient.subscribe(testSubs.c_str());
    digitalWrite(RELAY, LOW);
    digitalWrite(RELAYLAMP, LOW);
  }
  else
  {
    Serial.print("failed, rc=");
    Serial.print(mqttClient.state());
    Serial.println(" try again in 5 seconds");
    // Wait 5 seconds before retrying
    delay(5000);
  }

  return mqttClient.connected();
}


void setup()
{
  Serial.begin(115200);

  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);

  //  Testing LED

  delay(500);

  changeColor(CRGB::Red);
  delay(500);

  changeColor(CRGB::Blue);
  delay(500);

  changeColor(CRGB::Green);
  delay(500);

  changeColor(CRGB::Magenta);
  delay(500);

  changeColor(CRGB::Moccasin);
  delay(500);

  setup_wifi();

  registerDevice();
  pinMode(RELAY, OUTPUT);
  pinMode(RELAYLAMP, OUTPUT);
  pinMode(IR, INPUT);

  timeClient.begin();

  mqttClient.setServer(mqtt_server, 1883);
  mqttClient.setCallback(callback);
}

void loop()
{

  if (WiFi.status() != WL_CONNECTED)
  {
    leds[0] = CRGB::Red;
    FastLED.show();

    setup_wifi();
  }


  timeClient.update();

  int thisHour = timeClient.getHours();
  int thisMinute = timeClient.getMinutes();
  int thisSecond = timeClient.getSeconds();

  if (thisHour == 0 && thisMinute == 1 && thisSecond == 0)
  {
    Serial.println("Checking firmware update");
    FirmwareUpdate();
  }

  if (thisHour == 1 && thisMinute == 1 && thisSecond == 0)
  {
    Serial.println("Checking firmware update");
    FirmwareUpdate();
  }

  if (thisHour == 2 && thisMinute == 1 && thisSecond == 0)
  {
    Serial.println("Checking firmware update");
    FirmwareUpdate();
  }

  if (thisHour == 3 && thisMinute == 1 && thisSecond == 0)
  {
    Serial.println("Checking firmware update");
    FirmwareUpdate();
  }

  if (!mqttClient.connected())
  {
    Serial.println("MQTT is NOT connected");
    int lastReconnectAttempt = 0;
    long now = millis();
    if (now - lastReconnectAttempt > 5000)
    {
      lastReconnectAttempt = now;
      // Attempt to reconnect
      if (reconnect())
      {
        lastReconnectAttempt = 0;
      }
    }
  }
  else
  {
    // Client connected
    locked = digitalRead(IR);

    if (locked == LOW)
    {
      digitalWrite(RELAYLAMP, LOW);
      Serial.println("Terkunci");
      Serial.println(locked);


      changeColor(CRGB::Red);

      updateLocked = locked;
    }
    else
    {

      Serial.println("Tidak Terkunci");
      Serial.println(locked);

      changeColor(CRGB::Blue);

      unsigned long currentMillis = millis();
      if ((currentMillis - previousMillis) >= interval)
      {
        previousMillis = currentMillis;

        digitalWrite(RELAYLAMP, HIGH);
      }
      updateLocked = locked;
    }
    if (locked != updateLocked)
    {

    }

    mqttClient.loop();
  }

  delay(1000);
}
