# AME27 DBMS Design Challenge — Reference Spec

Clean spec extracted from the official AME27 Embedded Systems Design Challenge
(Distributed Battery Management System). Only the sections relevant to this
challenge are included. This file is the source of truth for the project —
do not consult the original PDF.

## Ground rules (from the challenge header)

1. AI use is allowed, but the interview will ask about the design process —
   "overreliance on AI is easy to spot."
2. The challenge is intentionally very open-ended; take time to learn concepts
   you don't already know. You are the designer.
3. It is based on a real project the team works on.

## Background

You are designing the fault-handling system for a BMS (Battery Management
System). The BMS monitors the entire battery pack, made up of 130 battery cells
wired in series. For safety, the BMS must monitor each individual cell voltage,
as even if a single cell is over its maximum voltage or temperature, it could
be catastrophic.

When the BMS detects a fault, it has the ability to open the SDC (Shutdown
Circuit), immediately de-energizing the car. It should also send the reason for
the fault over the CAN bus to be displayed on the vehicle's dash and debugging
tools. The SDC is normally-open, so do not close the SDC until you know the
battery is safe.

After a fault is thrown, the BMS should latch the fault state, only clearing
the fault and re-energizing the car when a FAULTS_CLEAR frame is received on
the CAN bus. An "active" fault is one that is currently present. A "latched"
fault is a fault that was active at some point, but may or may not still be
present.

## Faults

| Fault | Condition |
|---|---|
| CELL_OVER_VOLTAGE | any single cell's voltage is above 4.2 V |
| CELL_UNDER_VOLTAGE | any single cell's voltage is below 2.5 V |
| CELL_OVER_TEMPERATURE | any single cell's temperature is above 60 °C |
| CELL_DELTA_EXCEEDED | max minus min cell voltage is above 0.2 V |
| PACK_OVER_CURRENT | current sensor reads above 200 A |

## CAN bus basics

Standard CAN frame: 11-bit ID field + up to 64-bit (8-byte) data field. The
BMS is allocated all CAN IDs matching `0BX` in hex (i.e. `0xB0`–`0xBF`) — pick
any ID in that range for the outgoing fault-status frame.

| Frame | CAN ID | Rate | Format |
|---|---|---|---|
| ISENSE_DATA | 0x511 | 10 Hz | Current in mA, signed big-endian int, in data bytes 2-4. Positive = discharge (out of pack), negative = charge (into pack) |
| DIAGNOSTIC_HEARTBEAT | 0x1CD | 1 Hz while tool connected | No data. Optional — use or ignore |
| FAULTS_CLEAR | 0x1CF | On demand (from diagnostic tool) | No data. Clear latched faults on receipt, unless the faulting condition is still present |

## Shutdown circuit

The SDC is a single circuit through all the E-stops and safety devices on the
car, controlling the battery pack's isolation relays. "Opening" the SDC turns
off the high-voltage pack directly. "Closing" it allows the pack to energize,
assuming every other safety device in the chain is also closed. `HAL_SetSDC`
is the abstraction for it.

## Task

Every `Iter` cycle (~20 Hz): read cell data, check for faults, update SDC
state, and send fault data over CAN. Internal state may be organized however
you like; reasonable assumptions may be made and documented.

**Constraint on `RxCan`:** it runs as an interrupt and can be called at any
point during `Iter`'s execution. Inside `RxCan` only `HAL_RecvCanMsg` may be
called — no other `HAL_*` functions. Anything `RxCan` receives (a current
reading, a clear request) must be stored in shared state for `Iter` to act on
next cycle.

Send which faults are currently active and which are latched — ideally in one
frame rather than several, to limit bus traffic.

## Deliverables

A PDF documenting design decisions, supporting reasoning, and the CAN frame
format chosen, plus a link to the code (GitHub or Google Drive — make sure
the team has access). Any assumptions made must be outlined in the
submission.

## Optional bonus ideas (not required)

