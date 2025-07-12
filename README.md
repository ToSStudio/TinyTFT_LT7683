# TinyTFT_LT7683
“Driving a 10.1 inch 1024x600 TFT with the LT7683 controller using an ATtiny85 via I2C. Possibly the smallest microcontroller ever used for such a display.”
# TinyTFT_LT7683

**Driving a 1024×600 TFT display with an ATtiny85 over I²C**

---

This project demonstrates how a minimal 8-bit microcontroller, the **ATtiny85**, can control a large **LT7683-based TFT display (1024x600 pixels)** via the I²C interface. It serves as a proof of concept for ultra-compact embedded systems with impressive graphical output.

## 🧠 Highlights

- **ATtiny85** with only 8 KB Flash and 512 B RAM
- **LT7683** graphics controller via I²C (vendor-supplied protocol)
- **Technoblogy-style I2C bit-banged routines** for USI on Tiny85
- Only ~3.5 KB program space used
- Completely software-driven, no external RAM or framebuffer required
- Can be used as the basis for graphical meters, terminals, dashboards

## 🛠️ Hardware Setup

- ATtiny85 (on breakout board or DIP)
- LT7683 10" TFT panel (1024×600)
- I²C pull-up resistors (4.7kΩ recommended)
- Power supply capable of sourcing **>1A** for the display
- Optional: USB-to-serial adapter or programmer for ATtiny85

_Switching circuitry may be required to isolate the programmer from I²C lines._

## 📂 Project Structure

| File / Folder | Description |
|---------------|-------------|
| `main.ino` or `main.cpp` | ATtiny85 firmware |
| `LT7683_TinyI2C.cpp/h` | Display driver using bit-banged I2C |
| `README.md` | Project overview |
| `docs/` | Images, hardware diagrams (optional) |

## 🚀 Getting Started

1. **Flash firmware** to the ATtiny85 using your preferred programmer.
2. Connect the display to the correct I²C pins:
   - SDA → Pin 5
   - SCL → Pin 7
3. Power the display (separately if needed).
4. Verify startup by observing the display initialize (geometric primitives, etc.)

## 🧪 Status

✅ I²C proof of concept  
✅ TinyNeoPixel LED output  
🧪 Working on timeout handling and robustness  
🧪 WindO.ino graphical environment (planned)

## 📸 Gallery

_TODO: Add hardware photos and screenshots here._

## 📝 License

MIT License — see `LICENSE` file for details.

---

> Developed by ToSSoft_Berlin 🇩🇪  
> "Big displays on tiny chips"
