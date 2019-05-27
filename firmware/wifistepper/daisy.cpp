#include <Arduino.h>

#include "wifistepper.h"

#define D_BAUDRATE      (512000)

#define B_SIZE          (2048)
#define B_MAGIC         (0xAB)

#define CP_DAISY        (0x00)
#define CP_MOTOR        (0x20)

#define CMD_PING        (CP_DAISY | 0x00)
#define CMD_ACK         (CP_DAISY | 0x01)
#define CMD_SYNC        (CP_DAISY | 0x02)
#define CMD_CONFIG      (CP_DAISY | 0x03)
#define CMD_STATE       (CP_DAISY | 0x04)
#define CMD_CLEARERROR  (CP_DAISY | 0x05)
#define CMD_WIFI        (CP_DAISY | 0x06)

#define CMD_STOP        (CP_MOTOR | 0x01)
#define CMD_RUN         (CP_MOTOR | 0x02)
#define CMD_STEPCLK     (CP_MOTOR | 0x03)
#define CMD_MOVE        (CP_MOTOR | 0x04)
#define CMD_GOTO        (CP_MOTOR | 0x05)
#define CMD_GOUNTIL     (CP_MOTOR | 0x06)
#define CMD_RELEASESW   (CP_MOTOR | 0x07)
#define CMD_GOHOME      (CP_MOTOR | 0x08)
#define CMD_GOMARK      (CP_MOTOR | 0x09)
#define CMD_RESETPOS    (CP_MOTOR | 0x0A)
#define CMD_SETPOS      (CP_MOTOR | 0x0B)
#define CMD_SETMARK     (CP_MOTOR | 0x0C)
#define CMD_SETCONFIG   (CP_MOTOR | 0x0D)
#define CMD_WAITBUSY    (CP_MOTOR | 0x0E)
#define CMD_WAITRUNNING (CP_MOTOR | 0x0F)
#define CMD_WAITMS      (CP_MOTOR | 0x10)
#define CMD_WAITSWITCH  (CP_MOTOR | 0x11)
#define CMD_RUNQUEUE    (CP_MOTOR | 0x12)
#define CMD_EMPTYQUEUE  (CP_MOTOR | 0x13)
#define CMD_COPYQUEUE   (CP_MOTOR | 0x14)
#define CMD_SAVEQUEUE   (CP_MOTOR | 0x15)
#define CMD_LOADQUEUE   (CP_MOTOR | 0x16)
#define CMD_ESTOP       (CP_MOTOR | 0x17)

#define SELF            (0x00)

#define CTO_PING        (1000)
#define CTO_CONFIG      (2000)
#define CTO_STATE       (250)

typedef struct __attribute__((packed)) {
  uint8_t magic;
  uint8_t head_checksum;
  uint8_t body_checksum;
  uint8_t target;
  uint8_t queue;
  id_t id;
  uint8_t opcode;
  uint16_t length;
} daisy_head_t;

#define daisy_expectlen(elen)  ({ if (len != (elen)) { seterror(ESUB_DAISY, id); return; } })

// In Buffer
uint8_t B[B_SIZE] = {0};
volatile size_t Blen = 0;
volatile size_t Bskip = 0;

// Outbox
uint8_t O[B_SIZE] = {0};
volatile size_t Olen = 0;
#define daisy_dumpoutbox()  ({ if (Olen > 0) { Serial.write(O, Olen); Olen = 0; } })

// Ack list
#define A_SIZE        (64)
id_t A[A_SIZE] = {0};
volatile size_t Alen = 0;

// TODO - make crc8
static uint8_t daisy_checksum8(uint8_t * data, size_t length) {
  uint8_t sum = 0;
  for (size_t i = 0; i < length; i++) {
    sum += data[i];
  }
  return sum;
}

static uint8_t daisy_checksum8(daisy_head_t * head) {
  uint8_t * uhead = (uint8_t *)head;
  return daisy_checksum8(&uhead[offsetof(daisy_head_t, body_checksum)], sizeof(daisy_head_t) - offsetof(daisy_head_t, body_checksum));
}

void daisy_init() {
  Serial.begin(D_BAUDRATE);
  Serial.flush();
  Serial.println();
}

