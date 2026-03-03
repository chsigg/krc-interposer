# KRC Interposer

The **KRC Interposer** is a smart hardware intermediary that upgrades a standard induction stove dial into a closed-loop temperature controller. Designed specifically for use with **Kuhn Rikon Comfort (KRC)** smart cookware—such as the **Duromatic** pressure cooker or **Hotpan**—it intercepts the analog signal from the stove’s physical knob, connects to the smart lid via Bluetooth Low Energy (BLE), and automatically regulates the stove's power to maintain the cooking temperature.

Built around the **Seeed Studio XIAO nRF52840**, the interposer is designed to be completely invisible and zero-power during normal stove operation, only activating when its smart features are explicitly requested.

## How It Works

### Standard Stove Operation (Pass-Through Mode)

The physical dial acts as a potentiometer (ratiometer), outputting an analog voltage to the stove's electronics based on its position:
*   **0 to 9:** Standard heating power levels.
*   **Below 0 (Boil Mode):** Turning the dial physically below 0 drops the resistance to a minimum. The stove interprets this as a command to heat at high power for a set duration before returning to the power level indicated by the dial.
*   **Above 9 (Boost Mode):** Turning the dial past 9 activates one of two high-power boost levels.

By default, an analog switch connects the physical dial directly to the stove. The XIAO microcontroller is powered off (in deep sleep), drawing virtually zero power. The stove behaves exactly as it did before the interposer was installed.

### Smart Temperature Control (Active Mode)

The interposer's smart features are triggered by using the stove's built-in **Boil Mode** (turning the dial below 0).

1.  **Wake Up:** The low resistance of the Boil position triggers a hardware comparator (`LPCOMP`) on the sleeping XIAO, waking it up.
2.  **Connection:** The XIAO takes control of the analog switch, disconnecting the physical dial from the stove and routing its own filtered PWM output to the stove instead. It begins scanning for a Kuhn Rikon Comfort lid via BLE. When the lid is successfully found and connected, the interposer plays an **accept beep** to notify the user.
3.  **Telemetry (Optional):** Simultaneously, the interposer acts as a Bluetooth Peripheral. This allows the [KRC Companion App](http://github.com/chsigg/krc-app) to connect and display real-time measured and target temperatures, though the app is not required for normal operation.
4.  **Temperature Targeting:** Once connected, the physical dial's 0 to 9 scale is repurposed. Setting the dial to **0 acts as an "Off" switch**. Turning the dial slightly above 0 up to 9 sets a target temperature between **30°C and 120°C**.
5.  **Closed-Loop Control:** The interposer continuously reads the temperature from the BLE lid and simulates the analog dial voltage to the stove, dynamically adjusting the power to reach and hold the target temperature (PID control). It automatically engages the stove's native hardware boost modes to rapidly heat up cold pans, without the user needing to manually turn the dial past 9. It intelligently handles edge cases, such as preventing power spikes when the pan lid is lifted (which causes a sudden, temporary drop in read temperature).
6.  **Safety & Disconnection:** If the Bluetooth connection to the lid is lost, the interposer sounds a warning beep and eventually drops the stove power to zero until the connection is restored.
7.  **Power Off:** If the user turns the dial to 0, the interposer waits 5 seconds before powering itself down entirely. The analog switch falls back to its default state, reconnecting the physical dial to the stove.

## Development

This project is built using **PlatformIO** and the **Arduino** framework.

### Build

To compile the firmware for the XIAO nRF52840:

```bash
pio run
```

### Testing

Unit tests are implemented using `doctest` and `ArduinoFake` to run logic natively on your host machine.

```bash
pio test -e native
```

## Code Design

The firmware is designed with a strict separation of concerns, decoupling the high-level control logic from the underlying hardware to enable robust native testing.

*   **`StoveSupervisor` (State Machine):** The central orchestrator. It manages the high-level states (`SCANNING`, `CONNECTED`, `ACTIVE`, `DISCONNECTED`), handles timeouts (like the 5-second shutdown timer), triggers user feedback (beeps), and decides when to engage automatic hardware boost if the target temperature is far away.
*   **`ThermalController` (PID Control):** Responsible for the closed-loop temperature logic. It calculates the necessary power output based on the error between the target and current temperatures. It also includes edge-case logic, such as detecting a sudden drop in temperature slope (indicating the lid was opened) and temporarily freezing the power output to prevent severe overshoot.
*   **`StoveActuator` (Hardware Translation):** Translates abstract "throttle" commands (0.0 to 1.0 position, plus discrete boost levels) into concrete, timed PWM signals. It handles the complex pulse-sequencing required to activate the stove's native hardware boost modes.
*   **`TrendAnalyzer`:** A statistical utility that buffers incoming BLE temperature readings. It smooths the data and calculates both the predicted current temperature (compensating for system lag) and the rate of change (slope).
*   **`BleThermometer` (BLE Central):** Actively scans for and connects to supported Kuhn Rikon lids. It parses incoming temperature data and pushes it to the `TrendAnalyzer`.
*   **`BleTelemetry` (BLE Peripheral):** Acts as a Bluetooth Peripheral, broadcasting internal state (target temperature, current temperature, and log stream) to allow the companion app to monitor the process.
*   **Hardware Abstractions:** Components like `AnalogWritePin` and `DigitalWritePin` wrap the Arduino-specific `analogWrite` and `digitalWrite` functions. This allows the core logic to be tested purely on the host machine using mock objects (via `ArduinoFake` and `doctest`) without requiring the physical hardware.
