#include <FS.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>

#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>

#include "powerstep01.h"
#include "wifistepper.h"
#include "ecc508a.h"

//#define MAINLOOP_DEBUG

#define TYPE_HTML     "text/html"
#define TYPE_CSS      "text/css"
#define TYPE_JS       "application/javascript"
#define TYPE_PNG      "image/png"

#define WTO_REVERTAP  (5 * 60 * 1000)
#define WTO_CONNECT   (1000)
#define WTO_RSSI      (1000)

queue_t queue[QS_SIZE];
volatile id_t _id = ID_START;

ESP8266WebServer server(PORT_HTTP);
WebSocketsServer websocket(PORT_HTTPWS);

StaticJsonBuffer<2560> jsonbuf;

volatile bool flag_reboot = false;
volatile bool flag_wifiled = false;

config_t config = {
  .wifi = {
    //.mode = M_STATION,
    .mode = M_ACCESSPOINT,
    .accesspoint = {
      .ssid = {0},
      .password = {0},
      .encryption = false,
      .channel = 1,
      .hidden = false
    },
    .station = {
      //.ssid = {'B','D','F','i','r','e',0},
      //.password = {'m','c','d','e','r','m','o','t','t',0},
      //.ssid = {'A','T','T','5','4','9',0},
      //.password = {'9','0','7','1','9','1','8','6','0','1',0},
      //.ssid = {'B','i','l','l','W','i','T','h','e','S','c','i','e','n','c','e','F','i',0},
      //.password = {'p','i','n','e','a','p','p','l','e','1','2','3',0},
      .ssid = {0},
      .password = {0},
      .encryption = false,
      .forceip = {0},
      .forcesubnet = {0},
      .forcegateway = {0},
      .revertap = true
    }
  },
  .service = {
    .hostname = {'w','s','x','1','0','0',0},
    .security = {'n','o','n','e',0},
    .http = {
      .enabled = true,
    },
    .mdns = {
      .enabled = true
    },
    .auth = {
      .enabled = false,
      .username = {0},
      .password = {0}
    },
    .lowcom = {
      .enabled = true,
      .std_enabled = true,
      .crypto_enabled = false
    },
    .mqtt = {
      .enabled = false,
      .server = {0},
      .port = 1883,
      .auth_enabled = false,
      .username = {0},
      .password = {0},
      .state_topic = {0},
      .state_publish_period = 10.0,
      .command_topic = {0}
    },
    .ota = {
      .enabled = false,
      .password = {0}
    }
  },
  .daisy = {
    .enabled = true,
    .master = false,
    .slavewifioff = false
  },
  .io = {
    .wifiled = {
      .usercontrol = false,
      .is_output = true
    }
  },
  .motor = {
    .mode = MODE_CURRENT,
    .stepsize = STEP_16,
    .ocd = 500.0,
    .ocdshutdown = true,
    .maxspeed = 10000.0,
    .minspeed = 0.0,
    .accel = 1000.0,
    .decel = 1000.0,
    .fsspeed = 2000.0,
    .fsboost = false,
    .cm = {
      .kthold = 2.5,
      .ktrun = 2.5,
      .ktaccel = 2.5,
      .ktdecel = 2.5,
      .switchperiod = 44,
      .predict = true,
      .minon = 21,
      .minoff = 21,
      .fastoff = 4,
      .faststep = 20
    },
    .vm = {
      .kthold = 15.0,
      .ktrun = 15.0,
      .ktaccel = 15.0,
      .ktdecel = 15.0,
      .pwmfreq = 23.4,
      .stall = 750.0,
      .volt_comp = false,
      .bemf_slopel = 0.0375,
      .bemf_speedco = 61.5072,
      .bemf_slopehacc = 0.0615,
      .bemf_slopehdec = 0.0615
    },
    .reverse = false
  }
};

state_t state = { 0 };
sketch_t sketch = { 0 };

id_t nextid() { return _id++; }
id_t currentid() { return _id; }

unsigned long timesince(unsigned long t1, unsigned long t2) {
  return (t1 <= t2)? (t2 - t1) : (ULONG_MAX - t1 + t2);
}

void seterror(uint8_t subsystem, id_t onid, int type, int8_t arg) {
  if (!state.error.errored) {
    state.error.when = millis();
    state.error.subsystem = subsystem;
    state.error.id = onid;
    state.error.type = type;
    state.error.arg = arg;

    lowcom_senderror(&state.error);
  }
}