- Send cell data over CAN for the diagnostic tool
- Store/send peak values associated with a fault (e.g. peak current during an overcurrent event)
- Indicate fault state on a status LED
- A configuration interface for fault thresholds
- Anything else, as long as it doesn't impact the safety requirements

## Provided code

### hal.h

"Hardware Abstraction Layer" — wraps the BMS hardware. There is no
implementation for these functions; applicants are encouraged to implement
them however necessary to test their code (that is what `test/mock_hal.c` is).

```c
#ifndef _HAL_H_
#define _HAL_H_

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define CAN_LEN 8       // A CAN message has 8 data bytes
#define N_CELLS 130

// Reads the voltage data for all the cells in volts
void HAL_ReadVoltages(float data[N_CELLS]);

// Reads the temperature data for all the cells in deg C
void HAL_ReadTemperatures(float data[N_CELLS]);

// Sets the state of the Shutdown Circuit
void HAL_SetSDC(bool closed);

// Sets a status LED to an RGB color (0-255 each)
void HAL_SetLED(uint8_t r, uint8_t g, uint8_t b);

// Send a CAN message onto the bus
void HAL_SendCanMsg(uint16_t id, const uint8_t data[CAN_LEN]);

// Read a CAN message from the bus.
// The ID is written into *id and the data into the buffer.
void HAL_RecvCanMsg(uint16_t* id, uint8_t data[CAN_LEN]);

// Gets the time in milliseconds since the controller was powered on.
uint32_t HAL_GetMS(void);

#endif
```

### BMS.c (template to implement)

```c
#include "hal.h"

void Init()
{
    // This function runs once on startup
}

void Iter()
{
    // This function runs periodically at ~20Hz
}

void RxCan()
{
    // Called every time a CAN frame is received on the bus, using an interrupt.
    // Keep in mind, this can be called at any point in the execution of your program.
    // You may not use any HAL_* functions here except HAL_RecvCanMsg,
    // which is how you can pull the message from the bus.
    uint8_t data[CAN_LEN];
    uint16_t id;
    HAL_RecvCanMsg(&id, data);
}
```

---

## Design decisions already made

### Fault bit encoding (one byte, bits 5-7 reserved)

| Bit | Fault |
|---|---|
| 0 | CELL_OVER_VOLTAGE |
| 1 | CELL_UNDER_VOLTAGE |
| 2 | CELL_OVER_TEMPERATURE |
| 3 | CELL_DELTA_EXCEEDED |
| 4 | PACK_OVER_CURRENT |

Two bytes of this shape: one for active faults, one for latched faults.

### Outgoing CAN frame

CAN ID `0xB0`. Data0 = active faults bitmask, Data1 = latched faults bitmask,
Data2-7 unused. Sent once per `Iter` cycle.

### `Iter` order of operations (this order matters)

1. Read cell data — `HAL_ReadVoltages`, `HAL_ReadTemperatures`
2. Evaluate the 5 faults (including the current reading, sourced from `RxCan`)
3. `latched |= active` — always latch any newly active fault, unconditionally
4. If a clear was requested (flag set by `RxCan` on `FAULTS_CLEAR`):
   `latched &= active`, then clear the flag — this drops any bit that's no
   longer active, while a bit that's still active survives (it was just
   re-OR'd in step 3)
5. `SDC_closed = (latched == 0)` — close only if there is zero latched fault state
6. Send the `0xB0` status frame

### `RxCan` handling

- On `ISENSE_DATA` (`0x511`): parse the signed current value (mA) out of the
  data bytes and store it in a `volatile` shared variable for `Iter` to check
- On `FAULTS_CLEAR` (`0x1CF`): set a `volatile` `clear_requested` flag for
  `Iter` to act on
- Nothing else — no HAL calls beyond `HAL_RecvCanMsg`

### Open assumption to document explicitly

The spec says PACK_OVER_CURRENT throws "above 200A," but current can be
negative (charging). Recommended: check `abs(current) > 200A` rather than just
`current > 200A`, since a 200A charging current is arguably just as dangerous
to the cells as discharge. This is a judgment call, not something the spec
states outright — flag it as an assumption in the design doc.
