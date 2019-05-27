#include <SPI.h>
#include <float.h>

#include "powerstep01.h"
#include "powerstep01priv.h"

#define PS_PIN_RST      (15)
#define PS_PIN_CS       (4)

//#define PS_DEBUG

uint8_t _ps_xferbyte(uint8_t b) {
  digitalWrite(PS_PIN_CS, LOW);
  SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE2));
  b = SPI.transfer(b);
  SPI.endTransaction();
  digitalWrite(PS_PIN_CS, HIGH);

  // Per datasheet, must raise CS between bytes and hold for at least 625ns
  delayMicroseconds(1);

  return b;
}

void _ps_xfer(uint8_t cmd, uint8_t * data, size_t len) {
  #ifdef PS_DEBUG
  {
    Serial.print("SPI Write: (");
    Serial.print(cmd, BIN);
    Serial.print(")");
    for (size_t i = 0; i < len; i++) {
      Serial.print(", ");
      Serial.print(data[i], BIN);
    }
    Serial.println();
  }
  #endif

  cmd = _ps_xferbyte(cmd);
  for (size_t i = 0; i < len; i++) {
    data[i] = _ps_xferbyte(data[i]);
  }

  #ifdef PS_DEBUG
  {
    Serial.print("SPI Read: ");
    if (cmd != 0) {
      Serial.print("XX----> ");
    }
    Serial.print(cmd, BIN);
    for (size_t i = 0; i < len; i++) {
      Serial.print(", ");
      Serial.print(data[i], BIN);
    }
    Serial.println();
  }
  #endif
}

#define _ps_mask(b)                           ((1 << (b))-1)

#define _ps_setsplit16(v, b1, b2, s)          ({(b1) = (uint8_t)(((uint16_t)(v) >> (s)) & _ps_mask(16-(s))); (b2) = (uint8_t)((uint16_t)(v) & _ps_mask(s));})
#define ps_setsplit16(v, n, s)                _ps_setsplit16((v), (n ## _U), (n ## _L), (s))
#define ps_set16(v, b)                        _ps_setsplit16((v), (b)[0], (b)[1], 8)

#define _ps_getsplit16(b1, b2, s)             ((((uint16_t)(b1) & _ps_mask(16-(s))) << (s)) | ((uint16_t)(b2) & _ps_mask(s)))
#define ps_getsplit16(n, s)                   _ps_getsplit16((n ## _U), (n ## _L), (s))
#define ps_get16(b)                           _ps_getsplit16((b)[0], (b)[1], 8)

#define _ps_setsplit24(v, b1, b2, b3, sh, sl) ({(b1) = (uint8_t)(((uint32_t)(v) >> (sh)) & _ps_mask(24-(sh))); (b2) = (uint8_t)(((uint32_t)(v) >> (sl)) & _ps_mask((sh)-(sl))); (b3) = (uint8_t)((uint32_t)(v) & _ps_mask(sl));})
#define ps_setsplit24(v, n, sh, sl)           _ps_setsplit24((v), (n ## _U), (n ## _M), (n ## _L), (sh), (sl))
#define ps_set24(v, b)                        _ps_setsplit24((v), (b)[0], (b)[1], (b)[2], 16, 8)

#define _ps_getsplit24(b1, b2, b3, sh, sl)    ((((uint32_t)(b1) & _ps_mask(24-(sh))) << (sh)) | (((uint32_t)(b2) & _ps_mask((sh)-(sl))) << (sl)) | ((uint32_t)(b3) & _ps_mask(sl)))
#define ps_getsplit24(n, sh, sl)              _ps_getsplit24((n ## _U), (n ## _M), (n ## _L), (sh), (sl))
#define ps_get24(b)                           _ps_getsplit24((b)[0], (b)[1], (b)[2], 16, 8)

#define ps_xferreg(cmdname, cmd, reg)    ps_xfer((cmdname), (cmd), (uint8_t *)&(reg), sizeof(reg))

#ifdef PS_DEBUG
void ps_xfer(const char * cmdname, uint8_t cmd, uint8_t * data, size_t len) {
  Serial.print("SPI Cmd: ");
  Serial.println(cmdname);
  _ps_xfer(cmd, data, len);
}

void ps_print(const ps_status_reg * r) {
  Serial.println("StepperMotor Status:");
  Serial.print("Stall A: "); Serial.println(r->stall_a, BIN);
  Serial.print("Stall B: "); Serial.println(r->stall_b, BIN);
  Serial.print("OCD: "); Serial.println(r->ocd, BIN);
  Serial.print("Th Status: "); Serial.println(r->th_status, BIN);
  Serial.print("UVLO ADC: "); Serial.println(r->uvlo_adc, BIN);
  Serial.print("UVLO: "); Serial.println(r->uvlo, BIN);
  Serial.print("Step Clk: "); Serial.println(r->stck_mod, BIN);
  Serial.print("Cmd ERR: "); Serial.println(r->cmd_error, BIN);
  Serial.print("Motor Status: "); Serial.println(r->mot_status, BIN);
  Serial.print("Direction: "); Serial.println(r->dir, BIN);
  Serial.print("Sw Event: "); Serial.println(r->sw_evn, BIN);
  Serial.print("Sw F: "); Serial.println(r->sw_f, BIN);
  Serial.print("Busy: "); Serial.println(r->busy, BIN);
  Serial.print("HiZ: "); Serial.println(r->hiz, BIN);
  Serial.println();
}

