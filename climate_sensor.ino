//version 1.3

#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include <ESPAsyncWebServer.h>
#include <Ticker.h>
#include <EEPROM.h>
#include "DHT.h"
#include <time.h>
#include "FS.h"

Ticker check_tick;

#include "structures.h"
#include "eeprom_conf.h"
#include "Pages_html.h"
#include "server_handlers.h"
#include "wifi_conf.h"
#include "pin_intr.h"


void setup (void) {
  Serial.begin (115200);
  dht.begin();
  delay (100);
  Serial.println ();
  Serial.println ("Starting ES8266");
  ReadConfig ();
  WiFi.mode (WIFI_OFF);
  WiFi.setPhyMode (WIFI_PHY_MODE_11G);
  WiFi.macAddress (MAC);
  Serial.println ("Serial number: " + (String)MAC [0] + ":" + (String)MAC [1] + ":" + (String)MAC [2] + ":" + (String)MAC [3] + ":" + (String)MAC [4] + ":" + (String)MAC [5]);
  //INIT RESSET BUTTON
  pinMode (D3, INPUT_PULLUP);
  attachInterrupt (D3, rst_btn, FALLING);


  pinMode (D6, INPUT_PULLUP); //phr
  pinMode (D8, OUTPUT); //external output


  if (config_dev.D0_EN) {
    pinMode (D0, OUTPUT);
    digitalWrite (D0, LOW);
  }
  else pinMode (D0, INPUT);

  if (config_dev.D4_EN) {
    pinMode (D4, OUTPUT);
    digitalWrite (D4, HIGH);
  }
  else pinMode (D4, INPUT);
  if (config_dev.D6_EN) {
    if (digitalRead (D6) == HIGH) phr = true;
    else phr = false;
    attachInterrupt (D6, D6_intr, CHANGE);
  }
  else detachInterrupt (D6);
  if (config_dev.D8_EN) {
    pinMode (D8, OUTPUT);
    digitalWrite (D8, LOW);
  }
  else pinMode (D8, INPUT);

  second_tick.attach (1, update_time); //Increment time every 1 second
 
}

