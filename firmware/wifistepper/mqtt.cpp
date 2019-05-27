#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

#include "wifistepper.h"
#include "PubSubClient.h"

//#define MQTT_DEBUG

WiFiClient mqtt_conn;
PubSubClient mqtt_client(mqtt_conn);

extern StaticJsonBuffer<2560> jsonbuf;

#ifdef MQTT_DEBUG
void mqtt_debug(String msg) {
  Serial.println(msg);
}
void mqtt_debug(String msg, const char * str) {
  Serial.print(msg);
  Serial.print(": ");
  Serial.println(str);
}
#else
#define mqtt_debug(...)
#endif

void mqtt_callback(char * topic, byte * payload, unsigned int length) {
  if (
    config.service.mqtt.command_topic[0] != 0 &&
    strcmp(config.service.mqtt.command_topic, topic) == 0
  ) {
    if (length == MQTT_MAX_PACKET_SIZE) {
      seterror(ESUB_MQTT, 0, ETYPE_IBUF);
      return;
    }

    payload[length] = 0;
    mqtt_debug((const char *)payload);
    
    JsonObject& root = jsonbuf.parseObject(payload);
    if (root == JsonObject::invalid()) {
      mqtt_debug("Bad parse!");
      return;
    }
    
    if (root.containsKey("commands")) {
      JsonArray& arr = root["commands"].as<JsonArray&>();
      cmdq_read(arr, 0);
    }
    jsonbuf.clear();
  }
}

bool mqtt_connect() {
  if (!mqtt_client.connected()) {
    bool connected = config.service.mqtt.auth_enabled? mqtt_client.connect(PRODUCT " " MODEL " (" BRANCH ")", config.service.mqtt.username, config.service.mqtt.password) : mqtt_client.connect(PRODUCT " " MODEL " (" BRANCH ")");
    if (connected) mqtt_client.subscribe(config.service.mqtt.command_topic);
    mqtt_debug(connected? "MQTT connected" : "MQTT Not connected");
    return connected;
  }
  
  return true;
}

void mqtt_init() {
  if (!config.service.mqtt.enabled) return;
  
  mqtt_client.setServer(config.service.mqtt.server, config.service.mqtt.port);
  mqtt_client.setCallback(mqtt_callback);
  mqtt_connect();
}

void mqtt_loop(unsigned long now) {
  if (!config.service.mqtt.enabled) return;
  
  bool connected = state.service.mqtt.connected = mqtt_client.connected();
  if (!connected && timesince(sketch.service.mqtt.last.connect, now) > TIME_MQTT_RECONNECT) {
    connected = state.service.mqtt.connected = mqtt_connect();
    sketch.service.mqtt.last.connect = now;
  }

  mqtt_client.loop();

  if (
    connected &&
    config.service.mqtt.state_publish_period != 0.0 &&
    config.service.mqtt.state_topic[0] != 0 &&
    timesince(sketch.service.mqtt.last.publish, now) > (config.service.mqtt.state_publish_period * 1000.0)
  ) {
    mqtt_debug("Send state", config.service.mqtt.state_topic);
    
    motor_state * st = &state.motor;
    JsonObject& root = jsonbuf.createObject();
    root["stepss"] = st->stepss;
    root["pos"] = st->pos;
    root["mark"] = st->mark;
    root["vin"] = st->vin;
    root["dir"] = json_serialize(st->status.direction);
    root["movement"] = json_serialize(st->status.movement);
    root["hiz"] = st->status.hiz;
    root["busy"] = st->status.busy;
    root["switch"] = st->status.user_switch;
    root["stepclock"] = st->status.step_clock;
    JsonObject& alarms = root.createNestedObject("alarms");
    alarms["commanderror"] = st->status.alarms.command_error;
    alarms["overcurrent"] = st->status.alarms.overcurrent;
    alarms["undervoltage"] = st->status.alarms.undervoltage;
    alarms["thermalshutdown"] = st->status.alarms.thermal_shutdown;
    alarms["thermalwarning"] = st->status.alarms.thermal_warning;
    alarms["stalldetect"] = st->status.alarms.stall_detect;
    alarms["switch"] = st->status.alarms.user_switch;
    JsonVariant v = root;
    if (!mqtt_client.publish(config.service.mqtt.state_topic, v.as<String>().c_str())) {
      mqtt_debug("Failed to publish!");
    }
    jsonbuf.clear();
    sketch.service.mqtt.last.publish = now;
  }
}