void clearerror() {
  memset(&state.error, 0, sizeof(error_state));
}

void resetcfg() {
  SPIFFS.remove(FNAME_WIFICFG);
  SPIFFS.remove(FNAME_SERVICECFG);
  SPIFFS.remove(FNAME_DAISYCFG);
  SPIFFS.remove(FNAME_MOTORCFG);
  for (int i = 1; i < QS_SIZE; i++) queuecfg_reset(i);
}

void wificfg_read(wifi_config * cfg) {
  File fp = SPIFFS.open(FNAME_WIFICFG, "r");
  if (fp) {
    size_t size = fp.size();
    if (size <= FILE_MAXSIZE) {
      char buf[size+1];
      fp.readBytes(buf, size);
      buf[size] = 0;
      
      JsonObject& root = jsonbuf.parseObject(buf);
      if (root.containsKey("mode"))                   cfg->mode = parse_wifimode(root["mode"].as<char *>());
      if (root.containsKey("accesspoint_ssid"))       strlcpy(cfg->accesspoint.ssid, root["accesspoint_ssid"].as<char *>(), LEN_SSID);
      if (root.containsKey("accesspoint_password"))   strlcpy(cfg->accesspoint.password, root["accesspoint_password"].as<char *>(), LEN_PASSWORD);
      if (root.containsKey("accesspoint_encryption")) cfg->accesspoint.encryption = root["accesspoint_encryption"].as<bool>();
      if (root.containsKey("accesspoint_channel"))    cfg->accesspoint.channel = root["accesspoint_channel"].as<int>();
      if (root.containsKey("accesspoint_hidden"))     cfg->accesspoint.hidden = root["accesspoint_hidden"].as<bool>();
      if (root.containsKey("station_ssid"))           strlcpy(cfg->station.ssid, root["station_ssid"].as<char *>(), LEN_SSID);
      if (root.containsKey("station_password"))       strlcpy(cfg->station.password, root["station_password"].as<char *>(), LEN_PASSWORD);
      if (root.containsKey("station_encryption"))     cfg->station.encryption = root["station_encryption"].as<bool>();
      if (root.containsKey("station_forceip"))        strlcpy(cfg->station.forceip, root["station_forceip"].as<char *>(), LEN_IP);
      if (root.containsKey("station_forcesubnet"))    strlcpy(cfg->station.forcesubnet, root["station_forcesubnet"].as<char *>(), LEN_IP);
      if (root.containsKey("station_forcegateway"))   strlcpy(cfg->station.forcegateway, root["station_forcegateway"].as<char *>(), LEN_IP);
      if (root.containsKey("station_revertap"))       cfg->station.revertap = root["station_revertap"].as<bool>();
      jsonbuf.clear();
    }
    fp.close();
  }
}

void wificfg_write(wifi_config * const cfg) {
  JsonObject& root = jsonbuf.createObject();
  root["mode"] = json_serialize(cfg->mode);
  root["accesspoint_ssid"] = cfg->accesspoint.ssid;
  root["accesspoint_password"] = cfg->accesspoint.password;
  root["accesspoint_encryption"] = cfg->accesspoint.encryption;
  root["accesspoint_channel"] = cfg->accesspoint.channel;
  root["accesspoint_hidden"] = cfg->accesspoint.hidden;
  root["station_ssid"] = cfg->station.ssid;
  root["station_password"] = cfg->station.password;
  root["station_encryption"] = cfg->station.encryption;
  root["station_forceip"] = cfg->station.forceip;
  root["station_forcesubnet"] = cfg->station.forcesubnet;
  root["station_forcegateway"] = cfg->station.forcegateway;
  root["station_revertap"] = cfg->station.revertap;
  File fp = SPIFFS.open(FNAME_WIFICFG, "w");
  root.printTo(fp);
  fp.close();
  jsonbuf.clear();
}

