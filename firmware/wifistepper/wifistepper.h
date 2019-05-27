#ifndef __WIFISTEPPER_H
#define __WIFISTEPPER_H

#include <ArduinoJson.h>

#include "powerstep01.h"

#define PRODUCT           "Wi-Fi Stepper"
#define MODEL             "wsx100"
#define BRANCH            "stable"
#define VERSION           (1)

#define RESET_PIN         (5)
#define RESET_TIMEOUT     (3000)
#define WIFI_LEDPIN       (16)
#define WIFI_ADCCOEFF     (0.003223) /*(0.003125)*/
#define MOTOR_RSENSE      (0.0675)
#define MOTOR_ADCCOEFF    (2.65625)
#define MOTOR_CLOCK       (CLK_INT16)

#define FILE_MAXSIZE      (768)
#define FNAME_WIFICFG     "/wificfg.json"
#define FNAME_SERVICECFG  "/servicecfg.json"
#define FNAME_DAISYCFG    "/daisycfg.json"
#define FNAME_MOTORCFG    "/motorcfg.json"
#define FNAME_QUEUECFG    "/queue%dcfg.json"

#define PORT_HTTP         (80)
#define PORT_HTTPWS       (81)
#define PORT_LOWCOM       (1000)

#define AP_IP             (IPAddress(192, 168, 4, 1))
#define AP_MASK           (IPAddress(255, 255, 255, 0))

#define LEN_PRODUCT       (36)
#define LEN_INFO          (36)
#define LEN_HOSTNAME      (24)
#define LEN_USERNAME      (64)
#define LEN_URL           (256)
#define LEN_SSID          (32)
#define LEN_PASSWORD      (64)
#define LEN_IP            (16)
#define LEN_MESSAGE       (64)

#define TIME_MQTT_RECONNECT   30000

typedef struct {
  size_t len, maxlen;
  uint8_t * Q;
} queue_t;

#define QS_SIZE       (16)
#define Q0            (&queue[0])

extern queue_t queue[QS_SIZE];
static inline queue_t * queue_get(uint8_t q) { return q < QS_SIZE? &queue[q] : NULL; }

#define ID_START      (1)
typedef uint32_t id_t;
id_t nextid();
id_t currentid();

unsigned long timesince(unsigned long t1, unsigned long t2);

#define add_headers() \
  server.sendHeader("Access-Control-Allow-Credentials", "true"); \
  server.sendHeader("Access-Control-Allow-Origin", "*"); \
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST"); \
  server.sendHeader("Access-Control-Allow-Headers", "Authorization, application/json");

#define check_auth() \
  if (config.service.auth.enabled && !server.authenticate(config.service.auth.username, config.service.auth.password)) { \
    return server.requestAuthentication(); \
  }


typedef enum {
  M_OFF = 0x0,
  M_ACCESSPOINT = 0x1,
  M_STATION = 0x2
} wifi_mode;

#define ispacked __attribute__((packed))

typedef struct {
  wifi_mode mode;
  struct {
    char ssid[LEN_SSID];
    char password[LEN_PASSWORD];
    bool encryption;
    int channel;
    bool hidden;
  } accesspoint;
  struct {
    char ssid[LEN_SSID];
    char password[LEN_PASSWORD];
    bool encryption;
    char forceip[LEN_IP];
    char forcesubnet[LEN_IP];
    char forcegateway[LEN_IP];
    bool revertap;
  } station;
} wifi_config;

typedef struct {
  char hostname[LEN_HOSTNAME];
  char security[LEN_INFO];
  struct {
    bool enabled;
  } http;
  struct {
    bool enabled;
  } mdns;
  struct {
    bool enabled;
    char username[LEN_USERNAME];
    char password[LEN_PASSWORD];
  } auth;
  struct {
    bool enabled;
    bool std_enabled;
    bool crypto_enabled;
  } lowcom;
  struct {
    bool enabled;
    char server[LEN_URL];
    int port;
    bool auth_enabled;
    char username[LEN_USERNAME];
    char password[LEN_PASSWORD];
    char state_topic[LEN_URL];
    float state_publish_period;
    char command_topic[LEN_URL];
  } mqtt;
  struct {
    bool enabled;
    char password[LEN_PASSWORD];
  } ota;
} service_config;