static void daisy_writeoutbox() {
  if (Olen == 0) return;

  size_t wrote = Serial.write(O, Olen);
  if (wrote != Olen) {
    memmove(O, &O[wrote], Olen - wrote);
    Olen -= wrote;
    
  } else {
    Olen = 0;
  }
}

static void * daisy_alloc(uint8_t target, uint8_t queue, id_t id, uint8_t opcode, size_t len) {
  // Check if we have enough memory in buffers
  if (len > 0xFFFF || (B_SIZE - Olen) < (sizeof(daisy_head_t) + len) || Alen >= A_SIZE) {
    if (id != 0) seterror(ESUB_DAISY, id);
    return NULL;
  }
  
  // Add id to ack list
  if (config.daisy.master && id != 0) {
    A[Alen++] = id;
  }

  // Allocate and initialize packet
  daisy_head_t * packet = (daisy_head_t *)(&O[Olen]);
  packet->magic = B_MAGIC;
  packet->target = target;
  packet->queue = queue;
  packet->id = id;
  packet->opcode = opcode;
  packet->length = (uint16_t)len;

  // Extend outbox buffer size
  Olen += sizeof(daisy_head_t) + len;

  // Return the packet payload
  return &packet[1];
}

static void * daisy_pack(void * data) {
  if (data == NULL) return NULL;

  uint8_t * udata = (uint8_t *)data;

  // Find start of packet
  daisy_head_t * packet = (daisy_head_t *)(udata - sizeof(daisy_head_t));

  // Compute checksums
  size_t len = packet->length;
  packet->body_checksum = daisy_checksum8(udata, len);
  packet->head_checksum = daisy_checksum8(packet);
  return packet;
}

static void daisy_masterconsume(int8_t target, uint8_t q, id_t id, uint8_t opcode, void * data, uint16_t len) {
  if (id != 0) {
    if (A[0] != id) {
      // Handle Ack error
      seterror(ESUB_DAISY, A[0]);

      // Shift up the Ack list
      size_t i = 0;
      for (; i < Alen; i++) {
        if (A[i] == id) break;
      }
      memmove(A, &A[i], sizeof(id_t) * (Alen - i));
      Alen -= i;
      
    } else {
      // Ack good, pop ack from list
      memmove(A, &A[1], sizeof(id_t) * (Alen - 1));
      Alen -= 1;
    }
  }

  // Check for bad target;
  if (target > 0) {
    seterror(ESUB_DAISY, id);
    return;
  }

  // Handle pings
  if (opcode == CMD_PING) {
    if (target == 0) {
      // Master shorted with itself
      return;
    }
    
    uint8_t slaves = abs(target);
    if (state.daisy.slaves != slaves) {
      // New number of slaves present, reallocate states and sync
      sketch.daisy.slave = (sketch.daisy.slave == NULL)? (daisy_slave_t *)malloc(sizeof(daisy_slave_t) * slaves) : (daisy_slave_t *)realloc(sketch.daisy.slave, sizeof(daisy_slave_t) * slaves);
      memset(sketch.daisy.slave, 0, sizeof(daisy_slave_t) * slaves);
      for (uint8_t i = 1; i <= slaves; i++) {
        // Send sync to all slaves and turn wifi off if config'd
        daisy_pack(daisy_alloc(i, 0, nextid(), CMD_SYNC, 0));
        if (config.daisy.slavewifioff) daisy_wificontrol(i, nextid(), false);
      }

      // update number of slaves
      state.daisy.slaves = slaves;
    }
    
    return;
  }

  // Get slave array index
  int i = state.daisy.slaves + target - 1;
  if (i < 0 || i >= state.daisy.slaves) {
    // Bad slave index
    seterror(ESUB_DAISY, id);
    return;
  }
  
  switch (opcode) {
    case CMD_SYNC: {
      daisy_expectlen(sizeof(daisy_slave_t));
      memcpy(&sketch.daisy.slave[i], data, sizeof(daisy_slave_t));
      return;
    }
    case CMD_CONFIG: {
      daisy_expectlen(sizeof(daisy_slaveconfig));
      memcpy(&sketch.daisy.slave[i].config, data, sizeof(daisy_slaveconfig));
      return;
    }
    case CMD_STATE: {
      daisy_expectlen(sizeof(daisy_slavestate));
      memcpy(&sketch.daisy.slave[i].state, data, sizeof(daisy_slavestate));
      daisy_slavestate * slavestate = (daisy_slavestate *)data;
      if (!state.error.errored && slavestate->error.errored) memcpy(&state.error, &slavestate->error, sizeof(error_state));
      return;
    }
  }
}

