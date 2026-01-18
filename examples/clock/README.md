!(../../docs/img/clock_II.jpeg)

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
- The I²C bit-banged routines are based on David Johnson-Davies' USI I²C approach.

## Hints
- if used with the bootloader this sketch will use 100 % of program storage space.
- this could be reduced by avoiding trigonometry and storing the pointer-positions in Progmem / EEPROM.
- the clock is not true in time. It will drift after a while.
- Vcc and Tmp in the lower-right rectangle are "fake". The T.85 offers to read those values however w/o external hardware.