typedef struct ispacked {
  struct {
    bool usercontrol;
    bool is_output;
  } wifiled;
} io_config;

typedef struct {
  bool enabled;
  bool master;
  bool slavewifioff;
} daisy_config;

typedef struct ispacked {
  ps_mode mode;
  ps_stepsize stepsize;
  float ocd;
  bool ocdshutdown;
  float maxspeed;
  float minspeed;
  float accel, decel;
  float fsspeed;
  bool fsboost;
  struct {
    float kthold;
    float ktrun;
    float ktaccel;
    float ktdecel;
    float switchperiod;
    bool predict;
    float minon;
    float minoff;
    float fastoff;
    float faststep;
  } cm;
  struct {
    float kthold;
    float ktrun;
    float ktaccel;
    float ktdecel;
    float pwmfreq;
    float stall;
    bool volt_comp;
    float bemf_slopel;
    float bemf_speedco;
    float bemf_slopehacc;
    float bemf_slopehdec;
  } vm;
  bool reverse;
} motor_config;

typedef struct {
  wifi_config wifi;
  service_config service;
  daisy_config daisy;
  io_config io;
  motor_config motor;
} config_t;


#define ESUB_UNK      (0x00)
#define ESUB_WIFI     (0x01)
#define ESUB_CMD      (0x02)
#define ESUB_MOTOR    (0x03)
#define ESUB_DAISY    (0x04)
#define ESUB_LC       (0x05)
#define ESUB_HTTP     (0x06)
#define ESUB_MQTT     (0x07)

#define ETYPE_UNK     (0x00)
#define ETYPE_MEM     (0x01)
#define ETYPE_NOQUEUE (0x02)
#define ETYPE_IBUF    (0x03)
#define ETYPE_OBUF    (0x04)
#define ETYPE_MSG     (0x05)

void seterror(uint8_t subsystem = ESUB_UNK, id_t onid = 0, int type = ETYPE_UNK, int8_t arg = -1);
void clearerror();

typedef struct ispacked {
  bool errored;
  unsigned long when;
  uint8_t subsystem;
  id_t id;
  int type;
  int8_t arg;
} error_state;

typedef struct ispacked {
  id_t this_command;
  id_t last_command;
  unsigned int last_completed;
} command_state;

typedef struct ispacked {
  wifi_mode mode;
  uint32_t ip;
  uint8_t mac[6];
  uint32_t chipid;
  long rssi;
} wifi_state;

typedef struct {
  struct {
    int clients;
  } lowcom;
  struct {
    int connected;
  } mqtt;
  struct {
    bool fault;
    bool probed;
    bool available;
    bool provisioned;
  } crypto;
} service_state;

typedef struct {
  bool active;
  uint8_t slaves;
} daisy_state;

typedef struct ispacked {
  ps_status status;
  float stepss;
  int pos;
  int mark;
  float vin;
} motor_state;

typedef struct {
  error_state error;
  command_state command;
  wifi_state wifi;
  service_state service;
  daisy_state daisy;
  motor_state motor;
} state_t;

typedef struct {
  struct {
    unsigned long connection_check;
    unsigned long revertap;
    unsigned long rssi;
  } last;
} wifi_sketch;

typedef struct {
  struct {
    struct {
      unsigned long ping;
    } last;
  } lowcom;
  struct {
    int mark;
  } crypto;
  struct {
    struct {
      unsigned long connect;
      unsigned long publish;
    } last;
  } mqtt;
} service_sketch;

typedef struct ispacked {
  char product[LEN_PRODUCT];
  char model[LEN_INFO];
  char branch[LEN_INFO];
  uint16_t version;
  io_config io;
  motor_config motor;
} daisy_slaveconfig;

typedef struct ispacked {
  error_state error;
  command_state command;
  wifi_state wifi;
  motor_state motor;
} daisy_slavestate;

typedef struct {
  daisy_slaveconfig config;
  daisy_slavestate state;
} daisy_slave_t;

