#ifndef __ECC508A_H
#define __ECC508A_H

#ifndef __ecc_packed
#define __ecc_packed   __attribute__((packed))
#endif


typedef struct __ecc_packed {
  uint8_t cmd;
  uint8_t wait_typ;
  uint8_t wait_max;
  uint8_t rxlen;
  uint8_t datalen;
  unsigned long started;
} ecc_cmd_t;

typedef struct __ecc_packed {
  uint8_t sn_0;       uint8_t sn_1;       uint8_t sn_2;       uint8_t sn_3;
  uint8_t revnum_0;   uint8_t revnum_1;   uint8_t revnum_2;   uint8_t revnum_3;
  uint8_t sn_4;       uint8_t sn_5;       uint8_t sn_6;       uint8_t sn_7;
  uint8_t sn_8;       uint8_t __reserved_0;
  
  uint8_t i2c_enable : 1;
  uint8_t __unused_0 : 7;
  uint8_t __reserved_1;
  uint8_t i2c_address;
  uint8_t __reserved_2;
  uint8_t otpmode;
  struct {
    uint8_t selectormode : 1;
    uint8_t ttlenable : 1;
    uint8_t watchdog : 1;
    uint8_t __reserved_0 : 5;
  } chipmode;
  union {
    uint8_t bytes[2];
    struct {
      uint8_t readkey : 4;
      uint8_t nomac : 1;
      uint8_t limiteduse : 1;
      uint8_t encryptread : 1;
      uint8_t issecret : 1;
      uint8_t writekey : 4;
      uint8_t writeconfig: 4;
    } bits;
  } slotconfig[16];
  struct {
    uint8_t bytes[8];
  } counter[2];
  uint8_t lastkeyuse[16];
  uint8_t userextra;    uint8_t selector;   uint8_t lockvalue;  uint8_t lockconfig;
  union {
    struct {
      uint8_t slotlocked_7 : 1;
      uint8_t slotlocked_6 : 1;
      uint8_t slotlocked_5 : 1;
      uint8_t slotlocked_4 : 1;
      uint8_t slotlocked_3 : 1;
      uint8_t slotlocked_2 : 1;
      uint8_t slotlocked_1 : 1;
      uint8_t slotlocked_0 : 1;
      
      uint8_t slotlocked_F : 1;
      uint8_t slotlocked_E : 1;
      uint8_t slotlocked_D : 1;
      uint8_t slotlocked_C : 1;
      uint8_t slotlocked_B : 1;
      uint8_t slotlocked_A : 1;
      uint8_t slotlocked_9 : 1;
      uint8_t slotlocked_8 : 1;
    } bits;
    uint8_t bytes[2];
  } slotlocked;
  uint16_t __rfu;
  uint8_t x509format[4];
  union {
    uint8_t bytes[2];
    struct {
      uint8_t priv : 1;
      uint8_t pubinfo : 1;
      uint8_t keytype : 3;
      uint8_t lockable : 1;
      uint8_t reqrandom : 1;
      uint8_t reqauth : 1;
      uint8_t authkey : 4;
      uint8_t intrusiondisable : 1;
      uint8_t __rfu : 1;
      uint8_t x509id : 2;
    } bits;
  } keyconfig[16];
} ecc_configzone_t;

#define ECC_VERSION       ("WSX100 1")
#define ECC_FAILURE       (1)
#define ECC_SUCCESS       (2)

void ecc_init();
void ecc_loop(unsigned long now);
bool ecc_ok();
bool ecc_locked();
uint32_t ecc_random();
bool ecc_provision(const char * master, const char * newpass);
bool ecc_setpassword(const char * master, const char * newpass);
bool ecc_lowcom_hmac(uint32_t nonce, uint8_t * meta, size_t metalen, uint8_t * data, size_t dataoff, size_t datalen);

#endif
