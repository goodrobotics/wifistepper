#ifndef __POWERSTEP01PRIV_H
#define __POWERSTEP01PRIV_H

#ifndef __ps_packed
#define __ps_packed   __attribute__((packed))
#endif

#define __MS(v, m, s)           (((v) & m) << s)

#define MASK_PARAM              (0x1F)
#define MASK_DIR                (0x01)
#define MASK_ACT                (0x01)

#define SHIFT_PARAM             (0)
#define SHIFT_DIR               (0)
#define SHIFT_ACT               (3)

#define CMD_NOP()               (0b00000000)
#define CMD_SETPARAM(param)     (0b00000000 | __MS(param, MASK_PARAM, SHIFT_PARAM))
#define CMD_GETPARAM(param)     (0b00100000 | __MS(param, MASK_PARAM, SHIFT_PARAM))
#define CMD_RUN(dir)            (0b01010000 | __MS(dir, MASK_DIR, SHIFT_DIR))
#define CMD_STEPCLOCK(dir)      (0b01011000 | __MS(dir, MASK_DIR, SHIFT_DIR))
#define CMD_MOVE(dir)           (0b01000000 | __MS(dir, MASK_DIR, SHIFT_DIR))
#define CMD_GOTO()              (0b01100000)
#define CMD_GOTODIR(dir)        (0b01101000 | __MS(dir, MASK_DIR, SHIFT_DIR))
#define CMD_GOUNTIL(act, dir)   (0b10000010 | __MS(act, MASK_ACT, SHIFT_ACT) | __MS(dir, MASK_DIR, SHIFT_DIR))
#define CMD_RELEASESW(act, dir) (0b10010010 | __MS(act, MASK_ACT, SHIFT_ACT) | __MS(dir, MASK_DIR, SHIFT_DIR))
#define CMD_GOHOME()            (0b01110000)
#define CMD_GOMARK()            (0b01111000)
#define CMD_RESETPOS()          (0b11011000)
#define CMD_RESETDEVICE()       (0b11000000)
#define CMD_SOFTSTOP()          (0b10110000)
#define CMD_HARDSTOP()          (0b10111000)
#define CMD_SOFTHIZ()           (0b10100000)
#define CMD_HARDHIZ()           (0b10101000)
#define CMD_GETSTATUS()         (0b11010000)

#define CMDSIZE(b)              ((b) + 1)

#define PARAM_ABSPOS            (0x01)
#define PARAM_ELPOS             (0x02)
#define PARAM_MARK              (0x03)
#define PARAM_SPEED             (0x04)
#define PARAM_ACC               (0x05)
#define PARAM_DEC               (0x06)
#define PARAM_MAXSPEED          (0x07)
#define PARAM_MINSPEED          (0x08)
#define PARAM_ADCOUT            (0x12)
#define PARAM_OCDTH             (0x13)
#define PARAM_FSSPD             (0x15)
#define PARAM_STEPMODE          (0x16)
#define PARAM_ALARMEN           (0x17)
#define PARAM_GATECFG1          (0x18)
#define PARAM_GATECFG2          (0x19)
#define PARAM_STATUS            (0x1B)
#define PARAM_CONFIG            (0x1A)

/* Both Voltage & Current Mode */
#define PARAM_KTVALHOLD         (0x09)
#define PARAM_KTVALRUN          (0x0A)
#define PARAM_KTVALACC          (0x0B)
#define PARAM_KTVALDEC          (0x0C)

/* Voltage Mode */
#define PARAM_INTSPEED          (0x0D)
#define PARAM_STSLP             (0x0E)
#define PARAM_FNSLPACC          (0x0F)
#define PARAM_FNSLPDEC          (0x10)
#define PARAM_KTHERM            (0x11)
#define PARAM_STALLTH           (0x14)

/* Current Mode */
#define PARAM_TFAST             (0x0E)
#define PARAM_TONMIN            (0x0F)
#define PARAM_TOFFMIN           (0x10)

/* REG ABSPOS */
#define ABSPOS_MASK       (0x003FFFFF)

/* REG ELPOS */
typedef struct __ps_packed {
  uint8_t __unused : 7;
  uint8_t step_U : 1;
  
  uint8_t microstep : 7;
  uint8_t step_L : 1;
} ps_elpos_reg;

/* REG MARK */
#define MARK_MASK       (0x003FFFFF)

/* REG SPEED */
#define SPEED_COEFF       (0.015)
#define SPEED_MAX         (15625.0)

/* REG ACC */
#define ACC_COEFF         (0.137438)
#define ACC_MASK          (0x0FFF)

/* REG DEC */
#define DEC_COEFF         (0.137438)
#define DEC_MASK          (0x0FFF)

/* REG MAXSPEED */
#define MAXSPEED_COEFF    (0.065536)
#define MAXSPEED_MASK     (0x03FF)

/* REG MINSPEED */
typedef struct __ps_packed {
  uint8_t min_speed_U : 4;
  uint8_t lspd_opt : 1;
  uint8_t __unused : 3;

  uint8_t min_speed_L : 8;
} ps_minspeed_reg;

