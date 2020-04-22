/*
   uMQTTBroker demo for Arduino (C++-style)

   The program defines a custom broker class with callbacks,
   starts it, subscribes locally to anything, and publishs a topic every second.
   Try to connect from a remote client and publish something - the console will show this as well.
*/

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include "uMQTTBroker.h"
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"

/*****************************Global Variables*********************************/
#define SELMASTER_PIN    5
#define MASTER           1
#define SLAVE            0

uint8_t NowState, NewState;
uint8_t wificonfig_state = 0;
uint8_t wifi_connect[4] = {0xBB, 0xBB, 0xCC, 0xCC};
uint8_t wifi_disconnect[4] = {0xBB, 0xBB, 0xCC, 0xDD};
uint8_t mqtt_connect[4] = {0xBB, 0xBB, 0xDD, 0xDD};
uint8_t mqtt_disconnect[4] = {0xBB, 0xBB, 0xDD, 0xEE};
uint8_t wificonfig_start[4] = {0xBB, 0xBB, 0xEE, 0xEE};
uint8_t wificonfig_end[4] = {0xBB, 0xBB, 0xEE, 0xDD};
/*****************************************************************************/

/**********************Wifi config Webserver and EEPROM**********************/
const char* myap_ssid = "MyndHub";
const char* myap_pass = "11111111";
String st;
String content;
int statusCode;

ESP8266WebServer server(80);

void launchWeb(int webtype)
{
  //Serial.println("");
  //Serial.println("WiFi connected");
  //Serial.print("Local IP: ");
  //Serial.println(WiFi.localIP());
  //Serial.print("SoftAP IP: ");
  //Serial.println(WiFi.softAPIP());
  createWebServer(webtype);
  // Start the server
  server.begin();
  //Serial.println("Server started");
}

