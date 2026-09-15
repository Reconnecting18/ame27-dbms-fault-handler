# BMS Fault Handler — Design Document

**AME27 Embedded Systems Design Challenge — Texas A&M Formula E**
**Author:** Ethan Cheng
**Code:** https://github.com/Reconnecting18/ame27-dbms-fault-handler

## 1. Overview

This is the fault-handling system for the BMS: the small computer that watches a pack of 130 lithium cells wired in series. Because the cells are in series, one bad cell is enough to make the whole pack dangerous, so the BMS has to look at every cell individually, not the pack total.

Every 50 ms the system answers one question: *is it safe to keep the car energized right now, and if not, why not?* It acts on the answer through two outputs. The shutdown circuit (SDC) is a safety relay in series with every E-stop on the car; opening it de-energizes the high-voltage pack. The CAN bus is the car's communication network; the BMS broadcasts a status frame on it so the dashboard and the diagnostic tool can show which fault fired.

## 2. Architecture

The team's firmware calls three functions:

| Function | When | Job |
|---|---|---|
| `Init()` | once at power-on | put the system in its safe starting state |
| `Iter()` | every 50 ms (~20 Hz) | read, inspect, remember, decide, report |
| `RxCan()` | the instant any CAN frame arrives (interrupt) | store what arrived; nothing else |

Data reaches the BMS through two different doors:

- **Cell voltages and temperatures** — those sensors are wired to the BMS board itself, so `Iter` reads them directly with `HAL_ReadVoltages` / `HAL_ReadTemperatures` (130 values each, filled in one call).
- **Pack current and clear requests** — the current sensor is a separate box on the CAN bus that broadcasts its reading ten times a second (frame `0x511`), and the diagnostic tool sends clear requests (frame `0x1CF`). Both arrive through the `RxCan` interrupt.

`RxCan` is a mailbox. It can fire between any two lines of `Iter`, and the challenge restricts it to `HAL_RecvCanMsg` only, so it never decides anything and never touches hardware. It reads the frame, and if the ID is one of the two it cares about, it leaves a note in shared memory for `Iter` to find on the next tick.

Three variables live at file scope in `bms.c`, marked `static` so they persist for the life of the program (a private field, in Java terms) and are invisible to other files:

| Variable | Written by | Read by | Purpose |
|---|---|---|---|
| `s_latched` | `Iter` | `Iter` | the fault memory — one byte, survives between ticks |
| `s_current_mA` (`volatile`) | `RxCan` | `Iter` | last current reading, in mA |
| `s_clear_requested` (`volatile`) | `RxCan` | `Iter` | a FAULTS_CLEAR frame arrived since last tick |

Everything else — the two sensor arrays, the `active` byte, the outgoing frame — is local to `Iter` and is rebuilt from scratch every tick. The BMS keeps no history of readings; the only thing it remembers is which faults have fired.

## 3. Fault detection

Five faults, each a named dangerous condition:

| Fault | Condition | What is physically happening | Why it matters |
|---|---|---|---|
| CELL_OVER_VOLTAGE | any cell > 4.2 V | cell is being overcharged | chemistry breaks down; fire risk |
| CELL_UNDER_VOLTAGE | any cell < 2.5 V | cell is being drained too far | permanent damage; can short internally later |
| CELL_OVER_TEMPERATURE | any cell > 60 °C | cell is overheating | thermal runaway |
| CELL_DELTA_EXCEEDED | max − min cell voltage > 0.2 V | cells are drifting apart | one cell is weak or failing; the pack is unbalanced and will hit OV/UV soon |
| PACK_OVER_CURRENT | abs(current) > 200 A | too much current through the pack | wiring and cells heat up; cells can't safely deliver it |

`Iter` evaluates all of them in a single pass over the 130 cells. Inside the loop, each cell is checked against the OV, UV, and OT thresholds, and a running maximum and minimum voltage are tracked. After the loop, when max and min are final, the delta check runs. Over-current is not a per-cell property, so it is checked after the loop too, against the last value `RxCan` stored from the current sensor. A first version had a second 130-cell loop nested inside the first for the max/min hunt; hoisting it into the main loop cut the work per tick from 16,900 comparisons to 130.

The result of inspection is `active`: one `uint8_t` where each bit is a fault that is present *on this tick*. It is not a counter — it is five switches. Setting one is `active |= FAULT_X`, and two faults at once simply means two bits on.

Over-current is checked two-sided. The spec says "above 200 A," but the sensor is signed — positive is discharge, negative is charging — and 200 A into the cells is as dangerous as 200 A out of them. So the check is `current > 200 A || current < -200 A`. This is a documented assumption (see section 8).

## 4. Latching and clearing

> *Ethan writes this section.* Define active vs. latched in your words. Put in the tick table from your notebook (cell 42 at 4.3 V for one tick, then 4.1 V) and say what a non-latching design does wrong on tick 2 — the driver feels a hiccup and nobody ever learns a cell hit 4.3 V. Then the two operations: `s_latched |= active` every tick ("OR: 1 wins — copies the tick's switches into memory, never clears anything"), and `s_latched &= active` when a clear is requested ("AND: 0 wins — keeps only the faults that are still present"). Explain why that one line gives "clear unless the condition is still present" with no per-fault special cases, and what it does to a lazy mechanic who sends clear while the cell is still over.

## 5. Shutdown circuit control

The SDC is normally-open, so a BMS that does nothing leaves the car de-energized — a dead BMS means a dead car, which is the safe direction to fail. `Init` still calls `HAL_SetSDC(false)` explicitly rather than trusting that default; the safe state is asserted, not assumed.

