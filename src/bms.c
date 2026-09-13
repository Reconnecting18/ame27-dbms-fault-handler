/*
 * bms.c — BMS fault handler
 *
 * Ethan: this is yours to fill in. The comments below are the plan from
 * docs/SPEC.md, broken into the order that matters. Build with `make`,
 * run the tests with `make test`, and step through with `make debug`.
 */
#include "hal.h"
#include "bms.h"

/* ---- Shared state ---------------------------------------------------------
 * RxCan() is an interrupt and can fire between ANY two lines of Iter().
 * Anything RxCan writes and Iter reads must be `volatile` so the compiler
 * re-reads it from memory every time instead of caching it in a register.
 * `static` keeps these private to this file (no other .c file can see them).
 */
/* TODO: volatile current reading (mA) written by RxCan, read by Iter   */
/* TODO: volatile clear_requested flag written by RxCan, read by Iter    */
/* TODO: latched fault byte (only Iter touches this — no volatile needed) */

void Init(void)
{
    /* TODO: SDC is normally-open. Leave it open until the first Iter()
     *       has proven the pack is safe. Zero all state. */
    HAL_SETSDC() = 1 /*SDC is open*/

}

void Iter(void)
{
    /* 1. Read cell data into two local float arrays of N_CELLS.          */
    for (int i = 0; i++) {
        
    }

    /* 2. Evaluate the 5 faults into a local `active` byte:
     *      - walk the arrays once, tracking max/min voltage & max temp
     *      - OV / UV / OT / DELTA from those
     *      - OC from the volatile current reading (abs value — see spec)   */

    /* 3. latched |= active   (unconditionally remember anything new)     */

    /* 4. if clear was requested: latched &= active; then clear the flag  */

    /* 5. HAL_SetSDC(latched == 0)                                        */

    /* 6. Build the 0xB0 frame: data[0]=active, data[1]=latched, rest 0.  */
}

void RxCan(void)
{
    /* Called every time a CAN frame is received on the bus, using an interrupt.
     * You may not use any HAL_* functions here except HAL_RecvCanMsg. */
    uint8_t data[CAN_LEN];
    uint16_t id;
    HAL_RecvCanMsg(&id, data);

    /* TODO: switch on id:
     *   CAN_ID_ISENSE_DATA  -> decode bytes 2..4 as a signed 24-bit
     *                          big-endian integer (mA), store it
     *   CAN_ID_FAULTS_CLEAR -> set the clear_requested flag
     *   anything else       -> ignore                                    */
}
