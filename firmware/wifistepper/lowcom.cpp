#include <Arduino.h>
//#include <ArduinoJson.h>
#include <ESP8266WiFi.h>

#include "wifistepper.h"
#include "ecc508a.h"

//#define LOWCOM_DEBUG

extern StaticJsonBuffer<2560> jsonbuf;
extern volatile bool flag_reboot;

#define LTC_SIZE      (2)

#define LTCB_ISIZE    (1024)
#define LTCB_OSIZE    (LTCB_ISIZE / 4)

// PACKET LAYOUT
#define L_MAGIC_1     (0xAE)
#define L_MAGIC_2     (0x7B11)

typedef struct ispacked {
  uint8_t magic1;
  uint16_t magic2;
  uint8_t type;
} lc_preamble;

typedef struct ispacked {
  uint8_t hmac[32];
} lc_crypto;

typedef struct ispacked {
  uint8_t opcode;
  uint8_t subcode;
  uint8_t target;
  uint8_t queue;
  uint16_t packetid;
  uint16_t length;
} lc_header;

#define TYPE_ERROR        (0x00)
#define TYPE_HELLO        (0x01)
#define TYPE_GOODBYE      (0x02)
#define TYPE_PING         (0x03)
#define TYPE_STD          (0x04)
#define TYPE_CRYPTO       (0x05)
#define TYPE_MAX          TYPE_CRYPTO

typedef struct ispacked {
  char product[LEN_PRODUCT];
  char model[LEN_INFO];
  char swbranch[LEN_INFO];
  uint16_t version;
  char hostname[LEN_HOSTNAME];
  uint32_t chipid;
  bool std_enabled;
  bool crypto_enabled;
  uint32_t nonce;
} type_hello;

#define MODE_STD            (0x00)
#define MODE_CRYPTO         (0x01)

#define TARGET_CLIENT       (0x00)
#define TARGET_COMMAND      (0x01)

typedef struct ispacked {
  uint8_t client;
  uint8_t target;
} lc_cryptometa_t;

#define OPCODE_ESTOP        (0x00)
#define OPCODE_PING         (0x01)
#define OPCODE_CLEARERROR   (0x02)
#define OPCODE_LASTWILL     (0x03)
#define OPCODE_SETHTTP      (0x04)
#define OPCODE_RUNQUEUE     (0x05)
#define OPCODE_SETCONFIG    (0x06)
#define OPCODE_GETCONFIG    (0x07)
#define OPCODE_GETSTATE     (0x08)


#define OPCODE_STOP         (0x11)
#define OPCODE_RUN          (0x12)
#define OPCODE_STEPCLOCK    (0x13)
#define OPCODE_MOVE         (0x14)
#define OPCODE_GOTO         (0x15)
#define OPCODE_GOUNTIL      (0x16)
#define OPCODE_RELEASESW    (0x17)
#define OPCODE_GOHOME       (0x18)
#define OPCODE_GOMARK       (0x19)
#define OPCODE_RESETPOS     (0x1A)
#define OPCODE_SETPOS       (0x1B)
#define OPCODE_SETMARK      (0x1C)

#define OPCODE_WAITBUSY     (0x21)
#define OPCODE_WAITRUNNING  (0x22)
#define OPCODE_WAITMS       (0x23)
#define OPCODE_WAITSWITCH   (0x24)

#define OPCODE_EMPTYQUEUE   (0x31)
#define OPCODE_SAVEQUEUE    (0x32)
#define OPCODE_LOADQUEUE    (0x33)
#define OPCODE_ADDQUEUE     (0x34)
#define OPCODE_COPYQUEUE    (0x35)

#define OPCODE_GETQUEUE     (0x37)


#define SUBCODE_NACK      (0x00)
#define SUBCODE_ACK       (0x01)
#define SUBCODE_CMD       (0x02)
#define SUBCODE_REPLY     (0x03)

#define LTO_PING          (5000)

