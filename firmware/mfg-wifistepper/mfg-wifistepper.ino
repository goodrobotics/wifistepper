#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Wire.h>
#include <FS.h>

#include "powerstep01.h"

#define SSID              "ATT549"
#define PASSWORD          "9071918601"
#define HOST              "192.168.1.142"

//#define SSID              "BDFire"
//#define PASSWORD          "mcdermott"
//#define HOST              "10.1.10.195"

#define PORT_PRINT        (2000)
#define PORT_UPDATE       (2002)
#define RESET_PIN         (5)
#define WIFI_LEDPIN       (16)
#define WIFI_ADCCOEFF     (0.003223)
#define MOTOR_RSENSE      (0.0675)
#define MOTOR_ADCCOEFF    (2.65625)
#define MOTOR_CLOCK       (CLK_INT16)

#define ECC_ADDR          (0x60)
#define ECC_PIN_SDA       (2)
#define ECC_PIN_SCL       (5)
#define WAIT_RAND_TYP     (1000)
#define WAIT_RAND_MAX     (23)


WiFiClient print_client;
bool failed = false;

bool update_firmware(const char * host, int port);

void host_print(const char * str) {
  host_print(str, "");
}

void host_print(const char * str1, const char * str2) {
  Serial.printf("%s%s\n", str1, str2);
  print_client.printf("%s%s\n", str1, str2);
}

void host_print(const char * str1, size_t i) {
  char b[25];
  sprintf(b, "%u", i);
  host_print(str1, b);
}

void host_print(const char * str1, double d) {
  char b[35];
  sprintf(b, "%f", d);
  host_print(str1, b);
}

bool check_button(unsigned long ms) {
  pinMode(RESET_PIN, INPUT_PULLUP);
  unsigned long s = millis();
  while (digitalRead(RESET_PIN) && ((millis() - s) < ms || ms == 0)) {
    ESP.wdtFeed();
    delay(10);
  }

  bool v = !digitalRead(RESET_PIN);
  while (!digitalRead(RESET_PIN) && ((millis() - s) < ms || ms == 0)) {
    ESP.wdtFeed();
    delay(10);
  }

  return v;
}

bool fail(bool check=true) {
  if (check) failed = true;
  return check;
}

#define pf(f)  ((f)? "** [FAILED] **" : "PASSED")

uint16_t _ecc_crc16(uint8_t * data, uint8_t len, uint8_t * crc) {
  uint8_t counter;
  uint16_t crc_register = 0;
  uint16_t polynom = 0x8005;
  uint8_t shift_register;
  uint8_t data_bit, crc_bit;
  for (counter = 0; counter < len; counter++) {
    for (shift_register = 0x01; shift_register > 0x00; shift_register <<= 1) {
      data_bit = (data[counter] & shift_register) ? 1 : 0;
      crc_bit = crc_register >> 15;
      crc_register <<= 1;
      if (data_bit != crc_bit)
        crc_register ^= polynom;
    }
  }
  crc[0] = (uint8_t) (crc_register & 0x00FF);
  crc[1] = (uint8_t) (crc_register >> 8);
}

void _ecc_pack(uint8_t * data, size_t len) {
  if (len == 0 || data == NULL) return;
  data[0] = (uint8_t)len;
  _ecc_crc16(data, len-2, &data[len-2]);
}

bool _ecc_check(uint8_t * data, size_t len) {
  if (len == 0 || data == NULL) return false;
  uint8_t crc[2] = {0, 0};
  _ecc_crc16(data, len-2, crc);
  return data[len-2] == crc[0] && data[len-1] == crc[1];
}

size_t _ecc_waitread(uint8_t * data, size_t len, unsigned int typ_us, unsigned long max_ms) {
  unsigned long start = millis();
  delayMicroseconds(typ_us);
  size_t rlen = 0;
  do {
    rlen = _ecc_read(data, len);
  } while (rlen == 0 && (millis() - start) < max_ms);
  return rlen;
}

void _ecc_write(uint8_t mode, uint8_t * data, size_t len) {
  Wire.beginTransmission(ECC_ADDR);
  Wire.write(mode);
  if (len > 0)
    Wire.write(data, len);
  Wire.endTransmission();
}


size_t _ecc_read(uint8_t * data, size_t len) {
  size_t r = 0;
  Wire.requestFrom(ECC_ADDR, len);
  while(Wire.available())
    data[r++] = Wire.read();
  return r;
}

void print_packet(const char * name, uint8_t * p, size_t len) {
  Serial.print("Packet ");
  Serial.print(name);
  Serial.println(":");
  for (size_t i = 0; i < len; i++) {
    Serial.print("(");
    Serial.print(i);
    Serial.print(") = ");
    Serial.println(p[i], HEX);
  }
  
  Serial.print("LENGTH=");
  Serial.println(len);
  Serial.print("CRC=");
  Serial.println(_ecc_check(p, len));
}