bool wificfg_connect(wifi_mode mode, wifi_config * const cfg) {
  bool success = false;
  switch (mode) {
    case M_ACCESSPOINT: {
      WiFi.mode(WIFI_AP);
      WiFi.softAPConfig(AP_IP, AP_IP, AP_MASK);
      //Serial.printf("\nAP mode. SSID=%s, PSWD=%s, CH=%d, H=%d\n", cfg->accesspoint.ssid, cfg->accesspoint.password, cfg->accesspoint.channel, cfg->accesspoint.hidden);
      success = WiFi.softAP(cfg->accesspoint.ssid, cfg->accesspoint.password, cfg->accesspoint.channel, cfg->accesspoint.hidden);
      state.wifi.ip = WiFi.softAPIP();
      state.wifi.mode = M_ACCESSPOINT;
      break;
    }
    case M_STATION: {
      WiFi.mode(WIFI_STA);
      if (cfg->station.forceip[0] != 0 && cfg->station.forcesubnet[0] != 0 && cfg->station.forcegateway[0] != 0) {
        IPAddress addr, subnet, gateway;
        if (addr.fromString(cfg->station.forceip) && subnet.fromString(cfg->station.forcesubnet) && gateway.fromString(cfg->station.forcegateway)) {
          WiFi.config(addr, subnet, gateway);
        }
      }
      WiFi.begin(cfg->station.ssid, cfg->station.password);
      for (int i=0; i < 200 && WiFi.status() != WL_CONNECTED; i++) {
        ESP.wdtFeed();
        delay(50);
      }

      if (WiFi.status() == WL_CONNECTED) {
        state.wifi.ip = WiFi.localIP();
        state.wifi.mode = M_STATION;
        success = true;
      }
      break;
    }
    case M_OFF: {
      WiFi.mode(WIFI_OFF);
      state.wifi.mode = M_OFF;
      state.wifi.ip = 0;
      success = true;
      break;
    }
  }

  Serial.println();
  Serial.printf("\nWifi Config: %d, success=%d, ip=", mode, success);
  Serial.print(IPAddress(state.wifi.ip).toString());
  Serial.println();
  
  return success;
}

void wificfg_update(unsigned long now) {
  if (timesince(sketch.wifi.last.rssi, now) > WTO_RSSI) {
    sketch.wifi.last.rssi = now;
    state.wifi.rssi = WiFi.RSSI();
  }
  
  if (state.wifi.mode == M_ACCESSPOINT && config.wifi.mode == M_STATION && config.wifi.station.revertap && timesince(sketch.wifi.last.revertap, now) > WTO_REVERTAP) {
    // We've reverted to AP some time ago, try to reconnect in station mode now
    sketch.wifi.last.revertap = now;
    if (!wificfg_connect(M_STATION, &config.wifi)) {
      // Could not connect to station, revert back to AP mode
      wificfg_connect(M_ACCESSPOINT, &config.wifi);
    }
  }
  
  if (state.wifi.mode == M_STATION && timesince(sketch.wifi.last.connection_check, now) > WTO_CONNECT) {
    // Lost connection, try to reconnect
    sketch.wifi.last.connection_check = now;
    if (WiFi.status() != WL_CONNECTED && !wificfg_connect(M_STATION, &config.wifi)) {
      // Station connection failed, revert to AP if configured
      if (config.wifi.station.revertap) wificfg_connect(M_ACCESSPOINT, &config.wifi);
    }
  }
}