void ps_print(const ps_stepmode_reg * r) {
  Serial.println("Step Mode:");
  Serial.print("Sync En: "); Serial.println(r->sync_en, BIN);
  Serial.print("Sync Sel: "); Serial.println(r->sync_sel, BIN);
  Serial.print("CM VM: "); Serial.println(r->cm_vm, BIN);
  Serial.print("Step Sel: "); Serial.println(r->step_sel, BIN);
  Serial.println();
}

void ps_print(const ps_minspeed_reg * r) {
  Serial.println("Min Speed:");
  Serial.print("Lowspeed Optim: "); Serial.println(r->lspd_opt, BIN);
  Serial.print("Min Speed: "); Serial.println(ps_getsplit16(r->min_speed, 8), BIN);
  Serial.println();
}

void ps_print(const ps_fsspd_reg * r) {
  Serial.println("Full Speed:");
  Serial.print("Boost Mode: "); Serial.println(r->boost_mode, BIN);
  Serial.print("FullStep Speed: "); Serial.println(ps_getsplit16(r->fs_spd, 8), BIN);
  Serial.println();
}

void ps_print(const ps_gatecfg1_reg * r) {
  Serial.println("Gate Cfg 1:");
  Serial.print("WD: "); Serial.println(r->wd_en, BIN);
  Serial.print("TBoost: "); Serial.println(r->tboost, BIN);
  Serial.print("Slew: "); Serial.println(r->slew, BIN);
  Serial.println();
}

void ps_print(const ps_config_reg * r) {
  Serial.println("Configuration:");
  Serial.print("V PWM Int: "); Serial.println(r->vm.f_pwm_int, BIN);
  Serial.print("V PWM Dec: "); Serial.println(r->vm.f_pwm_dec, BIN);
  Serial.print("V Volt Comp: "); Serial.println(r->vm.en_vscomp, BIN);
  Serial.print("C Pred: "); Serial.println(r->cm.pred_en, BIN);
  Serial.print("C TSW: "); Serial.println(r->cm.tsw, BIN);
  Serial.print("- VCC: "); Serial.println(r->com.vccval, BIN);
  Serial.print("- UVLO: "); Serial.println(r->com.uvloval, BIN);
  Serial.print("- OC SD: "); Serial.println(r->com.oc_sd, BIN);
  Serial.print("- Switch Mode: "); Serial.println(r->com.sw_mode, BIN);
  Serial.print("- Clock Sel: "); Serial.println(r->com.clk_sel, BIN);
  Serial.println();
}

void ps_print(const ps_alarms_reg * r) {
  Serial.println("Alarms:");
  Serial.print("Overcurrent: "); Serial.println(r->overcurrent, BIN);
  Serial.print("Thermal Shutdown: "); Serial.println(r->thermal_shutdown, BIN);
  Serial.print("Thermal Warning: "); Serial.println(r->thermal_warning, BIN);
  Serial.print("Undervoltage: "); Serial.println(r->undervoltage, BIN);
  Serial.print("ADC Undervoltage: "); Serial.println(r->adc_undervoltage, BIN);
  Serial.print("Stall Detect: "); Serial.println(r->stall_detect, BIN);
  Serial.print("User Switch: "); Serial.println(r->user_switch, BIN);
  Serial.print("Command Error: "); Serial.println(r->command_error, BIN);
  Serial.println();
}

void ps_print(const ps_tfast_reg * r) {
  Serial.println("TFast:");
  Serial.print("Off Fast: "); Serial.println(r->toff_fast, BIN);
  Serial.print("Fast Step: "); Serial.println(r->fast_step, BIN);
  Serial.println();
}

void ps_print(const char * name, uint16_t data) {
  Serial.print(name);
  Serial.print(": 0x");
  Serial.println(data, HEX);
  Serial.println();
}

void ps_print(const char * name, uint8_t data) {
  Serial.print(name);
  Serial.print(": 0x");
  Serial.println(data, HEX);
  Serial.println();
}
#else

#define ps_xfer(cmdname, cmd, data, len)   _ps_xfer(cmd, data, len)
#define ps_print(...)

#endif


void ps_spiinit() {
  SPI.begin();

  pinMode(PS_PIN_RST, OUTPUT);
  pinMode(PS_PIN_CS, OUTPUT);
  digitalWrite(PS_PIN_RST, HIGH);
  digitalWrite(PS_PIN_RST, LOW);
  digitalWrite(PS_PIN_RST, HIGH);
  digitalWrite(PS_PIN_CS, HIGH);
}

ps_status ps_getstatus(bool clear_errors) {
  ps_status_reg reg = {};
  if (clear_errors)   ps_xferreg("getstatus", CMD_GETSTATUS(), reg);
  else                ps_xferreg("getparam status", CMD_GETPARAM(PARAM_STATUS), reg);
  ps_print(&reg);
  return (ps_status){
    .direction = (ps_direction)reg.dir,
    .movement = (ps_movement)reg.mot_status,
    .hiz = (bool)reg.hiz,
    .busy = !(bool)reg.busy,
    .user_switch = (bool)reg.sw_f,
    .step_clock = (bool)reg.stck_mod,
    .alarms = {
      .command_error = reg.cmd_error,
      .overcurrent = !reg.ocd,
      .undervoltage = !reg.uvlo,
      .thermal_shutdown = reg.th_status == TH_BRIDGESHUTDOWN || reg.th_status == TH_DEVICESHUTDOWN,
      .user_switch = reg.sw_evn,
      .thermal_warning = reg.th_status == TH_WARNING,
      .stall_detect = !reg.stall_a || !reg.stall_b,
      .adc_undervoltage = !reg.uvlo_adc
    }
  };
}

