#include <Arduino.h>
#include <Servo.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncWebSocket.h>

#include "page.hpp"
#include "math.hpp"

#define BIZERBA 0
#define LINEAR 1

#if BIZERBA
// cannot get ip from DHCP in Bizerba network
IPAddress LOCAL_IP(172, 17, 103, 220);
IPAddress GATEWAY(172, 17, 65, 1);
IPAddress SUBNET(255, 255, 192, 0);
IPAddress PRIMARY_DNS(10, 1, 1, 100);   //optional
IPAddress SECONDARY_DNS(10, 1, 1, 101); //optional
#endif

const char WIFI_SSID[] = "IOT";
const char WIFI_PASS[] = "_IOT1234";

namespace servo_app {
  const int PIN = 5;
  const int MIN = 0;
  const int MAX = 180;
  const float DEFAULT_PERCENTAGE = 100;

  Servo servo;
  int value = 0;
  float percentage = 0.0f;

  int percentage_to_value(float percentage) {
#if LINEAR
    // calculate linear height from circular frequency
    // angular functions are calculated in radians 
    // radians = degrees/180*PI;
    // degrees = radian*180/PI;
    
    float deg = remap(0, 100, MIN, MAX, percentage);
    float rad = deg / 180 * PI;

    // Circular motion, harmonic oscillation
    // Y=Ymax*sin(phi) | Ymax=r
    float y = 90 * sin(rad);
    float unclamped = deg <= 90 ? y : (180 - y);

    int unclamped_int = (int)(unclamped + 0.5f);
    return clamp(unclamped_int, MIN, MAX);
#else
    float unclamped = remap(0, 100, MIN, MAX, percentage);
    int unclamped_int = (int)(unclamped + 0.5f);
    return clamp(unclamped_int, MIN, MAX);
#endif
  }

  void set_percentage(float p) {
    percentage = p;
    value = percentage_to_value(percentage);
    servo.write(value);
    Serial.printf("value: %.1f%% (%i)\n", percentage, value);
  }

  void reset() {
    set_percentage(DEFAULT_PERCENTAGE);
  }

  void setup() {
    servo.attach(PIN, 544, 2400);
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
    // Serial.printf("ws[%u] event %u\n", client->id(), (unsigned)type);

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
      // TODO: properly handle multiple frames and packets
      //       see: https://github.com/me-no-dev/ESPAsyncWebServer#async-websocket-event
      
      const auto info = (AwsFrameInfo*)arg;

      if (info->opcode != WS_TEXT) {
        return;
      }

      const auto text = (char*)data;
      text[len] = '\0';

      // Serial.printf("ws[%u]: %s\n", client->id(), text);
      servo_app::set_percentage(atof(text));
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

    server.addHandler(&socket_app::socket);

    server.onNotFound([](AsyncWebServerRequest* request) {
      Serial.printf("Not found: %s!\r\n", request->url().c_str());
      request->send(404);
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

#if BIZERBA
  // Configures static IP address
  if (!WiFi.config(LOCAL_IP, GATEWAY, SUBNET, PRIMARY_DNS, SECONDARY_DNS)) {
    Serial.println("STA Failed to configure");
  }
#endif

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