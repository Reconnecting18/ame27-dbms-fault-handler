/*
 * mock_hal.c — fake hardware for running bms.c on a PC.
 *
 * Every HAL_* function here has the same signature as in hal.h, so the
 * linker happily uses these instead of real drivers. See mock_hal.h for
 * the test-facing knobs.
 */
#include <string.h>
#include "mock_hal.h"

/* ---- backing storage for the "sensors" ---- */
static float    s_voltages[N_CELLS];
static float    s_temperatures[N_CELLS];
static uint32_t s_ms;

/* ---- the one pending RX frame RxCan() will pull with HAL_RecvCanMsg ---- */
static uint16_t s_rx_id;
static uint8_t  s_rx_data[CAN_LEN];

/* ---- captured outputs ---- */
bool     mock_sdc_closed;
int      mock_sdc_call_count;
uint8_t  mock_led[3];
uint16_t mock_can_tx_id;
uint8_t  mock_can_tx_data[CAN_LEN];
int      mock_can_tx_count;

/* ===================== test-side control ===================== */

void mock_reset(void)
{
    mock_set_all_voltages(3.7f);       /* healthy nominal cell */
    mock_set_all_temperatures(25.0f);  /* room temperature     */
    s_ms = 0;
    s_rx_id = 0;
    memset(s_rx_data, 0, sizeof s_rx_data);

    mock_sdc_closed = false;
    mock_sdc_call_count = 0;
    memset(mock_led, 0, sizeof mock_led);
    mock_can_tx_id = 0;
    memset(mock_can_tx_data, 0, sizeof mock_can_tx_data);
    mock_can_tx_count = 0;
}

void mock_set_all_voltages(float v)
{
    for (int i = 0; i < N_CELLS; i++) s_voltages[i] = v;
}

void mock_set_voltage(int cell, float v)
{
    if (cell >= 0 && cell < N_CELLS) s_voltages[cell] = v;
}

void mock_set_all_temperatures(float t)
{
    for (int i = 0; i < N_CELLS; i++) s_temperatures[i] = t;
}

void mock_set_temperature(int cell, float t)
{
    if (cell >= 0 && cell < N_CELLS) s_temperatures[cell] = t;
}

void mock_set_ms(uint32_t ms)
{
    s_ms = ms;
}

void mock_inject_rx(uint16_t id, const uint8_t data[CAN_LEN])
{
    s_rx_id = id;
    memcpy(s_rx_data, data, CAN_LEN);
}

/*
 * ISENSE_DATA (0x511): current in mA as a signed big-endian integer in
 * data bytes 2..4. "Big-endian" = most significant byte first, so byte 2
 * holds bits 23..16, byte 3 holds bits 15..8, byte 4 holds bits 7..0.
 *
 * Negative numbers work automatically because C stores int32_t in two's
 * complement: shifting a negative value right and masking with 0xFF gives
 * you exactly the low 24 bits, which is what a 24-bit signed field holds.
 * (The decoder in RxCan has to *sign-extend* those 24 bits back to 32 —
 * that's the tricky half, and it's the half Ethan writes.)
 */
void mock_inject_current_mA(int32_t mA)
{
    uint8_t d[CAN_LEN] = {0};
    d[2] = (uint8_t)((mA >> 16) & 0xFF);
    d[3] = (uint8_t)((mA >>  8) & 0xFF);
    d[4] = (uint8_t)( mA        & 0xFF);
    mock_inject_rx(0x511, d);
}

void mock_inject_faults_clear(void)
{
    uint8_t d[CAN_LEN] = {0};
    mock_inject_rx(0x1CF, d);
}

/* ===================== HAL implementation ===================== */

void HAL_ReadVoltages(float data[N_CELLS])
{
    memcpy(data, s_voltages, sizeof s_voltages);
}

void HAL_ReadTemperatures(float data[N_CELLS])
{
    memcpy(data, s_temperatures, sizeof s_temperatures);
}

void HAL_SetSDC(bool closed)
{
    mock_sdc_closed = closed;
    mock_sdc_call_count++;
}

void HAL_SetLED(uint8_t r, uint8_t g, uint8_t b)
{
    mock_led[0] = r;
    mock_led[1] = g;
    mock_led[2] = b;
}

void HAL_SendCanMsg(uint16_t id, const uint8_t data[CAN_LEN])
{
    mock_can_tx_id = id;
    memcpy(mock_can_tx_data, data, CAN_LEN);
    mock_can_tx_count++;
}

void HAL_RecvCanMsg(uint16_t* id, uint8_t data[CAN_LEN])
{
    *id = s_rx_id;
    memcpy(data, s_rx_data, CAN_LEN);
}

uint32_t HAL_GetMS(void)
{
    return s_ms;
}
