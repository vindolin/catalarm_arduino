#include <Arduino.h>
#include <WiFi.h>

bool has_text(String needle, String haystack);

typedef struct {
  char *ssid;
  char *password;
} Network;

bool wifiConnect(int num_networks, Network *networks);
