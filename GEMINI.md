# KRC Interposer - Agent Instructions

This file provides architectural context, project constraints, and coding conventions for the KRC Interposer project.
Always refer to these guidelines when making modifications or suggesting changes.

## General Agent Guidelines
- **Be cautious with code changes:** Resist your urge to write code. Do not assume the user wants you to write code.
- **Ask before acting:** Whenever you want to make unrelated changes or fixes, you MUST ask the user first.
- **Fresh Context:** ALWAYS read the latest content of a file before attempting to modify it, to ensure your edits are based on the most current state.
- **Honest Uncertainty:** When explaining technical behavior or causes that you are unsure about, do NOT use authoritative adverbs that imply statistical certainty (e.g., "often", "usually", "many"). Instead, use probabilistic language that honestly communicates your uncertainty (e.g., "probably", "might", "it is possible that"). Never guess authoritatively.

## Project Overview
The KRC Interposer is a smart hardware device that sits between an induction stove and its physical control dial. It upgrades the standard dial into a closed-loop PID temperature controller by connecting to Kuhn Rikon Comfort (KRC) smart cookware (Hotpan, Duromatic) via BLE.

## Hardware Platform
- **Microcontroller:** Seeed Studio XIAO nRF52840.
- **Power State:** The device is designed to be zero-power by default. The XIAO remains in deep sleep until the user triggers the stove's "Boil Mode" (dial turned physically below 0). The companion PIC microcontroller detects this condition via its ADC and begins sending UART telemetry, which wakes the XIAO via a UART RX start bit interrupt.
- **Bypass Switch:** An internal state dictates who controls the stove, which is handled via passthrough control.
  - Passthrough Enabled: The physical dial is effectively connected directly to the stove (Pass-through mode). This is maintained during the `SCANNING` phase so the user can use the stove normally if a lid is not immediately found.
  - Passthrough Disabled: The XIAO's filtered PWM output via `StoveController` controls the stove (Active smart mode).

## Core Architecture & Components
The firmware employs strict separation of concerns to allow native testing on host machines. Hardware interactions are abstracted behind interfaces (e.g., `DialSensor`, `StoveActuator`).

- **`StoveSupervisor`:** The central state machine. Manages transitions (`SCANNING`, `CONNECTED`, `ACTIVE`, `DISCONNECTED`), controls the passthrough via the `StoveController`, manages the 5-second shutdown timer, handles user feedback (`Beeper`), and triggers automatic hardware boost modes.
- **`ThermalController`:** Implements the closed-loop PID control. Handles edge cases like detecting when the pan lid is opened (by looking for a sudden negative temperature slope) and freezing power output to prevent overshoot.
- **`StoveController`:** Translates abstract throttle requests (0.0 - 1.0 position, discrete boost levels) into timed PWM signals via the `StoveActuator` interface, and handles slewing and high-level control.
- **`StoveActuator`:** Interface for low-level hardware translation of PWM output and passthrough control to the physical pins.
- **`TrendAnalyzer`:** A statistical buffer for BLE temperature readings. Calculates moving averages, the current rate of change (slope), and predicts future temperatures to compensate for system thermal lag.
- **`BleThermometer` (Central):** Scans for and connects to specific KRC BLE names ("DUROMATIC", "HOTPAN", "FAKEPOT") to read temperatures.
- **`BleTelemetry` (Peripheral):** Broadcasts current state data to the optional companion app.

## State Machine Workflow
1. **Sleep:** XIAO is powered off.
2. **Wake:** UART RX activity from the PIC microcontroller wakes the device.
3. **`SCANNING`:** Searching for BLE lid. Passthrough is enabled (stove functions normally).
4. **`CONNECTED`:** Lid found. Passthrough is disabled (XIAO takes control). Beeps to accept. Target temperature is set via the 0-9 dial scale (mapped to 30°C - 120°C). The device slews the throttle from 1.0 to minimum over 1 second before activating.
5. **`ACTIVE`:** Normal PID regulation. Engages boost if temperature is > 20°C away from target, disengages when < 10°C away (hysteresis). If dial is turned to 0, a 5-second timeout starts. If 5s elapses, the device powers off entirely.
6. **`DISCONNECTED`:** Lid signal lost for > 30s. Power drops to zero, beeps error. Recovers to `ACTIVE` if signal returns.

## Coding Style & Conventions
- **Language:** C++17.
- **Control Flow:** Strongly prefer early returns (guard clauses) over deeply nested `if` statements.
- **Dependency Injection:** Pass dependencies as references in constructors (e.g., `StoveController(StoveActuator &actuator, ...)`). Do not instantiate hardware components inside logic classes.
- **Naming:**
  - Classes/Structs: `PascalCase`
  - Methods: `camelCase`
  - Variables: `snake_case` (with trailing underscores for private members: `variable_`)
  - Constants: `kCamelCase` (e.g., `kStovePwmPin`) or `UPPER_SNAKE_CASE`.

## Testing Strategy
- **Framework:** PlatformIO using `doctest` and `ArduinoFake`.
- **Command:** `pio test -e native`
- **Rule:** All logic modules (`StoveSupervisor`, `StoveController`, `ThermalController`, etc.) MUST be thoroughly unit-tested. Hardware abstractions (mocking pins, time, etc.) must be used to ensure tests can run entirely natively without target hardware. When changing behavior or state transitions, always update the corresponding tests.
