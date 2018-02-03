#include <Arduino.h>
#include <AsyncTCP.h>
#include "ESPAsyncWebServer.h"
#include "SPIFFS.h"
#include "WiFiHelper.h"
#include "Networks.h"

AsyncWebServer server(80);
AsyncWebSocket ws("/ws"); // access at ws://[esp ip]/ws
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

void onEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len){
  if(type == WS_EVT_CONNECT){
    //client connected
    Serial.printf("ws[%s][%u] connect\n", server->url(), client->id());
    client->printf("Hello Client %u :)", client->id());
    client->ping();
  } else if(type == WS_EVT_DISCONNECT){
    //client disconnected
    Serial.printf("ws[%s][%u] disconnect: %u\n", server->url(), client->id());
  } else if(type == WS_EVT_ERROR){
    //error was received from the other end
    Serial.printf("ws[%s][%u] error(%u): %s\n", server->url(), client->id(), *((uint16_t*)arg), (char*)data);
  } else if(type == WS_EVT_PONG){
    //pong message was received (in response to a ping request maybe)
    Serial.printf("ws[%s][%u] pong[%u]: %s\n", server->url(), client->id(), len, (len)?(char*)data:"");
  } else if(type == WS_EVT_DATA){
    //data packet
    AwsFrameInfo * info = (AwsFrameInfo*)arg;
    if(info->final && info->index == 0 && info->len == len){
      //the whole message is in a single frame and we got all of it's data
      Serial.printf("ws[%s][%u] %s-message[%llu]: ", server->url(), client->id(), (info->opcode == WS_TEXT)?"text":"binary", info->len);
      if(info->opcode == WS_TEXT){
        data[len] = 0;
        Serial.printf("%s\n", (char*)data);
      } else {
        for(size_t i=0; i < info->len; i++){
          Serial.printf("%02x ", data[i]);
        }
        Serial.printf("\n");
      }
      if(info->opcode == WS_TEXT)
        client->text("I got your text message");
      else
        client->binary("I got your binary message");
    } else {
      //message is comprised of multiple frames or the frame is split into multiple packets
      if(info->index == 0){
        if(info->num == 0)
          Serial.printf("ws[%s][%u] %s-message start\n", server->url(), client->id(), (info->message_opcode == WS_TEXT)?"text":"binary");
        Serial.printf("ws[%s][%u] frame[%u] start[%llu]\n", server->url(), client->id(), info->num, info->len);
      }

      Serial.printf("ws[%s][%u] frame[%u] %s[%llu - %llu]: ", server->url(), client->id(), info->num, (info->message_opcode == WS_TEXT)?"text":"binary", info->index, info->index + len);
      if(info->message_opcode == WS_TEXT){
        data[len] = 0;
        Serial.printf("%s\n", (char*)data);
      } else {
        for(size_t i=0; i < len; i++){
          Serial.printf("%02x ", data[i]);
        }
        Serial.printf("\n");
      }

      if((info->index + len) == info->len){
        Serial.printf("ws[%s][%u] frame[%u] end[%llu]\n", server->url(), client->id(), info->num, info->len);
        if(info->final){
          Serial.printf("ws[%s][%u] %s-message end\n", server->url(), client->id(), (info->message_opcode == WS_TEXT)?"text":"binary");
          if(info->message_opcode == WS_TEXT)
            client->text("I got your text message");
          else
            client->binary("I got your binary message");
        }
      }
    }
  }
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

    // attach AsyncWebSocket
    ws.onEvent(onEvent);
    server.addHandler(&ws);

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