Every tick, `Iter` makes exactly one decision: `HAL_SetSDC(s_latched == 0)`, closed only if the fault memory is clean. That line comes after all inspection and latching is finished, and it is the only call to `HAL_SetSDC` in `Iter`. One place looks, one place acts. There is no code path that can energize the car while a fault is latched, and the car only comes alive after the first full inspection has found nothing.

## 6. CAN frame format

### Outgoing: BMS_FAULT_STATUS, ID `0xB0`, sent once per `Iter` (~20 Hz)

| Byte | Contents |
|---|---|
| 0 | active faults (bitmask) |
| 1 | latched faults (bitmask) |
| 2–7 | reserved, sent as 0 |

Both bytes use the same bit layout:

| Bit | Fault |
|---|---|
| 0 | CELL_OVER_VOLTAGE |
| 1 | CELL_UNDER_VOLTAGE |
| 2 | CELL_OVER_TEMPERATURE |
| 3 | CELL_DELTA_EXCEEDED |
| 4 | PACK_OVER_CURRENT |
| 5–7 | reserved |

Active and latched go in one frame rather than two because the spec asks for both and bus traffic matters: one frame at 20 Hz instead of two. The dashboard decodes it by testing bits (`byte & (1 << n)`), never by comparing the byte to a number, since several faults can be on at once — a single over-voltage cell in an otherwise healthy pack trips delta as well. The ID is in the `0xB0`–`0xBF` block allocated to the BMS.

### Incoming: frames `RxCan` consumes

| Frame | ID | Handling |
|---|---|---|
| ISENSE_DATA | `0x511` | current in mA, signed 24-bit big-endian in data bytes 2–4; decoded and stored in `s_current_mA` |
| FAULTS_CLEAR | `0x1CF` | no data; sets `s_clear_requested` |
| anything else | — | ignored |

Decoding the current: byte 2 holds bits 23–16, byte 3 bits 15–8, byte 4 bits 7–0, so the value is reassembled with shifts and ORs. Because the field is 24 bits but it is stored in a 32-bit signed integer, a negative reading needs sign extension: if bit 23 is set, subtract 2^24 (`0x1000000`). Without that step, −50 A decodes as +16,727 A and the BMS would throw over-current on a healthy pack while charging.

## 7. Interrupt safety

`s_current_mA` and `s_clear_requested` are written by `RxCan` and read by `Iter`, and `RxCan` can fire at any instant. They are declared `volatile` so the compiler re-reads them from memory every time instead of caching a value in a register and never seeing what the interrupt wrote.

`Iter` reads `s_current_mA` once at the top of the tick into a local snapshot. If a new current frame arrives mid-tick, the tick keeps working with the value it started with, so the decision and the report always agree.

The clear flag has one possible race: `Iter` checks it, acts, then resets it to `false`. If a second FAULTS_CLEAR arrives in the microseconds between the check and the reset, that second request is overwritten. The outcome is harmless — the first request was just processed, and a fault that is still present stays latched either way. A clear that arrives after the reset is handled on the next tick, 50 ms later.

## 8. Assumptions

- **Over-current is two-sided.** The spec says "above 200 A"; this design treats abs(current) > 200 A as the fault because charging current is equally dangerous to the cells.
- **Thresholds are strict.** A cell at exactly 4.2 V or 60 °C is not a fault (`>`, not `>=`).
- **Unknown CAN IDs are ignored.** `RxCan` acts only on `0x511` and `0x1CF`.
- **DIAGNOSTIC_HEARTBEAT (`0x1CD`) is not used.** The spec makes it optional; nothing in the safety logic depends on whether the tool is attached.
- **`Init` explicitly opens the SDC** rather than relying on the relay's normally-open default.
- **A clear request is consumed on the next `Iter`**, not instantly; worst-case latency is one tick (50 ms).
- **The current reading persists between frames.** If the sensor stops sending, `Iter` keeps using the last value. (Detecting a stale reading via `HAL_GetMS` would be a reasonable extension.)

## 9. Testing

> *Ethan writes this section.* There is no hardware, so `test/mock_hal.c` implements every `HAL_*` function with settable sensor values and captured outputs (SDC state, last CAN frame), and `test/test_bms.c` runs scenario tests against it. List the tests and what each proves — init is safe, healthy pack closes SDC, one bad cell opens it, the flicker (fault stays latched after the condition clears), delta with every cell individually fine, clear-after-recovery closes, clear-with-fault-present is ignored, the three current tests. Then the bugs the tests actually caught: the UV/OV constant mix-up in a test, asserting `== 1` on a byte that had two bits set, and the `2^24` (XOR, not power) sign-extension bug that the −50 A test exists for.

## 10. Future work

- **Status LED** — `HAL_SetLED` is unused; green for clean, red for latched, amber for active-but-not-yet-cleared would give a glanceable state without a diagnostic tool.
- **Peak values** — record the peak current during an over-current event (and peak voltage/temperature for the cell faults) and send them in the reserved bytes 2–7 of the status frame.
- **Configurable thresholds** — accept a CAN frame from the diagnostic tool that sets thresholds, bounded to safe ranges.
- **Stale-sensor detection** — use `HAL_GetMS` to fault if no ISENSE_DATA frame has arrived in, say, 500 ms.
- **Heartbeat-aware behavior** — only honor FAULTS_CLEAR while `0x1CD` has been seen recently, so a stray frame can't clear faults with no tool attached.
