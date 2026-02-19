T85 GPS UTC Instrument (Application)

This folder contains a complete, ready-to-run GPS-disciplined UTC instrument for the TinyTFT_LT7683 project.

It drives a 10" LT7683-based 1024×600 TFT from an ATtiny85 over I²C, while decoding GPS NMEA via a custom lightweight UART.

Features

Large 7-segment UTC time (main stage)

Date + Day-Of-Week

Latitude / Longitude (RMC)

degrees shown with a graphical ° symbol (drawn as a circle)

two-field text rendering so the ° is not overwritten

Satellites (GGA)

HDOP bars (GGA) → reception quality at a glance

Altitude (MSL) (GGA) → right-aligned, integers only

GPS state indicators (TIME-ONLY vs FIX)
