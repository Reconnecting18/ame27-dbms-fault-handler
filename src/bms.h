#ifndef BMS_H
#define BMS_H

#include <stdint.h>

/*
 * Public interface of the BMS fault handler.
 *
 * Init / Iter / RxCan are the three entry points the firmware framework
 * calls (see docs/SPEC.md). Everything below them is the "contract" the
 * tests and the design doc rely on: which bit means which fault, and
 * what the thresholds are.
 */

void Init(void);   /* runs once at power-on                              */
void Iter(void);   /* runs every ~50 ms (20 Hz)                          */
void RxCan(void);  /* interrupt: runs whenever a CAN frame arrives       */

/* ---- Fault bit layout (one byte, bits 5-7 reserved) --------------------
 * `1u << n` is "the number with only bit n set". Writing them this way
 * instead of 0x01, 0x02, 0x04... makes the bit position obvious.
 */
#define FAULT_CELL_OVER_VOLTAGE     (1u << 0)
#define FAULT_CELL_UNDER_VOLTAGE    (1u << 1)
#define FAULT_CELL_OVER_TEMPERATURE (1u << 2)
#define FAULT_CELL_DELTA_EXCEEDED   (1u << 3)
#define FAULT_PACK_OVER_CURRENT     (1u << 4)

/* ---- Thresholds (from the spec) --------------------------------------- */
#define CELL_OV_THRESHOLD_V     4.2f
#define CELL_UV_THRESHOLD_V     2.5f
#define CELL_OT_THRESHOLD_C     60.0f
#define CELL_DELTA_THRESHOLD_V  0.2f
#define PACK_OC_THRESHOLD_MA    200000   /* 200 A, in mA to match ISENSE_DATA */

/* ---- CAN IDs ------------------------------------------------------------ */
#define CAN_ID_BMS_FAULT_STATUS 0x0B0    /* ours: Data0=active, Data1=latched */
#define CAN_ID_ISENSE_DATA      0x511    /* in:  current, mA, signed BE, bytes 2-4 */
#define CAN_ID_DIAG_HEARTBEAT   0x1CD    /* in:  optional, 1 Hz while tool attached */
#define CAN_ID_FAULTS_CLEAR     0x1CF    /* in:  request to clear latched faults */

#endif /* BMS_H */
