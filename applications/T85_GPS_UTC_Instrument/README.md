ATtiny85 GPS UTC Instrument

10" LT7683 TFT driven by a Tiny85 at 99% Flash

Overview

This application turns an ATtiny85 into a fully functional GPS-disciplined UTC instrument driving a 10" LT7683-based TFT display.

Despite running at ~99% flash utilization (bootloader enabled), the system:

Decodes GPS NMEA (RMC + GGA)

Displays large 7-segment UTC time

Shows Date + Day-Of-Week

Displays Latitude / Longitude (with graphical ° symbol)

Shows Satellite count

Visualizes HDOP as quality bars

Displays Altitude (MSL)

Runs glitch-free for many hours (soak tested)

All of this on a microcontroller introduced nearly 20 years ago.

Hardware

MCU: ATtiny85

Display: 10" TFT with LT7683 controller

GPS: Standard NMEA output module (9600 baud)

Interface:

Custom lightweight software UART

I²C to LT7683

Bootloader: Serial bootloader retained for ease-of-use

The bootloader consumes flash space, but significantly simplifies development and replication.

Architecture Highlights
1. Custom UART Receiver

A minimal, timing-accurate UART reader implemented specifically for the Tiny85.

No heavy serial libraries

No external dependencies

Designed to fit within strict flash limits

2. Lean NMEA Decoder

A purpose-built sentence-level parser:

Supports RMC (time, date, lat/lon)

Supports GGA (fix quality, sats, HDOP, altitude)

Commits data atomically at end-of-sentence

No floating point

No dynamic allocation

No external GPS libraries

Designed specifically to fit inside Tiny85 flash constraints.

3. Display Discipline

Each widget defines its own x0, y0

Redraw only when values change

Graphical ° symbol drawn once during setup

Lat/Lon rendered in two text fields to avoid overwriting the graphical degree symbol

No printf, no heavy formatting functions

Display Layout

Main stage:

Large 7-segment UTC time

Supporting data:

Date + DOW

Latitude

Longitude

Altitude (MSL)

Satellite count

HDOP bars

GPS state indicators

The layout is intentionally modular.
Users may adjust widget positions to match their own panel geometry.

Stability

The system has been run continuously for many hours without:

Lockups

Parser failures

Display corruption

State glitches

The decoder uses sentence-level commit logic to ensure data consistency.

Why ATtiny85?

Because it forces discipline.

This project demonstrates that:

Careful architecture matters more than raw CPU power.

A 20-year-old 8-bit MCU can drive a 10" display and decode GPS reliably.

Constraints produce clean embedded design.

Flash Usage

Application runs at approximately 99% flash usage with bootloader enabled.

No millis()

No B.O.D.

No floating point

No unnecessary libraries

Future Variants

A second application based on the same hardware platform:

Navigation Dash

Large 7-segment speed display

COG (course over ground)

Altitude

Reception quality

Different visual identity

Repository Structure
/examples      → minimal demonstrations
/applications  → finished applications

This sketch lives in:

/applications/T85_GPS_UTC_Instrument
Notes

Time is displayed in UTC.

Latitude / Longitude jitter reflects real HDOP quality.

Altitude is shown as MSL from GGA.

Layout and colors can be modified to taste.