void loop (void) {

  //Start WiFi configuration
  if (config_dev.AdminEnabled) {
    if ((wifi_status == OFF_MODE) or (wifi_conf == true)) {
      //SET WiFi as Access Point
      WiFi.mode (WIFI_AP_STA);
      WiFi.softAPConfig (ServerIP, ServerGW, ServerSN);
      WiFi.softAP (ACCESS_POINT_NAME, ACCESS_POINT_PASSWORD);
      ConfigureWifi ();
      Serial.println ("Access Point started");
      ConfigureServer ();
      WiFi.scanNetworks (true);
      wifi_status = AP_MODE;
      wifi_conf = false;
    }

    if (save_cfg) {
      WriteConfig ();
      save_cfg = false;
    }

    if (pin_cfg) {
      if (!config_dev.AdminEnabled) attachInterrupt (D3, rst_btn, FALLING);
      if (config_dev.D0_EN) {
        pinMode (D0, OUTPUT);
        digitalWrite (D0, LOW);
      }
      else pinMode (D0, INPUT);
      if (config_dev.D4_EN) {
        pinMode (D4, OUTPUT);
        digitalWrite (D4, HIGH);
      }
      else pinMode (D4, INPUT);
      if (config_dev.D6_EN) {
        if (digitalRead (D6) == HIGH) phr = true;
        else phr = false;
        attachInterrupt (D6, D6_intr, CHANGE);
      }
      else detachInterrupt (D6);
      if (config_dev.D8_EN) {
        pinMode (D8, OUTPUT);
        digitalWrite (D8, LOW);
      }
      else pinMode (D8, INPUT);
      pin_cfg = false;
    }

    if (wifi_cfg) {
      ConfigureWifi ();
      wifi_cfg = false;
    }

  }
  else {
    if ((wifi_status == OFF_MODE) or (wifi_conf == true)) {
      //SET WiFi in Station Mode
      if (wifi_status == AP_MODE) delay (5000);
      if (server_status == true) server.reset ();
      WiFi.mode (WIFI_STA);
      ConfigureWifi ();
      wifi_status = STA_MODE;
      wifi_conf = false;
      Serial.println ("Station mode started");
    }
  }

  //----------Create mqtt settings---------------
  if (connect_mqtt) {
    Serial.println ("Connected to mqtt");
    
    //----------Set the interval between sending messages---------------
    if((config_dev.MQTT_interval != delay2send) && (config_dev.MQTT_interval < 1000) && (config_dev.MQTT_interval > 1)) { //Change if the value has changed
      send_delay.detach();
      send_delay.attach (config_dev.MQTT_interval, Send_Data);
      Serial.println ("Delay seconds between sends: "+  (String)config_dev.MQTT_interval);
      delay2send = config_dev.MQTT_interval;
    } else if (config_dev.MQTT_interval <=0){
       Serial.println ("Error value: interval is less than zero ");
       config_dev.MQTT_interval = 10;
    }
    
    //----------Choice of security implementation client---------------
    if (config_dev.SSL) {
      
       //----------File certificate reading----------------
      if (!SPIFFS.begin()) {
        Serial.println("Failed to mount file system");
        return;
      }
      Serial.print("Heap: "); Serial.println(ESP.getFreeHeap());
      
      File cert = SPIFFS.open("/snslog_client.der", "r"); //replace ca eith your uploaded file name
      if (!cert) 
        Serial.println("Failed to open cert ");
      else
        Serial.println("Success to open cert");
        
      //----------Create a secure client to MQTT----------------
      mqtt_client.setClient(secure_client); //Create mqtt secure client
      if(secure_client.loadCACert(cert))  //Checking a certificate from a file
        Serial.println("Cert loaded");
      else
        Serial.println("Cert failed");
        Serial.print("Heap: "); Serial.println(ESP.getFreeHeap()); 
    } else {
      //----------Create a not secure client to MQTT----------------
      mqtt_client.setClient(WF_client);
    }
    
    //----------Connecting to MQTT server---------------
    if (!mqtt_client.connected ()) {
       mqtt_client.setServer (config_dev.MQTT_server.c_str(), config_dev.MQTT_port);
      Serial.println ("Connecting to MQTT server");
      if (mqtt_client.connect (config_dev.DeviceName.c_str(), config_dev.MQTT_login.c_str(), config_dev.MQTT_password.c_str())) {
        Serial.println ("Connected to MQTT server");
        send_delay.attach (DELAY2SEND, Send_Data);
        if ((config_dev.timestamp) or (config_dev.D0_EN) or (config_dev.D4_EN) or (config_dev.D8_EN)) {
          mqtt_client.setCallback (MQTT_subscribe);
          if (config_dev.timestamp) mqtt_client.subscribe (config_dev.time_topic.c_str());
          else mqtt_client.unsubscribe (config_dev.time_topic.c_str());
          if (config_dev.D0_EN) mqtt_client.subscribe (config_dev.D0_topic.c_str());
          else mqtt_client.unsubscribe (config_dev.D0_topic.c_str());
          if (config_dev.D4_EN) mqtt_client.subscribe (config_dev.D4_topic.c_str());
          else mqtt_client.unsubscribe (config_dev.D4_topic.c_str());
          if (config_dev.D8_EN) mqtt_client.subscribe (config_dev.D8_topic.c_str());
          else mqtt_client.unsubscribe (config_dev.D8_topic.c_str());
          mqtt_status = true;
        } else {
          mqtt_client.unsubscribe (config_dev.time_topic.c_str());
          mqtt_client.unsubscribe (config_dev.D0_topic.c_str());
          mqtt_client.unsubscribe (config_dev.D4_topic.c_str());
          mqtt_client.unsubscribe (config_dev.D8_topic.c_str());
        }
      } else {
        mqtt_client.disconnect ();
        Serial.println ("Can not connect to MQTT server");
        mqtt_status = false;
      }
    }
    connect_mqtt = false;
  }

  if (mqtt_client.connected ()) {
    if (send_mqtt_data) {
      char msg[30];
      if (config_dev.ADC_EN) {
        if (config_dev.timestamp) sprintf (msg, "%u : %u", UnixTimestamp, ADC_data);
        else sprintf (msg, "%u", ADC_data);
        Serial.println ("Send ADC data: " + (String)msg);
        mqtt_client.publish (config_dev.ADC_topic.c_str(), msg); //send to MQTT server data
      }
      if (config_dev.D6_EN) {
        if (config_dev.timestamp) sprintf (msg, "%u : %u", UnixTimestamp, phr);
        else sprintf (msg, "%u", phr);
        Serial.println ("Send photoresistor state: " + (String)msg);
        mqtt_client.publish (config_dev.D6_topic.c_str(), msg); //send to MQTT server data
      }
      if (dht_data_receiving()){//РµСЃР»Рё РґР°РЅРЅС‹Рµ РµСЃС‚СЊ, С‚Рѕ РѕС‚РїСЂР°РІРёС‚СЊ РёС…
        if (config_dev.D1_T_EN) {
          if (config_dev.timestamp) {dtostrf (temp, 3, 2, sTemp); sprintf (msg, "%u : %s", UnixTimestamp,sTemp);}
          else dtostrf (temp, 3, 2, msg);
          Serial.println ("Send temperature data: " + (String)msg);
          mqtt_client.publish (config_dev.D1_T_topic.c_str(), msg); //send to MQTT server data
        }
        if (config_dev.D1_H_EN) {
          if (config_dev.timestamp) {dtostrf (humid, 3, 2, sHumid); sprintf (msg, "%u : %s", UnixTimestamp, sHumid);}
          else dtostrf (humid, 3, 2, msg);
          Serial.println ("Send humidity data: " + (String)msg);
          mqtt_client.publish (config_dev.D1_H_topic.c_str(), msg); //send to MQTT server data
        }
      }
      
      send_mqtt_data = false;
    }
    mqtt_client.loop (); // This allows the client to maintain the connection and check for any incoming messages.

  } else if (!b_check_tick) { //
    
      //----------WiFi or MQTT disconnected ---------------
      if (WiFi.status() == WL_CONNECTED) {
        if (mqtt_status) {
          Serial.println ("m_Disconnected from MQTT server");
       }
      connect_mqtt = true;
    }else{
       //----------Disconnected from WIFI ---------------
       Serial.println ("m_Disconnected from WiFi ");
       wifi_cfg = true;
       check_tick.attach (10, statusRequestWifi);  //Check WiFi 
    }
    b_check_tick = true;                  
  }
}

