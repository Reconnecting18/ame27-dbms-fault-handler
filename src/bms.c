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
static volatile bool s_clear_requested;
static uint8_t s_latched; /*power on means no history*/
static volatile int32_t s_current_mA;

void Init(void)
{
    /*SDC is normally-open. Leave it open until the first Iter() confirms pack is safe*/
    HAL_SetSDC(false);
    s_latched = 0;
    s_current_mA = 0;
    s_clear_requested = false;
}

void Iter(void)
{
    int32_t current_mA = s_current_mA; /*snapshot*/
    uint8_t active = 0;

    float voltages[N_CELLS];
    HAL_ReadVoltages(voltages);
    float temperatures[N_CELLS];
    HAL_ReadTemperatures(temperatures);

    float max_voltage = voltages[0];
    float min_voltage = voltages[0];


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

        /*Determine the biggest voltage and the smallest voltage in the battery pack*/
        
        if (voltages[i] > max_voltage) {
            max_voltage = voltages[i];
        }
        if (voltages[i] < min_voltage) {
            min_voltage = voltages[i];
        }

        /*over-current*/
        /*Sensor to measure amps? IF not we need to determine if its in watts or resistance (ohms)*/
    }
    /*delta exceeded*/
    if (max_voltage-min_voltage > CELL_DELTA_THRESHOLD_V) { /*determine if delta exceeds threshold*/
        active |= FAULT_CELL_DELTA_EXCEEDED;
    }
    if (current_mA > PACK_OC_THRESHOLD_MA || current_mA < -PACK_OC_THRESHOLD_MA) {
        active |= FAULT_PACK_OVER_CURRENT;
    }
    s_latched |= active; /*copies tick's switches into memory, never clears anything*/
    if (s_clear_requested) {
        s_latched &= active;
        s_clear_requested = false;
    }
    /*closed = (is active qual to 0?)*/
    HAL_SetSDC(s_latched == 0); /*Decision - question that evaluates to true (true if no switch is on) or false (open, car dead)*/
    uint8_t frame[CAN_LEN] = {0}; /*73-75 are the reports, so it allows dashboards to listen for 0xB0, opens slot 0, turns on warning light*/
    frame[0] = active;
    frame[1] = s_latched;            
    HAL_SendCanMsg(CAN_ID_BMS_FAULT_STATUS, frame); /*Shows driver why car died*/
}

void RxCan(void)
{
    /* Called every time a CAN frame is received on the bus, using an interrupt.
     * You may not use any HAL_* functions here except HAL_RecvCanMsg. */
    uint8_t data[CAN_LEN];
    uint16_t id;
    HAL_RecvCanMsg(&id, data);
    switch (id) {
        case CAN_ID_FAULTS_CLEAR:
            s_clear_requested = true;
            break;
        default:
            break;
    case CAN_ID_ISENSE_DATA: {
        int32_t raw = ((int32_t)data[2] << 16) | ((int32_t)data[3] << 8) | (int32_t)data[4]; /*Byte 3 lands at bits 15-8, so lowest bit goes to 8*/
        if (raw & 0x800000) {
            raw -= 0x1000000;
        }
        s_current_mA = raw;
        break;
    }
    }

    /* TODO: switch on id:
     *   CAN_ID_ISENSE_DATA  -> decode bytes 2..4 as a signed 24-bit
     *                          big-endian integer (mA), store it
     *   CAN_ID_FAULTS_CLEAR -> set the clear_requested flag
     *   anything else       -> ignore                                    */
}
