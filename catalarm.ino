#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <HTTPClient.h>
#include "WiFiHelper.h"
#include "Networks.h"

AsyncWebServer server(80);
HTTPClient http;

// the EventSource back channel for notifying the clients when an alarm happens
// and to synchronize the gui widgets
AsyncEventSource events("/events");

// buzzer sequences, odd = on duration, even = off duration
int alarm_sequence[4] = {100, 100, 100, 0};
int set_state_sequence[6] = {50, 50, 50, 50, 50, 0};
int connect_success_sequence[6] = {50, 50, 50, 50, 50, 50};
int connect_fail_sequence[6] = {200, 50, 200, 50, 200, 50};

const int pin_sensor = 14;
const int pin_sensor_led = 19;
const int pin_buzzer = 22;
const int pin_status_led = 21;

volatile bool movement_flag;

bool buzzer_enabled = true;
bool im_enabled = false;
char state_buffer[64];

void onRequest(AsyncWebServerRequest *request){
  request->send(404);
}

void buzzerSequence(int sequence[], int length) {
    for (int i = 0; i < length / 2; i++) {
        digitalWrite(pin_buzzer, HIGH);
        delay(sequence[i * 2]);
        digitalWrite(pin_buzzer, LOW);
        delay(sequence[i * 2 + 1]);
    }
}

void send_state_event() {
    sprintf(
        state_buffer,
        "{\"buzzer\": %s, \"im\": %s}",
        buzzer_enabled ? "true" : "false",
        im_enabled ? "true" : "false"
    );

    events.send(state_buffer, "state");
}

void setup(){

    pinMode(pin_sensor, INPUT_PULLUP);
    pinMode(pin_sensor_led, OUTPUT);
    pinMode(pin_buzzer, OUTPUT);
    pinMode(pin_status_led, OUTPUT);

    digitalWrite(pin_sensor_led, LOW);
    digitalWrite(pin_buzzer, LOW);
    digitalWrite(pin_status_led, LOW);

    buzzerSequence(alarm_sequence, 4);

    Serial.begin(115200);
    Serial.printf("starting up...\n");

    // when the movement sensor fires, set a flag that gets handled in the loop function
    attachInterrupt(digitalPinToInterrupt(pin_sensor), []() {
        movement_flag = true;
    }, CHANGE);

    // try to connect to a wifi until one works
    while(true) {
        if(wifiConnect(num_networks, networks)) {
            buzzerSequence(connect_success_sequence, 6);
            break;
        } else {
            buzzerSequence(connect_fail_sequence, 6);
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

    // return the state for buzzer/im
    server.on("/get_state", HTTP_GET, [](AsyncWebServerRequest *request){
        sprintf(
            state_buffer,
            "{\"buzzer\": %s, \"im\": %s}",
            buzzer_enabled ? "true" : "false",
            im_enabled ? "true" : "false"
        );
        request->send(200, "application/json", state_buffer);
    });

    // set state
    server.on("/set_state", HTTP_GET, [](AsyncWebServerRequest *request){
        buzzerSequence(set_state_sequence, 6);
        if(request->hasParam("im")) {
            im_enabled = String(request->getParam("im")->value()) == "true";
        }

        if(request->hasParam("buzzer")) {
            buzzer_enabled = String(request->getParam("buzzer")->value()) == "true";
        }

        Serial.printf("Buzzer (%s) IM (%S)\n", buzzer_enabled?"on":"off", im_enabled?"on":"off");
        request->send(200, "text/plain", "ok");
        send_state_event();
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

void handle_alarm() {
    Serial.println("movement");

    // make some noise if there is a kitty in front of the sensor
    if(digitalRead(pin_sensor)) {
        events.send("movement", "movement");

        digitalWrite(pin_sensor_led, HIGH);
        if(buzzer_enabled) {
            buzzerSequence(alarm_sequence, 4);
        }

        if(im_enabled) {
            // talk to telegram through https://github.com/vindolin/https_relay
            http.begin("http://vault:8077/botXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX/sendMessage");
            http.addHeader("X-Relay-Target", "api.telegram.org");
            int httpCode = http.POST("chat_id=XXXXXXXXX&text=Katzenalarm!!");
            http.end();
        }

    } else {
        digitalWrite(pin_sensor_led, LOW);
    }
}

void loop(){

    // this flag is set by the interrupt routine
    if(movement_flag) {
        handle_alarm();
        movement_flag = false;
    }

}