static void daisy_ack(uint8_t q, id_t id) {
  daisy_pack(daisy_alloc(SELF, q, id, CMD_ACK, 0));
}

static void daisy_slaveconsume(uint8_t q, id_t id, uint8_t opcode, void * data, uint16_t len) {
  queue_t * queue = queue_get(q);
  
  switch (opcode) {
    // Daisy state opcodes
    case CMD_SYNC: {
      daisy_expectlen(0);
      daisy_slave_t * self = (daisy_slave_t *)daisy_alloc(SELF, q, id, CMD_SYNC, sizeof(daisy_slave_t));
      if (self != NULL) {
        memset(self, 0, sizeof(daisy_slave_t));
        strlcpy(self->config.product, PRODUCT, LEN_PRODUCT);
        strlcpy(self->config.model, MODEL, LEN_INFO);
        strlcpy(self->config.branch, BRANCH, LEN_INFO);
        self->config.version = VERSION;
        memcpy(&self->config.io, &config.io, sizeof(io_config));
        memcpy(&self->config.motor, &config.motor, sizeof(motor_config));
        memcpy(&self->state.error, &state.error, sizeof(error_state));
        memcpy(&self->state.command, &state.command, sizeof(command_state));
        memcpy(&self->state.wifi, &state.wifi, sizeof(wifi_state));
        memcpy(&self->state.motor, &state.motor, sizeof(motor_state));
      }
      daisy_pack(self);
      break;
    }
    case CMD_CLEARERROR: {
      daisy_expectlen(0);
      cmd_clearerror();
      clearerror();
      daisy_ack(q, id);
      break;
    }
    case CMD_WIFI: {
      daisy_expectlen(sizeof(uint8_t));
      uint8_t * enabled = (uint8_t *)data;
      wificfg_connect(enabled[0]? config.wifi.mode : M_OFF, &config.wifi);
      daisy_ack(q, id);
      break;
    }
    
    // Motor CMD opcodes
    case CMD_STOP: {
      daisy_expectlen(sizeof(cmd_stop_t));
      cmd_stop_t * cmd = (cmd_stop_t *)data;
      cmd_stop(queue, id, cmd->hiz, cmd->soft);
      daisy_ack(q, id);
      break;
    }
    case CMD_RUN: {
      daisy_expectlen(sizeof(cmd_run_t));
      cmd_run_t * cmd = (cmd_run_t *)data;
      cmd_run(queue, id, cmd->dir, cmd->stepss);
      daisy_ack(q, id);
      break;
    }
    case CMD_STEPCLK: {
      daisy_expectlen(sizeof(cmd_stepclk_t));
      cmd_stepclk_t * cmd = (cmd_stepclk_t *)data;
      cmd_stepclock(queue, id, cmd->dir);
      daisy_ack(q, id);
      break;
    }
    case CMD_MOVE: {
      daisy_expectlen(sizeof(cmd_move_t));
      cmd_move_t * cmd = (cmd_move_t *)data;
      cmd_move(queue, id, cmd->dir, cmd->microsteps);
      daisy_ack(q, id);
      break;
    }
    case CMD_GOTO: {
      daisy_expectlen(sizeof(cmd_goto_t));
      cmd_goto_t * cmd = (cmd_goto_t *)data;
      if (cmd->hasdir)  cmd_goto(queue, id, cmd->pos, cmd->dir);
      else              cmd_goto(queue, id, cmd->pos);
      daisy_ack(q, id);
      break;
    }
    case CMD_GOUNTIL: {
      daisy_expectlen(sizeof(cmd_gountil_t));
      cmd_gountil_t * cmd = (cmd_gountil_t *)data;
      cmd_gountil(queue, id, cmd->action, cmd->dir, cmd->stepss);
      daisy_ack(q, id);
      break;
    }
    case CMD_RELEASESW: {
      daisy_expectlen(sizeof(cmd_releasesw_t));
      cmd_releasesw_t * cmd = (cmd_releasesw_t *)data;
      cmd_releasesw(queue, id, cmd->action, cmd->dir);
      daisy_ack(q, id);
      break;
    }
    case CMD_GOHOME: {
      daisy_expectlen(0);
      cmd_gohome(queue, id);
      daisy_ack(q, id);
      break;
    }
    case CMD_GOMARK: {
      daisy_expectlen(0);
      cmd_gomark(queue, id);
      daisy_ack(q, id);
      break;
    }
    case CMD_RESETPOS: {
      daisy_expectlen(0);
      cmd_resetpos(queue, id);
      daisy_ack(q, id);
      break;
    }
    case CMD_SETPOS: {
      daisy_expectlen(sizeof(cmd_setpos_t));
      cmd_setpos_t * cmd = (cmd_setpos_t *)data;
      cmd_setpos(queue, id, cmd->pos);
      daisy_ack(q, id);
      break;
    }
    case CMD_SETMARK: {
      daisy_expectlen(sizeof(cmd_setpos_t));
      cmd_setpos_t * cmd = (cmd_setpos_t *)data;
      cmd_setmark(queue, id, cmd->pos);
      daisy_ack(q, id);
      break;
    }
    case CMD_SETCONFIG: {
      if (((const char *)data)[len-1] != 0) {
        seterror(ESUB_DAISY, id);
        return;
      }
      cmd_setconfig(queue, id, (const char *)data);
      daisy_ack(q, id);
      break;
    }
    case CMD_WAITBUSY: {
      daisy_expectlen(0);
      cmd_waitbusy(queue, id);
      daisy_ack(q, id);
      break;
    }
    case CMD_WAITRUNNING: {
      daisy_expectlen(0);
      cmd_waitrunning(queue, id);
      daisy_ack(q, id);
      break;
    }
    case CMD_WAITMS: {
      daisy_expectlen(sizeof(cmd_waitms_t));
      cmd_waitms_t * cmd = (cmd_waitms_t *)data;
      cmd_waitms(queue, id, cmd->ms);
      daisy_ack(q, id);
      break;
    }
    case CMD_WAITSWITCH: {
      daisy_expectlen(sizeof(cmd_waitsw_t));
      cmd_waitsw_t * cmd = (cmd_waitsw_t *)data;
      cmd_waitswitch(queue, id, cmd->state);
      daisy_ack(q, id);
      break;
    }
    case CMD_RUNQUEUE: {
      daisy_expectlen(sizeof(cmd_runqueue_t));
      cmd_runqueue_t * cmd = (cmd_runqueue_t *)data;
      cmd_runqueue(queue, id, cmd->targetqueue);
      daisy_ack(q, id);
      break;
    }
    case CMD_EMPTYQUEUE: {
      daisy_expectlen(0);
      cmdq_empty(queue, id);
      daisy_ack(q, id);
      break;
    }
    case CMD_COPYQUEUE: {
      daisy_expectlen(sizeof(uint8_t));
      uint8_t * src = (uint8_t *)data;
      cmdq_copy(queue, id, queue_get(src[0]));
      daisy_ack(q, id);
      break;
    }
    case CMD_SAVEQUEUE: {
      daisy_expectlen(0);
      queuecfg_write(q);
      daisy_ack(q, id);
      break;
    }
    case CMD_LOADQUEUE: {
      daisy_expectlen(0);
      queuecfg_read(q);
      daisy_ack(q, id);
      break;
    }
    case CMD_ESTOP: {
      daisy_expectlen(sizeof(cmd_stop_t));
      cmd_stop_t * cmd = (cmd_stop_t *)data;
      cmd_estop(id, cmd->hiz, cmd->soft);
      daisy_ack(q, id);
      break;
    }
  }
}

