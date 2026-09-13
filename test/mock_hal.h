#ifndef MOCK_HAL_H
#define MOCK_HAL_H

/*
 * Test-side control panel for the fake hardware in mock_hal.c.
 *
 * The real HAL talks to ADCs, a relay, and a CAN controller. This mock
 * implements the exact same HAL_* function signatures (so bms.c doesn't
 * know the difference) but backs them with plain variables the tests can
 * set before calling Iter() and read after.
 */

#include <stdint.h>
#include <stdbool.h>
#include "hal.h"

/* ---- Inputs: what the "sensors" will report on the next HAL_Read*() ---- */
void mock_reset(void);                              /* everything to a safe, no-fault pack */
void mock_set_all_voltages(float v);
void mock_set_voltage(int cell, float v);
void mock_set_all_temperatures(float t);
void mock_set_temperature(int cell, float t);
void mock_set_ms(uint32_t ms);

/* ---- Inputs: pretend a CAN frame just arrived (then the test calls RxCan()) ---- */
void mock_inject_rx(uint16_t id, const uint8_t data[CAN_LEN]);
void mock_inject_current_mA(int32_t mA);            /* builds a 0x511 ISENSE_DATA frame */
void mock_inject_faults_clear(void);                /* builds a 0x1CF FAULTS_CLEAR frame */

/* ---- Outputs: what bms.c did to the "hardware" ---- */
extern bool     mock_sdc_closed;                    /* last HAL_SetSDC() argument */
extern int      mock_sdc_call_count;
extern uint8_t  mock_led[3];                        /* last HAL_SetLED() r,g,b */
extern uint16_t mock_can_tx_id;                     /* last HAL_SendCanMsg() id */
extern uint8_t  mock_can_tx_data[CAN_LEN];          /* last HAL_SendCanMsg() payload */
extern int      mock_can_tx_count;

#endif /* MOCK_HAL_H */
