#ifndef __COMMAND_H
#define __COMMAND_H

#include "wifistepper.h"

#define QPRE_NONE       (0x00)
#define QPRE_STATUS     (0x20)
#define QPRE_NOTBUSY    (0x40)
#define QPRE_STOPPED    (0x80)

//#define CMD_NOP         (QPRE_NONE | 0x00)
#define CMD_STOP        (QPRE_NONE | 0x01)
#define CMD_RUN         (QPRE_NONE | 0x02)
#define CMD_STEPCLK     (QPRE_STOPPED | 0x03)
#define CMD_MOVE        (QPRE_STOPPED | 0x04)
#define CMD_GOTO        (QPRE_NOTBUSY | 0x05)
#define CMD_GOUNTIL     (QPRE_NONE | 0x06)
#define CMD_RELEASESW   (QPRE_NOTBUSY | 0x07)
#define CMD_GOHOME      (QPRE_NOTBUSY | 0x08)
#define CMD_GOMARK      (QPRE_NOTBUSY | 0x09)
#define CMD_RESETPOS    (QPRE_STOPPED | 0x0A)
#define CMD_SETPOS      (QPRE_NONE | 0x0B)
#define CMD_SETMARK     (QPRE_NONE | 0x0C)
#define CMD_SETCONFIG   (QPRE_STOPPED | 0x0D)
#define CMD_WAITBUSY    (QPRE_NOTBUSY | 0x0E)
#define CMD_WAITRUNNING (QPRE_STOPPED | 0x0F)
#define CMD_WAITMS      (QPRE_NONE | 0x10)
#define CMD_WAITSWITCH  (QPRE_STATUS | 0x11)
#define CMD_RUNQUEUE    (QPRE_NONE | 0x12)

typedef struct ispacked {
  uint32_t ms;
  unsigned int started;
} sketch_waitms_t;

#endif