#ifdef LOWCOM_DEBUG
void lc_debug(String msg) {
  Serial.print("(");
  Serial.print(millis());
  Serial.print(") LOWCOM -> ");
  Serial.println(msg);
}
void lc_debug(String msg, int i1) {
  Serial.print("(");
  Serial.print(millis());
  Serial.print(") LOWCOM -> ");
  Serial.print(msg);
  Serial.print(" (");
  Serial.print(i1);
  Serial.println(")");
}
void lc_debug(String msg, int i1, float f1) {
  Serial.print("(");
  Serial.print(millis());
  Serial.print(") LOWCOM -> ");
  Serial.print(msg);
  Serial.print(" (");
  Serial.print(i1);
  Serial.print(", ");
  Serial.print(f1);
  Serial.println(")");
}
void lc_debug(String msg, int i1, int i2) {
  Serial.print("(");
  Serial.print(millis());
  Serial.print(") LOWCOM -> ");
  Serial.print(msg);
  Serial.print(" (");
  Serial.print(i1);
  Serial.print(", ");
  Serial.print(i2);
  Serial.println(")");
}
void lc_debug(String msg, int i1, int i2, int i3) {
  Serial.print("(");
  Serial.print(millis());
  Serial.print(") LOWCOM -> ");
  Serial.print(msg);
  Serial.print(" (");
  Serial.print(i1);
  Serial.print(", ");
  Serial.print(i2);
  Serial.print(", ");
  Serial.print(i3);
  Serial.println(")");
}
void lc_debug(String msg, int i1, int i2, int i3, int i4, int i5) {
  Serial.print("(");
  Serial.print(millis());
  Serial.print(") LOWCOM -> ");
  Serial.print(msg);
  Serial.print(" (");
  Serial.print(i1);
  Serial.print(", ");
  Serial.print(i2);
  Serial.print(", ");
  Serial.print(i3);
  Serial.print(", ");
  Serial.print(i4);
  Serial.print(", ");
  Serial.print(i5);
  Serial.println(")");
}
void lc_debug(const char * name, uint8_t * sha) {
  Serial.print("(");
  Serial.print(millis());
  Serial.print(") LOWCOM -> ");
  Serial.print(name); Serial.print(": (sha256) ");
  for (size_t i = 0; i < 32; i++) Serial.printf("%02x", sha[i]);
  Serial.println();
}
#else
#define lc_debug(...)
#endif

WiFiServer lowcom_server(PORT_LOWCOM);
struct {
  WiFiClient sock;
  uint8_t I[LTCB_ISIZE];
  uint8_t O[LTCB_OSIZE];
  size_t Ilen, Olen;
  bool active;
  bool initialized;
  uint8_t lastwill;
  struct {
    uint32_t nonce;
    uint16_t packetid;
  } crypto;
  struct {
    unsigned long ping;
  } last;
} lowcom_client[LTC_SIZE];

static inline void lc_packpreamble(lc_preamble * p, uint8_t type) {
  if (p == NULL) return;
  p->magic1 = L_MAGIC_1;
  p->magic2 = L_MAGIC_2;
  p->type = type;
}

static void lc_send(size_t client, uint8_t * data, size_t len) {
  size_t i = 0;
  if (lowcom_client[client].Olen == 0 && lowcom_client[i].sock.availableForWrite() > 0) {
    i = lowcom_client[i].sock.write(data, len);
  }
  if (i != len) {
    if ((len - i) > (LTCB_OSIZE - lowcom_client[client].Olen)) {
      seterror(ESUB_LC, 0, ETYPE_OBUF, client);
      return;
    }
    memcpy(&lowcom_client[client].O[lowcom_client[client].Olen], &data[i], len - i);
    lowcom_client[client].Olen += len - i;
  }
}

static void lc_reply_std(size_t client, uint8_t opcode, uint8_t subcode, uint8_t target, uint8_t queue, uint16_t packetid, uint8_t * data, size_t len) {
  uint8_t packet[sizeof(lc_preamble) + sizeof(lc_header) + len];
  
  lc_preamble * preamble = (lc_preamble *)&packet[0];
  lc_header * header = (lc_header *)&preamble[1];
  uint8_t * payload = (uint8_t *)&header[1];
  
  lc_packpreamble(preamble, TYPE_STD);
  *header = {.opcode = opcode, .subcode = subcode, .target = target, .queue = queue, .packetid = packetid, .length = len};
  memcpy(payload, data, len);
  
  lc_send(client, packet, sizeof(lc_preamble) + sizeof(lc_header) + len);
}

