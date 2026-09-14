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
static uint8_t s_latched; /*power on means no history*/

void Init(void)
{
    /* TODO: SDC is normally-open. Leave it open until the first Iter()
     *       has proven the pack is safe. Zero all state. */
    HAL_SetSDC(false);
    s_latched = 0;
}

void Iter(void)
{
    uint8_t active = 0;

    float voltages[N_CELLS];
    HAL_ReadVoltages(voltages);
    float temperatures[N_CELLS];
    HAL_ReadTemperatures(temperatures);

    for (int i = 0; i < N_CELLS; i++) { /*130 cells in battery pack, must iterate through each one*/
        /*Go through different fault cases*/
        /*over-voltage*/
        if (voltages[i] > CELL_OV_THRESHOLD_V) {
            active |= FAULT_CELL_OVER_VOLTAGE;
        }
        /*under-voltage*/
        if (voltages[i] < CELL_UV_THRESHOLD_V) {
            active |= FAULT_CELL_UNDER_VOLTAGE;
        }
        /*over-temp*/
        if (temperatures[i] > CELL_OT_THRESHOLD_C) {
            active |= FAULT_CELL_OVER_TEMPERATURE;
        }
        /*delta exceeded*/
        /*float max_voltage = voltages[i];
        float min_voltage = voltages[i];

        Determine the biggest voltage and the smallest voltage in the battery pack
        for (int k = 0; k < N_CELLS; k++) {
            if (voltages[k] > max_voltage) {
                max_voltage = voltages[k];
            }
            if (voltages[k] < min_voltage) {
                min_voltage = voltages[k];
            }
        }
        if (max_voltage-min_voltage > 0.2) {
            HAL_SetSDC(false);
        }
        over-current
         Sensor to measure amps? IF not we need to determine if its in watts or resistance (ohms)*/
    }
    s_latched |= active; /*copies tick's switches into memory, never clears anything*/
    /*closed = (is active qual to 0?)*/
    HAL_SetSDC(s_latched == 0); /*Decision - question that evaluates to true (true if no switch is on) or false (open, car dead)*/
    uint8_t frame[CAN_LEN] = {0}; /*73-75 are the reports, so it allows dashboards to listen for 0xB0, opens slot 0, turns on warning light*/
    frame[0] = active;
    frame[1] = s_latched;            
    HAL_SendCanMsg(CAN_ID_BMS_FAULT_STATUS, frame); /*Shows driver why car died*/

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
