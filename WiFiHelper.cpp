#include <Arduino.h>
#include <WiFi.h>
#include "WiFiHelper.h"

bool has_text(String needle, String haystack) {
  int foundpos = -1;
  for (int i = 0; i <= haystack.length() - needle.length(); i++) {
    if (haystack.substring(i, needle.length() + i) == needle) {
      return true;
    }
  }
  return false;
}

bool wifiConnect(int num_networks, Network *networks) {
    // Set WiFi to station mode and disconnect from an AP if it was previously connected
    // delay(3000);
    WiFi.mode(WIFI_STA);

    Serial.println("setup done");
    Serial.println("scan start");

    // WiFi.scanNetworks will return the number of networks found
    int num_wifi = WiFi.scanNetworks();
    if (num_wifi == 0) {
        Serial.println("no networks found");
    } else {
        Serial.println("scan found the following networks:");
        for (int i = 0; i < num_wifi; ++i) {
            Serial.print("\t");
            Serial.println(WiFi.SSID(i));
        }

        for (int n = 0; n < num_networks; n++) {
            for (int i = 0; i < num_wifi; ++i) {
                // look if one of the found wifis matches one of our networkds
                if(! has_text(WiFi.SSID(i), networks[n].ssid))
                    continue;

                Serial.printf("Trying: %s\n", networks[n].ssid);

                delay(500);
                WiFi.begin(networks[n].ssid, networks[n].password);

                if(WiFi.waitForConnectResult() == WL_CONNECTED) {
                    Serial.println("Success! Local IP address: " + WiFi.localIP().toString());
                    return true;
                } else {
                    Serial.printf("Fail!\n");
                }
            }
        }
    }
    return false;
}