bool ps_isbusy() {
  ps_status_reg reg = {};
  ps_xferreg("getparam status", CMD_GETPARAM(PARAM_STATUS), reg);
  return !reg.busy;
}

void ps_waitbusy(ps_waitcb waitf) {
  ps_status_reg reg = {};
  while (true) {
    memset(&reg, 0, sizeof(reg));
    ps_xferreg("getparam status", CMD_GETPARAM(PARAM_STATUS), reg);
    if (reg.busy) return;
    if (waitf != NULL) waitf();
  }
}

bool ps_isrunning() {
  ps_status_reg reg = {};
  ps_xferreg("getparam status", CMD_GETPARAM(PARAM_STATUS), reg);
  return reg.mot_status != M_STOPPED;
}

bool ps_ishiz() {
  ps_status_reg reg = {};
  ps_xferreg("getparam status", CMD_GETPARAM(PARAM_STATUS), reg);
  return reg.hiz? true : false;
}

void ps_reset() {
  ps_xfer("resetdevice", CMD_RESETDEVICE(), NULL, 0);
}

void ps_nop() {
  ps_xfer("nop", CMD_NOP(), NULL, 0);
}

ps_mode ps_getmode() {
  ps_stepmode_reg reg = {};
  ps_xferreg("getparam stepmode", CMD_GETPARAM(PARAM_STEPMODE), reg);
  return (ps_mode)reg.cm_vm;
}

void ps_setmode(ps_mode mode) {
  ps_stepmode_reg reg = {};
  ps_xferreg("getparam stepmode", CMD_GETPARAM(PARAM_STEPMODE), reg);
  reg.cm_vm = mode;
  ps_xferreg("setparam stepmode", CMD_SETPARAM(PARAM_STEPMODE), reg);
}

ps_stepsize ps_getstepsize() {
  ps_stepmode_reg reg = {};
  ps_xferreg("getparam stepmode", CMD_GETPARAM(PARAM_STEPMODE), reg);
  return (ps_stepsize)reg.step_sel;
}

void ps_setstepsize(ps_stepsize stepsize) {
  ps_stepmode_reg reg = {};
  ps_xferreg("getparam stepmode", CMD_GETPARAM(PARAM_STEPMODE), reg);
  reg.step_sel = stepsize;
  ps_xferreg("setparam stepmode", CMD_SETPARAM(PARAM_STEPMODE), reg);
}

ps_syncinfo ps_getsync() {
  ps_stepmode_reg reg = {};
  ps_xferreg("getparam stepmode", CMD_GETPARAM(PARAM_STEPMODE), reg);
  ps_print(&reg);
  return (ps_syncinfo){
    .sync_mode = (ps_sync)reg.sync_en,
    .sync_stepsize = (ps_stepsize)reg.sync_sel
  };
}

void ps_setsync(ps_sync sync, ps_stepsize stepsize) {
  ps_stepmode_reg reg = {};
  ps_xferreg("getparam stepmode", CMD_GETPARAM(PARAM_STEPMODE), reg);
  reg.sync_en = sync;
  reg.sync_sel = stepsize;
  ps_xferreg("setparam stepmode", CMD_SETPARAM(PARAM_STEPMODE), reg);
}

float ps_getmaxspeed() {
  uint8_t buf[2] = {};
  ps_xfer("getparam maxspeed", CMD_GETPARAM(PARAM_MAXSPEED), buf, 2);
  uint16_t maxspeed = ps_get16(buf);
  ps_print("Max Speed", maxspeed);
  return ((float)(maxspeed & MAXSPEED_MASK))/MAXSPEED_COEFF;
}

void ps_setmaxspeed(float steps_per_second) {
  uint8_t buf[2] = {};
  uint16_t maxspeed = min((int)round(steps_per_second * MAXSPEED_COEFF), MAXSPEED_MASK);
  ps_set16(maxspeed, buf);
  ps_xfer("setparam maxspeed", CMD_SETPARAM(PARAM_MAXSPEED), buf, 2);
}

ps_minspeed ps_getminspeed() {
  ps_minspeed_reg reg = {};
  ps_xferreg("getparam minspeed", CMD_GETPARAM(PARAM_MINSPEED), reg);
  ps_print(&reg);
  return (ps_minspeed){
    .steps_per_sec = ((float)(ps_getsplit16(reg.min_speed, 8) & MINSPEED_MASK))/MINSPEED_COEFF,
    .lowspeed_optim = (bool)reg.lspd_opt
  };
}

void ps_setminspeed(float steps_per_second, bool lowspeed_optim) {
  ps_minspeed_reg reg = {};
  uint16_t steps_per_sec = min((int)round(steps_per_second * MINSPEED_COEFF), MINSPEED_MASK);
  ps_setsplit16(steps_per_second, reg.min_speed, 8);
  reg.lspd_opt = lowspeed_optim? 0x1 : 0x0;
  ps_xferreg("setparam minspeed", CMD_SETPARAM(PARAM_MINSPEED), reg);
}