void daisy_loop(unsigned long now) {
  if (!config.daisy.enabled) return;
  ESP.wdtFeed();
  Blen += Serial.read((char *)&B[Blen], B_SIZE - Blen);

  // If no packets to parse, dump outbox
  if (Blen == 0) {
    daisy_writeoutbox();
  }

  // Parse packets
  while (Blen > 0) {
    // Skip bytes if needed
    if (Bskip > 0) {
      size_t skipped = min(Blen, Bskip);
      if (!config.daisy.master && state.daisy.active) Serial.write(B, skipped);
      memmove(B, &B[skipped], Blen - skipped);
      Bskip -= skipped;
      Blen -= skipped;
      continue;
    }
    
    // Sync to start of packet (SOP)
    size_t i = 0;
    for (; i < Blen; i++) {
      if (B[i] == B_MAGIC) break;
    }

    // Forward all unparsed data if not master
    if (i > 0) {
      Bskip = i;
      continue;
    }

    // Ensure length of header
    if (Blen < sizeof(daisy_head_t)) return;
    daisy_head_t * head = (daisy_head_t *)B;

    // Check rest of header
    bool isvalid = daisy_checksum8(head) == head->head_checksum;
    
    // If this is a ping, set active flag
    if (isvalid && head->opcode == CMD_PING) {
      state.daisy.active = true;
      sketch.daisy.last.ping_rx = now;
    }

    // Check if it's for us
    if (isvalid && !config.daisy.master && head->target != 0x01) {
      // We're not master and the packet is not for us, skip it
      head->target -= 1;
      head->head_checksum = daisy_checksum8(head);
      Bskip = sizeof(daisy_head_t) + head->length;
      continue;
    }
    if (!isvalid) {
      // Not a valid header, shift out one byte and continue
      Bskip = 1;
      continue;
    }

    // Make sure we have whole packet
    if (Blen < (sizeof(daisy_head_t) + head->length)) return;

    // Validate checksum
    if (daisy_checksum8((uint8_t *)&head[1], head->length) != head->body_checksum) {
      // Bad checksum, shift out one byte and continue
      Bskip = 1;
      continue;
    }

    daisy_writeoutbox();

    // Consume packet
    {
      if (config.daisy.master)  daisy_masterconsume(head->target, head->queue, head->id, head->opcode, &head[1], head->length);
      else                      daisy_slaveconsume(head->queue, head->id, head->opcode, &head[1], head->length);
    }

    daisy_writeoutbox();

    // Clear packet from buffer
    size_t fullsize = sizeof(daisy_head_t) + head->length;
    memmove(B, &B[fullsize], Blen - fullsize);
    Blen -= fullsize;
  }
}