void setup() {
  Serial.begin(115200);
  SPIFFS.begin();

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
  
  if (!print_client.connect(HOST, PORT_PRINT) || !print_client.connected()) {
    Serial.println("ERROR: Failed to connect to print server.");
    return;
  }

  int boot_mode = (GPI >> 16) & 0xf;
  if (boot_mode == 1) {
    host_print("Reset board to continue");
    return;
  }

  host_print("MFG Test Start =======================");
  host_print("* ChipID: ", ESP.getChipId());

  {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char buf[20];
    sprintf(buf, "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    host_print("* MAC: ", buf);
  }

  delay(100);
  host_print("Testing ECC");
  {
    Wire.begin(ECC_PIN_SDA, ECC_PIN_SCL);

    _ecc_write(0x1, NULL, 0);
    _ecc_write(0x0, NULL, 0);
    //uint8_t reply1[0x04];
    //size_t r1 = _ecc_waitread(reply1, 0x04, 1, 2);
    //print_packet("Wake REPLY", reply1, r1);
    //bool f1 = fail(!_ecc_check(reply1, r1));
    //host_print("* WAKE check: ", pf(f1));
    delay(10);

    uint8_t rnd[7] = {0, 0x1B, 0, 0, 0, 0, 0};
    _ecc_pack(rnd, 7);
    //print_packet("Random CMD", rnd, 7);
    _ecc_write(0x3, rnd, 7);
    
    uint8_t reply2[0x23];
    size_t r2 = _ecc_waitread(reply2, 0x23, WAIT_RAND_TYP, WAIT_RAND_MAX);
    //print_packet("Random REPLY", reply2, r2);
    bool f2 = fail(!_ecc_check(reply2, r2));
    host_print("* CRC check: ", pf(f2));

    bool f3 = fail(reply2[1] != 0xFF || reply2[11] != 0x00 || reply2[30] != 0xFF || reply2[31] != 0x00);
    host_print("* DATA check: ", pf(f3));
  }

  delay(100);
  host_print("User Test ADC");
  {
    while (!check_button(1000)) {
      double adc = (double)analogRead(A0) * WIFI_ADCCOEFF;
      host_print("ADC value: ", adc);
      if (adc > 2.4 && adc < 2.6) {
        host_print("* VALUE check: PASSED");
        break;
      }
    }
  }

  delay(100);
  host_print("User Test LED ORANGE");
  {
    pinMode(WIFI_LEDPIN, OUTPUT);
    digitalWrite(WIFI_LEDPIN, LOW);
    bool v = true;

    host_print("* Press BUTTON to continue");
    while (!check_button(250)) {
      digitalWrite(WIFI_LEDPIN, v? HIGH : LOW);
      v = !v;
    }
    
    digitalWrite(WIFI_LEDPIN, HIGH);
  }

  delay(200);
  host_print("User Test LED RED");
  {
    ps_spiinit();
    host_print("* Press BUTTON to continue");
    check_button(0);
  }

  delay(100);
  host_print("Writing motor config");
  {
    ps_setsync(SYNC_BUSY);
    ps_setmode(MODE_VOLTAGE);
    ps_setstepsize(STEP_128);
    ps_setmaxspeed(10000.0);
    ps_setminspeed(0.0, true);
    ps_setaccel(1000.0);
    ps_setdecel(1000.0);
    ps_setfullstepspeed(2000.0, false);
    
    ps_setslewrate(SR_520);
  
    ps_setocd(500.0, true);
    
    ps_setktvals(MODE_VOLTAGE, 0.15, 0.15, 0.15, 0.15);
    ps_vm_pwmfreq pwmfreq = ps_vm_pwmfreq2coeffs(MOTOR_CLOCK, 23.4 * 1000.0);
    ps_vm_setpwmfreq(&pwmfreq);
    ps_vm_setstall(750.0);
    ps_vm_setbemf(0.0375, 61.5072, 0.0615, 0.0615);
    ps_vm_setvscomp(false);
    
    ps_setswmode(SW_USER);
    ps_setclocksel(MOTOR_CLOCK);
    
    ps_setalarmconfig(true, true, true, true);
    ps_getstatus(true);
  }

  delay(100);
  host_print("Testing Vin");
  {
    double adc = (float)ps_readadc() * MOTOR_ADCCOEFF;
    bool f = fail(adc < 23.5 || adc > 24.5);
    host_print("* VIN value: ", adc);
    host_print("* VIN check: ", pf(f));
  }

  delay(100);
  host_print("User Test Powerstep SW");
  {
    host_print("* Close SW");
    ps_status s = ps_getstatus(false);
    while (!s.user_switch) {
      delay(10);
      s = ps_getstatus(false);
    }

    host_print("* Open SW");
    while (s.user_switch) {
      delay(10);
      s = ps_getstatus(false);
    }
  }

  delay(100);
  host_print("User Test PWM");
  {
    ps_run(FWD, 100.0);
    host_print("* Check PWM signals");
    check_button(0);
    ps_softhiz();
  }

  host_print("Done. Result = ", pf(failed));
  if (!failed) {
    host_print("Updating image....");
    bool s = update_firmware(HOST, PORT_UPDATE);
    if (!s) host_print("FAILED UPDATE!");
  }

  SPIFFS.end();
}

void loop() {
  digitalWrite(WIFI_LEDPIN, LOW);
  delay(10);
}

