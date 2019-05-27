#include <Arduino.h>
#include <ESP8266WiFi.h>

#define SSID              "ATT549"
#define PASSWORD          "9071918601"
#define HOST              "192.168.1.142"

#define PORT_UPDATE       (2001)
#define WIFI_LEDPIN       (16)

bool update_firmware(const char * host, int port);

bool failed = true;

void setup() {
  Serial.begin(115200);

  int boot_mode = (GPI >> 16) & 0xf;
  if (boot_mode == 1) {
    Serial.println("Reset board to continue");
    return;
  }

  pinMode(WIFI_LEDPIN, OUTPUT);
  digitalWrite(WIFI_LEDPIN, LOW);
  delay(250);
  digitalWrite(WIFI_LEDPIN, HIGH);

  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  failed = !update_firmware(HOST, PORT_UPDATE);
  if (failed) Serial.println("FAILED UPDATE!");
  digitalWrite(WIFI_LEDPIN, LOW);
}

void loop() {
  if (failed) {
    bool isoff = (millis() / 250) % 2 == 0;
    digitalWrite(WIFI_LEDPIN, isoff? HIGH : LOW);
  }
  delay(10);
}