void daisy_update(unsigned long now) {
  if (!config.daisy.enabled) return;

  if (timesince(sketch.daisy.last.ping_rx, now) > CTO_PING) {
    state.daisy.active = false;
    state.daisy.slaves = 0;
    if (sketch.daisy.slave != NULL) {
      free(sketch.daisy.slave);
      sketch.daisy.slave = NULL;
    }
  }
  if (config.daisy.master && timesince(sketch.daisy.last.ping_tx, now) > (CTO_PING / 4)) {
    sketch.daisy.last.ping_tx = now;
    daisy_pack(daisy_alloc(SELF, 0, 0, CMD_PING, 0));
  }

  if (state.daisy.active && !config.daisy.master && timesince(sketch.daisy.last.config, now) > CTO_CONFIG) {
    sketch.daisy.last.config = now;
    daisy_slaveconfig * slaveconfig = (daisy_slaveconfig *)daisy_alloc(SELF, 0, 0, CMD_CONFIG, sizeof(daisy_slaveconfig));
    if (slaveconfig != NULL) {
      memcpy(&slaveconfig->io, &config.io, sizeof(io_config));
      memcpy(&slaveconfig->motor, &config.motor, sizeof(motor_config));
    }
    daisy_pack(slaveconfig);
  }
  if (state.daisy.active && !config.daisy.master && timesince(sketch.daisy.last.state, now) > CTO_STATE) {
    sketch.daisy.last.state = now;
    daisy_slavestate * slavestate = (daisy_slavestate *)daisy_alloc(SELF, 0, 0, CMD_STATE, sizeof(daisy_slavestate));
    if (slavestate != NULL) {
      memcpy(&slavestate->error, &state.error, sizeof(error_state));
      memcpy(&slavestate->command, &state.command, sizeof(command_state));
      memcpy(&slavestate->wifi, &state.wifi, sizeof(wifi_state));
      memcpy(&slavestate->motor, &state.motor, sizeof(motor_state));
    }
    daisy_pack(slavestate);
  }
}