void setupAP(void)
{
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  int n = WiFi.scanNetworks();
  //Serial.println("scan done");
  if (n == 0)
  {
    //Serial.println("no networks found");
  }
  else
  {
    //Serial.print(n);
    //Serial.println(" networks found");
    for (int i = 0; i < n; ++i)
    {
      // Print myap_ssid and RSSI for each network found
      //Serial.print(i + 1);
      //Serial.print(": ");
      //Serial.print(WiFi.SSID(i));
      //Serial.print(" (");
      //Serial.print(WiFi.RSSI(i));
      //Serial.print(")");
      //Serial.println((WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*");
      delay(10);
    }
  }
  //Serial.println("");
  st = "<ol>";
  for (int i = 0; i < n; ++i)
  {
    // Print myap_ssid and RSSI for each network found
    st += "<li>";
    st += WiFi.SSID(i);
    st += " (";
    st += WiFi.RSSI(i);
    st += ")";
    st += (WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*";
    st += "</li>";
  }
  st += "</ol>";
  delay(100);
  WiFi.softAP(myap_ssid, myap_pass, 6);
  //Serial.println("softap");
  launchWeb(1);
  //Serial.println("over");
}

void createWebServer(int webtype)
{
  if ( webtype == 1 )
  {
    server.on("/", []() {
      IPAddress ip = WiFi.softAPIP();
      String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
      content = "<!DOCTYPE HTML>\r\n<html>Hello from ESP8266 at ";
      content += ipStr;
      content += "<p>";
      content += st;
      content += "</p><form method='get' action='setting'><label>myap_ssid: </label><input name='myap_ssid' length=32><input name='pass' length=64><input type='submit'></form>";
      content += "</html>";
      server.send(200, "text/html", content);
    });
    server.on("/setting", []() {
      String qsid = server.arg("myap_ssid");
      String qpass = server.arg("pass");
      if (qsid.length() > 0 && qpass.length() > 0) {
        //Serial.println("clearing eeprom");
        for (int i = 0; i < 96; ++i) {
          EEPROM.write(i, 0);
        }
        //Serial.println(qsid);
        //Serial.println("");
        //Serial.println(qpass);
        //Serial.println("");

        //Serial.println("writing eeprom myap_ssid:");
        for (int i = 0; i < qsid.length(); ++i)
        {
          EEPROM.write(i, qsid[i]);
          //Serial.print("Wrote: ");
          //Serial.println(qsid[i]);
        }
        //Serial.println("writing eeprom pass:");
        for (int i = 0; i < qpass.length(); ++i)
        {
          EEPROM.write(32 + i, qpass[i]);
          //Serial.print("Wrote: ");
          //Serial.println(qpass[i]);
        }
        EEPROM.commit();
        content = "{\"Success\":\"saved to eeprom... reset to boot into new wifi\"}";
        statusCode = 200;
        Serial.write(wificonfig_end, 4);
        delay(1000);
        ESP.restart();
      } else {
        content = "{\"Error\":\"404 not found\"}";
        statusCode = 404;
        //Serial.println("Sending 404");
      }
      server.send(statusCode, "application/json", content);
    });
  } else if (webtype == 0) {
    server.on("/", []() {
      IPAddress ip = WiFi.localIP();
      String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
      server.send(200, "application/json", "{\"IP\":\"" + ipStr + "\"}");
    });
    server.on("/cleareeprom", []() {
      content = "<!DOCTYPE HTML>\r\n<html>";
      content += "<p>Clearing the EEPROM</p></html>";
      server.send(200, "text/html", content);
      //Serial.println("clearing eeprom");
      for (int i = 0; i < 96; ++i) {
        EEPROM.write(i, 0);
      }
      EEPROM.commit();
    });
  }
}

/*****************************************************************************/

/*****************************Uart*******************************************/

uint8_t serialBuffer[64];

/**********************************WiFi******************************************/
/*
   Your WiFi config here
*/
String my_ssid;     // your network myap_ssid (name)
String my_pass; // your network password
bool WiFiAP = false;      // Do yo want the ESP as AP?

IPAddress staticIP(192, 168, 0, 40);

IPAddress gateway(192, 168, 0, 9);

IPAddress subnet(255, 255, 255, 0);
/*
   WiFi init stuff
*/
bool startWiFiClient()
{
  //Serial.println("Connecting to " + (String)myap_ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(my_ssid.c_str(), my_pass.c_str());

  int c = 0;
  //Serial.println("Waiting for Wifi to connect");
  while ( c < 20 ) {
    if (WiFi.status() == WL_CONNECTED)
    {
      //Serial.println("");
      //Serial.print("Connected, IP address: ");
      //Serial.println(WiFi.localIP());
      return true;
    }
    delay(500);
    //Serial.print(WiFi.status());
    c++;
  }
  //Serial.println("Connect timed out, opening AP");
  return false;
}
bool startWiFiClient_master()
{
  //Serial.println("Connecting to " + (String)my_ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(my_ssid.c_str(), my_pass.c_str());
  WiFi.config(staticIP, gateway, subnet);

  int c = 0;
  //Serial.println("Waiting for Wifi to connect");
  while ( c < 20 ) {
    if (WiFi.status() == WL_CONNECTED)
    {
      //Serial.println("");
      //Serial.print("Connected, IP address: ");
      //Serial.println(WiFi.localIP());
      return true;
    }
    delay(500);
    //Serial.print(WiFi.status());
    c++;
  }
  //Serial.println("Connect timed out, opening AP");
  return false;
}

/****************************************************************************************************/

/**************************************MQTT Server**************************************************/
/*
   Custom broker class with overwritten callback functions
*/
class myMQTTBroker: public uMQTTBroker
{
  public:
    virtual bool onConnect(IPAddress addr, uint16_t client_count) {
      //Serial.println(addr.toString() + " connected");
      return true;
    }

    virtual bool onAuth(String username, String password) {
      //Serial.println("Username/Password: " + username + "/" + password);
      return true;
    }

    virtual void onData(String topic, const char *data, uint32_t length) {
      char data_str[length + 1];
      os_memcpy(data_str, data, length);
      data_str[length] = '\0';

      //Serial.println("received topic '" + topic + "' with data '" + (String)data_str + "'");
    }
};

myMQTTBroker myBroker;
/****************************************************************************************************/

/****************************************MQTT client************************************************/
/************************* Adafruit.io Setup *********************************/

#define AIO_SERVER      "192.168.0.40"
#define AIO_SERVERPORT  1883                   // use 8883 for SSL
#define AIO_USERNAME    "MyndPlay"
#define AIO_KEY         ""

/************ Global State (you don't need to change this!) ******************/

// Create an ESP8266 WiFiClient class to connect to the MQTT server.
WiFiClient client;
// or... use WiFiFlientSecure for SSL
//WiFiClientSecure client;

// Setup the MQTT client class by passing in the WiFi client and MQTT server and login details.
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

/****************************** Feeds ***************************************/

// Setup a feed called 'photocell' for publishing.
// Notice MQTT paths for AIO follow the form: <username>/feeds/<feedname>
//Adafruit_MQTT_Publish photocell = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "photocell");

// Setup a feed called 'EEGdata' for subscribing to changes.
Adafruit_MQTT_Subscribe EEGdata = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/EEGdata", MQTT_QOS_1);

// Function to connect and reconnect as necessary to the MQTT server.
// Should be called in the loop function and it will take care if connecting.
void MQTT_connect() {
  int8_t ret;
  int cnt = 100;

  // Stop if already connected.
  if (mqtt.connected()) {
    return;
  }
  else
  {
    Serial.write(mqtt_disconnect, 4);
  }

  //  //Serial.print("Connecting to MQTT... ");

  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
    //    //Serial.println(mqtt.connectErrorString(ret));
    //    //Serial.println("Retrying MQTT connection in 5 seconds...");
    //    mqtt.disconnect();
    delay(5000);  // wait 5 seconds
    retries--;
    if (retries == 0) {
      // basically die and wait for WDT to reset me
      while (cnt != 0)
      {
        NewState = get_master();
        if (NewState != NowState)
        {
          ESP.restart();
        }
        cnt--;
        delay(100);
      }
      //      //Serial.println("end");
      ESP.restart();
      //      while (1);
    }
  }
  Serial.write(mqtt_connect, 4);
  //  //Serial.println("MQTT Connected!");
}