void servicecfg_read(service_config * cfg) {
  File fp = SPIFFS.open(FNAME_SERVICECFG, "r");
  if (fp) {
    size_t size = fp.size();
    if (size <= FILE_MAXSIZE) {
      char buf[size+1];
      fp.readBytes(buf, size);
      buf[size] = 0;
      
      fp.readBytes(buf, size);
      JsonObject& root = jsonbuf.parseObject(buf);
      if (root.containsKey("hostname"))       strlcpy(cfg->hostname, root["hostname"].as<char *>(), LEN_HOSTNAME);
      if (root.containsKey("security"))       strlcpy(cfg->security, root["security"].as<char *>(), LEN_INFO);
      if (root.containsKey("mdns_enabled"))   cfg->mdns.enabled = root["mdns_enabled"].as<bool>();
      if (root.containsKey("http_enabled"))   cfg->http.enabled = root["http_enabled"].as<bool>();
      if (root.containsKey("auth_enabled"))   cfg->auth.enabled = root["auth_enabled"].as<bool>();
      if (root.containsKey("auth_username"))  strlcpy(cfg->auth.username, root["auth_username"].as<char *>(), LEN_USERNAME);
      if (root.containsKey("auth_password"))  strlcpy(cfg->auth.password, root["auth_password"].as<char *>(), LEN_PASSWORD);
      if (root.containsKey("mqtt_enabled"))   cfg->mqtt.enabled = root["mqtt_enabled"].as<bool>();
      if (root.containsKey("mqtt_server"))    strlcpy(cfg->mqtt.server, root["mqtt_server"].as<char *>(), LEN_URL);
      if (root.containsKey("mqtt_port"))      cfg->mqtt.port = root["mqtt_port"].as<int>();
      if (root.containsKey("mqtt_auth_enabled")) cfg->mqtt.auth_enabled = root["mqtt_auth_enabled"].as<bool>();
      if (root.containsKey("mqtt_username"))  strlcpy(cfg->mqtt.username, root["mqtt_username"].as<char *>(), LEN_USERNAME);
      if (root.containsKey("mqtt_password"))  strlcpy(cfg->mqtt.password, root["mqtt_password"].as<char *>(), LEN_PASSWORD);
      if (root.containsKey("mqtt_state_topic")) strlcpy(cfg->mqtt.state_topic, root["mqtt_state_topic"].as<char *>(), LEN_URL);
      if (root.containsKey("mqtt_state_publish_period")) cfg->mqtt.state_publish_period = root["mqtt_state_publish_period"].as<float>();
      if (root.containsKey("mqtt_command_topic")) strlcpy(cfg->mqtt.command_topic, root["mqtt_command_topic"].as<char *>(), LEN_URL);
      if (root.containsKey("ota_enabled"))    cfg->ota.enabled = root["ota_enabled"].as<bool>();
      if (root.containsKey("ota_password"))   strlcpy(cfg->ota.password, root["ota_password"].as<char *>(), LEN_PASSWORD);
      jsonbuf.clear();
    }
    fp.close();
  }
}

void servicecfg_write(service_config * const cfg) {
  JsonObject& root = jsonbuf.createObject();
  root["hostname"] = cfg->hostname;
  root["security"] = cfg->security;
  root["mdns_enabled"] = cfg->mdns.enabled;
  root["http_enabled"] = cfg->http.enabled;
  root["auth_enabled"] = cfg->auth.enabled;
  root["auth_username"] = cfg->auth.username;
  root["auth_password"] = cfg->auth.password;
  root["mqtt_enabled"] = cfg->mqtt.enabled;
  root["mqtt_server"] = cfg->mqtt.server;
  root["mqtt_port"] = cfg->mqtt.port;
  root["mqtt_auth_enabled"] = cfg->mqtt.auth_enabled;
  root["mqtt_username"] = cfg->mqtt.username;
  root["mqtt_password"] = cfg->mqtt.password;
  root["mqtt_state_topic"] = cfg->mqtt.state_topic;
  root["mqtt_state_publish_period"] = cfg->mqtt.state_publish_period;
  root["mqtt_command_topic"] = cfg->mqtt.command_topic;
  root["ota_enabled"] = cfg->ota.enabled;
  root["ota_password"] = cfg->ota.password;
  File fp = SPIFFS.open(FNAME_SERVICECFG, "w");
  root.printTo(fp);
  fp.close();
  jsonbuf.clear();
}

void daisycfg_read(daisy_config * cfg) {
  File fp = SPIFFS.open(FNAME_DAISYCFG, "r");
  if (fp) {
    size_t size = fp.size();
    if (size <= FILE_MAXSIZE) {
      char buf[size+1];
      fp.readBytes(buf, size);
      buf[size] = 0;
      
      fp.readBytes(buf, size);
      JsonObject& root = jsonbuf.parseObject(buf);
      if (root.containsKey("enabled"))  cfg->enabled = root["enabled"].as<bool>();
      if (root.containsKey("master"))   cfg->master = root["master"].as<bool>();
      if (root.containsKey("slavewifioff")) cfg->slavewifioff = root["slavewifioff"].as<bool>();
      jsonbuf.clear();
    }
    fp.close();
  }
}

void daisycfg_write(daisy_config * const cfg) {
  JsonObject& root = jsonbuf.createObject();
  root["enabled"] = cfg->enabled;
  root["master"] = cfg->master;
  root["slavewifioff"] = cfg->slavewifioff;
  File fp = SPIFFS.open(FNAME_DAISYCFG, "w");
  root.printTo(fp);
  fp.close();
  jsonbuf.clear();
}

