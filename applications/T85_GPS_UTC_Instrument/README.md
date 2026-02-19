# T85_GPS_UTC_Instrument

A complete GPS-disciplined UTC instrument for the **TinyTFT_LT7683** project.

This application demonstrates how a **10″ 1024×600 LT7683-based TFT display**
can be driven from an **ATtiny85 over I²C**, while decoding live GPS NMEA data.

---

## What This Application Provides

### Core Display
- Large **7-segment UTC clock**
- Date and Day-Of-Week
- Latitude *(RMC)*
- Longitude *(RMC)*
- Altitude (MSL) *(GGA)*
- Satellite count *(GGA)*
- HDOP visualized as quality bars
- GPS status indicators (TIME-ONLY / FIX)

---

## Design Philosophy

### Lean by Design
- Custom lightweight **software UART**
- Purpose-built **NMEA parser**
- Sentence-level commit logic
- No floating point
- No dynamic allocation
- No heavy formatting functions
- No external GPS libraries

Only what is displayed is decoded.

---

### Display Discipline
Each widget defines its own `x0, y0`.

- Redraw only when values change
- Text output grouped with `TextBegin()` / `TextEnd()`
- Degree symbol rendered as a **graphic circle**
- Latitude and longitude printed in two text fields to prevent overwriting the graphical °

---

## Flash Usage

This sketch runs at approximately **99% flash usage**  
*(serial bootloader enabled)*

To stay within limits:
- `millis()` disabled
- B.O.D. disabled
- No floating point math
- No unused framework code

The bootloader is intentionally retained for ease-of-use.

---

## Stability

The system has been run continuously for many hours without:

- Lockups
- Parser corruption
- Display glitches
- State inconsistencies

Sentence-level commit ensures consistent data updates.

---

## Why ATtiny85?

Because constraints enforce clean architecture.

This application shows that a nearly 20-year-old 8-bit MCU can:

- Decode GPS reliably
- Drive a 10″ TFT
- Maintain stable long-term operation
- Deliver structured, instrument-grade output

---

## Repository Structure

```
/examples        → minimal demonstrations
/applications    → finished applications
```

This sketch lives in:

```
/applications/T85_GPS_UTC_Instrument
```

---

## Planned Variant

### T85_GPS_NAV_Dash

A navigation-oriented application featuring:

- Large 7-segment speed display
- Course-over-ground (COG)
- Altitude
- Reception quality
- Distinct color scheme

---

## Notes

- Time is displayed in **UTC**
- Lat/Lon jitter reflects real reception quality (HDOP)
- Layout can be adjusted by modifying widget origins
