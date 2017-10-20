Ticker D3_delay;
Ticker send_delay;
Ticker second_tick;

void update_time (void) {
  UnixTimestamp++;
}

void Send_Data (void) {
  send_mqtt_data = true;
  if (config_dev.ADC_EN) {
    ADC_data = analogRead (A0);
    Serial.println ("ADC data: " + (String)ADC_data);
  }
}
boolean  dht_data_receiving(){
   //http://wikihandbk.com/wiki/ESP8266
  if (config_dev.D1_H_EN || config_dev.D1_T_EN){
    humid = dht.readHumidity();
    temp = dht.readTemperature();
    if (isnan(humid) || isnan(temp)){
      Serial.println("Failed to read from DHT sensor!");
      return false;
    }
    else{
     // dtostrf(humid, 6, 2, sHumid);
     // dtostrf(temp, 6, 2, sTemp);
     // Serial.println("Humidity: " + sHumid + " %\t Temperature: " + sTemp + " *C "); 
      Serial.println("Humidity: " + (String)humid + " %\t Temperature: " + (String)temp + " *C ");
      return true;   
    }
  }
}
void D3_pressed (void) {
  D3_delay.detach ();
  if (digitalRead (D3) == 0) {
    if (!config_dev.AdminEnabled) {
      config_dev.AdminEnabled = true;
      save_cfg = true;
      Serial.println("AdminEnabled=true");
    }
    wifi_conf = true;
    Serial.println ("Start WiFi Access Point");
  } else {
    Serial.println ("RST button relesed");
    pin_cfg = true;
  }
}

void rst_btn (void) {
  detachInterrupt (D3);
  D3_delay.once (3, D3_pressed); //3 seconds wait
  Serial.println ("RST button pressed");
}

void D6_intr (void) {                     
  static unsigned long D6_millis_prev;
  Serial.println ("D6");
  if(millis()-205 > D6_millis_prev) {
    D6_millis_prev = millis();
    if (digitalRead (D6) == HIGH) phr = true;
    else phr = false;
    Serial.println ("Photoresistor: " + (String)phr);
    send_mqtt_data = true;
    }
  }