bool queuecfg_read(uint8_t qid) {
  char fname[20] = {0};
  sprintf(fname, FNAME_QUEUECFG, qid);
  File fp = SPIFFS.open(fname, "r");
  if (!fp) {
    // TODO set error
    return false;
  }

  bool success = false;
  size_t size = fp.size();
  if (size <= FILE_MAXSIZE) {
    char buf[size+1];
    fp.readBytes(buf, size);
    buf[size] = 0;
    
    fp.readBytes(buf, size);
    JsonArray& arr = jsonbuf.parseArray(buf);
    cmdq_empty(queue_get(qid), nextid());
    cmdq_read(arr, 0, qid);
    jsonbuf.clear();
    success = true;
  }
  fp.close();
  return success;
}

bool queuecfg_write(uint8_t qid) {
  if (queue == NULL) {
    // TODO set error
    return false;
  }
  char fname[20] = {0};
  sprintf(fname, FNAME_QUEUECFG, qid);
  JsonArray& arr = jsonbuf.createArray();
  cmdq_write(arr, queue_get(qid));
  File fp = SPIFFS.open(fname, "w");
  arr.printTo(fp);
  fp.close();
  jsonbuf.clear();
}

bool queuecfg_reset(uint8_t qid) {
  char fname[20] = {0};
  sprintf(fname, FNAME_QUEUECFG, qid);
  SPIFFS.remove(fname);
}

void motorcfg_pull(motor_config * cfg) {
  cfg->mode = ps_getmode();
  cfg->stepsize = ps_getstepsize();
  ps_ocd ocd = ps_getocd();
  cfg->ocd = ocd.millivolts;
  cfg->ocdshutdown = ocd.shutdown;
  cfg->maxspeed = ps_getmaxspeed();
  ps_minspeed minspeed = ps_getminspeed();
  cfg->minspeed = minspeed.steps_per_sec;
  cfg->accel = ps_getaccel();
  cfg->decel = ps_getdecel();
  ps_ktvals ktvals = ps_getktvals(cfg->mode);
  ps_fullstepspeed fullstepspeed = ps_getfullstepspeed();
  cfg->fsspeed = fullstepspeed.steps_per_sec;
  cfg->fsboost = fullstepspeed.boost_mode;
  if (cfg->mode == MODE_CURRENT) {
    cfg->cm.kthold = ktvals.hold / MOTOR_RSENSE;
    cfg->cm.ktrun = ktvals.run / MOTOR_RSENSE;
    cfg->cm.ktaccel = ktvals.accel / MOTOR_RSENSE;
    cfg->cm.ktdecel = ktvals.decel / MOTOR_RSENSE;
    cfg->cm.switchperiod = ps_cm_getswitchperiod();
    cfg->cm.predict = ps_cm_getpredict();
    ps_cm_ctrltimes ctrltimes = ps_cm_getctrltimes();
    cfg->cm.minon = ctrltimes.min_on_us;
    cfg->cm.minoff = ctrltimes.min_off_us;
    cfg->cm.fastoff = ctrltimes.fast_off_us;
    cfg->cm.faststep = ctrltimes.fast_step_us;
  } else if (cfg->mode == MODE_VOLTAGE) {
    cfg->vm.kthold = ktvals.hold * 100.0;
    cfg->vm.ktrun = ktvals.run * 100.0;
    cfg->vm.ktaccel = ktvals.accel * 100.0;
    cfg->vm.ktdecel = ktvals.decel * 100.0;
    ps_vm_pwmfreq pwmfreq = ps_vm_getpwmfreq();
    cfg->vm.pwmfreq = ps_vm_coeffs2pwmfreq(MOTOR_CLOCK, &pwmfreq) / 1000.0;
    cfg->vm.stall = ps_vm_getstall();
    cfg->vm.volt_comp = ps_vm_getvscomp();
    ps_vm_bemf bemf = ps_vm_getbemf();
    cfg->vm.bemf_slopel = bemf.slopel;
    cfg->vm.bemf_speedco = bemf.speedco;
    cfg->vm.bemf_slopehacc = bemf.slopehacc;
    cfg->vm.bemf_slopehdec = bemf.slopehdec;
  }
}

