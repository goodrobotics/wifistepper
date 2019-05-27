#ifndef __POWERSTEP01_H
#define __POWERSTEP01_H

#ifndef __ps_packed
#define __ps_packed   __attribute__((packed))
#endif


typedef enum __ps_packed {
  REV       = 0x0,
  FWD       = 0x1
} ps_direction;

typedef enum __ps_packed {
  M_STOPPED     = 0x0,
  M_ACCEL       = 0x1,
  M_DECEL       = 0x2,
  M_CONSTSPEED  = 0x3
} ps_movement;

typedef struct __ps_packed {
  bool command_error;
  bool overcurrent;
  bool undervoltage;
  bool thermal_shutdown;
  bool user_switch;
  bool thermal_warning;
  bool stall_detect;
  bool adc_undervoltage;
} ps_alarms;

typedef struct __ps_packed {
  ps_direction direction;
  ps_movement movement;
  bool hiz;
  bool busy;
  bool user_switch;
  bool step_clock;
  ps_alarms alarms;
} ps_status;

typedef enum {
  POS_RESET         = 0x0,
  POS_COPYMARK      = 0x1
} ps_posact;


/* ACC */
float ps_getaccel();
void ps_setaccel(float steps_per_sec_2);

/* DEC */
float ps_getdecel();
void ps_setdecel(float steps_per_sec_2);

/* MAXSPEED */
float ps_getmaxspeed();
void ps_setmaxspeed(float steps_per_second);

/* MINSPEED */
typedef struct {
  float steps_per_sec;
  bool lowspeed_optim;
} ps_minspeed;

ps_minspeed ps_getminspeed();
void ps_setminspeed(float steps_per_second, bool lowspeed_optim);

/* ADCOUT */
int ps_readadc();

/* OCDTH */
typedef struct {
  float millivolts;
  bool shutdown;
} ps_ocd;

ps_ocd ps_getocd();
void ps_setocd(float millivolts, bool shutdown =true);

/* FULLSPEED */
typedef struct {
  float steps_per_sec;
  bool boost_mode;
} ps_fullstepspeed;

ps_fullstepspeed ps_getfullstepspeed();
void ps_setfullstepspeed(float steps_per_sec, bool boost_mode =false);

/* STEPMODE */
typedef enum __ps_packed {
  MODE_VOLTAGE  = 0x0,
  MODE_CURRENT  = 0x1
} ps_mode;

typedef enum {
  STEP_1    = 0x0,
  STEP_2    = 0x1,
  STEP_4    = 0x2,
  STEP_8    = 0x3,
  STEP_16   = 0x4,
  STEP_32   = 0x5,
  STEP_64   = 0x6,
  STEP_128  = 0x7
} ps_stepsize;

typedef enum {
  SYNC_BUSY     = 0x0,
  SYNC_STEP     = 0x1
} ps_sync;

typedef struct {
  ps_sync sync_mode;
  ps_stepsize sync_stepsize;
} ps_syncinfo;

ps_mode ps_getmode();
ps_stepsize ps_getstepsize();
ps_syncinfo ps_getsync();
void ps_setmode(ps_mode mode);
void ps_setstepsize(ps_stepsize stepsize);
void ps_setsync(ps_sync sync, ps_stepsize stepsize =STEP_1);

/* ALARMEN */
ps_alarms ps_getalarmconfig();
void ps_setalarmconfig(const ps_alarms * alarms);
void ps_setalarmconfig(bool command_error, bool overcurrent, bool undervoltage, bool thermal_shutdown, bool user_switch =false, bool thermal_warning =false, bool stall_detect =false, bool adc_undervoltage =false);
ps_alarms ps_getalarms();

/* SLEW RATES */
typedef enum {
  SR_114        = (0x40 | 0x18),
  SR_220        = (0x60 | 0x0C),
  SR_400        = (0x80 | 0x07),
  SR_520        = (0xA0 | 0x06),
  SR_790        = (0xC0 | 0x03),
  SR_980        = (0xD0 | 0x02)
} ps_slewrate;

ps_slewrate ps_getslewrate();
void ps_setslewrate(ps_slewrate slew);