typedef struct {
  daisy_slave_t * slave;
  struct {
    unsigned long ping_rx;
    unsigned long ping_tx;
    unsigned long config;
    unsigned long state;
  } last;
} daisy_sketch;

typedef struct {
  bool iserror;
  char message[LEN_MESSAGE];
  uint8_t ontype;
  uint8_t preamble[123];
  size_t preamblelen, length;
  char imagemd5[33];
  unsigned int files;
} update_sketch;

typedef struct {
  struct {
    unsigned int status;
    unsigned int state;
  } last;
} motor_sketch;

typedef struct {
  wifi_sketch wifi;
  service_sketch service;
  daisy_sketch daisy;
  update_sketch update;
  motor_sketch motor;
} sketch_t;


// Command structs
typedef struct ispacked {
  id_t id;
  uint8_t opcode;
} cmd_head_t;

typedef struct ispacked {
  bool hiz;
  bool soft;
} cmd_stop_t;

typedef struct ispacked {
  ps_direction dir;
  float stepss;
} cmd_run_t;

typedef struct ispacked {
  ps_direction dir;
} cmd_stepclk_t;

typedef struct ispacked {
  ps_direction dir;
  uint32_t microsteps;
} cmd_move_t;

typedef struct ispacked {
  bool hasdir;
  ps_direction dir;
  int32_t pos;
} cmd_goto_t;

typedef struct ispacked {
  ps_posact action;
  ps_direction dir;
  float stepss;
} cmd_gountil_t;

typedef struct ispacked {
  ps_posact action;
  ps_direction dir;
} cmd_releasesw_t;

typedef struct ispacked {
  int32_t pos;
} cmd_setpos_t;

typedef struct ispacked {
  uint32_t ms;
} cmd_waitms_t;

typedef struct ispacked {
  bool state;
} cmd_waitsw_t;

typedef struct ispacked {
  uint8_t targetqueue;
} cmd_runqueue_t;


void cmd_init();
void cmd_loop(unsigned long now);
void cmd_update(unsigned long now);

// Commands for local Queue
//bool cmd_nop(queue_t * q, id_t id);
bool cmd_estop(id_t id, bool hiz, bool soft);
void cmd_clearerror();
bool cmd_runqueue(queue_t * q, id_t id, uint8_t targetqueue);
bool cmd_stop(queue_t * q, id_t id, bool hiz, bool soft);
bool cmd_run(queue_t * q, id_t id, ps_direction dir, float stepss);
bool cmd_stepclock(queue_t * q, id_t id, ps_direction dir);
bool cmd_move(queue_t * q, id_t id, ps_direction dir, uint32_t microsteps);
bool cmd_goto(queue_t * q, id_t id, int32_t pos, bool hasdir = false, ps_direction dir = FWD);
bool cmd_gountil(queue_t * q, id_t id, ps_posact action, ps_direction dir, float stepss);
bool cmd_releasesw(queue_t * q, id_t id, ps_posact action, ps_direction dir);
bool cmd_gohome(queue_t * q, id_t id);
bool cmd_gomark(queue_t * q, id_t id);
bool cmd_resetpos(queue_t * q, id_t id);
bool cmd_setpos(queue_t * q, id_t id, int32_t pos);
bool cmd_setmark(queue_t * q, id_t id, int32_t mark);
bool cmd_setconfig(queue_t * q, id_t id, const char * data);
bool cmd_waitbusy(queue_t * q, id_t id);
bool cmd_waitrunning(queue_t * q, id_t id);
bool cmd_waitms(queue_t * q, id_t id, uint32_t ms);
bool cmd_waitswitch(queue_t * q, id_t id, bool state);

void cmdq_read(JsonArray& arr, uint8_t target, uint8_t queue);
void cmdq_read(JsonArray& arr, uint8_t target);
void cmdq_read(JsonArray& arr);
void cmdq_write(JsonArray& arr, queue_t * queue);
bool cmdq_empty(queue_t * q, id_t id);
bool cmdq_copy(queue_t * q, id_t id, queue_t * sourcequeue);


void daisy_init();
void daisy_loop(unsigned long now);
void daisy_update(unsigned long now);