static void lc_reply_crypto(size_t client, uint8_t opcode, uint8_t subcode, uint8_t target, uint8_t queue, uint16_t packetid, uint8_t * data, size_t len) {
  uint8_t packet[sizeof(lc_preamble) + sizeof(lc_crypto) + sizeof(lc_header) + len];
  
  lc_preamble * preamble = (lc_preamble *)&packet[0];
  lc_crypto * crypto = (lc_crypto *)&preamble[1];
  lc_header * header = (lc_header *)&crypto[1];
  uint8_t * payload = (uint8_t *)&header[1];
  
  lc_packpreamble(preamble, TYPE_CRYPTO);
  *header = {.opcode = opcode, .subcode = subcode, .target = target, .queue = queue, .packetid = packetid, .length = len};
  memcpy(payload, data, len);

  lc_cryptometa_t meta = {.client = client, .target = TARGET_CLIENT};
  ecc_lowcom_hmac(lowcom_client[client].crypto.nonce, (uint8_t *)&meta, sizeof(lc_cryptometa_t), (uint8_t *)preamble, sizeof(lc_preamble) + sizeof(lc_crypto), sizeof(lc_preamble) + sizeof(lc_crypto) + sizeof(lc_header) + len);
}

static void lc_reply(size_t client, uint8_t mode, uint8_t opcode, uint8_t subcode, uint8_t target, uint8_t queue, uint16_t packetid, uint8_t * data, size_t len) {
  switch (mode) {
    case MODE_STD:    lc_reply_std(client, opcode, subcode, target, queue, packetid, data, len);     break;
    case MODE_CRYPTO: lc_reply_crypto(client, opcode, subcode, target, queue, packetid, data, len);  break;
  }
}

static void lc_replyack(size_t client, uint8_t mode, uint8_t opcode, uint8_t target, uint8_t queue, uint16_t packetid, id_t id) {
  lc_reply(client, mode, opcode, SUBCODE_ACK, target, queue, packetid, (uint8_t *)&id, sizeof(id_t));
}

static void lc_replynack(size_t client, uint8_t mode, uint8_t opcode, uint8_t target, uint8_t queue, uint16_t packetid, const char * message) {
  lc_reply(client, mode, opcode, SUBCODE_NACK, target, queue, packetid, (uint8_t *)message, strlen(message) + 1);
}