/*************************************************************************************************/
uint8_t get_master()
{
  return digitalRead(SELMASTER_PIN);
}
/*************************************************************************************************/

void setup()
{
  Serial.begin(115200);
  EEPROM.begin(512);
  delay(1000);
//  //Serial.println();
  //Serial.println();
  //Serial.print("Start");
//  delay(100);
  wificonfig_state = 0;
  pinMode(SELMASTER_PIN, INPUT);
  Serial.swap();
  delay(100);
  Serial.write(wifi_disconnect, 4);
  Serial.write(mqtt_disconnect, 4);
  Serial.write(wificonfig_end, 4);
  /***********Read ssid and psw from eeprom****************/
  String esid;
  for (int i = 0; i < 32; ++i)
  {
    esid += char(EEPROM.read(i));
  }
  //Serial.print("myap_ssid: ");
  //Serial.println(esid);
  //Serial.println("Reading EEPROM pass");
  String epass = "";
  for (int i = 32; i < 96; ++i)
  {
    epass += char(EEPROM.read(i));
  }
  //Serial.print("PASS: ");
  //Serial.println(epass);
  /**************Check ssid and psw**********************/
  if ( esid.length() > 1 )
  {
    my_ssid = esid;
    my_pass = epass;
  }
  else
  {
    Serial.write(wificonfig_start, 4);
    wificonfig_state = 1;
    setupAP();
  }
  /***************Get mode: Master or slave******************/
  NowState = get_master();

  /************************Master setting*************************/
  if (NowState == MASTER)
  {
//    Serial.print("Master");
    if (startWiFiClient_master())
    {
      Serial.write(wifi_connect, 4);
      // Start the broker
//      Serial.println("Starting MQTT broker");
      myBroker.init();
      myBroker.subscribe("#");
    }
    else
    {
      Serial.write(wificonfig_start, 4);
      wificonfig_state = 1;
      setupAP();
    }
  }
  /***********************slave setting****************************/
  else if (NowState == SLAVE)
  {
    Serial.print("Slave");
    if (startWiFiClient())
    {
      Serial.write(wifi_connect, 4);
      mqtt.subscribe(&EEGdata);
    }
    else
    {
      Serial.write(wificonfig_start, 4);
      wificonfig_state = 1;
      setupAP();
    }
  }
  /**********************************************************/
}

void loop()
{
  if (wificonfig_state == 1)  // wifi configuration mode
  {
     server.handleClient();
  }
  else
  {
    NewState = get_master();
    if (NewState != NowState)
    {
      ESP.restart();
    }
/**********************************Master process********************************/
    if (NowState == MASTER)
    {
      uint8_t len = Serial.available();
      if (len > 0)
      {
        Serial.readBytes(serialBuffer, len);
        //Serial.write(serialBuffer, len);
        myBroker.publish("MyndPlay/EEGdata", serialBuffer, len);
      }
    }
/******************************Slave process*********************************/
    else if (NowState == SLAVE)
    {
      MQTT_connect();
      Adafruit_MQTT_Subscribe *subscription;
      while ((subscription = mqtt.readSubscription(5000)))
      {
        if (subscription == &EEGdata)
        {
          Serial.write((uint8_t *)EEGdata.lastread, EEGdata.datalen);
        }
      }
    }
    delay(100);
  }
}
