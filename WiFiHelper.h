#include <WiFi.h>

bool _has_text(String needle, String haystack);

typedef struct {
  char *ssid;
  char *password;
} Network;

bool wifiConnect(int num_networks, Network *networks);