#define lc_expectlen(elen)  ({ if (len != (elen)) { lc_replynack(client, mode, opcode, target, queue, packetid, "Bad message length"); return; } })
static void lc_handlepacket(size_t client, uint8_t mode, uint8_t opcode, uint8_t subcode, uint8_t target, uint8_t queue, uint16_t packetid, uint8_t * data, size_t len) {
  lc_debug("CMD received", opcode, subcode, target, queue, packetid);

  if (subcode != SUBCODE_CMD) {
    lc_debug("ERR bad subcode (expect CMD)");
    lc_replynack(client, mode, opcode, target, queue, packetid, "Bad subcode");
    return;
  }

  // Check if client initialized
  if (!lowcom_client[client].initialized) {
    lc_debug("ERR client not initialized");
    lc_replynack(client, mode, opcode, target, queue, packetid, "Client not initialized (must send HELLO)");
    return;
  }

  id_t id = nextid();
  switch (opcode) {
    case OPCODE_ESTOP: {
      lc_expectlen(sizeof(cmd_stop_t));
      cmd_stop_t * cmd = (cmd_stop_t *)data;
      lc_debug("CMD estop", cmd->hiz, cmd->soft);
      m_estop(target, id, cmd->hiz, cmd->soft);
      lc_replyack(client, mode, opcode, target, queue, packetid, id);
      break;
    }
    case OPCODE_PING: {
      lc_expectlen(0);
      lc_debug("CMD ping");
      lc_replyack(client, mode, opcode, target, queue, packetid, id);
      break;
    }
    case OPCODE_CLEARERROR: {
      lc_expectlen(0);
      lc_debug("CMD clearerror");
      m_clearerror(target, id);
      lc_replyack(client, mode, opcode, target, queue, packetid, id);
      break;
    }
    case OPCODE_LASTWILL: {
      lc_expectlen(0);
      lc_debug("CMD lastwill", queue);
      lowcom_client[client].lastwill = queue;
      lc_replyack(client, mode, opcode, 0, queue, packetid, id);
      break;
    }
    case OPCODE_SETHTTP: {
      lc_expectlen(sizeof(uint8_t));
      lc_debug("CMD sethttp", data[0]);
      config.service.http.enabled = data[0]? true : false;
      lc_replyack(client, mode, opcode, 0, 0, packetid, id);
      break;
    }
    case OPCODE_RUNQUEUE: {
      lc_expectlen(sizeof(uint8_t));
      lc_debug("CMD runqueue", data[0]);
      m_runqueue(target, queue, id, data[0]);
      lc_replyack(client, mode, opcode, target, queue, packetid, id);
      break;
    }
    case OPCODE_SETCONFIG: {
      if (len == 0 || data[len-1] != 0) {
        lc_replynack(client, mode, opcode, target, queue, packetid, "Bad config data");
        return;
      }
      lc_debug("CMD setconfig");
      m_setconfig(target, queue, id, (const char *)data);
      lc_replyack(client, mode, opcode, target, queue, packetid, id);
      break;
    }
    case OPCODE_GETCONFIG: {
      lc_expectlen(0);
      lc_debug("CMD getconfig");
      if (target < 0 || target > state.daisy.slaves) {
        lc_replynack(client, mode, opcode, target, queue, packetid, "Invalid target");
        return;
      }
      motor_config * cfg = target == 0? &config.motor : &sketch.daisy.slave[target - 1].config.motor;
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
      String reply = v.as<String>();
      lc_reply(client, mode, opcode, SUBCODE_REPLY, target, queue, packetid, (uint8_t *)reply.c_str(), reply.length()+1);
      jsonbuf.clear();
      break;
    }
    case OPCODE_GETSTATE: {
      lc_expectlen(0);
      lc_debug("CMD getstate");
      if (target < 0 || target > state.daisy.slaves) {
        lc_replynack(client, mode, opcode, target, queue, packetid, "Invalid target");
        return;
      }
      motor_state * st = target == 0? &state.motor : &sketch.daisy.slave[target - 1].state.motor;
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
      String reply = v.as<String>();
      lc_reply(client, mode, opcode, SUBCODE_REPLY, target, queue, packetid, (uint8_t *)reply.c_str(), reply.length()+1);
      jsonbuf.clear();
      break;
    }
    
    case OPCODE_STOP: {
      lc_expectlen(sizeof(cmd_stop_t));
      cmd_stop_t * cmd = (cmd_stop_t *)data;
      lc_debug("CMD stop", cmd->hiz, cmd->soft);
      m_stop(target, queue, id, cmd->hiz, cmd->soft);
      lc_replyack(client, mode, opcode, target, queue, packetid, id);
      break;
    }
    case OPCODE_RUN: {
      lc_expectlen(sizeof(cmd_run_t));
      cmd_run_t * cmd = (cmd_run_t *)data;
      lc_debug("CMD run", cmd->dir, cmd->stepss);
      m_run(target, queue, id, cmd->dir, cmd->stepss);
      lc_replyack(client, mode, opcode, target, queue, packetid, id);
      break;
    }
    case OPCODE_STEPCLOCK: {
      lc_expectlen(sizeof(cmd_stepclk_t));
      cmd_stepclk_t * cmd = (cmd_stepclk_t *)data;
      lc_debug("CMD stepclock", cmd->dir);
      m_stepclock(target, queue, id, cmd->dir);
      lc_replyack(client, mode, opcode, target, queue, packetid, id);
      break;
    }
    case OPCODE_MOVE: {
      lc_expectlen(sizeof(cmd_move_t));
      cmd_move_t * cmd = (cmd_move_t *)data;
      lc_debug("CMD move", cmd->dir, (float)cmd->microsteps);
      m_move(target, queue, id, cmd->dir, cmd->microsteps);
      lc_replyack(client, mode, opcode, target, queue, packetid, id);
      break;
    }
    case OPCODE_GOTO: {
      lc_expectlen(sizeof(cmd_goto_t));
      cmd_goto_t * cmd = (cmd_goto_t *)data;
      lc_debug("CMD goto", cmd->pos, cmd->hasdir, cmd->dir);
      m_goto(target, queue, id, cmd->pos, cmd->hasdir, cmd->dir);
      lc_replyack(client, mode, opcode, target, queue, packetid, id);
      break;
    }
    case OPCODE_GOUNTIL: {
      lc_expectlen(sizeof(cmd_gountil_t));
      cmd_gountil_t * cmd = (cmd_gountil_t *)data;
      lc_debug("CMD gountil", cmd->action, cmd->dir, cmd->stepss);
      m_gountil(target, queue, id, cmd->action, cmd->dir, cmd->stepss);
      lc_replyack(client, mode, opcode, target, queue, packetid, id);
      break;
    }
    case OPCODE_RELEASESW: {
      lc_expectlen(sizeof(cmd_releasesw_t));
      cmd_releasesw_t * cmd = (cmd_releasesw_t *)data;
      lc_debug("CMD releasesw", cmd->action, cmd->dir);
      m_releasesw(target, queue, id, cmd->action, cmd->dir);
      lc_replyack(client, mode, opcode, target, queue, packetid, id);
      break;
    }
    case OPCODE_GOHOME: {
      lc_expectlen(0);
      lc_debug("CMD gohome");
      m_gohome(target, queue, id);
      lc_replyack(client, mode, opcode, target, queue, packetid, id);
      break;
    }
    case OPCODE_GOMARK: {
      lc_expectlen(0);
      lc_debug("CMD gomark");
      m_gomark(target, queue, id);
      lc_replyack(client, mode, opcode, target, queue, packetid, id);
      break;
    }
    case OPCODE_RESETPOS: {
      lc_expectlen(0);
      lc_debug("CMD resetpos");
      m_resetpos(target, queue, id);
      lc_replyack(client, mode, opcode, target, queue, packetid, id);
      break;
    }
    case OPCODE_SETPOS: {
      lc_expectlen(sizeof(cmd_setpos_t));
      cmd_setpos_t * cmd = (cmd_setpos_t *)data;
      lc_debug("CMD setpos", cmd->pos);
      m_setpos(target, queue, id, cmd->pos);
      lc_replyack(client, mode, opcode, target, queue, packetid, id);
      break;
    }
    case OPCODE_SETMARK: {
      lc_expectlen(sizeof(cmd_setpos_t));
      cmd_setpos_t * cmd = (cmd_setpos_t *)data;
      lc_debug("CMD setpos", cmd->pos);
      m_setmark(target, queue, id, cmd->pos);
      lc_replyack(client, mode, opcode, target, queue, packetid, id);
      break;
    }
    
    case OPCODE_WAITBUSY: {
      lc_expectlen(0);
      lc_debug("CMD waitbusy");
      m_waitbusy(target, queue, id);
      lc_replyack(client, mode, opcode, target, queue, packetid, id);
      break;
    }
    case OPCODE_WAITRUNNING: {
      lc_expectlen(0);
      lc_debug("CMD waitrunning");
      m_waitrunning(target, queue, id);
      lc_replyack(client, mode, opcode, target, queue, packetid, id);
      break;
    }
    case OPCODE_WAITMS: {
      lc_expectlen(sizeof(cmd_waitms_t));
      cmd_waitms_t * cmd = (cmd_waitms_t *)data;
      lc_debug("CMD waitms", cmd->ms);
      m_waitms(target, queue, id, cmd->ms);
      lc_replyack(client, mode, opcode, target, queue, packetid, id);
      break;
    }
    case OPCODE_WAITSWITCH: {
      lc_expectlen(sizeof(cmd_waitsw_t));
      cmd_waitsw_t * cmd = (cmd_waitsw_t *)data;
      lc_debug("CMD waitswitch", cmd->state);
      m_waitswitch(target, queue, id, cmd->state);
      lc_replyack(client, mode, opcode, target, queue, packetid, id);
      break;
    }

    case OPCODE_EMPTYQUEUE: {
      lc_expectlen(0);
      lc_debug("CMD emptyqueue");
      m_emptyqueue(target, queue, id);
      lc_replyack(client, mode, opcode, target, queue, packetid, id);
      break;
    }
    case OPCODE_SAVEQUEUE: {
      lc_expectlen(0);
      lc_debug("CMD savequeue");
      m_savequeue(target, queue, id);
      lc_replyack(client, mode, opcode, target, queue, packetid, id);
      break;
    }
    case OPCODE_LOADQUEUE: {
      lc_expectlen(0);
      lc_debug("CMD loadqueue");
      m_loadqueue(target, queue, id);
      lc_replyack(client, mode, opcode, target, queue, packetid, id);
      break;
    }
    case OPCODE_ADDQUEUE: {
      // TODO
      break;
    }
    case OPCODE_COPYQUEUE: {
      lc_expectlen(sizeof(uint8_t));
      lc_debug("CMD copyqueue", data[0]);
      m_copyqueue(target, queue, id, data[0]);
      lc_replyack(client, mode, opcode, target, queue, packetid, id);
      break;
    }
    case OPCODE_GETQUEUE: {
      // TODO
      break;
    }
  }
}

