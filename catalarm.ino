#include <Arduino.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include "WiFiHelper.h"
#include "Networks.h"

AsyncWebServer server(80);

// the EventSource back channel for notifying the clients when an alarm happens
// and to synchronize the gui widget
AsyncEventSource events("/events");

// buzzer sequences, odd = on duration, even = off duration
int alarm_sequence[4] = {100, 100, 100, 0};
int enable_sequence[6] = {50, 50, 50, 50, 50, 0};
int disable_sequence[6] = {50, 50, 50, 50, 200, 0};
int connect_success_sequence[6] = {50, 50, 50, 50, 50, 50};
int connect_fail_sequence[6] = {200, 50, 200, 50, 200, 50};

const int pin_sensor = 14;
const int pin_sensor_led = 19;
const int pin_buzzer = 22;
const int pin_status_led = 21;

volatile bool movement_flag;

bool buzzer_enabled = true;

void onRequest(AsyncWebServerRequest *request){
  request->send(404);
}

void beepSequence(int sequence[], int length) {
    for (int i = 0; i < length / 2; i++) {
        digitalWrite(pin_buzzer, HIGH);
        delay(sequence[i * 2]);
        digitalWrite(pin_buzzer, LOW);
        delay(sequence[i * 2 + 1]);
    }
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

    // when the movement sensor fires, set a flag that gets handled in the loop function
    attachInterrupt(digitalPinToInterrupt(pin_sensor), []() {
        movement_flag = true;
    }, CHANGE);

    // try to connect to a wifi until one works
    while(true) {
        if(wifiConnect(num_networks, networks)) {
            beepSequence(connect_success_sequence, 6);
            break;
        } else {
            beepSequence(connect_fail_sequence, 6);
        }
        delay(5);
    }

    // start the SPIFFS fs where our static files are located
    if(!SPIFFS.begin(true)){
        Serial.println("SPIFFS Mount Failed");
        return;
    }

    // attach AsyncEventSource
    server.addHandler(&events);

    // show free heap
    server.on("/heap", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", String(ESP.getFreeHeap()));
    });

    // enable the buzzer
    server.on("/enable", HTTP_GET, [](AsyncWebServerRequest *request){
        buzzer_enabled = true;
        beepSequence(enable_sequence, 6);
        request->send(200, "text/plain", "ok");
        send_status_event();
    });

    // disable the buzzer
    server.on("/disable", HTTP_GET, [](AsyncWebServerRequest *request){
        buzzer_enabled = false;
        beepSequence(disable_sequence, 6);
        request->send(200, "text/plain", "ok");
        send_status_event();
    });

    // return if the buzzer is enabled or not
    server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", buzzer_enabled ? "true" : "false");
    });

    // map / to index.htm
    server.on("/", HTTP_ANY, [](AsyncWebServerRequest *request){
        request->send(SPIFFS, "/index.htm");
    });

    // hello kitty!
    server.on("/favicon.ico", HTTP_ANY, [](AsyncWebServerRequest *request){
        request->send(SPIFFS, "/favicon.ico");
    });

    server.onNotFound(onRequest);

    // this is needed for the events to work on index.htm in file:/// mode, when developing
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
    server.begin();

    digitalWrite(pin_status_led, HIGH);
}

void loop(){

    // this flag is set by the interrupt routine
    if(movement_flag) {
        Serial.println("movement");

        // make some noise if there is a kitty in front of the sensor
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