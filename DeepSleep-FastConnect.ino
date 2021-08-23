/*
  ESP DeepSleep and fast reconnect to WiFi Demo

  I found this series of articles immensely helpful in comming up with this solution:
  
  https://www.bakke.online/index.php/2017/05/21/reducing-wifi-power-consumption-on-esp8266-part-1/

  I also took ideas from the ESP8266 LowPowerDemo sketch.

  For the fastest connention times use a static IP address, and set the gateway and netmask.
  Waiting for an IP address to be assigned by DHCP ca add .5 to 1 additional second when connecting.
  The biggest time save though is saving the mac address and channel of the WiFi Access Point in RTC
  memory.  This eliminates the need for the esp to do a network scan befor it can begin the connection.

  Make sure your board is set up to wake from DeepSleep! For example...
  D1 Mini > connect D0 to RST.
  ESP12-F > connect GPIO16 to RST
  ESP01 > see here: https://blog.enbiso.com/post/esp-01-deep-sleep/ to make the necessary modifications.
          for this mod I personnaly like to use conductive paint and a sharp needle to apply it...
        
*/

#include <ESP8266WiFi.h>

const char* AP_SSID = "CHANGE ME";  // your router's SSID here
const char* AP_PASS = "CHANGE ME";  // your router's password here
// These are an example! Change to match your own network.
// This x.x.32.1-254 range is not likely to clash with router assigned addresses.
IPAddress staticIP(192, 168, 32, 101);
// This MUST be your routers address. (this is a very common one)
IPAddress gateway(192, 168, 0, 1);
// Usually this would be 255, 255, 255, 0. but doing it this way we can take an
// address safely outside the ones assigned by router, but still reach network servers.
IPAddress subnet(255, 255, 0, 0);
IPAddress dns1(192, 168, 0, 1);   // router is primary DNS
IPAddress dns2(9, 9, 9, 9);       // fallback DNS

struct nv_s {
  uint32_t crc;   // Stored outside of the rtcData struct so we don't have to worry about offset when we calculate crc32 of the data.
  struct {
    // Add anything here that you want to save in RTC_USER_MEM. MUST be 4-byte aligned for crc to work!
    uint32_t rstCount;  // stores the Deep Sleep reset count
    uint32_t noWifi;     // stores the number of consecutive missed connections
    uint32_t channel;    // stores the wifi channel for faster no-scan connetion
    uint32_t bssid[6];   // stores mac address of AP for fast no-san connection
  } rtcData;
};

static nv_s* nv = (nv_s*)RTC_USER_MEM; // user RTC RAM area
uint32_t resetCount = 0;     // keeps track of the number of Deep Sleep tests / resets
uint32_t wifiMissed;         // keeps track of the number of consecutive missed wifi conections
uint32_t wifiChan;          // wifi channel needed for fast reconnect.
uint8_t wifiID[6];          // wifi bssid for fast reconnect.
uint32_t naptime = 30e6;    // how long to sleep. 30 seconds default for this demo.