void lowcom_ecc_hmacend(uint8_t * sha, uint8_t * data, size_t datalen) {
  lc_cryptometa_t * meta = (lc_cryptometa_t *)&data[0];
  

  if (meta->target == TARGET_COMMAND) {
    lc_crypto * crypto = (lc_crypto *)&meta[1];
    lc_header * header = (lc_header *)&crypto[1];

    //lc_debug("Calculated", sha);
    //lc_debug("Given", crypto->hmac);
    if (memcmp(sha, crypto->hmac, 32) != 0) {
      lc_debug("HMAC bad signature");
      lc_replynack(meta->client, MODE_STD, header->opcode, header->target, header->queue, header->packetid, "Bad hmac signature (Key error)");
      return;
    }

    //lc_debug("HMAC valid signature");
    //lc_debug("Crypto packet length", datalen);
    lc_debug("Crypto packet received", header->opcode, header->subcode, header->target, header->queue, header->packetid);
    lc_handlepacket(meta->client, MODE_CRYPTO, header->opcode, header->subcode, header->target, header->queue, header->packetid, (uint8_t *)&header[1], header->length);

  } else if (meta->target == TARGET_CLIENT) {
    lc_preamble * preamble = (lc_preamble *)&meta[1];
    lc_crypto * crypto = (lc_crypto *)&preamble[1];
    lc_header * header = (lc_header *)&crypto[1];

    lc_debug("Send crypto data to client", meta->client);
    
    memcpy(crypto->hmac, sha, 32);
    lc_send(meta->client, (uint8_t *)preamble, datalen - sizeof(lc_cryptometa_t));
  }
}