ps_fullstepspeed ps_getfullstepspeed() {
  ps_fsspd_reg reg = {};
  ps_xferreg("getparam fsspd", CMD_GETPARAM(PARAM_FSSPD), reg);
  ps_print(&reg);
  return (ps_fullstepspeed){
    .steps_per_sec = ((float)(ps_getsplit16(reg.fs_spd, 8) & FSSPD_MASK) + FSSPD_OFFSET)/FSSPD_COEFF,
    .boost_mode = (bool)reg.boost_mode
  };
}

void ps_setfullstepspeed(float steps_per_sec, bool boost_mode) {
  ps_fsspd_reg reg = {};
  uint16_t fs_spd = min((int)round(steps_per_sec * FSSPD_COEFF - FSSPD_OFFSET), FSSPD_MASK);
  ps_setsplit16(fs_spd, reg.fs_spd, 8);
  reg.boost_mode = boost_mode? 0x1 : 0x0;
  ps_xferreg("setparam fsspd", CMD_SETPARAM(PARAM_FSSPD), reg);
}

float ps_getaccel() {
  uint8_t buf[2] = {};
  ps_xfer("getparam acc", CMD_GETPARAM(PARAM_ACC), buf, 2);
  uint16_t acc = ps_get16(buf);
  ps_print("Acc", acc);
  return ((float)(acc & ACC_MASK))/ACC_COEFF;
}

void ps_setaccel(float steps_per_sec_2) {
  uint8_t buf[2] = {};
  uint16_t acc = min((int)round(steps_per_sec_2 * ACC_COEFF), ACC_MASK);
  ps_set16(acc, buf);
  ps_xfer("setparam acc", CMD_SETPARAM(PARAM_ACC), buf, 2);
}

float ps_getdecel() {
  uint8_t buf[2] = {};
  ps_xfer("getparam dec", CMD_GETPARAM(PARAM_DEC), buf, 2);
  uint16_t dec = ps_get16(buf);
  ps_print("Dec", dec);
  return ((float)(dec & DEC_MASK)) / DEC_COEFF;
}

void ps_setdecel(float steps_per_sec_2) {
  uint8_t buf[2] = {};
  uint16_t dec = min((int)round(steps_per_sec_2 * DEC_COEFF), DEC_MASK);
  ps_set16(dec, buf);
  ps_xfer("setparam dec", CMD_SETPARAM(PARAM_DEC), buf, 2);
}

ps_slewrate ps_getslewrate() {
  ps_gatecfg1_reg reg = {};
  ps_xferreg("getparam gatecfg1", CMD_GETPARAM(PARAM_GATECFG1), reg);
  ps_print(&reg);
  return (ps_slewrate)reg.slew;
}

void ps_setslewrate(ps_slewrate slew) {
  ps_gatecfg1_reg reg = {};
  ps_xferreg("getparam gatecfg1", CMD_GETPARAM(PARAM_GATECFG1), reg);
  reg.slew = slew;
  ps_xferreg("setparam gatecfg1", CMD_SETPARAM(PARAM_GATECFG1), reg);
}

ps_ocd ps_getocd() {
  uint8_t ocdth = 0;
  ps_config_reg reg = {};
  ps_xfer("getparam ocdth", CMD_GETPARAM(PARAM_OCDTH), &ocdth, 1);
  ps_xferreg("getparam config", CMD_GETPARAM(PARAM_CONFIG), reg);
  ps_print("OCD Th", ocdth);
  ps_print("OCD En", reg.com.oc_sd);
  return (ps_ocd){
    .millivolts = ((float)(ocdth & OCDTH_MASK)) / OCDTH_COEFF,
    .shutdown = reg.com.oc_sd
  };
}

void ps_setocd(float millivolts, bool shutdown) {
  uint8_t ocdth = min((int)round(millivolts * OCDTH_COEFF), OCDTH_MASK);
  ps_config_reg reg = {};
  ps_xfer("setparam ocdth", CMD_SETPARAM(PARAM_OCDTH), &ocdth, 1);
  ps_xferreg("getparam config", CMD_GETPARAM(PARAM_CONFIG), reg);
  reg.com.oc_sd = shutdown? 0x1 : 0x0;
  ps_xferreg("setparam config", CMD_SETPARAM(PARAM_CONFIG), reg);
}

float ps_getclockfreq(ps_clocksel clock) {
  switch (clock) {
    case CLK_EXT8_XTAL:
    case CLK_EXT8_OSC:
      return 8000000;
    case CLK_INT16:
    case CLK_INT16_EXT2:
    case CLK_INT16_EXT4:
    case CLK_INT16_EXT16:
    case CLK_EXT16_XTAL:
    case CLK_EXT16_OSC:
      return 16000000;
    case CLK_EXT24_XTAL:
    case CLK_EXT24_OSC:
      return 24000000;
    case CLK_EXT32_XTAL:
    case CLK_EXT32_OSC:
      return 32000000;
    default:
      return 0;
  }
}