#define MINSPEED_COEFF    (4.194304)
#define MINSPEED_MASK     (0x0FFF)

/* REG OCDTH */
#define OCDTH_COEFF       (0.032)
#define OCDTH_MASK        (0x1F)

/* REG FSSPD */
typedef struct __ps_packed {
  uint8_t fs_spd_U : 2;
  uint8_t boost_mode : 1;
  uint8_t __unused : 5;
  
  uint8_t fs_spd_L : 8;
} ps_fsspd_reg;

#define FSSPD_COEFF       (0.065536)
#define FSSPD_OFFSET      (0.5)
#define FSSPD_MASK        (0x03FF)

/* REG STEPMODE */
typedef struct __ps_packed {
  uint8_t step_sel : 3;
  uint8_t cm_vm : 1;
  uint8_t sync_sel : 3;
  uint8_t sync_en : 1;
} ps_stepmode_reg;

/* REG ALARMEN */
typedef struct __ps_packed {
  uint8_t overcurrent : 1;
  uint8_t thermal_shutdown : 1;
  uint8_t thermal_warning : 1;
  uint8_t undervoltage : 1;
  uint8_t adc_undervoltage : 1;
  uint8_t stall_detect : 1;
  uint8_t user_switch : 1;
  uint8_t command_error : 1;
} ps_alarms_reg;

/* REG GATECFG1 */
typedef struct __ps_packed {
  uint8_t tboost : 3;
  uint8_t wd_en : 1;
  uint8_t __unused : 4;

  uint8_t slew : 8;
} ps_gatecfg1_reg;

/* REG GATECFG2 */
typedef struct __ps_packed {
  uint8_t tdt : 5;
  uint8_t tblank : 3;
} ps_gatecfg2_reg;

/* REG STATUS */
typedef struct __ps_packed {
  uint8_t stck_mod : 1;
  uint8_t uvlo : 1;
  uint8_t uvlo_adc : 1;
  uint8_t th_status : 2;
  uint8_t ocd : 1;
  uint8_t stall_b : 1;
  uint8_t stall_a : 1;
  
  uint8_t hiz : 1;
  uint8_t busy : 1;
  uint8_t sw_f : 1;
  uint8_t sw_evn : 1;
  uint8_t dir : 1;
  uint8_t mot_status: 2;
  uint8_t cmd_error : 1;
} ps_status_reg;

typedef enum __ps_thstatus {
  TH_NORMAL           = 0x0,
  TH_WARNING          = 0x1,
  TH_BRIDGESHUTDOWN   = 0x2,
  TH_DEVICESHUTDOWN   = 0x3
} ps_thstatus;

/* REG CONFIG */
typedef struct __ps_packed {
  uint8_t __mask3 : 2;
  uint8_t f_pwm_dec : 3;
  uint8_t f_pwm_int : 3;

  uint8_t __mask1 : 5;
  uint8_t en_vscomp : 1;
  uint8_t __unused : 1;
  uint8_t __mask2 : 1;
} ps_config_vm_reg;

typedef struct __ps_packed {
  uint8_t __mask3 : 2;
  uint8_t tsw : 5;
  uint8_t pred_en : 1;

  uint8_t __mask1 : 5;
  uint8_t en_tqreg : 1;
  uint8_t __unused : 1;
  uint8_t __mask2 : 1;
} ps_config_cm_reg;

typedef struct __ps_packed {
  uint8_t uvloval : 1;
  uint8_t vccval : 1;
  uint8_t __mask2 : 6;

  uint8_t clk_sel : 4;
  uint8_t sw_mode : 1;
  uint8_t __mask1 : 2;
  uint8_t oc_sd : 1;
} ps_config_com_reg;

typedef union __ps_config_reg {
  ps_config_vm_reg vm;
  ps_config_cm_reg cm;
  ps_config_com_reg com;
} ps_config_reg;

#define CM_TSW_COEFF        (0.25)
#define CM_TSW_MASK         (0x1F)

/* KVALS */
#define KTVALS_COEFF        (0.00392156)

/* VM BEMF */
#define BEMFSPEEDCO_COEFF   (0.0596)
#define BEMFSPEEDCO_MASK    (0x3FFF)
#define BEMFSLOPE_COEFF     (0.0015)
#define BEMFSLOPE_MASK      (0xFF)

/* VM STALL */
#define STALL_COEFF         (31.25)
#define STALL_OFFSET        (31.25)
#define STALL_MASK          (0x1F)

/* REG TON_MIN / TOFF_MIN */
#define MINCTRL_COEFF       (0.5)
#define MINCTRL_OFFSET      (0.5)
#define MINCTRL_MASK        (0x7F)

/* REG TFAST */
typedef struct __ps_packed {
  uint8_t fast_step : 4;
  uint8_t toff_fast : 4;
} ps_tfast_reg;

#define TFAST_COEFF         (2.0)
#define TFAST_OFFSET        (2.0)
#define TFAST_MASK          (0x0F)

/* CMD MOVE */
#define MOVE_MASK         (0x003FFFFF)

#endif