static size_t lc_handletype(size_t client, uint8_t * data, size_t len) {
  lc_preamble * preamble = (lc_preamble *)data;

  // Check preamble
  if (preamble->magic2 != L_MAGIC_2 || preamble->type > TYPE_MAX) {
    // Bad preamble
    return 1;
  }
  
  switch (preamble->type) {
    case TYPE_HELLO: {
      lc_debug("RX hello");
      uint8_t reply[sizeof(lc_preamble) + sizeof(type_hello)] = {0};

      // Reset client data
      {
        lowcom_client[client].crypto.nonce = ecc_random();
        lowcom_client[client].crypto.packetid = 0;
      }
      
      // Set preamble
      {
        lc_preamble * preamble = (lc_preamble *)(&reply[0]);
        lc_packpreamble(preamble, TYPE_HELLO);
      }
      
      // Set reply payload
      {
        type_hello * payload = (type_hello *)(&reply[sizeof(lc_preamble)]);
        strncpy(payload->product, PRODUCT, LEN_PRODUCT-1);
        strncpy(payload->model, MODEL, LEN_INFO-1);
        strncpy(payload->swbranch, BRANCH, LEN_INFO-1);
        payload->version = VERSION;
        strncpy(payload->hostname, config.service.hostname, LEN_HOSTNAME-1);
        payload->chipid = state.wifi.chipid;
        payload->std_enabled = config.service.lowcom.std_enabled;
        payload->crypto_enabled = ecc_locked() && config.service.lowcom.crypto_enabled;
        payload->nonce = lowcom_client[client].crypto.nonce;
      }
      
      lowcom_client[client].initialized = true;
      lc_send(client, reply, sizeof(reply));
      return sizeof(lc_preamble);
    }
    case TYPE_GOODBYE: {
      lc_debug("RX goodbye");
      if (lowcom_client[client].lastwill != 0) {
        // Execute last will
        cmdq_copy(Q0, nextid(), queue_get(lowcom_client[client].lastwill));

        // Execute last will on slaves
        if (config.daisy.enabled && config.daisy.master && state.daisy.active) {
          for (uint8_t i = 1; i <= state.daisy.slaves; i++) {
            daisy_emptyqueue(i, 0, nextid());
            daisy_copyqueue(i, 0, nextid(), lowcom_client[client].lastwill);
          }
        }
        
        lowcom_client[client].lastwill = 0;
      }
      lc_send(client, data, len);
      yield();
      //lowcom_client[client].sock.stop();
      lowcom_client[client].active = false;
      return sizeof(lc_preamble);
    }
    case TYPE_PING: {
      lc_debug("RX ping");
      lowcom_client[client].last.ping = millis();
      return sizeof(lc_preamble);
    }
    case TYPE_STD: {
      lc_debug("RX std");
      
      // Ensure at lease full header in buffer
      size_t expectlen = sizeof(lc_preamble) + sizeof(lc_header);
      if (len < expectlen) return 0;

      // Capture header
      lc_header * header = (lc_header *)&preamble[1];

      // Ensure full packet in buffer
      expectlen += header->length;
      if (len < expectlen) return 0;

      // TODO - make sure packetid is increasing

      if (!config.service.lowcom.std_enabled) {
        lc_replynack(client, MODE_STD, header->opcode, header->target, header->queue, header->packetid, "ComStandard not enabled");
        return expectlen;
      }
      
      lc_handlepacket(client, MODE_STD, header->opcode, header->subcode, header->target, header->queue, header->packetid, (uint8_t *)&header[1], header->length);
      return expectlen;
    }
    case TYPE_CRYPTO: {
      lc_debug("RX crypto");

      // Ensure at lease full header in buffer
      size_t expectlen = sizeof(lc_preamble) + sizeof(lc_crypto) + sizeof(lc_header);
      if (len < expectlen) return 0;

      // Capture crypto + header
      lc_crypto * crypto = (lc_crypto *)&preamble[1];
      lc_header * header = (lc_header *)&crypto[1];

      // Ensure full packet in buffer
      expectlen += header->length;
      if (len < expectlen) return 0;

      if (!config.service.lowcom.crypto_enabled) {
        lc_replynack(client, MODE_STD, header->opcode, header->target, header->queue, header->packetid, "ComCrypto not enabled");
        return expectlen;
      }

      // Make sure crypto is provisioned
      if (!ecc_locked()) {
        lc_replynack(client, MODE_STD, header->opcode, header->target, header->queue, header->packetid, "Crypto not provisioned (Set key)");
        return expectlen;
      }

      // Make sure crypto chip not faulted
      if (state.service.crypto.fault) {
        lc_replynack(client, MODE_STD, header->opcode, header->target, header->queue, header->packetid, "Crypto fault");
        return expectlen;
      }

      // Make sure packetid is increasing
      if (header->packetid <= lowcom_client[client].crypto.packetid) {
        lc_replynack(client, MODE_CRYPTO, header->opcode, header->target, header->queue, header->packetid, "Bad packetid (Must be increasing)");
        return expectlen;
      }
      lowcom_client[client].crypto.packetid = header->packetid;
      
      lc_cryptometa_t meta = {.client = client, .target = TARGET_COMMAND};
      if (!ecc_lowcom_hmac(lowcom_client[client].crypto.nonce, (uint8_t *)&meta, sizeof(lc_cryptometa_t), (uint8_t *)crypto, sizeof(lc_crypto), sizeof(lc_crypto) + sizeof(lc_header) + header->length)) {
        lc_replynack(client, MODE_STD, header->opcode, header->target, header->queue, header->packetid, "Out of crypto memory");
        seterror(ESUB_LC, 0, ETYPE_MEM, client);
        return expectlen;
      }
      
      return expectlen;
    }
    default: {
      // Unknown type.
      lc_debug("RX unknown type", preamble->type);
      return 1;
    }
  }
}