ps_clocksel ps_getclocksel() {
  ps_config_reg reg = {};
  ps_xferreg("getparam config", CMD_GETPARAM(PARAM_CONFIG), reg);
  ps_print(&reg);
  return (ps_clocksel)reg.com.clk_sel;
}

void ps_setclocksel(const ps_clocksel clock) {
  ps_config_reg reg = {};
  ps_xferreg("getparam config", CMD_GETPARAM(PARAM_CONFIG), reg);
  reg.com.clk_sel = clock;
  ps_xferreg("setparam config", CMD_SETPARAM(PARAM_CONFIG), reg);
}

ps_swmode ps_getswmode() {
  ps_config_reg reg = {};
  ps_xferreg("getparam config", CMD_GETPARAM(PARAM_CONFIG), reg);
  ps_print(&reg);
  return (ps_swmode)reg.com.sw_mode;
}

void ps_setswmode(const ps_swmode swmode) {
  ps_config_reg reg = {};
  ps_xferreg("getparam config", CMD_GETPARAM(PARAM_CONFIG), reg);
  reg.com.sw_mode = swmode;
  ps_xferreg("setparam config", CMD_SETPARAM(PARAM_CONFIG), reg);
}

#define LEN_PWMFREQ_DIVS    8
#define LEN_PWMFREQ_MULS    8
float pwmfreq_divs[LEN_PWMFREQ_DIVS] = {1, 2, 3, 4, 5, 6, 7, 7};
float pwmfreq_muls[LEN_PWMFREQ_MULS] = {0.625, 0.75, 0.875, 1, 1.25, 1.5, 1.75, 2};

float ps_vm_coeffs2pwmfreq(ps_clocksel clock, ps_vm_pwmfreq * coeffs) {
  return (ps_getclockfreq(clock) * pwmfreq_muls[coeffs->mul]) / (512.0 * pwmfreq_divs[coeffs->div]);
}

ps_vm_pwmfreq ps_vm_pwmfreq2coeffs(ps_clocksel clock, float pwmfreq) {
  ps_vm_pwmfreq coeffs = {.div = 0, .mul = 0};
  float best = FLT_MAX;
  for (size_t d = 0; d < LEN_PWMFREQ_DIVS; d++) {
    for (size_t m = 0; m < LEN_PWMFREQ_MULS; m++) {
      ps_vm_pwmfreq check = {.div = d, .mul = m};
      float result = abs(ps_vm_coeffs2pwmfreq(clock, &check) - pwmfreq);
      if (result < best) {
        coeffs = check;
        best = result;
      }
    }
  }
  return coeffs;
}

ps_vm_pwmfreq ps_vm_getpwmfreq() {
  ps_config_reg reg = {};
  ps_xferreg("getparam config", CMD_GETPARAM(PARAM_CONFIG), reg);
  ps_print(&reg);
  return (ps_vm_pwmfreq){
    .div = reg.vm.f_pwm_int,
    .mul = reg.vm.f_pwm_dec
  };
}

void ps_vm_setpwmfreq(ps_vm_pwmfreq * coeffs) {
  ps_config_reg reg = {};
  ps_xferreg("getparam config", CMD_GETPARAM(PARAM_CONFIG), reg);
  reg.vm.f_pwm_int = coeffs->div;
  reg.vm.f_pwm_dec = coeffs->mul;
  ps_xferreg("setparam config", CMD_SETPARAM(PARAM_CONFIG), reg);
}

ps_alarms ps_getalarmconfig() {
  ps_alarms_reg reg = {};
  ps_xferreg("getparam alarmen", CMD_GETPARAM(PARAM_ALARMEN), reg);
  ps_print(&reg);
  return (ps_alarms){
    .command_error = reg.command_error,
    .overcurrent = reg.overcurrent,
    .undervoltage = reg.undervoltage,
    .thermal_shutdown = reg.thermal_shutdown,
    .user_switch = reg.user_switch,
    .thermal_warning = reg.thermal_warning,
    .stall_detect = reg.stall_detect,
    .adc_undervoltage = reg.adc_undervoltage
  };
}

void ps_setalarmconfig(const ps_alarms * alarms) {
  ps_setalarmconfig(alarms->command_error, alarms->overcurrent, alarms->undervoltage, alarms->thermal_shutdown, alarms->user_switch, alarms->thermal_warning, alarms->stall_detect, alarms->adc_undervoltage);
}

void ps_setalarmconfig(bool command_error, bool overcurrent, bool undervoltage, bool thermal_shutdown, bool user_switch, bool thermal_warning, bool stall_detect, bool adc_undervoltage) {
  ps_alarms_reg reg = {
    .overcurrent = overcurrent? 0x1 : 0x0,
    .thermal_shutdown = thermal_shutdown? 0x1 : 0x0,
    .thermal_warning = thermal_warning? 0x1 : 0x0,
    .undervoltage = undervoltage? 0x1 : 0x0,
    .adc_undervoltage = adc_undervoltage? 0x1 : 0x0,
    .stall_detect = stall_detect? 0x1 : 0x0,
    .user_switch = user_switch? 0x1 : 0x0,
    .command_error = command_error? 0x1 : 0x0,
  };
  ps_xferreg("setparam alarmen", CMD_SETPARAM(PARAM_ALARMEN), reg);
}