bool daisy_clearerror(uint8_t target, id_t id) {
  return daisy_pack(daisy_alloc(target, 0, id, CMD_CLEARERROR, 0)) != NULL;
}

bool daisy_wificontrol(uint8_t target, id_t id, bool enabled) {
  uint8_t * en = (uint8_t *)daisy_alloc(target, 0, id, CMD_WIFI, sizeof(uint8_t));
  en[0] = enabled;
  return daisy_pack(en) != NULL;
}

bool daisy_stop(uint8_t target, uint8_t q, id_t id, bool hiz, bool soft) {
  cmd_stop_t * cmd = (cmd_stop_t *)daisy_alloc(target, q, id, CMD_STOP, sizeof(cmd_stop_t));
  if (cmd != NULL) *cmd = { .hiz = hiz, .soft = soft };
  return daisy_pack(cmd) != NULL;
}

bool daisy_run(uint8_t target, uint8_t q, id_t id, ps_direction dir, float stepss) {
  cmd_run_t * cmd = (cmd_run_t *)daisy_alloc(target, q, id, CMD_RUN, sizeof(cmd_run_t));
  if (cmd != NULL) *cmd = { .dir = dir, .stepss = stepss };
  return daisy_pack(cmd) != NULL;
}

bool daisy_stepclock(uint8_t target, uint8_t q, id_t id, ps_direction dir) {
  cmd_stepclk_t * cmd = (cmd_stepclk_t *)daisy_alloc(target, q, id, CMD_STEPCLK, sizeof(cmd_stepclk_t));
  if (cmd != NULL) *cmd = { .dir = dir };
  return daisy_pack(cmd) != NULL;
}

bool daisy_move(uint8_t target, uint8_t q, id_t id, ps_direction dir, uint32_t microsteps) {
  cmd_move_t * cmd = (cmd_move_t *)daisy_alloc(target, q, id, CMD_MOVE, sizeof(cmd_move_t));
  if (cmd != NULL) *cmd = { .dir = dir, .microsteps = microsteps };
  return daisy_pack(cmd) != NULL;
}

bool daisy_goto(uint8_t target, uint8_t q, id_t id, int32_t pos, bool hasdir, ps_direction dir) {
  cmd_goto_t * cmd = (cmd_goto_t *)daisy_alloc(target, q, id, CMD_GOTO, sizeof(cmd_goto_t));
  if (cmd != NULL) *cmd = { .hasdir = hasdir, .dir = dir, .pos = pos };
  return daisy_pack(cmd) != NULL;
}

bool daisy_gountil(uint8_t target, uint8_t q, id_t id, ps_posact action, ps_direction dir, float stepss) {
  cmd_gountil_t * cmd = (cmd_gountil_t *)daisy_alloc(target, q, id, CMD_GOUNTIL, sizeof(cmd_gountil_t));
  if (cmd != NULL) *cmd = { .action = action, .dir = dir, .stepss = stepss };
  return daisy_pack(cmd) != NULL;
}

bool daisy_releasesw(uint8_t target, uint8_t q, id_t id, ps_posact action, ps_direction dir) {
  cmd_releasesw_t * cmd = (cmd_releasesw_t *)daisy_alloc(target, q, id, CMD_RELEASESW, sizeof(cmd_releasesw_t));
  if (cmd != NULL) *cmd = { .action = action, .dir = dir };
  return daisy_pack(cmd) != NULL;
}

bool daisy_gohome(uint8_t target, uint8_t q, id_t id) {
  return daisy_pack(daisy_alloc(target, q, id, CMD_GOHOME, 0)) != NULL;
}

bool daisy_gomark(uint8_t target, uint8_t q, id_t id) {
  return daisy_pack(daisy_alloc(target, q, id, CMD_GOMARK, 0)) != NULL;
}