void lowcom_init() {
  for (size_t i = 0; i < LTC_SIZE; i++) {
    lowcom_client[i].active = false;
  }

  if (config.service.lowcom.enabled) {
    lowcom_server.begin();
    lowcom_server.setNoDelay(true);
  }
}

void lowcom_loop(unsigned long now) {
  if (!config.service.lowcom.enabled) return;
  
  // Check all packets for rx
  for (size_t ci = 0; ci < LTC_SIZE; ci++) {
    if (lowcom_client[ci].active && lowcom_client[ci].sock.available()) {
      // Check limit
      if (lowcom_client[ci].Ilen == LTCB_ISIZE) {
        // Input buffer is full, drain here (bad data, could not parse)
        seterror(ESUB_LC, 0, ETYPE_IBUF, ci);
        lowcom_client[ci].Ilen = 0;
      }
      
      // Read in as much as possible
      size_t bytes = lowcom_client[ci].sock.read(&lowcom_client[ci].I[lowcom_client[ci].Ilen], LTCB_ISIZE - lowcom_client[ci].Ilen);
      lowcom_client[ci].Ilen += bytes;
    }
  }

  // Handle payload
  for (size_t ci = 0; ci < LTC_SIZE; ci++) {
    if (lowcom_client[ci].active && lowcom_client[ci].Ilen > 0) {
      uint8_t * B = lowcom_client[ci].I;
      size_t Blen = lowcom_client[ci].Ilen;

      // Check for valid SOF
      if (B[0] != L_MAGIC_1) {
        // Not valid, find first instance of magic, sync to that
        size_t index = 1;
        for (; index < Blen; index++) {
          if (B[index] == L_MAGIC_1) break;
        }

        if (index < lowcom_client[ci].Ilen) memmove(B, &B[index], Blen - index);
        lowcom_client[ci].Ilen -= index;
        continue;
      }

      // Ensure full preamble
      if (Blen < sizeof(lc_preamble)) continue;
      
      // Consume packet
      size_t consume = lc_handletype(ci, B, Blen);

      // Clear packet from buffer
      if (consume > 0) {
        memmove(B, &B[consume], Blen - consume);
        lowcom_client[ci].Ilen -= consume;
      }
    }
  }
}