ps_alarms ps_getalarms() {
  ps_status_reg reg = {};
  ps_xferreg("getparam status", CMD_GETPARAM(PARAM_STATUS), reg);
  return (ps_alarms){
    .command_error = reg.cmd_error,
    .overcurrent = !reg.ocd,
    .undervoltage = !reg.uvlo,
    .thermal_shutdown = reg.th_status == TH_BRIDGESHUTDOWN || reg.th_status == TH_DEVICESHUTDOWN,
    .user_switch = reg.sw_evn,
    .thermal_warning = reg.th_status == TH_WARNING,
    .stall_detect = !reg.stall_a || !reg.stall_b,
    .adc_undervoltage = !reg.uvlo_adc
  };
}

ps_ktvals ps_getktvals(ps_mode mode) {
  uint8_t kthold = 0, ktrun = 0, ktacc = 0, ktdec = 0;
  ps_xfer("getparam ktvalhold", CMD_GETPARAM(PARAM_KTVALHOLD), &kthold, 1);
  ps_xfer("getparam ktvalrun", CMD_GETPARAM(PARAM_KTVALRUN), &ktrun, 1);
  ps_xfer("getparam ktvalacc", CMD_GETPARAM(PARAM_KTVALACC), &ktacc, 1);
  ps_xfer("getparam ktvaldec", CMD_GETPARAM(PARAM_KTVALDEC), &ktdec, 1);
  return (ps_ktvals){
    .hold = (float)kthold * KTVALS_COEFF * (mode == MODE_VOLTAGE? 1.0 : 2.0),
    .run = (float)ktrun * KTVALS_COEFF * (mode == MODE_VOLTAGE? 1.0 : 2.0),
    .accel = (float)ktacc * KTVALS_COEFF * (mode == MODE_VOLTAGE? 1.0 : 2.0),
    .decel = (float)ktdec * KTVALS_COEFF * (mode == MODE_VOLTAGE? 1.0 : 2.0)
  };
}

void ps_setktvals(ps_mode mode, float hold, float run, float accel, float decel) {
  uint8_t kthold = (uint8_t)(round(constrain(hold, 0.0, 1.0) * (1.0 / KTVALS_COEFF) * (mode == MODE_VOLTAGE? 1.0 : 0.5)));
  uint8_t ktrun = (uint8_t)(round(constrain(run, 0.0, 1.0) * (1.0 / KTVALS_COEFF) * (mode == MODE_VOLTAGE? 1.0 : 0.5)));
  uint8_t ktacc = (uint8_t)(round(constrain(accel, 0.0, 1.0) * (1.0 / KTVALS_COEFF) * (mode == MODE_VOLTAGE? 1.0 : 0.5)));
  uint8_t ktdec = (uint8_t)(round(constrain(decel, 0.0, 1.0) * (1.0 / KTVALS_COEFF) * (mode == MODE_VOLTAGE? 1.0 : 0.5)));
  ps_xfer("setparam ktvalhold", CMD_SETPARAM(PARAM_KTVALHOLD), &kthold, 1);
  ps_xfer("setparam ktvalrun", CMD_SETPARAM(PARAM_KTVALRUN), &ktrun, 1);
  ps_xfer("setparam ktvalacc", CMD_SETPARAM(PARAM_KTVALACC), &ktacc, 1);
  ps_xfer("setparam ktvaldec", CMD_SETPARAM(PARAM_KTVALDEC), &ktdec, 1);
}

ps_vm_bemf ps_vm_getbemf() {
  uint8_t buf[2] = {};
  uint8_t stslp = 0, fnslpacc = 0, fnslpdec = 0;
  ps_xfer("getparam stslp", CMD_GETPARAM(PARAM_STSLP), &stslp, 1);
  ps_xfer("getparam intspeed", CMD_GETPARAM(PARAM_INTSPEED), buf, 2);
  ps_xfer("getparam fnslpacc", CMD_GETPARAM(PARAM_FNSLPACC), &fnslpacc, 1);
  ps_xfer("getparam fnslpdec", CMD_GETPARAM(PARAM_FNSLPDEC), &fnslpdec, 1);
  uint16_t intspeed = ps_get16(buf);
  return (ps_vm_bemf){
    .slopel = ((float)stslp) * BEMFSLOPE_COEFF,
    .speedco = ((float)(intspeed & BEMFSPEEDCO_MASK)) * BEMFSPEEDCO_COEFF,
    .slopehacc = ((float)fnslpacc) * BEMFSLOPE_COEFF,
    .slopehdec = ((float)fnslpdec) * BEMFSLOPE_COEFF
  };
}

void ps_vm_setbemf(float slopel, float speedco, float slopehacc, float slopehdec) {
  uint8_t buf[2] = {};
  uint8_t stslp = min((int)round(slopel / BEMFSLOPE_COEFF), BEMFSLOPE_MASK);
  uint16_t intspeed = min((int)round(speedco / BEMFSPEEDCO_COEFF), BEMFSPEEDCO_MASK);
  uint8_t fnslpacc = min((int)round(slopehacc / BEMFSLOPE_COEFF), BEMFSLOPE_MASK);
  uint8_t fnslpdec = min((int)round(slopehdec / BEMFSLOPE_COEFF), BEMFSLOPE_MASK);
  ps_set16(intspeed, buf);
  ps_xfer("setparam stslp", CMD_SETPARAM(PARAM_STSLP), &stslp, 1);
  ps_xfer("setparam intspeed", CMD_SETPARAM(PARAM_INTSPEED), buf, 2);
  ps_xfer("setparam fnslpacc", CMD_SETPARAM(PARAM_FNSLPACC), &fnslpacc, 1);
  ps_xfer("setparam fnslpdec", CMD_SETPARAM(PARAM_FNSLPDEC), &fnslpdec, 1);
}