void motorcfg_push(motor_config * cfg) {
  ps_setsync(SYNC_BUSY);
  ps_setmode(cfg->mode);
  ps_setstepsize(cfg->stepsize);
  ps_setmaxspeed(cfg->maxspeed);
  ps_setminspeed(cfg->minspeed, true);
  ps_setaccel(cfg->accel);
  ps_setdecel(cfg->decel);
  ps_setfullstepspeed(cfg->fsspeed, cfg->fsboost);
  
  ps_setslewrate(SR_520);

  ps_setocd(cfg->ocd, cfg->ocdshutdown);
  if (cfg->mode == MODE_CURRENT) {
    ps_setktvals(cfg->mode, cfg->cm.kthold * MOTOR_RSENSE, cfg->cm.ktrun * MOTOR_RSENSE, cfg->cm.ktaccel * MOTOR_RSENSE, cfg->cm.ktdecel * MOTOR_RSENSE);
    ps_cm_setswitchperiod(cfg->cm.switchperiod);
    ps_cm_setpredict(cfg->cm.predict);
    ps_cm_setctrltimes(cfg->cm.minon, cfg->cm.minoff, cfg->cm.fastoff, cfg->cm.faststep);
    ps_cm_settqreg(false);
  } else if (cfg->mode == MODE_VOLTAGE) {
    ps_setktvals(cfg->mode, cfg->vm.kthold / 100.0, cfg->vm.ktrun / 100.0, cfg->vm.ktaccel / 100.0, cfg->vm.ktdecel / 100.0);
    ps_vm_pwmfreq pwmfreq = ps_vm_pwmfreq2coeffs(MOTOR_CLOCK, cfg->vm.pwmfreq * 1000.0);
    ps_vm_setpwmfreq(&pwmfreq);
    ps_vm_setstall(cfg->vm.stall);
    ps_vm_setbemf(cfg->vm.bemf_slopel, cfg->vm.bemf_speedco, cfg->vm.bemf_slopehacc, cfg->vm.bemf_slopehdec);
    ps_vm_setvscomp(cfg->vm.volt_comp);
  }
  
  ps_setswmode(SW_USER);
  ps_setclocksel(MOTOR_CLOCK);
  
  ps_setalarmconfig(true, true, true, true);

  // Clear errors at end of push
  cmd_clearerror();
}

void motorcfg_write(motor_config * cfg) {
  JsonObject& root = jsonbuf.createObject();
  root["mode"] = json_serialize(cfg->mode);
  root["stepsize"] = json_serialize(cfg->stepsize);
  root["ocd"] = cfg->ocd;
  root["ocdshutdown"] = cfg->ocdshutdown;
  root["maxspeed"] = cfg->maxspeed;
  root["minspeed"] = cfg->minspeed;
  root["accel"] = cfg->accel;
  root["decel"] = cfg->decel;
  root["fsspeed"] = cfg->fsspeed;
  root["fsboost"] = cfg->fsboost;
  root["cm_kthold"] = cfg->cm.kthold;
  root["cm_ktrun"] = cfg->cm.ktrun;
  root["cm_ktaccel"] = cfg->cm.ktaccel;
  root["cm_ktdecel"] = cfg->cm.ktdecel;
  root["cm_switchperiod"] = cfg->cm.switchperiod;
  root["cm_predict"] = cfg->cm.predict;
  root["cm_minon"] = cfg->cm.minon;
  root["cm_minoff"] = cfg->cm.minoff;
  root["cm_fastoff"] = cfg->cm.fastoff;
  root["cm_faststep"] = cfg->cm.faststep;
  root["vm_kthold"] = cfg->vm.kthold;
  root["vm_ktrun"] = cfg->vm.ktrun;
  root["vm_ktaccel"] = cfg->vm.ktaccel;
  root["vm_ktdecel"] = cfg->vm.ktdecel;
  root["vm_pwmfreq"] = cfg->vm.pwmfreq;
  root["vm_stall"] = cfg->vm.stall;
  root["vm_volt_comp"] = cfg->vm.volt_comp;
  root["vm_bemf_slopel"] = cfg->vm.bemf_slopel;
  root["vm_bemf_speedco"] = cfg->vm.bemf_speedco;
  root["vm_bemf_slopehacc"] = cfg->vm.bemf_slopehacc;
  root["vm_bemf_slopehdec"] = cfg->vm.bemf_slopehdec;
  root["reverse"] = cfg->reverse;
  JsonVariant v = root;
  File fp = SPIFFS.open(FNAME_MOTORCFG, "w");
  root.printTo(fp);
  fp.close();
  jsonbuf.clear();
}