// Remote management commands
bool daisy_clearerror(uint8_t target, id_t id);
bool daisy_wificontrol(uint8_t target, id_t id, bool enabled);

// Remote queue commands
bool daisy_stop(uint8_t target, uint8_t q, id_t id, bool hiz, bool soft);
bool daisy_run(uint8_t target, uint8_t q, id_t id, ps_direction dir, float stepss);
bool daisy_stepclock(uint8_t target, uint8_t q, id_t id, ps_direction dir);
bool daisy_move(uint8_t target, uint8_t q, id_t id, ps_direction dir, uint32_t microsteps);
bool daisy_goto(uint8_t target, uint8_t q, id_t id, int32_t pos, bool hasdir = false, ps_direction dir = FWD);
bool daisy_gountil(uint8_t target, uint8_t q, id_t id, ps_posact action, ps_direction dir, float stepss);
bool daisy_releasesw(uint8_t target, uint8_t q, id_t id, ps_posact action, ps_direction dir);
bool daisy_gohome(uint8_t target, uint8_t q, id_t id);
bool daisy_gomark(uint8_t target, uint8_t q, id_t id);
bool daisy_resetpos(uint8_t target, uint8_t q, id_t id);
bool daisy_setpos(uint8_t target, uint8_t q, id_t id, int32_t pos);
bool daisy_setmark(uint8_t target, uint8_t q, id_t id, int32_t mark);
bool daisy_setconfig(uint8_t target, uint8_t q, id_t id, const char * data);
bool daisy_waitbusy(uint8_t target, uint8_t q, id_t id);
bool daisy_waitrunning(uint8_t target, uint8_t q, id_t id);
bool daisy_waitms(uint8_t target, uint8_t q, id_t id, uint32_t millis);
bool daisy_waitswitch(uint8_t target, uint8_t q, id_t id, bool state);
bool daisy_runqueue(uint8_t target, uint8_t q, id_t id, uint8_t targetqueue);

bool daisy_emptyqueue(uint8_t target, uint8_t q, id_t id);
bool daisy_copyqueue(uint8_t target, uint8_t q, id_t id, uint8_t sourcequeue);
bool daisy_savequeue(uint8_t target, uint8_t q, id_t id);
bool daisy_loadqueue(uint8_t target, uint8_t q, id_t id);
bool daisy_estop(uint8_t target, id_t id, bool hiz, bool soft);


void lowcom_init();
void lowcom_loop(unsigned long now);
void lowcom_update(unsigned long now);
void lowcom_ecc_hmacend(uint8_t * sha, uint8_t * data, size_t datalen);
void lowcom_senderror(error_state * e);

void resetcfg();

void wificfg_read(wifi_config * cfg);
void wificfg_write(wifi_config * const cfg);
bool wificfg_connect(wifi_mode mode, wifi_config * const cfg);
void wificfg_update(unsigned long now);

void servicecfg_read(service_config * cfg);
void servicecfg_write(service_config * const cfg);

void daisycfg_read(daisy_config * cfg);
void daisycfg_write(daisy_config * const cfg);

bool queuecfg_read(uint8_t qid);
bool queuecfg_write(uint8_t qid);
bool queuecfg_reset(uint8_t qid);

void motorcfg_read(motor_config * cfg);
void motorcfg_write(motor_config * const cfg);
void motorcfg_pull(motor_config * cfg);
void motorcfg_push(motor_config * const cfg);


void api_init();
void websocket_init();
void update_init();

void mqtt_init();
void mqtt_loop(unsigned long looptime);