float ps_vm_getstall() {
  uint8_t stallth = 0;
  ps_xfer("getparam stallth", CMD_GETPARAM(PARAM_STALLTH), &stallth, 1);
  return ((float)(stallth & STALL_MASK) * STALL_COEFF) + STALL_OFFSET;
}

void ps_vm_setstall(float millivolts) {
  if (millivolts < STALL_OFFSET) millivolts = STALL_OFFSET;
  uint8_t stallth = min((int)round((millivolts - STALL_OFFSET) / STALL_COEFF), STALL_MASK);
  ps_xfer("setparam stallth", CMD_SETPARAM(PARAM_STALLTH), &stallth, 1);
}

bool ps_vm_getvscomp() {
  ps_config_reg reg = {};
  ps_xferreg("getparam config", CMD_GETPARAM(PARAM_CONFIG), reg);
  return (bool)reg.vm.en_vscomp;
}

void ps_vm_setvscomp(bool voltage_compensation) {
  ps_config_reg reg = {};
  ps_xferreg("getparam config", CMD_GETPARAM(PARAM_CONFIG), reg);
  reg.vm.en_vscomp = voltage_compensation? 0x1 : 0x0;
  ps_xferreg("setparam config", CMD_SETPARAM(PARAM_CONFIG), reg);
}

ps_cm_ctrltimes ps_cm_getctrltimes() {
  uint8_t tonmin = 0, toffmin = 0;
  ps_tfast_reg reg = {};
  ps_xfer("getparam tonmin", CMD_GETPARAM(PARAM_TONMIN), &tonmin, 1);
  ps_xfer("getparam toffmin", CMD_GETPARAM(PARAM_TOFFMIN), &toffmin, 1);
  ps_xferreg("getparam tfast", CMD_GETPARAM(PARAM_TFAST), reg);
  ps_print(&reg);
  return (ps_cm_ctrltimes){
    .min_on_us = ((float)(tonmin & MINCTRL_MASK) * MINCTRL_COEFF) + MINCTRL_OFFSET,
    .min_off_us = ((float)(toffmin & MINCTRL_MASK) * MINCTRL_COEFF) + MINCTRL_OFFSET,
    .fast_off_us = ((float)(reg.toff_fast & TFAST_MASK) * TFAST_COEFF) + TFAST_OFFSET,
    .fast_step_us = ((float)(reg.fast_step & TFAST_MASK) * TFAST_COEFF) + TFAST_OFFSET
  };
}

void ps_cm_setctrltimes(float min_on_us, float min_off_us, float fast_off_us, float fast_step_us) {
  if (min_on_us < MINCTRL_OFFSET) min_on_us = MINCTRL_OFFSET;
  if (min_off_us < MINCTRL_OFFSET) min_off_us = MINCTRL_OFFSET;
  if (fast_off_us < TFAST_OFFSET) fast_off_us = TFAST_OFFSET;
  if (fast_step_us < TFAST_OFFSET) fast_step_us = TFAST_OFFSET;
  ps_tfast_reg reg = {};
  uint8_t tonmin = min((int)round((min_on_us - MINCTRL_OFFSET) / MINCTRL_COEFF), MINCTRL_MASK);
  uint8_t toffmin = min((int)round((min_off_us - MINCTRL_OFFSET) / MINCTRL_COEFF), MINCTRL_MASK);
  reg.toff_fast = min((int)round((fast_off_us - TFAST_OFFSET) / TFAST_COEFF), TFAST_MASK);
  reg.fast_step = min((int)round((fast_step_us - TFAST_OFFSET) / TFAST_COEFF), TFAST_MASK);
  ps_xfer("setparam tonmin", CMD_SETPARAM(PARAM_TONMIN), &tonmin, 1);
  ps_xfer("setparam toffmin", CMD_SETPARAM(PARAM_TOFFMIN), &toffmin, 1);
  ps_xferreg("setparam tfast", CMD_SETPARAM(PARAM_TFAST), reg);
}

bool ps_cm_getpredict() {
  ps_config_reg reg = {};
  ps_xferreg("getparam config", CMD_GETPARAM(PARAM_CONFIG), reg);
  return (bool)reg.cm.pred_en;
}

void ps_cm_setpredict(bool enable_predict) {
  ps_config_reg reg = {};
  ps_xferreg("getparam config", CMD_GETPARAM(PARAM_CONFIG), reg);
  reg.cm.pred_en = enable_predict? 0x1 : 0x0;
  ps_xferreg("setparam config", CMD_SETPARAM(PARAM_CONFIG), reg);
}

float ps_cm_getswitchperiod() {
  ps_config_reg reg = {};
  ps_xferreg("getparam config", CMD_GETPARAM(PARAM_CONFIG), reg);
  return (float)reg.cm.tsw / CM_TSW_COEFF;
}