void static_serve(String contenttype, String path) {
  check_auth()
  File fp = SPIFFS.open(path, "r");
  if (fp) {
    server.streamFile(fp, contenttype);
    fp.close();
  } else {
    server.send(404, "text/plain", "File not found.");
  }
}

void static_init() {
  server.on("/", [](){
    check_auth()
    String path = config.wifi.mode == M_ACCESSPOINT && memcmp(config.wifi.accesspoint.ssid, "wsx100-", 7) == 0? "/settings" : "/quickstart";
    server.sendHeader("Location", path);
    server.send(302, "text/plain", String("Redirect to ") + path);
  });
  
  server.on("/quickstart", [](){ static_serve(TYPE_HTML, "/quickstart.html.gz"); });
  server.on("/dashboard", [](){ static_serve(TYPE_HTML, "/dashboard.html.gz"); });
  server.on("/settings", [](){ static_serve(TYPE_HTML, "/settings.html.gz"); });
  server.on("/documentation", [](){ static_serve(TYPE_HTML, "/documentation.html.gz"); });
  server.on("/troubleshoot", [](){ static_serve(TYPE_HTML, "/troubleshoot.html.gz"); });
  server.on("/about", [](){ static_serve(TYPE_HTML, "/about.html.gz"); });
  
  server.on("/js/axios.min.js", [](){ static_serve(TYPE_JS, "/js/axios.min.js.gz"); });
  server.on("/js/clipboard.min.js", [](){ static_serve(TYPE_JS, "/js/clipboard.min.js.gz"); });
  server.on("/js/prism.min.js", [](){ static_serve(TYPE_JS, "/js/prism.min.js.gz"); });
  server.on("/js/vue.min.js", [](){ static_serve(TYPE_JS, "/js/vue.min.js.gz"); });
  server.on("/js/vue-cookies.min.js", [](){ static_serve(TYPE_JS, "/js/vue-cookies.min.js.gz"); });
}


void setup() {
  // Initialize early subsystems
  {
    cmd_init();
    daisy_init();
    ecc_init();

    // Get wifi info from ESP
    state.wifi.chipid = ESP.getChipId();
    WiFi.macAddress(state.wifi.mac);

    // Set default ap name
    sprintf(config.wifi.accesspoint.ssid, "wsx100-ap-%02x%02x%02x", state.wifi.mac[3], state.wifi.mac[4], state.wifi.mac[5]);
  }

  // Initialize FS
  {
    SPIFFS.begin();
    
    // Check reset
    pinMode(RESET_PIN, INPUT_PULLUP);
    unsigned long startcheck = millis();
    while (!digitalRead(RESET_PIN) && timesince(startcheck, millis()) < RESET_TIMEOUT) {
      ESP.wdtFeed();
      delay(1);
    }
    if (timesince(startcheck, millis()) >= RESET_TIMEOUT) {
      // Reset chosen
      Serial.println("Factory reset");
      resetcfg();

      // Wait for reset to depress
      while (!digitalRead(RESET_PIN)) {
        delay(100);
      }
    }
  }

  // Read configuration
  {
    wificfg_read(&config.wifi);
    servicecfg_read(&config.service);
    daisycfg_read(&config.daisy);

    // Load queues
    for (int i = 1; i < QS_SIZE; i++) {
      queuecfg_read(i);
    }
  }

  // Wifi connection
  {
    WiFi.persistent(false);
    WiFi.hostname(config.service.hostname);
    
    if (!config.io.wifiled.usercontrol) {
      pinMode(WIFI_LEDPIN, OUTPUT);
      digitalWrite(WIFI_LEDPIN, HIGH);
    }
    
    if (!wificfg_connect(config.wifi.mode, &config.wifi)) {
      switch (config.wifi.mode) {
        case M_STATION: {
          // Station could not connect, revert to AP or off
          if (config.wifi.station.revertap)   wificfg_connect(M_ACCESSPOINT, &config.wifi);
          else                                wificfg_connect(M_OFF, &config.wifi);
          break;
        }
      }
    }
  }

  // Read configuration
  /*{
    servicecfg_read(&config.service);
    daisycfg_read(&config.daisy);

    // Load queues
    for (int i = 1; i < QS_SIZE; i++) {
      queuecfg_read(i);
    }
  }*/

  // Initialize web services
  {
    lowcom_init();
    
    if (config.service.mdns.enabled) {
      MDNS.begin(config.service.hostname);
      if (config.service.http.enabled) MDNS.addService("http", "tcp", PORT_HTTP);
      if (config.service.lowcom.enabled) MDNS.addService("lowcom", "tcp", PORT_LOWCOM);
    }

    if (config.service.ota.enabled) {
      ArduinoOTA.setHostname(config.service.hostname);
      if (config.service.ota.password[0] != 0)
        ArduinoOTA.setPassword(config.service.ota.password);
      ArduinoOTA.begin();
    }

    api_init();
    static_init();
    update_init();
    server.begin();
    websocket_init();
    mqtt_init();
  }

  // Initialize SPI and Stepper Motor config
  {
    ps_spiinit();

    // Read motor config
    File fp = SPIFFS.open(FNAME_MOTORCFG, "r");
    if (fp) {
      size_t size = fp.size();
      if (size <= FILE_MAXSIZE) {
        char buf[size+1];
        fp.readBytes(buf, size);
        buf[size] = 0;
        
        fp.readBytes(buf, size);
        cmd_setconfig(Q0, nextid(), buf);
      }
      fp.close();
      
    } else {
      // No motor config, send default
      cmd_setconfig(Q0, nextid(), "");
    }
  }

  // Run initial queue (queue 1)
  {
    cmdq_copy(Q0, nextid(), queue_get(1));
  }
}

