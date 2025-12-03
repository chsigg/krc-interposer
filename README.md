# KRC Interposer

Firmware for an intelligent stove knob interposer. This project intercepts the analog signal from a stove dial, processes it to add smart features (such as thermal control, safety cutoffs, and boost modes), and controls the stove output using a digital potentiometer.

## Features

*   **Stove Dial Input:** Reads and normalizes analog inputs from the stove knob, supporting "boost" gestures.
*   **Thermal Control:** Implements a `ThermalController` with PID-like behavior, lookahead prediction (to compensate for system lag), and feed-forward physics modeling.
*   **Trend Analysis:** Uses a `TrendAnalyzer` to estimate temperature slope and predict future states.
*   **Safety:** Includes logic for detecting open lids (sudden temperature drops) and freezing output to prevent overheating.
*   **Actuation:** Controls a digital potentiometer to simulate knob positions to the stove electronics.

## Development

This project is built using PlatformIO and the Arduino framework.

### Build

To build the firmware:

```bash
pio run
```

### Testing

Unit tests are implemented using `doctest` and `ArduinoFake`. To run the tests:

```bash
pio test
```
