# Examples

This folder contains small demonstration sketches for the **TinyTFT_LT7683** project.

Each example is self-contained and meant to illustrate a specific aspect of driving an
LT7683-based 1024×600 TFT display from an **ATtiny85 over I²C**.

## Available Examples

### 01_TestScreen *(recommended first)*
A static diagnostic screen showing:
- screen borders and coordinate system
- origin (0,0) in the upper-left corner
- basic graphic primitives (lines, rectangles, circles)
- text rendering at different scales

Use this sketch to verify wiring, orientation, colors, and basic functionality.

### 02_ClockDemo
A simple digital clock using large 7-segment style digits drawn from graphic primitives.
Demonstrates:
- efficient use of geometric drawing functions
- large, readable numerals without a framebuffer
- periodic screen updates over I²C

### 03_Starfield
A subtle “warp-style” starfield animation inspired by classic screen savers.
Demonstrates:
- direct GRAM pixel access over I²C
- animation without external RAM or framebuffer
- performance limits and tuning on an ATtiny85

Star count and motion parameters are intentionally conservative to ensure smooth animation.

## Notes

- All examples target an **ATtiny85 running at 8 MHz** using I²C Fast Mode (400 kHz).
- The LT7683 graphics controller handles all drawing; the microcontroller sends only commands and pixel data.
- Examples are intentionally simple and meant to be modified and extended.

- ## Debug Output (Serial vs I²C)

`Serial.print()` / SoftwareSerial debugging is **not recommended** in these examples.

On the ATtiny85, the available GPIO pins are shared between multiple functions.
In the reference wiring, **PB0** is used both for I²C (SDA) and for a USB-to-serial
adapter (e.g. CH340). Using SoftwareSerial on these pins will interfere with I²C
timing and can lead to unreliable behavior or a non-responsive display.

Additionally, SoftwareSerial at 8 MHz combined with frequent I²C transfers places
a heavy timing burden on the ATtiny85.

**Recommended alternatives for debugging:**
- use the on-board LED for status indication
- display debug information directly on the TFT
- temporarily disable I²C while debugging serial output

This project intentionally prioritizes reliable I²C communication with the
LT7683 controller over serial debug output.


Feel free to experiment with colors, animation parameters, and drawing routines.
