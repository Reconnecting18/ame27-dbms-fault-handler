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
    HAL_SetSDC(true);  /*SDC is open*/

}

void Iter(void)
{
    float HAL_ReadVoltages[N_CELLS];
    float HAL_ReadTemperatures[N_CELLS];

    for (int i = 0; i < N_CELLS; i++) { /*130 cells in battery pack, must iterate through each one*/
        /*Go through different fault cases*/
        /*over-voltage*/
        if (HAL_ReadVoltages[i]>4.2) {
            HAL_SetSDC(false);
        }
        /*under-voltage*/
        if (HAL_ReadVoltages[i]<2.5) {
            HAL_SetSDC(false);
        }

        /*over-temp*/
        if (HAL_ReadTemperatures[i] > 60) {
            HAL_SetSDC(false);
        }
        /*delta exceeded*/
        float max_voltage = HAL_ReadVoltages[i];
        float min_voltage = HAL_ReadVoltages[i];

        /*Determine the biggest voltage and the smallest voltage in the battery pack*/
        for (int k = 0; k < N_CELLS; k++) {
            if (HAL_ReadVoltages[k] > max_voltage) {
                max_voltage = HAL_ReadVoltages[k];
            }
            if (HAL_ReadVoltages[k] < min_voltage) {
                min_voltage = HAL_ReadVoltages[k];
            }
        }
        if (max_voltage-min_voltage > 0.2) {
            HAL_SetSDC(false);
        }
        /*over-current*/
         /*Sensor to measure amps? IF not we need to determine if its in watts or resistance (ohms)*/
    }

    /* 1. Read cell data into two local float arrays of N_CELLS.          */


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