bool daisy_resetpos(uint8_t target, uint8_t q, id_t id) {
  return daisy_pack(daisy_alloc(target, q, id, CMD_RESETPOS, 0)) != NULL;
}

bool daisy_setpos(uint8_t target, uint8_t q, id_t id, int32_t pos) {
  cmd_setpos_t * cmd = (cmd_setpos_t *)daisy_alloc(target, q, id, CMD_SETPOS, sizeof(cmd_setpos_t));
  if (cmd != NULL) *cmd = { .pos = pos };
  return daisy_pack(cmd) != NULL;
}

bool daisy_setmark(uint8_t target, uint8_t q, id_t id, int32_t mark) {
  cmd_setpos_t * cmd = (cmd_setpos_t *)daisy_alloc(target, q, id, CMD_SETMARK, sizeof(cmd_setpos_t));
  if (cmd != NULL) *cmd = { .pos = mark };
  return daisy_pack(cmd) != NULL;
}

bool daisy_setconfig(uint8_t target, uint8_t q, id_t id, const char * data) {
  size_t ldata = strlen(data) + 1;
  void * buf = daisy_alloc(target, q, id, CMD_SETCONFIG, ldata);
  if (buf != NULL) memcpy(buf, data, ldata);
  return daisy_pack(buf) != NULL;
}

bool daisy_waitbusy(uint8_t target, uint8_t q, id_t id) {
  return daisy_pack(daisy_alloc(target, q, id, CMD_WAITBUSY, 0)) != NULL;
}

bool daisy_waitrunning(uint8_t target, uint8_t q, id_t id) {
  return daisy_pack(daisy_alloc(target, q, id, CMD_WAITRUNNING, 0)) != NULL;
}

bool daisy_waitms(uint8_t target, uint8_t q, id_t id, uint32_t ms) {
  cmd_waitms_t * cmd = (cmd_waitms_t *)daisy_alloc(target, q, id, CMD_WAITMS, sizeof(cmd_waitms_t));
  if (cmd != NULL) *cmd = { .ms = ms };
  return daisy_pack(cmd) != NULL;
}

bool daisy_waitswitch(uint8_t target, uint8_t q, id_t id, bool state) {
  cmd_waitsw_t * cmd = (cmd_waitsw_t *)daisy_alloc(target, q, id, CMD_WAITSWITCH, sizeof(cmd_waitsw_t));
  if (cmd != NULL) *cmd = { .state = state };
  return daisy_pack(cmd) != NULL;
}

bool daisy_runqueue(uint8_t target, uint8_t q, id_t id, uint8_t targetqueue) {
  cmd_runqueue_t * cmd = (cmd_runqueue_t *)daisy_alloc(target, q, id, CMD_RUNQUEUE, sizeof(cmd_runqueue_t));
  if (cmd != NULL) *cmd = { .targetqueue = targetqueue };
  return daisy_pack(cmd) != NULL;
}

bool daisy_emptyqueue(uint8_t target, uint8_t q, id_t id) {
  return daisy_pack(daisy_alloc(target, q, id, CMD_EMPTYQUEUE, 0)) != NULL;
}

bool daisy_copyqueue(uint8_t target, uint8_t q, id_t id, uint8_t src) {
  uint8_t * queue = (uint8_t *)daisy_alloc(target, q, id, CMD_COPYQUEUE, sizeof(uint8_t));
  *queue = src;
  return daisy_pack(queue) != NULL;
}

bool daisy_savequeue(uint8_t target, uint8_t q, id_t id) {
  return daisy_pack(daisy_alloc(target, q, id, CMD_SAVEQUEUE, 0)) != NULL;
}

bool daisy_loadqueue(uint8_t target, uint8_t q, id_t id) {
  return daisy_pack(daisy_alloc(target, q, id, CMD_LOADQUEUE, 0)) != NULL;
}

bool daisy_estop(uint8_t target, id_t id, bool hiz, bool soft) {
  cmd_stop_t * cmd = (cmd_stop_t *)daisy_alloc(target, 0, id, CMD_ESTOP, sizeof(cmd_stop_t));
  if (cmd != NULL) *cmd = { .hiz = hiz, .soft = soft };
  return daisy_pack(cmd) != NULL;
}