void lowcom_update(unsigned long now) {
  if (!config.service.lowcom.enabled) return;
  
  // Accept new clients
  if (lowcom_server.hasClient()) {
    lc_debug("NEW client");
    
    size_t ci = 0;
    for (; ci < LTC_SIZE; ci++) {
      if (!lowcom_client[ci].active) {
        lc_debug("NEW slot", ci);
        
        memset(&lowcom_client[ci], 0, sizeof(lowcom_client[ci]));
        lowcom_client[ci].sock = lowcom_server.available();
        lowcom_client[ci].active = true;
        lowcom_client[ci].crypto.nonce = 0;
        lowcom_client[ci].crypto.packetid = 0;
        lowcom_client[ci].last.ping = now;
        break;
      }
    }

    if (ci == LTC_SIZE) {
      // No open slots
      lc_debug("ERR no client slots available");
      //lowcom_server.available().stop();
    }
  }

  // Handle client socket close
  for (size_t ci = 0; ci < LTC_SIZE; ci++) {
    if (lowcom_client[ci].active && (timesince(lowcom_client[ci].last.ping, millis()) > LTO_PING || !lowcom_client[ci].sock.connected())) {
      // Socket has timed out
      lc_debug("KILL client disconnect", ci);
      lc_preamble goodbye = {0};
      lc_packpreamble(&goodbye, TYPE_GOODBYE);
      lc_handletype(ci, (uint8_t *)&goodbye, sizeof(lc_preamble));
    }
  }

  // Ping clients
  if (timesince(sketch.service.lowcom.last.ping, now) > LTO_PING) {
    sketch.service.lowcom.last.ping = now;
    state.service.lowcom.clients = 0;

    for (size_t i = 0; i < LTC_SIZE; i++) {
      if (lowcom_client[i].active) {
        lc_debug("PING client", i);
        
        lc_preamble ping = {0};
        lc_packpreamble(&ping, TYPE_PING);
        if (lowcom_client[i].sock.availableForWrite() >= sizeof(lc_preamble)) {
          lc_debug("PING client write", i);
          lowcom_client[i].sock.write((uint8_t *)&ping, sizeof(lc_preamble));
        }
        state.service.lowcom.clients += 1;
      }
    }
  }
}

void lowcom_senderror(error_state * e) {
  for (size_t i = 0; i < LTC_SIZE; i++) {
    if (lowcom_client[i].active) {
      lc_debug("ERROR client", i);

      uint8_t packet[sizeof(lc_preamble) + sizeof(error_state)] = {0};
      lc_preamble * preamble = (lc_preamble *)&packet[0];
      error_state * error = (error_state *)&preamble[1];
      
      lc_packpreamble(preamble, TYPE_ERROR);
      memcpy(error, e, sizeof(error_state));
      
      if (lowcom_client[i].sock.availableForWrite() >= sizeof(packet)) {
        lc_debug("ERROR client write", i);
        lowcom_client[i].sock.write(packet, sizeof(packet));
      }
      state.service.lowcom.clients += 1;
    }
  }
}

