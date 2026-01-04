# Starfield Demo

This example demonstrates a subtle “warp-style” starfield animation on a
**1024×600 LT7683-based TFT display**, driven entirely by an **ATtiny85 over I²C**.

The demo is intentionally conservative in animation speed and star count to ensure
smooth operation on a minimal 8-bit microcontroller without external RAM or a framebuffer.

## Startup Sequence

On reset, the demo begins with a short introductory screen:

1. **Arcade-style text** rendered using the LT7683 text and graphics primitives  
2. A brief **count-down displayed in large 7-segment digits**
3. Automatic transition into the starfield animation

This serves both as a visual “opener” and as a demonstration of static graphics,
text rendering, and large numeric displays before animation begins.

## Starfield Animation

The starfield is inspired by classic screen savers and uses a simple 3D projection model:

- Stars originate near the center of the screen and move outward
- Motion is depth-based (Z-axis), giving a sense of perspective
- Near stars may be rendered slightly larger to enhance depth perception
- Each star is erased and redrawn individually (no trails)

All drawing is done via direct GRAM access on the LT7683; the ATtiny85 only sends
commands and pixel data over I²C.

## Performance Notes

- Designed for **ATtiny85 @ 8 MHz**
- I²C Fast Mode (400 kHz)
- No framebuffer, no external memory
- Star count and motion parameters are tuned for smooth animation

Users are encouraged to experiment with:
- star count
- motion speed
- colors
- projection parameters
- screen center position (for more psychedelic effects)

## Purpose

This example illustrates what is possible when a capable graphics controller
does the heavy lifting — even when paired with one of the smallest AVR microcontrollers.

It is meant as a demonstration, a starting point, and an invitation to tinker.
