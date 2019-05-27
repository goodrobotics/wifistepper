#include <ArduinoJson.h>

#include "command.h"
#include "wifistepper.h"

static void cmdq_parse(JsonObject& entry, id_t id, uint8_t target, uint8_t queue) {
  if (!entry.containsKey("type")) return;
  if (id == 0) id = nextid();
  
  String type = entry["type"].as<String>();
  if (type == "estop") {
    m_estop(target, id, entry["hiz"].as<bool>(), entry["soft"].as<bool>());
  } else if (type == "clearerror") {
    m_clearerror(target, id);
  } else if (type == "setconfig") {
    m_setconfig(target, queue, id, entry["config"].as<char *>());
  } else if (type == "runqueue") {
    m_runqueue(target, queue, id, entry["targetqueue"].as<uint8_t>());
  } else if (type == "stop") {
    m_stop(target, queue, id, entry["hiz"].as<bool>(), entry["soft"].as<bool>());
  } else if (type == "run") {
    m_run(target, queue, id, parse_direction(entry["dir"].as<String>(), FWD), entry["stepss"].as<float>());
  } else if (type == "stepclock") {
    m_stepclock(target, queue, id, parse_direction(entry["dir"].as<String>(), FWD));
  } else if (type == "move") {
    m_move(target, queue, id, parse_direction(entry["dir"].as<String>(), FWD), entry["microsteps"].as<uint32_t>());
  } else if (type == "goto") {
    m_goto(target, queue, id, entry["pos"].as<int32_t>(), entry["hasdir"].as<bool>(), parse_direction(entry["dir"].as<String>(), FWD));
  } else if (type == "gountil") {
    m_gountil(target, queue, id, parse_action(entry["action"].as<String>(), POS_RESET), parse_direction(entry["dir"].as<String>(), FWD), entry["stepss"].as<float>());
  } else if (type == "releasesw") {
    m_releasesw(target, queue, id, parse_action(entry["action"].as<String>(), POS_RESET), parse_direction(entry["dir"].as<String>(), FWD));
  } else if (type == "gohome") {
    m_gohome(target, queue, id);
  } else if (type == "gomark") {
    m_gomark(target, queue, id);
  } else if (type == "resetpos") {
    m_resetpos(target, queue, id);
  } else if (type == "setpos") {
    m_setpos(target, queue, id, entry["pos"].as<int32_t>());
  } else if (type == "setmark") {
    m_setmark(target, queue, id, entry["mark"].as<int32_t>());
  } else if (type == "waitbusy") {
    m_waitbusy(target, queue, id);
  } else if (type == "waitrunning") {
    m_waitrunning(target, queue, id);
  } else if (type == "waitms") {
    m_waitms(target, queue, id, entry["ms"].as<uint32_t>());
  } else if (type == "waitswitch") {
    m_waitswitch(target, queue, id, entry["state"].as<bool>());
  } else if (type == "emptyqueue") {
    m_emptyqueue(target, queue, id);
  } else if (type == "savequeue") {
    m_savequeue(target, queue, id);
  } else if (type == "loadqueue") {
    m_loadqueue(target, queue, id);
  } else if (type == "copyqueue") {
    m_copyqueue(target, queue, id, entry["sourcequeue"].as<uint8_t>());
  }
}

static void cmdq_parse(JsonObject& entry, uint8_t target, uint8_t queue) {
  cmdq_parse(entry, entry.containsKey("id")? entry["id"].as<id_t>() : 0, target, queue);
}

