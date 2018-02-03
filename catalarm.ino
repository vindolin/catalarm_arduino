#include <Arduino.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include "WiFiHelper.h"
#include "Networks.h"

AsyncWebServer server(80);
// AsyncWebSocket ws("/ws"); // access at ws://[esp ip]/ws
AsyncEventSource events("/events"); // event source (Server-Sent events)

int alarm_sequence[4] = {100, 100, 100, 0};
int enable_sequence[6] = {50, 50, 50, 50, 50, 0};
int disable_sequence[6] = {50, 50, 50, 50, 200, 0};
int connect_success_sequence[6] = {50, 50, 50, 50, 50, 50};
int connect_fail_sequence[6] = {200, 50, 200, 50, 200, 50};

const int pin_sensor = 14;
const int pin_sensor_led = 19;
const int pin_buzzer = 22;
const int pin_status_led = 21;

const int wifi_num_retries = 5;

volatile bool movement_flag;

bool buzzer_enabled = true;

void onRequest(AsyncWebServerRequest *request){
  //Handle Unknown Request
  request->send(404);
}

void onBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
  //Handle body
}

void onUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
  //Handle upload
}

void beepSequence(int sequence[], int length) {
    for (int i = 0; i < length / 2; i++) {
        digitalWrite(pin_buzzer, HIGH);
        delay(sequence[i * 2]);
        digitalWrite(pin_buzzer, LOW);
        delay(sequence[i * 2 + 1]);
    }
}

void on_movement() {
    movement_flag = true;
}


void send_status_event() {
  events.send(buzzer_enabled ? "true" : "false", "status");
}

void setup(){

    pinMode(pin_sensor, INPUT_PULLUP);
    pinMode(pin_sensor_led, OUTPUT);
    pinMode(pin_buzzer, OUTPUT);
    pinMode(pin_status_led, OUTPUT);

    digitalWrite(pin_sensor_led, LOW);
    digitalWrite(pin_buzzer, LOW);
    digitalWrite(pin_status_led, LOW);

    beepSequence(alarm_sequence, 4);

    Serial.begin(115200);
    Serial.printf("starting up...\n");

    attachInterrupt(digitalPinToInterrupt(pin_sensor), on_movement, CHANGE);

    while(true) {
        if(wifiConnect(num_networks, networks)) {
            beepSequence(connect_success_sequence, 6);
            break;
        } else {
            beepSequence(connect_fail_sequence, 6);
        }
        delay(5);
    }

    if(!SPIFFS.begin(true)){
        Serial.println("SPIFFS Mount Failed");
        return;
    }

    // attach AsyncEventSource
    server.addHandler(&events);

    // respond to GET requests on URL /heap
    server.on("/heap", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", String(ESP.getFreeHeap()));
    });

    // enable the beeper
    server.on("/enable", HTTP_GET, [](AsyncWebServerRequest *request){
        buzzer_enabled = true;
        beepSequence(enable_sequence, 6);
        request->send(200, "text/plain", "ok");
        send_status_event();
    });

    // enable the beeper
    server.on("/disable", HTTP_GET, [](AsyncWebServerRequest *request){
        buzzer_enabled = false;
        beepSequence(disable_sequence, 6);
        request->send(200, "text/plain", "ok");
        send_status_event();
    });

    server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", buzzer_enabled ? "true" : "false");
    });

    // send a file when /index is requested
    server.on("/", HTTP_ANY, [](AsyncWebServerRequest *request){
        request->send(SPIFFS, "/index.htm");
    });

    // send a file when /index is requested
    server.on("/favicon.ico", HTTP_ANY, [](AsyncWebServerRequest *request){
        request->send(SPIFFS, "/favicon.ico");
    });

    server.on("/opfa.jpg", HTTP_ANY, [](AsyncWebServerRequest *request){
        request->send(SPIFFS, "/opfa.jpg");
    });

    // attach filesystem root at URL /fs
    server.serveStatic("/fs", SPIFFS, "/");

    // Catch-All Handlers
    // Any request that can not find a Handler that canHandle it
    // ends in the callbacks below.
    server.onNotFound(onRequest);
    server.onFileUpload(onUpload);
    server.onRequestBody(onBody);

    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
    server.begin();

    digitalWrite(pin_status_led, HIGH);

}

void loop(){
    if(movement_flag) {
        Serial.println("movement");

        if(digitalRead(pin_sensor)) {
            events.send("movement", "movement");
            digitalWrite(pin_sensor_led, HIGH);
            if(buzzer_enabled)
                beepSequence(alarm_sequence, 4);
        } else {
            digitalWrite(pin_sensor_led, LOW);
        }
        movement_flag = false;
    }

}