/* CONFIG */
typedef enum {
  CLK_INT16         = 0x0,
  CLK_INT16_EXT2    = 0x8,
  CLK_INT16_EXT4    = 0x9,
  CLK_INT16_EXT8    = 0xA,
  CLK_INT16_EXT16   = 0xB,
  CLK_EXT8_XTAL     = 0x4,
  CLK_EXT16_XTAL    = 0x5,
  CLK_EXT24_XTAL    = 0x6,
  CLK_EXT32_XTAL    = 0x7,
  CLK_EXT8_OSC      = 0xC,
  CLK_EXT16_OSC     = 0xD,
  CLK_EXT24_OSC     = 0xE,
  CLK_EXT32_OSC     = 0xF
} ps_clocksel;

typedef enum {
  SW_HARDSTOP       = 0x0,
  SW_USER           = 0x1
} ps_swmode;

float ps_getclockfreq(ps_clocksel clock);
ps_clocksel ps_getclocksel();
void ps_setclocksel(ps_clocksel clock);

ps_swmode ps_getswmode();
void ps_setswmode(ps_swmode swmode);

/* KTVLAS */
typedef struct {
  float hold;
  float run;
  float accel;
  float decel;
} ps_ktvals;

ps_ktvals ps_getktvals(ps_mode mode);
void ps_setktvals(ps_mode mode, float hold, float run, float accel, float decel);

/* CONFIG */

/* VM */
typedef struct {
  uint8_t div;
  uint8_t mul;
} ps_vm_pwmfreq;

float ps_vm_coeffs2pwmfreq(ps_clocksel clock, ps_vm_pwmfreq * coeffs);
ps_vm_pwmfreq ps_vm_pwmfreq2coeffs(ps_clocksel clock, float pwmfreq);
ps_vm_pwmfreq ps_vm_getpwmfreq();
void ps_vm_setpwmfreq(ps_vm_pwmfreq * coeffs);

typedef struct {
  float slopel;
  float speedco;
  float slopehacc;
  float slopehdec;
} ps_vm_bemf;

ps_vm_bemf ps_vm_getbemf();
void ps_vm_setbemf(float slopel, float speedco, float slopehacc, float slopehdec);

float ps_vm_getstall();
void ps_vm_setstall(float millivolts);

bool ps_vm_getvscomp();
void ps_vm_setvscomp(bool voltage_compensation);

/* CM */
typedef struct {
  float min_on_us;
  float min_off_us;
  float fast_off_us;
  float fast_step_us;
} ps_cm_ctrltimes;

ps_cm_ctrltimes ps_cm_getctrltimes();
void ps_cm_setctrltimes(float min_on_us, float min_off_us, float fast_off_us, float fast_step_us);

bool ps_cm_getpredict();
void ps_cm_setpredict(bool enable_predict);

float ps_cm_getswitchperiod();
void ps_cm_setswitchperiod(float period_us);

bool ps_cm_gettqreg();
void ps_cm_settqreg(bool current_from_adc);

/* MOVE */
uint32_t ps_move(ps_direction dir, uint32_t steps);

void ps_softstop();
void ps_hardstop();
void ps_softhiz();
void ps_hardhiz();

int32_t ps_getpos();
void ps_setpos(int32_t pos);
void ps_resetpos();
void ps_gohome();

int32_t ps_getmark();
void ps_setmark(int32_t mark);
void ps_gomark();
void ps_goto(int32_t pos);
void ps_goto(int32_t pos, ps_direction dir);

float ps_getspeed();
void ps_run(ps_direction dir, float speed);
void ps_stepclock(ps_direction dir);

void ps_gountil(ps_posact act, ps_direction dir, float speed);
void ps_releasesw(ps_posact act, ps_direction dir);



void ps_spiinit();



typedef void (*ps_waitcb)(void);
ps_status ps_getstatus(bool clear_errors =false);
bool ps_isbusy();
void ps_waitbusy(ps_waitcb waitf =NULL);

bool ps_isrunning();
bool ps_ishiz();

void ps_reset();
void ps_nop();

#endif