size_t cmdq_serialize(JsonObject& entry, cmd_head_t * head) {
  void * data = (void *)&head[1];
  size_t consume = sizeof(cmd_head_t);

  entry["id"] = head->id;
  switch (head->opcode) {
    case CMD_SETCONFIG: {
      const char * data = (const char *)data;
      size_t ldata = strlen(data);
      entry["type"] = "setconfig";
      entry["config"] = data;
      consume += ldata + 1;
      break;
    }
    case CMD_RUNQUEUE: {
      cmd_runqueue_t * cmd = (cmd_runqueue_t *)data;
      entry["type"] = "runqueue";
      entry["targetqueue"] = cmd->targetqueue;
      consume += sizeof(cmd_runqueue_t);
      break;
    }
    case CMD_STOP: {
      cmd_stop_t * cmd = (cmd_stop_t *)data;
      entry["type"] = "stop";
      entry["hiz"] = cmd->hiz;
      entry["soft"] = cmd->soft;
      consume += sizeof(cmd_stop_t);
      break;
    }
    case CMD_RUN: {
      cmd_run_t * cmd = (cmd_run_t *)data;
      entry["type"] = "run";
      entry["dir"] = json_serialize(cmd->dir);
      entry["stepss"] = cmd->stepss;
      consume += sizeof(cmd_run_t);
      break;
    }
    case CMD_STEPCLK: {
      cmd_stepclk_t * cmd = (cmd_stepclk_t *)data;
      entry["type"] = "stepclock";
      entry["dir"] = json_serialize(cmd->dir);
      consume += sizeof(cmd_stepclk_t);
      break;
    }
    case CMD_MOVE: {
      cmd_move_t * cmd = (cmd_move_t *)data;
      entry["type"] = "move";
      entry["dir"] = json_serialize(cmd->dir);
      entry["microsteps"] = cmd->microsteps;
      consume += sizeof(cmd_move_t);
      break;
    }
    case CMD_GOTO: {
      cmd_goto_t * cmd = (cmd_goto_t *)data;
      entry["type"] = "goto";
      entry["pos"] = cmd->pos;
      entry["hasdir"] = cmd->hasdir;
      entry["dir"] = json_serialize(cmd->dir);
      consume += sizeof(cmd_goto_t);
      break;
    }
    case CMD_GOUNTIL: {
      cmd_gountil_t * cmd = (cmd_gountil_t *)data;
      entry["type"] = "gountil";
      entry["action"] = json_serialize(cmd->action);
      entry["dir"] = json_serialize(cmd->dir);
      entry["stepss"] = cmd->stepss;
      consume += sizeof(cmd_gountil_t);
      break;
    }
    case CMD_RELEASESW: {
      cmd_releasesw_t * cmd = (cmd_releasesw_t *)data;
      entry["type"] = "releasesw";
      entry["action"] = json_serialize(cmd->action);
      entry["dir"] = json_serialize(cmd->dir);
      consume += sizeof(cmd_releasesw_t);
      break;
    }
    case CMD_GOHOME: {
      entry["type"] = "gohome";
      break;
    }
    case CMD_GOMARK: {
      entry["type"] = "gomark";
      break;
    }
    case CMD_RESETPOS: {
      entry["type"] = "resetpos";
      break;
    }
    case CMD_SETPOS: {
      cmd_setpos_t * cmd = (cmd_setpos_t *)data;
      entry["type"] = "setpos";
      entry["pos"] = cmd->pos;
      consume += sizeof(cmd_setpos_t);
      break;
    }
    case CMD_SETMARK: {
      cmd_setpos_t * cmd = (cmd_setpos_t *)data;
      entry["type"] = "setmark";
      entry["mark"] = cmd->pos;
      consume += sizeof(cmd_setpos_t);
      break;
    }
    case CMD_WAITBUSY: {
      entry["type"] = "waitbusy";
      break;
    }
    case CMD_WAITRUNNING: {
      entry["type"] = "waitrunning";
      break;
    }
    case CMD_WAITMS: {
      sketch_waitms_t * sketch = (sketch_waitms_t *)data;
      entry["type"] = "waitms";
      entry["ms"] = sketch->ms;
      consume += sizeof(sketch_waitms_t);
      break;
    }
    case CMD_WAITSWITCH: {
      cmd_waitsw_t * cmd = (cmd_waitsw_t *)data;
      entry["type"] = "waitswitch";
      entry["state"] = cmd->state;
      consume += sizeof(cmd_waitsw_t);
      break;
    }
  }
  
  return consume;
}

void cmdq_read(JsonArray& arr, uint8_t target, uint8_t queue) {
  for (auto value : arr) {
    JsonObject& entry = value.as<JsonObject>();
    cmdq_parse(entry, target, queue);
  }
}

void cmdq_read(JsonArray& arr, uint8_t target) {
  for (auto value : arr) {
    JsonObject& entry = value.as<JsonObject>();
    uint8_t queue = entry["queue"].as<uint8_t>();
    cmdq_parse(entry, target, queue);
  }
}

void cmdq_read(JsonArray& arr) {
  for (auto value : arr) {
    JsonObject& entry = value.as<JsonObject>();
    uint8_t target = entry["target"].as<uint8_t>();
    uint8_t queue = entry["queue"].as<uint8_t>();
    cmdq_parse(entry, target, queue);
  }
}


void cmdq_write(JsonArray& arr, queue_t * queue) {
  size_t index = 0;
  while (index < queue->len) {
    JsonObject& entry = arr.createNestedObject();
    cmd_head_t * head = (cmd_head_t *)&(queue->Q[index]);
    index += cmdq_serialize(entry, head);
  }
}


bool cmdq_empty(queue_t * queue, id_t id) {
  if (queue == NULL) {
    seterror(ESUB_CMD, id, ETYPE_NOQUEUE);
    return false;
  }
  queue->len = 0;
  return true;
}