#define HANDLE_LOOPS()     ({ yield(); lowcom_loop(now); daisy_loop(now); ecc_loop(now); cmd_loop(now); yield(); })
void loop() {
  unsigned long now = millis();

  HANDLE_LOOPS();

#ifdef MAINLOOP_DEBUG
  if ((millis() - now) > 100) Serial.printf("Mainloop warn after HL 1: %lu\n", millis() - now);
#endif

  if (state.wifi.mode != M_OFF && config.service.http.enabled) {
    if (!config.service.auth.enabled) {
      // Websockets aren't supported under auth, client must use http
      websocket.loop();
    }
    server.handleClient();
  }

#ifdef MAINLOOP_DEBUG
  if ((millis() - now) > 100) Serial.printf("Mainloop warn after http: %lu\n", millis() - now);
#endif

  HANDLE_LOOPS();

#ifdef MAINLOOP_DEBUG
  if ((millis() - now) > 100) Serial.printf("Mainloop warn after HL 2: %lu\n", millis() - now);
#endif

  mqtt_loop(now);

#ifdef MAINLOOP_DEBUG
  if ((millis() - now) > 100) Serial.printf("Mainloop warn after mqtt: %lu\n", millis() - now);
#endif

  HANDLE_LOOPS();

#ifdef MAINLOOP_DEBUG
  if ((millis() - now) > 100) Serial.printf("Mainloop warn after HL 3: %lu\n", millis() - now);
#endif

  if (state.wifi.mode != M_OFF && config.service.mdns.enabled) {
    MDNS.update();
  }

  if (state.wifi.mode != M_OFF && config.service.ota.enabled) {
    ArduinoOTA.handle();
  }

#ifdef MAINLOOP_DEBUG
  if ((millis() - now) > 100) Serial.printf("Mainloop warn after aux services: %lu\n", millis() - now);
#endif

  // Reboot if requested
  if (flag_reboot) {
    SPIFFS.end();
    // Delay for a second before restarting
    for (size_t i = 0; i < 1000; i++) {
      yield();
      ESP.wdtFeed();
      delay(1);
    }
    ESP.restart();
  }

  cmd_update(now);
  daisy_update(now);
  lowcom_update(now);
  wificfg_update(now);
  yield();

#ifdef MAINLOOP_DEBUG
  if ((millis() - now) > 100) Serial.printf("Mainloop warn after update: %lu\n", millis() - now);
#endif

  // Handle wifi LED blinks
  if (!config.io.wifiled.usercontrol) {
    bool isoff = true;
    if (config.wifi.mode == M_ACCESSPOINT) {
      isoff = ((now / 200) % 20) == 1;
    } else if (config.wifi.mode == M_STATION) {
      isoff = ((now / 200) % 20) != 1;
    }
    digitalWrite(WIFI_LEDPIN, isoff? HIGH : LOW);
  }

#ifdef MAINLOOP_DEBUG
  if ((millis() - now) > 100) Serial.printf("Mainloop warn at end: %lu\n", millis() - now);
#endif
}

