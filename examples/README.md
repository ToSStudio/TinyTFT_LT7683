# Clock Demo (ATTiny85 + LT7683 over I²C)

This demo initializes the LT7683 in I²C mode and renders a simple clock UI.
The time base is derived from the compile time (`__TIME__`).

## Requirements
- Arduino IDE
- SpenceKonde ATTinyCore (ATtiny25/45/85)
- ATTiny85 @ 8 MHz internal (tested)
- `millis()` enabled in ATTinyCore board options

## Hardware notes
- TFT powered from 5 V (onboard step-down)
- TFT logic is 3.3 V; I²C pull-ups on the TFT board are to 3.3 V
- SDA: PB0, SCL: PB2
