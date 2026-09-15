# ame27-dbms-fault-handler
AME27 Embedded Systems Design Challenge
Designing a fault-handling system for a battery management system

# Overview
-	Fault-handling system for a BMS watching 130 series cells. Every 50 ms it decides whether it's safe to keep the car 
    energized. If not, it opens the shutdown circuit and reports which fault over CAN.
-	The SDC is normally-open, so BMS that does nothing leaves the car de-energized. A dead BMS means a dead car, which is the 
    safe place to fail. Init still calls HAL_SetSDC(false) explicitly rather than trusting that default. Every tick, Iter makes one decision: HAL_SetSDC(s_latched == 0), which closes only if the fault memory is wiped clean. This keeps the decision in one place which is after the inspection is done. This means that there is no code path that can energize the car while a fault is latched.

# Architecture
-	Battery Pack (130 Cells)  Sensors (volts, Celsius, amps)  bms.c (code)  SDC (open or closes to deenergize car) + CAN bus 
    (dashboard alerts)
-	Init() – once at power on – put system in safe starting state
-	Iter() – every 50 ms – read, inspect, remember, decide, report
-	RxCan() – the instant any CAN frame arrives (interrupt) – store what arrived

# Variables
-	S_latched – the fault memory, one byte, survives between ticks
    o	Written by Iter() | Read by Iter()

-	S_current_mA (volatile) – last current reading (mA)
    o	Written by RxCan | Read by Iter()

-	S_clear_requested (volatile) – FAULTS_CLEAR frame arrived since last tick
    o	Written by RxCan | Read by Iter()

# Fault Detection – Iter() evaluates all of them
-	CELL_OVER_VOLTAGE
    o	Condition – any cell > 4.2 V
    o	Cell is being overcharged
    o	Fire risk

-	CELL_UNDER_VOLTAGE
    o	Condition – any cell < 2.5 V
    o	Cell is being drained too far
    o	Perm damage, can short internally later

-	CELL_OVER_TEMPERATURE
    o	Any cell > 60 degrees Celsius
    o	Cell is overheating
    o	Thermal runaway

-	CELL_DELTA_EXCEEDED
    o	Max-min cell voltage > 0.2 V
    o	Cell are drifting apart
    o	One cell is weak/failing, pack is unbalanced, will hit OV/UV soon

-	PACK_OVER_CURRENT
    o	Abs(current) > 200 A
    o	Too much current through pack
    o	Wiring and cells heat up, cells can’t safely deliver

# Latching and Clearing
-	Active = dangerous condition
    o	Recomputed every 50 ms
    o	No memory

-	Latched = has fault fired at any point since last clear
    o	One byte survives between ticks

-	S_latched |= active
    o	Every tick
    o	OR bool, 1 wins, never clears anything

-	S_latched &= active
    o	When clear is requested
    o	AND bool, 0 wins, keeps only faults still present

# Shutdown Circuit Control (SDC)
-	SDC is normally open, so BMS does nothing and leaves car deenergized
-	Dead bms means dead car, safe place to fail
-	Init() would still call HAL_SetSDC(false)
-	HAL_SetSDC(s_latched == 0), closed only if fault memory is clean and comes after inspection and latching is finished\
-	Only call to HAL_SetSDC in Iter()
-	One place looks, one place acts

# CAN frame format
	Outgoing: BMS_FAULT_STATUS, ID 0XB0, sent once per Iter() 50 ms
-	Byte 0: active faults (bitmask)
-	Byte 1: latched faults (bitmask)
-	Byte 2-7: reserved, sent as 0

# Bit layout
-	Bit 0: CELL_OVER_VOLTAGE
-	Bit 1: CELL_UNDER_VOLTAGE
-	Bit 2: CELL_OVER_TEMPERATURE
-	Bit 3: CELL_DELTA_EXCEEDED
-	Bit 4: PACK_OVER_CURRENT
-	Bit 5-7: reserved

# Incoming: frames RxCan consumes
-	ISENSE_DATA (0x511): current in mA, signed 24-bit, in data bytes 2-4, stored in s_current_mA
-	FAULTS_CLEAR (0x1CF): no data, sets s_clear_requested
-	Anything else: ignored

# Interrupt safety
-	s_current_mA and s_clear requested are written by RxCan and read by Iter
-	RxCan can fire at any instant
-	Declared volatile so compiler re-reads them from memory every time instead of caching a value in a register and never seeing 
    what the interrupt wrote

-	Iter() reads s_current_mA once at the top of the tick into a local snapshot so if a new current frame arrives mid-tick, the 
    tick will keep working with the value it started with
    o	Decision/Report always agree

-	Iter reads the flag, acts, then resets it

# Assumptions
-	Over-current is 2-sided
    o	Spec says above 200 A, treats abs(current) > 200 A as the fault because charging current is equally dangerous to the battery cells

-	Thresholds are strict
    o	A cell at exactly 4.2 V or 60 °C is not a fault (>, not >=)

-	Unknown CAN IDs are ignored
    o	RxCan acts only on 0x511 and 0x1CF.

-	DIAGNOSTIC_HEARTBEAT (1x1CD) is not used
-	Init() explicitly opens SDC rather than relying on relay’s normally-open default

-	Clear request is consumed on next Iter and not instantly
    o	Worst case latency is one tick (50 ms)

-	Current reading persists between frames
    o	If sensor stops sending, Iter() keeps using last value

# Testing
-	Used make test and mock functions in order to make fictional tests to ensure the code works properly since there is no 
    hardware to test with

-	test/mock_hal.c
    o	implements every HAL_* function with settable sensor values and captured outputs (SDC state, last CAN frame)

-	test/test_bms.c
    o	runs scenario tests against the HAL_* functions