void ps_cm_setswitchperiod(float period_us) {
  ps_config_reg reg = {};
  uint8_t tsw = min((int)round(period_us * CM_TSW_COEFF), CM_TSW_MASK);
  ps_xferreg("getparam config", CMD_GETPARAM(PARAM_CONFIG), reg);
  reg.cm.tsw = tsw;
  ps_xferreg("setparam config", CMD_SETPARAM(PARAM_CONFIG), reg);
}

bool ps_cm_gettqreg() {
  ps_config_reg reg = {};
  ps_xferreg("getparam config", CMD_GETPARAM(PARAM_CONFIG), reg);
  return (bool)reg.cm.en_tqreg;
}

void ps_cm_settqreg(bool current_from_adc) {
  ps_config_reg reg = {};
  ps_xferreg("getparam config", CMD_GETPARAM(PARAM_CONFIG), reg);
  reg.cm.en_tqreg = current_from_adc? 0x1 : 0x0;
  ps_xferreg("setparam config", CMD_SETPARAM(PARAM_CONFIG), reg);
}

int ps_readadc() {
  uint8_t adc = 0;
  ps_xfer("getparam adcout", CMD_GETPARAM(PARAM_ADCOUT), &adc, 1);
  ps_print("Adc", adc);
  return (int)adc;
}

uint32_t ps_move(ps_direction dir, uint32_t steps) {
  uint8_t buf[3] = {};
  steps = min(steps, (uint32_t)MOVE_MASK);
  ps_set24(steps, buf);
  ps_xfer("move", CMD_MOVE(dir), buf, 3);
  return steps;
}

void ps_softstop() {
  ps_xfer("softstop", CMD_SOFTSTOP(), NULL, 0);
}

void ps_hardstop() {
  ps_xfer("hardstop", CMD_HARDSTOP(), NULL, 0);
}

void ps_softhiz() {
  ps_xfer("softhiz", CMD_SOFTHIZ(), NULL, 0);
}
void ps_hardhiz() {
  ps_xfer("hardhiz", CMD_HARDHIZ(), NULL, 0);
}

static inline int32_t ps_xferpos(int32_t pos, uint8_t * buf) {
  int32_t oldp = ps_get24(buf);
  if (oldp & 0x00200000) oldp |= 0xFFC00000;
  uint32_t newp = pos & ABSPOS_MASK;
  ps_set24(newp, buf);
  return oldp;
}

int32_t ps_getpos() {
  uint8_t buf[3] = {};
  ps_xfer("getparam abspos", CMD_GETPARAM(PARAM_ABSPOS), buf, 3);
  return ps_xferpos(0, buf);
}

void ps_setpos(int32_t pos) {
  uint8_t buf[3] = {};
  ps_xferpos(pos, buf);
  ps_xfer("setparam abspos", CMD_SETPARAM(PARAM_ABSPOS), buf, 3);
}

void ps_resetpos() {
  ps_xfer("resetpos", CMD_RESETPOS(), NULL, 0);
}

void ps_gohome() {
  ps_xfer("gohome", CMD_GOHOME(), NULL, 0);
}

int32_t ps_getmark() {
  uint8_t buf[3] = {};
  ps_xfer("getparam mark", CMD_GETPARAM(PARAM_MARK), buf, 3);
  return ps_xferpos(0, buf);
}

void ps_setmark(int32_t mark) {
  uint8_t buf[3] = {};
  ps_xferpos(mark, buf);
  ps_xfer("setparam mark", CMD_SETPARAM(PARAM_MARK), buf, 3);
}

void ps_gomark() {
  ps_xfer("gomark", CMD_GOMARK(), NULL, 0);
}

void ps_goto(int32_t pos) {
  uint8_t buf[3] = {};
  ps_xferpos(pos, buf);
  ps_xfer("goto", CMD_GOTO(), buf, 3);
}

void ps_goto(int32_t pos, ps_direction dir) {
  uint8_t buf[3] = {};
  ps_xferpos(pos, buf);
  ps_xfer("gotodir", CMD_GOTODIR(dir), buf, 3);
}

float ps_getspeed() {
  uint8_t buf[3] = {};
  ps_xfer("getparam speed", CMD_GETPARAM(PARAM_SPEED), buf, 3);
  return (float)ps_get24(buf) * SPEED_COEFF;
}

void ps_run(ps_direction dir, float stepss) {
  uint32_t spd = (uint32_t)round(constrain(stepss, 0.0, SPEED_MAX) / SPEED_COEFF);
  uint8_t buf[3] = {};
  ps_set24(spd, buf);
  ps_xfer("run", CMD_RUN(dir), buf, 3);
}

void ps_stepclock(ps_direction dir) {
  ps_xfer("stepclock", CMD_STEPCLOCK(dir), NULL, 0);
}

void ps_gountil(ps_posact act, ps_direction dir, float speed) {
  uint32_t spd = (uint32_t)round(constrain(speed, 0.0, SPEED_MAX) / SPEED_COEFF);
  uint8_t buf[3] = {};
  ps_set24(spd, buf);
  ps_xfer("gountil", CMD_GOUNTIL(act, dir), buf, 3);
}

void ps_releasesw(ps_posact act, ps_direction dir) {
  ps_xfer("releasesw", CMD_RELEASESW(act, dir), NULL, 0);
}