// Mux Functions
static inline bool m_estop(uint8_t target, id_t id, bool hiz, bool soft) { if (target == 0) { return cmd_estop(id, hiz, soft); } else { return daisy_estop(target, id, hiz, soft); } }
static inline bool m_clearerror(uint8_t target, id_t id) { if (target == 0) { clearerror(); return true; } else { return daisy_clearerror(target, id); } }
static inline bool m_setconfig(uint8_t target, uint8_t q, id_t id, const char * data) { if (target == 0) { return cmd_setconfig(queue_get(q), id, data); } else { return daisy_setconfig(target, q, id, data); } }
static inline bool m_stop(uint8_t target, uint8_t q, id_t id, bool hiz, bool soft) { if (target == 0) { return cmd_stop(queue_get(q), id, hiz, soft); } else { return daisy_stop(target, q, id, hiz, soft); } }
static inline bool m_run(uint8_t target, uint8_t q, id_t id, ps_direction dir, float stepss) { if (target == 0) { return cmd_run(queue_get(q), id, dir, stepss); } else { return daisy_run(target, q, id, dir, stepss); } }
static inline bool m_stepclock(uint8_t target, uint8_t q, id_t id, ps_direction dir) { if (target == 0) { return cmd_stepclock(queue_get(q), id, dir); } else { return daisy_stepclock(target, q, id, dir); } }
static inline bool m_move(uint8_t target, uint8_t q, id_t id, ps_direction dir, uint32_t microsteps) { if (target == 0) { return cmd_move(queue_get(q), id, dir, microsteps); } else { return daisy_move(target, q, id, dir, microsteps); } }
static inline bool m_goto(uint8_t target, uint8_t q, id_t id, int32_t pos, bool hasdir = false, ps_direction dir = FWD) { if (target == 0) { return cmd_goto(queue_get(q), id, pos, hasdir, dir); } else { return daisy_goto(target, q, id, pos, hasdir, dir); } }
static inline bool m_gountil(uint8_t target, uint8_t q, id_t id, ps_posact action, ps_direction dir, float stepss) { if (target == 0) { return cmd_gountil(queue_get(q), id, action, dir, stepss); } else { return daisy_gountil(target, q, id, action, dir, stepss); } }
static inline bool m_releasesw(uint8_t target, uint8_t q, id_t id, ps_posact action, ps_direction dir) { if (target == 0) { return cmd_releasesw(queue_get(q), id, action, dir); } else { return daisy_releasesw(target, q, id, action, dir); } }
static inline bool m_gohome(uint8_t target, uint8_t q, id_t id) { if (target == 0) { return cmd_gohome(queue_get(q), id); } else { return daisy_gohome(target, q, id); } }
static inline bool m_gomark(uint8_t target, uint8_t q, id_t id) { if (target == 0) { return cmd_gomark(queue_get(q), id); } else { return daisy_gomark(target, q, id); } }
static inline bool m_resetpos(uint8_t target, uint8_t q, id_t id) { if (target == 0) { return cmd_resetpos(queue_get(q), id); } else { return daisy_resetpos(target, q, id); } }
static inline bool m_setpos(uint8_t target, uint8_t q, id_t id, int32_t pos) { if (target == 0) { return cmd_setpos(queue_get(q), id, pos); } else { return daisy_setpos(target, q, id, pos); } }
static inline bool m_setmark(uint8_t target, uint8_t q, id_t id, int32_t mark) { if (target == 0) { return cmd_setmark(queue_get(q), id, mark); } else { return daisy_setmark(target, q, id, mark); } }
static inline bool m_waitbusy(uint8_t target, uint8_t q, id_t id) { if (target == 0) { return cmd_waitbusy(queue_get(q), id); } else { return daisy_waitbusy(target, q, id); } }
static inline bool m_waitrunning(uint8_t target, uint8_t q, id_t id) { if (target == 0) { return cmd_waitrunning(queue_get(q), id); } else { return daisy_waitrunning(target, q, id); } }
static inline bool m_waitms(uint8_t target, uint8_t q, id_t id, uint32_t ms) { if (target == 0) { return cmd_waitms(queue_get(q), id, ms); } else { return daisy_waitms(target, q, id, ms); } }
static inline bool m_waitswitch(uint8_t target, uint8_t q, id_t id, bool state) { if (target == 0) { return cmd_waitswitch(queue_get(q), id, state); } else { return daisy_waitswitch(target, q, id, state); } }
static inline bool m_runqueue(uint8_t target, uint8_t q, id_t id, uint8_t targetqueue) { if (target == 0) { return cmd_runqueue(queue_get(q), id, targetqueue); } else { return daisy_runqueue(target, q, id, targetqueue); } }
static inline bool m_emptyqueue(uint8_t target, uint8_t q, id_t id) { if (target == 0) { return cmdq_empty(queue_get(q), id); } else { return daisy_emptyqueue(target, q, id); } }
static inline bool m_savequeue(uint8_t target, uint8_t q, id_t id) { if (target == 0) { return queuecfg_write(q); } else { return daisy_savequeue(target, q, id); } }
static inline bool m_loadqueue(uint8_t target, uint8_t q, id_t id) { if (target == 0) { return queuecfg_read(q); } else { return daisy_loadqueue(target, q, id); } }
static inline bool m_copyqueue(uint8_t target, uint8_t q, id_t id, uint8_t sourcequeue) { if (target == 0) { return cmdq_copy(queue_get(q), id, queue_get(sourcequeue)); } else { return daisy_copyqueue(target, q, id, sourcequeue); } }


