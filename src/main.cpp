#include <Arduino.h>
#include <Servo.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncWebSocket.h>

#include "page.hpp"
#include "math.hpp"

const char WIFI_SSID[] = "IOT";
const char WIFI_PASS[] = "_IOT1234";

namespace servo_app {
  const int PIN = 5;
  const int MIN = 2;
  const int MAX = 152;
  const float DEFAULT_PERCENTAGE = 100;

  Servo servo;
  int value = 0;
  float percentage = 0.0f;

  void set_percentage(float percentage) {
    percentage = percentage;
    float unclamped_float = remap(0, 100, MIN, MAX, percentage);
    int unclamped = (int)(unclamped_float + 0.5f);
    value = clamp(unclamped, MIN, MAX);
    servo.write(value);
    Serial.printf("value: %.1f (%i)", percentage, value);
  }

  void reset() {
    set_percentage(DEFAULT_PERCENTAGE);
  }

  void setup() {
    servo.attach(PIN);
    reset();
  }
}

namespace socket_app {
  AsyncWebSocket socket("/ws");
  char buffer[64] = "";
  size_t index = 0;

  void clear() {
    buffer[0] = '\0';
    index = 0;
  }

  void cancel() {
    servo_app::reset();
    clear();
  }

  void handle(AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len) {
    if (type == WS_EVT_CONNECT) {
      Serial.printf("ws[%u] connect\n", client->id());
      for(auto other : server->getClients()) {
        if (other != client) {
          other->close(4000);
        }
      } 
    } else if (type == WS_EVT_DISCONNECT) {
      Serial.printf("ws[%u] disconnect\n", client->id());
      if (server->getClients().isEmpty()) {
        cancel();
      }
    } else if (type == WS_EVT_ERROR) {
      Serial.printf("ws[%u] error(%u): %s\n", client->id(), *((uint16_t*)arg), (char*)data);
      cancel();
    } else if (type == WS_EVT_DATA) {
      const auto info = (AwsFrameInfo*)arg;

      if (info->message_opcode != WS_TEXT) {
        return;
      }

      size_t space = sizeof(buffer) - index;
      size_t cpy_len = min(space, len);

      memcpy(buffer + index, (char*)data, cpy_len);
      index += cpy_len;

      if (info->final) {
        servo_app::set_percentage(atof(buffer));
        clear();
      }
    }
  }

  void setup() {
    socket.onEvent(handle);
  }
}

namespace server_app {
  AsyncWebServer server(80);

  void setup() {
    server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
      request->send_P(200, "text/html", PAGE);
    });

    server.on("/state", HTTP_GET, [](AsyncWebServerRequest* request) {
      request->send(200, "application/json", String(servo_app::percentage));
    });

    server.on("/state", HTTP_PUT, [](AsyncWebServerRequest* request) {
      if (request->_tempObject == nullptr) {
        request->send(400);
        return;
      }

      float percentage = atof(static_cast<char*>(request->_tempObject));
      servo_app::set_percentage(percentage);
      request->send(204);
    });

    server.onRequestBody([](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t idx, size_t total) {
      if (total > 64) {
        return;
      }

      if (request->_tempObject == nullptr) {
        request->_tempObject = new uint8_t[total + 1];
        static_cast<uint8_t*>(request->_tempObject)[total] = '\0';
      }

      memcpy(static_cast<uint8_t*>(request->_tempObject) + idx, data, len);
    });

    server.begin();
  }
}

void setup() {
  Serial.begin(115200);
  servo_app::setup();

  Serial.println("Connecting to ");
  Serial.println(WIFI_SSID);

  {
    byte mac[6];
    WiFi.macAddress(mac);
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "WeightServo-%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    WiFi.hostname(buffer);
  }

  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println(".");
  }

  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  socket_app::setup();
  server_app::setup();
}

void loop() {
}