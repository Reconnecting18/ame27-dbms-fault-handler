/*
 * test_bms.c — scenario tests for the fault handler.
 *
 * Pattern for every test:
 *   1. Init()                      — fresh BMS state (latched = 0, SDC open)
 *   2. mock_set_*() / mock_inject_*  — arrange the world
 *   3. RxCan() if you injected a frame — simulate the interrupt firing
 *   4. Iter()                      — one 50 ms tick
 *   5. ASSERT(...)                 — check what bms.c did to the hardware
 *
 * mock_reset() runs automatically before each test (see RUN below).
 * A test is a function that returns 1 on pass; ASSERT returns 0 on the
 * first failed condition and records where it happened.
 */
#include <stdio.h>
#include <string.h>
#include "bms.h"
#include "mock_hal.h"

/* ---------------------------- tiny test runner ---------------------------- */
static int  g_run, g_failed;
static char g_fail_msg[256];

#define ASSERT(cond)                                                          \
    do {                                                                      \
        if (!(cond)) {                                                        \
            snprintf(g_fail_msg, sizeof g_fail_msg, "%s:%d: %s",              \
                     __FILE__, __LINE__, #cond);                              \
            return 0;                                                         \
        }                                                                     \
    } while (0)

#define TEST(name) static int name(void)

#define RUN(name)                                                             \
    do {                                                                      \
        g_run++;                                                              \
        g_fail_msg[0] = '\0';                                                 \
        mock_reset();                                                         \
        if (name()) printf("[ OK ] %s\n", #name);                             \
        else { g_failed++; printf("[FAIL] %s\n       %s\n", #name, g_fail_msg); } \
    } while (0)

/* ------------------------------- the tests -------------------------------- */

TEST(init_leaves_sdc_open)
{
    Init();
    /* No Iter() yet -> nothing has proven the pack safe -> SDC must be open. */
    ASSERT(mock_sdc_closed == false);
    ASSERT(mock_sdc_call_count >= 1);
    return 1;
}

TEST(healthy_pack_closes_sdc_and_reports_no_faults)
{
    Init();
    Iter();                                   /* nominal 3.7 V / 25 C, 0 mA */

    ASSERT(mock_sdc_closed == true);
    ASSERT(mock_can_tx_count == 1);           /* exactly one status frame per Iter */
    ASSERT(mock_can_tx_id == CAN_ID_BMS_FAULT_STATUS);
    ASSERT(mock_can_tx_data[0] == 0);         /* active  */
    ASSERT(mock_can_tx_data[1] == 0);         /* latched */
    return 1;
}

TEST(single_cell_over_voltage_opens_sdc)
{
    Init();
    mock_set_voltage(42, CELL_OV_THRESHOLD_V + 0.05f);   /* one bad cell out of 130 */
    Iter();

    ASSERT(mock_sdc_closed == false);
    ASSERT(mock_can_tx_data[0] & FAULT_CELL_OVER_VOLTAGE);   /* active  */
    ASSERT(mock_can_tx_data[1] & FAULT_CELL_OVER_VOLTAGE);   /* latched */
    return 1;
}

TEST(fault_stays_latched_after_condition_clears)
{
    Init();
    mock_set_voltage(42, CELL_OV_THRESHOLD_V + 0.05f);
    Iter();
    mock_set_voltage(42, 3.7f);
    Iter();
    ASSERT(mock_sdc_closed == false);
    ASSERT(!(mock_can_tx_data[0] & FAULT_CELL_OVER_VOLTAGE));
    ASSERT(mock_can_tx_data[1] & FAULT_CELL_OVER_VOLTAGE);
    return 1;
}

TEST(delta_exceeded_opens_sdc) {
    Init();
    mock_set_all_voltages(3.7f);
    mock_set_voltage(1, 3.95f + 0.25f);
    Iter();
    ASSERT(mock_sdc_closed == false);
    ASSERT(mock_can_tx_data[0] & FAULT_CELL_DELTA_EXCEEDED);
    ASSERT(mock_can_tx_data[1] & FAULT_CELL_DELTA_EXCEEDED);
    return 1;
}

/*
 * Ethan — tests for you to write (one function each, then add a RUN line):
 *
 *   under_voltage_opens_sdc
 *   over_temperature_opens_sdc
 *   delta_exceeded_opens_sdc          (all cells fine individually, spread > 0.2 V)
 *   over_current_discharge_opens_sdc  (inject +200001 mA, RxCan(), Iter())
 *   over_current_charge_opens_sdc     (inject -200001 mA — your abs() assumption)
 *   negative_current_below_threshold_is_ok (inject -50000 mA — sign-extension check!)
 *   fault_stays_latched_after_condition_clears   (OV, Iter, fix cell, Iter -> still open)
 *   clear_request_with_fault_still_present_is_ignored
 *   clear_request_after_recovery_closes_sdc
 *   multiple_faults_set_multiple_bits
 */

int main(void)
{
    RUN(init_leaves_sdc_open);
    RUN(healthy_pack_closes_sdc_and_reports_no_faults);
    RUN(single_cell_over_voltage_opens_sdc);
    RUN(fault_stays_latched_after_condition_clears);
    RUN(delta_exceeded_opens_sdc);

    printf("\n%d tests, %d failed\n", g_run, g_failed);
    return g_failed ? 1 : 0;
}