// Utility functions
extern config_t config;
extern state_t state;
extern sketch_t sketch;

static inline ps_direction motorcfg_dir(ps_direction d) {
  if (!config.motor.reverse)  return d;
  switch (d) {
    case FWD: return REV;
    case REV: return FWD;
  }
}

static inline int32_t motorcfg_pos(int32_t p) {
  return config.motor.reverse? -p : p;
}

static inline wifi_mode parse_wifimode(const char * m) {
  if (strcmp(m, "station") == 0)  return M_STATION;
  else                            return M_ACCESSPOINT;
}

static inline int parse_channel(const String& c, int d) {
  int i = c.toInt();
  return i > 0 && i <= 13? i : d;
}

static inline ps_mode parse_motormode(const String& m, ps_mode d) {
  if (m == "voltage")       return MODE_VOLTAGE;
  else if (m == "current")  return MODE_CURRENT;
  else                      return d;
}

static inline ps_stepsize parse_stepsize(int s, ps_stepsize d) {
  switch (s) {
    case 1:   return STEP_1;
    case 2:   return STEP_2;
    case 4:   return STEP_4;
    case 8:   return STEP_8;
    case 16:  return STEP_16;
    case 32:  return STEP_32;
    case 64:  return STEP_64;
    case 128: return STEP_128;
    default:  return d;
  }
}

static inline ps_direction parse_direction(const String& s, ps_direction d) {
  if (s == "forward")       return FWD;
  else if (s == "reverse")  return REV;
  else                      return d;
}

static inline ps_posact parse_action(const String& s, ps_posact d) {
  if (s == "reset")         return POS_RESET;
  else if (s == "copymark") return POS_COPYMARK;
  else                      return d;
}

// JSON functions
static inline const char * json_serialize(wifi_mode m) {
  switch (m) {
    case M_OFF:         return "off";
    case M_ACCESSPOINT: return "accesspoint";
    case M_STATION:     return "station";
    default:            return "";
  }
}

static inline const char * json_serialize(ps_mode m) {
  switch (m) {
    case MODE_VOLTAGE:  return "voltage";
    case MODE_CURRENT:  return "current";
    default:            return "";
  }
}

static inline int json_serialize(ps_stepsize s) {
  switch (s) {
    case STEP_1:    return 1;
    case STEP_2:    return 2;
    case STEP_4:    return 4;
    case STEP_8:    return 8;
    case STEP_16:   return 16;
    case STEP_32:   return 32;
    case STEP_64:   return 64;
    case STEP_128:  return 128;
    default:        return 0;
  }
}

static inline const char * json_serialize(ps_direction d) {
  switch (d) {
    case FWD:   return "forward";
    case REV:   return "reverse";
    default:    return "";
  }
}

static inline const char * json_serialize(ps_posact a) {
  switch (a) {
    case POS_RESET:     return "reset";
    case POS_COPYMARK:  return "copymark";
    default:            return "";
  }
}

static inline const char * json_serialize(ps_movement m) {
  switch (m) {
    case M_STOPPED:     return "idle";
    case M_ACCEL:       return "accelerating";
    case M_DECEL:       return "decelerating";
    case M_CONSTSPEED:  return "spinning";
    default:            return "";
  }
}

#endif