void setup() {
  uint32_t setupStart = millis();
  bool useRTC = rtcValid();
  String resetCause = ESP.getResetReason();
  Serial.begin(115200);
  delay(2000);
  Serial.println();
  delay(1);
  if ((resetCause == "Deep-Sleep Wake") && (useRTC == true)) {
    resetCount = nv->rtcData.rstCount;  // read the previous reset count
    resetCount++;
    nv->rtcData.rstCount = resetCount; // update the reset count and save to rtc
    wifiMissed = nv->rtcData.noWifi;
    for (int mem = 0; mem < 6; mem++) {
      wifiID[mem] = nv->rtcData.bssid[mem];
    } 
  } else {
    // reset by some condition other than "Deep-Sleep Wake" or RTC not valid so don't try fast reconnect.
    resetCount++;
    nv->rtcData.rstCount = resetCount; // update the reset count and save to rtc
    wifiMissed = nv->rtcData.noWifi;
  }
  updateRTCcrc();
  delay(1);
  Serial.println(resetCause);
  Serial.print("boot time (ms): ");
  Serial.println(setupStart);
  delay(1);
  uint32_t startWifi = millis();
  uint32_t GiveUp = startWifi + 10000;   // 10 seconds max before giving up on wifi connection.
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);   // Dont's save WiFiState to (slow) flash we will store it in RTC RAM later.
  WiFi.config(staticIP, gateway, subnet);  // Comment this line out if you want to wait for router to assign IP address.
  if ((useRTC == true ) && (wifiMissed == 0)) {   
    WiFi.begin( AP_SSID, AP_PASS, nv->rtcData.channel, wifiID, true );
    Serial.println();
    Serial.print("Reconnecting to channel ");
    Serial.print(nv->rtcData.channel);
    Serial.print(" on network ");
    for (int mem = 0; mem < 6; mem++ ) {
      Serial.print(wifiID[mem], HEX);
    }
  } else {
    WiFi.begin(AP_SSID, AP_PASS);
    Serial.println();
    Serial.print("Starting new network connection");
  }
  while ( (WiFi.status() != WL_CONNECTED) && (millis() < GiveUp) ) {
    delay(50);
    Serial.print(".");
  }
  if (WiFi.status() != WL_CONNECTED) {
    wifiMissed = nv->rtcData.noWifi;  // read the previous wifi fail count
    wifiMissed++;
    nv->rtcData.noWifi = wifiMissed; // update the missed connection count and save to rtc
    Serial.println();
    Serial.println("WiFi connect failed. Retry in 60 Seconds.");
    updateRTCcrc();
    ESP.deepSleep(naptime);    // Try again in 30 seconds.
    delay(1);
  } else {
    Serial.println();
    Serial.print("Wifi connect took (ms): ");
    Serial.println(millis() - startWifi);
    nv->rtcData.channel = WiFi.channel();
    nv->rtcData.noWifi = 0;   // reset missed connection counter.
    uint8_t* bss_id = WiFi.BSSID();
    for (unsigned int len = 0; len < WL_MAC_ADDR_LENGTH; len++ ) {
      nv->rtcData.bssid[len] = bss_id[len];
    }
    updateRTCcrc();
  }
}

void loop() {
  Serial.print("Wake up counter = ");
  Serial.println(resetCount);
  if (nv->rtcData.noWifi != 0) {
    Serial.print("Missed the last " );
    Serial.print(nv->rtcData.noWifi);
    Serial.println(" wireless connections!"); 
  }
  Serial.print("Connected to ");
  Serial.print(WiFi.BSSIDstr());
  Serial.print("  -- Channel ");
  Serial.println(WiFi.channel());
  Serial.print("WiFi Signal strength: ");
  Serial.println(WiFi.RSSI());
  WiFi.disconnect( true );
  delay( 1 );
  WiFi.mode( WIFI_OFF );
  Serial.println("going to sleep for 30 seconds...");
  ESP.deepSleep(naptime);
  delay(1);
}

/* 
  CRC support functions.
*/
 
void updateRTCcrc() {  // updates the reset count CRC
  nv->crc = crc32((uint8_t*)&nv->rtcData, sizeof(nv->rtcData));
  if (!rtcValid()){
    //Serial.println("Failed to update RTC RAM");
  }
}

bool rtcValid() {
  bool valid;
  // Calculate the CRC of RTC memory
  uint32_t crc = crc32((uint8_t*)&nv->rtcData, sizeof(nv->rtcData));
  if( crc != nv->crc ) {
    //Serial.println("WARNING: rtcRAM failed crc validation!");
    valid = false;
  } else {
    valid = true;
  }
  return valid;
}

uint32_t crc32(const uint8_t *data, size_t length) {
  uint32_t crc = 0xffffffff;
  while (length--) {
    uint8_t c = *data++;
    for (uint32_t i = 0x80; i > 0; i >>= 1) {
      bool bit = crc & 0x80000000;
      if (c & i) {
        bit = !bit;
      }
      crc <<= 1;
      if (bit) {
        crc ^= 0x04c11db7;
      }
    }
  }
  return crc;
}
