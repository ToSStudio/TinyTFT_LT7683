// T.85 with TFT 1024 x 600 (LT7683 / RA8876) using barebone I2C (Technoblogy)
// connections: SDA PB0 = Pin7 (Display)
//              SCL PB2 = Pin8 (Display)
// connect the power supply of the Display to GND & 5 Volt.

// This is a basic demo

// Helper macro to convert ASCII to numeric
#define CHAR_TO_NUM(c) ((c) - '0')


// ======================================
// ****** I2C addresses of LT7683 *******
// ======================================
#define I2C_ADDR_CMD 0x7E   // write: command, read: status
#define I2C_ADDR_DATA 0x7F  // write: data, read: data

// ================================================
// ****** Array for Display-controller init *******
// ****** first number is the register ************
// ****** second is the register-value ************
// ================================================
const uint8_t initCode[][2] PROGMEM = {

  { 0x00, 0x01 },  // SW reset
  { 0x01, 0x01 },  // System configuration: 16-bit data bus

  // --- SDRAM Initialization ---
  { 0xE0, 0x29 },  // SDRAM Attribute: 16-bit, 4 banks, CAS=3
  { 0xE1, 0x03 },  // SDRAM Mode Register ACAS latency = 3
  { 0xE2, 0x0B },  // SDRAM Refresh Cycle Low Byte
  { 0xE3, 0x06 },  // SDRAM Refresh Cycle High Byte
  { 0xE4, 0x01 },  // Execute SDRAM Initialization

  // --- Graphic Mode Setup ---
  { 0x02, 0x00 },  // Graphic Mode Enable (0x00 = Graphic Mode)
  { 0x03, 0x00 },  // Rotation off

  // --- Window Geometry: Horizontal Display Period ---
  { 0x10, 0x04 },  // HDP: Horizontal Display Width (LSB) = 0x04
  { 0x12, 0x80 },  // HDP: Horizontal Display Width (MSB) = 0x80 → Total: 0x804 = 1024 px

  // --- Horizontal Non-Display Period (Back Porch + Front Porch) ---
  { 0x13, 0xB0 },  // HND = 0xB0 = 176 clocks

  // --- Horizontal Sync Pulse Width ---
  { 0x14, 0x7F },  // HSW = 0x7F = 127 clocks

  // --- Vertical Display Period ---
  { 0x15, 0x00 },  // VDP (LSB) = 0x00
  { 0x16, 0x12 },  // VDP (MSB) = 0x12 → Total: 0x1200 = 4608 (but that’s too high → misconfigured?)
  { 0x17, 0xF8 },  // Actually LSB of 600 (0x0258): 0xF8 = 248 ?
  { 0x18, 0x1A },  // VDP corrected: 0x07A8 = 1960? → double-check values later

  // --- Vertical Non-Display Period ---
  { 0x19, 0x07 },  // VND = 0x07 = 7 lines

  // --- Vertical Sync Pulse Width ---
  { 0x1A, 0x57 },  // VSW = 0x57 = 87 lines

  // --- Horizontal Start Position ---
  { 0x1B, 0x02 },  // HSP = 0x02 = 2 clocks

  // --- Horizontal Display Width ---
  { 0x1C, 0x10 },
  { 0x1D, 0x80 },  // HDW = 0x8010 = 1024 px → Confirm endianess

  // --- Vertical Start Position ---
  { 0x1E, 0x0E },  // VSP = 0x0E = 14 lines

  // --- Vertical Display Height ---
  { 0x1F, 0x04 },  // VDW = 0x04 = 1024 ? (again endianess may need review)

  // --- Main Image Start Address ---
  // {0x20, 0x00},  // L0_START0
  // {0x21, 0x00},  // L0_START1
  // {0x22, 0x00},  // L0_START2
  // {0x23, 0x00},  // L0_START3
  // {0x24, 0x00},  // L1_START0
  { 0x25, 0x04 },  // L1_START1
  // {0x26, 0x00},  // L1_START2
  // {0x27, 0x00},  // L1_START3
  // {0x28, 0x00},  // L2_START0
  // {0x29, 0x00},  // L2_START1

  // --- PIP Address Setup (if used) ---
  //  {0x50, 0x00},  // PIP1_ADDR0
  //  {0x51, 0x00},
  //  {0x52, 0x00},
  //  {0x53, 0x00},
  // {0x54, 0x00},
  { 0x55, 0x04 },

  // --- Memory Geometry (Frame Buffer Stride, etc) ---
  { 0x5B, 0x04 },  // PPL (Pixels per line) low byte
  { 0x5C, 0x58 },  // PPL high byte (0x0458 = 1112 px)
  { 0x5D, 0x02 },  // LPF (Lines per frame)
  { 0x5E, 0x01 },  // XY mode enable, 16 BPP

  // --- PWM Setup (Backlight Control) ---
  { 0x0B, 0x01 },  // Enable PWM0_IRQ
  { 0x0C, 0x01 },  // Clear PWM0_IRQ (was: 0x11)
  { 0x0D, 0x01 },  // Mask PWM0_IRQ
  { 0x85, 0x02 },  // PWM Clock Divider: ÷2 (was: 0x12)
  { 0x86, 0x0B },  // PWM Mode, Deadzone, Enable (was: 0x0B)
  { 0x87, 0x00 },  // Dead zone length = 0
  { 0x88, 0xFF },  // PWM High Duration
  { 0x89, 0x00 },  // PWM Low Duration

  // --- Internal Font CGROM Selection ---
  { 0xCC, 0x20 },  // Internal CGROM, ISO 8859-1, 16x32 font
  { 0xCD, 0x80 }   // Text Align enable

};



// 7-Seg scaling factor (Range: 1 -- 22)
uint8_t sc = 4;


// ================================
// ****** Class for our LED *******
// ================================
class ToS_LED {

public:

  // constructor
  ToS_LED(uint8_t pin)
    : m_pin(pin) {
    DDRB |= (1 << m_pin);
  }

  // flash-method
  void flash(int times = 2) {
    for (int i = times * 2; i; i--) {
      toggle();
      delay(85);
    }
  }

private:
  int m_pin;

  void toggle() {
    PINB = (1 << m_pin);
  }
};

ToS_LED LED(4);


//** Defines
// ================================
// DJD-I2C NOP-Based Timing (8 MHz)
// ================================

#define TWI_FAST_MODE  // Comment out for STANDARD mode (I2C 100 kHz)

#define NOP __asm__ __volatile__("nop")

#ifdef TWI_FAST_MODE
//** FAST Mode: SCL = 400 kHz
#define DELAY_T2TWI() \
  do { \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
  } while (0)  // ≈ 2.0 µs

#define DELAY_T4TWI() \
  do { \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
  } while (0)  // ≈ 1.0 µs

#else
//** STANDARD Mode: SCL ≤ 100 kHz
#define DELAY_T2TWI() \
  do { \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
  } while (0)  // ≈ 5.0 µs

#define DELAY_T4TWI() \
  do { \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
    NOP; \
  } while (0)  // ≈ 4.0 µs
#endif


// ======================================================
// +++++ I2C Low Level Routines for Tiny 85 using the USI
// +++++ based on code developed by David Johnson-Davies
// +++++ www.technoblogy.com                     ********
// ======================================================
class DJD_I2C {

public:

  //** Constants
  static constexpr uint8_t TWI_NACK_BIT = 0;  // Bit position for (N)ACK bit.

  // Prepare register value to: Clear flags, and set USI to shift 8 bits i.e. count 16 clock edges.
  static constexpr uint8_t USISR_8bit = 1 << USISIF | 1 << USIOIF | 1 << USIPF | 1 << USIDC | 0x0 << USICNT0;

  // Prepare register value to: Clear flags, and set USI to shift 1 bit i.e. count 2 clock edges.
  static constexpr uint8_t USISR_1bit = 1 << USISIF | 1 << USIOIF | 1 << USIPF | 1 << USIDC | 0xE << USICNT0;

  static bool init(uint8_t probeAddress) {
    // Setup lines and USI registers
    PORT_USI |= 1 << PIN_USI_SDA;     // Pullup SDA
    PORT_USI_CL |= 1 << PIN_USI_SCL;  // Pullup SCL

    DDR_USI_CL |= 1 << PIN_USI_SCL;  // SCL as output
    DDR_USI |= 1 << PIN_USI_SDA;     // SDA as output

    USIDR = 0xFF;
    USICR = 0 << USISIE | 0 << USIOIE | 1 << USIWM1 | 0 << USIWM0 | 1 << USICS1 | 0 << USICS0 | 1 << USICLK | 0 << USITC;
    USISR = 1 << USISIF | 1 << USIOIF | 1 << USIPF | 1 << USIDC | 0x0 << USICNT0;

    // === Probe device ===
    if (!start(probeAddress, 0)) {
      return false;  // Probe failed — no ACK from slave
    }

    // send stop after probe
    stop();

    return true;  // All good
  }

  // Start transmission by sending address
  static bool start(uint8_t address, int32_t readcount) {
    if (readcount != 0) {
      I2Ccount = readcount;
      readcount = 1;
    }
    uint8_t addressRW = (address << 1) | (readcount & 1);

    /* Release SCL to ensure that (repeated) Start can be performed */
    PORT_USI_CL |= 1 << PIN_USI_SCL;  // Release SCL.
    while (!(PIN_USI_CL & 1 << PIN_USI_SCL))
      ;  // Verify that SCL becomes high.
#ifdef TWI_FAST_MODE
    DELAY_T4TWI();
#else
    DELAY_T2TWI();
#endif

    /* Generate Start Condition */
    PORT_USI &= ~(1 << PIN_USI_SDA);  // Force SDA LOW.
    DELAY_T4TWI();
    PORT_USI_CL &= ~(1 << PIN_USI_SCL);  // Pull SCL LOW.
    PORT_USI |= 1 << PIN_USI_SDA;        // Release SDA.

    if (!(USISR & 1 << USISIF)) return false;

    /*Write address */
    PORT_USI_CL &= ~(1 << PIN_USI_SCL);  // Pull SCL LOW.
    USIDR = addressRW;                   // Setup data.
    transfer(USISR_8bit);                // Send 8 bits on bus.

    /* Clock and verify (N)ACK from slave */
    DDR_USI &= ~(1 << PIN_USI_SDA);                              // Enable SDA as input.
    if (transfer(USISR_1bit) & 1 << TWI_NACK_BIT) return false;  // No ACK

    return true;  // Start successfully completed
  }

  static uint8_t transfer(uint8_t data) {
    USISR = data;  // Set USISR according to data.
    // Prepare clocking.
    data = 0 << USISIE | 0 << USIOIE |                // Interrupts disabled
           1 << USIWM1 | 0 << USIWM0 |                // Set USI in Two-wire mode.
           1 << USICS1 | 0 << USICS0 | 1 << USICLK |  // Software clock strobe as source.
           1 << USITC;                                // Toggle Clock Port.
    do {
      DELAY_T2TWI();
      USICR = data;  // Generate positive SCL edge.
      while (!(PIN_USI_CL & 1 << PIN_USI_SCL))
        ;  // Wait for SCL to go high.
      DELAY_T4TWI();
      USICR = data;                    // Generate negative SCL edge.
    } while (!(USISR & 1 << USIOIF));  // Wait for USIOIF (USI Overflow Interrupt Flag) to indicate end of 8 or 1 bit transfer


    DELAY_T2TWI();
    data = USIDR;                   // Read out data.
    USIDR = 0xFF;                   // Release SDA.
    DDR_USI |= (1 << PIN_USI_SDA);  // Enable SDA as output.

    return data;  // Return the data from the USIDR
  }

  static uint8_t read(void) {
    if ((I2Ccount != 0) && (I2Ccount != -1)) I2Ccount--;

    /* Read a byte */
    DDR_USI &= ~(1 << PIN_USI_SDA);  // Enable SDA as input.
    uint8_t data = transfer(USISR_8bit);

    /* Prepare to generate ACK (or NACK in case of End Of Transmission) */
    if (I2Ccount == 0) USIDR = 0xFF;
    else USIDR = 0x00;
    transfer(USISR_1bit);  // Generate ACK/NACK.

    return data;  // Read successfully completed
  }

  static uint8_t readLast(void) {
    I2Ccount = 0;
    return read();
  }

  static bool write(uint8_t data) {
    /* Write a byte */
    PORT_USI_CL &= ~(1 << PIN_USI_SCL);  // Pull SCL LOW.
    USIDR = data;                        // Setup data.
    transfer(USISR_8bit);                // Send 8 bits on bus.

    /* Clock and verify (N)ACK from slave */
    DDR_USI &= ~(1 << PIN_USI_SDA);  // Enable SDA as input.
    if (transfer(USISR_1bit) & 1 << TWI_NACK_BIT) return false;

    return true;  // Write successfully completed
  }


  static bool restart(uint8_t address, int32_t readcount) {
    return start(address, readcount);
  }


  static void stop(void) {
    PORT_USI &= ~(1 << PIN_USI_SDA);  // Pull SDA low.
    PORT_USI_CL |= 1 << PIN_USI_SCL;  // Release SCL.
    while (!(PIN_USI_CL & 1 << PIN_USI_SCL))
      ;  // Wait for SCL to go high.
    DELAY_T4TWI();
    PORT_USI |= 1 << PIN_USI_SDA;  // Release SDA.
    DELAY_T2TWI();
  }

private:
  static int32_t I2Ccount;
};

// I2C count:
int32_t DJD_I2C::I2Ccount = 0;

//** constructor
DJD_I2C I2C;


//***********************************//
//******** LT7683 class *************//
//***********************************//
class ToS_LT7683 : public Print {
private:

  //** Text Cursor Position
  uint16_t xBase = 0;
  uint16_t yBase = 0;

  //***********************************************//
  //******** Medium Level I2C routines ************//
  //***********************************************//

  void cmdWrite(uint8_t cmd) {
    I2C.start(I2C_ADDR_CMD, 0);
    I2C.write(cmd);
    I2C.stop();
  }


  void dataWrite(uint8_t data) {
    I2C.start(I2C_ADDR_DATA, 0);
    I2C.write(data);
    I2C.stop();
  }

  uint8_t dataRead() {
    I2C.start(I2C_ADDR_DATA, 1);
    return (I2C.read());
  }

  uint8_t readStatus() {
    I2C.start(I2C_ADDR_CMD, 1);
    return (I2C.read());
  }


  //***************************************************//
  //********  Top Level I2C routines ******************//
  //***************************************************//

  void writeReg(uint8_t reg, uint8_t data) {
    cmdWrite(reg);
    dataWrite(data);
  }


  uint8_t readReg(uint8_t reg) {
    cmdWrite(reg);
    return dataRead();
  }


  void Check_Busy_Draw() {
    uint8_t temp;
    do {
      temp = readStatus();  // Get status byte
      // delayMicroseconds (20);      // Small delay to prevent bus overload
    } while (temp & 0x08);  // Wait until Bit 3 clears (DRAW engine idle)
  }


  //** Display control
public:
  void init_LT7683() {

    for (uint16_t i = 0; i < sizeof(initCode) / sizeof(initCode[0]); i++) {
      uint8_t reg = pgm_read_byte(&initCode[i][0]);
      uint8_t val = pgm_read_byte(&initCode[i][1]);
      writeReg(reg, val);
      delay(10);
    }

    Graphic_Mode();
    Display_ON();  // affects R.0x12 B.7
  }


  void Graphic_Mode() {
    uint8_t temp;
    temp = readReg(0x03);
    temp &= 0xFB;          //
    writeReg(0x03, temp);  //
  }


  void Text_Mode() {
    uint8_t temp;
    temp = readReg(0x03);
    temp |= 0x04;          //
    writeReg(0x03, temp);  //
  }


  void textSize(uint8_t height) {
    if ((height < 1) || (height > 3)) return;

    switch (height) {

      case 1: writeReg(0xCC, 0x00); break;
      case 2: writeReg(0xCC, 0x10); break;
      case 3: writeReg(0xCC, 0x20); break;
    }
  }


  void Display_ON(void) {
    uint8_t temp;
    temp = readReg(0x12);
    temp |= 0x40;
    writeReg(0x12, temp);
  }


  void setFrameAdr(uint16_t x_start, uint16_t y_start, uint16_t x_end, uint16_t y_end) {

    // start
    writeReg(0x68, x_start);       //
    writeReg(0x69, x_start >> 8);  //
    writeReg(0x6A, y_start);       //
    writeReg(0x6B, y_start >> 8);  //

    // end
    writeReg(0x6C, x_end);       //
    writeReg(0x6D, x_end >> 8);  //
    writeReg(0x6E, y_end);       //
    writeReg(0x6F, y_end >> 8);  //
  }

  void line(uint16_t x_start, uint16_t y_start, uint16_t x_end, uint16_t y_end) {
    setFrameAdr(x_start, y_start, x_end, y_end);
    writeReg(0x67, 0x80);  // line
    Check_Busy_Draw();
  }


  void rounded_rect(uint16_t x_start, uint16_t y_start, uint16_t x_end, uint16_t y_end, uint8_t radius) {
    setFrameAdr(x_start, y_start, x_end, y_end);
    writeReg(0x77, radius);
    writeReg(0x79, radius);
    writeReg(0x76, 0xB0);  // rounded rectangle
    Check_Busy_Draw();
  }


  void Fill_rounded_rect(uint16_t x_start, uint16_t y_start, uint16_t x_end, uint16_t y_end, uint8_t radius) {
    setFrameAdr(x_start, y_start, x_end, y_end);
    writeReg(0x77, radius);
    writeReg(0x79, radius);
    writeReg(0x76, 0xF0);  // rounded rectangle
    Check_Busy_Draw();
  }


  void Fill_rect(uint16_t x_start, uint16_t y_start, uint16_t x_end, uint16_t y_end) {
    setFrameAdr(x_start, y_start, x_end, y_end);
    writeReg(0x76, 0xE0);  // filled square
    Check_Busy_Draw();
  }

  void ellipse(uint16_t x, uint16_t y, uint16_t radius1, uint16_t radius2) {

    //** circle center
    writeReg(0x7B, x);       //
    writeReg(0x7C, x >> 8);  //
    writeReg(0x7D, y);       //
    writeReg(0x7E, y >> 8);  //

    //** radii
    writeReg(0x77, radius1);       //
    writeReg(0x78, radius1 >> 8);  //
    writeReg(0x79, radius2);       //
    writeReg(0x7A, radius2 >> 8);  //

    // draw Ellipse
    writeReg(0x76, 0x80);  // filled ellipse
    Check_Busy_Draw();
  }

  void circle(uint16_t x, uint16_t y, uint16_t radius) {
    ellipse(x, y, radius, radius);
  }

  void Fill_ellipse(uint16_t x, uint16_t y, uint16_t radius1, uint16_t radius2) {

    //** circle center
    writeReg(0x7B, x);       //
    writeReg(0x7C, x >> 8);  //
    writeReg(0x7D, y);       //
    writeReg(0x7E, y >> 8);  //

    //** radii
    writeReg(0x77, radius1);       //
    writeReg(0x78, radius1 >> 8);  //
    writeReg(0x79, radius2);       //
    writeReg(0x7A, radius2 >> 8);  //

    // draw Ellipse
    writeReg(0x76, 0xC0);  // filled ellipse
    Check_Busy_Draw();
  }


  void Fill_circle(uint16_t x, uint16_t y, uint16_t radius) {
    Fill_ellipse(x, y, radius, radius);
  }


  void Fill_triangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t x3, uint16_t y3) {

    //** point1
    writeReg(0x68, x1);       //
    writeReg(0x69, x1 >> 8);  //
    writeReg(0x6A, y1);       //
    writeReg(0x6B, y1 >> 8);  //

    //** point2
    writeReg(0x6C, x2);       //
    writeReg(0x6D, x2 >> 8);  //
    writeReg(0x6E, y2);       //
    writeReg(0x6F, y2 >> 8);  //

    //** point3
    writeReg(0x70, x3);       //
    writeReg(0x71, x3 >> 8);  //
    writeReg(0x72, y3);       //
    writeReg(0x73, y3 >> 8);  //

    //** draw
    writeReg(0x67, 0xA2);  // filled triangle
    Check_Busy_Draw();
  }

  void Fill_arc(uint16_t x, uint16_t y, uint16_t radius1, uint16_t radius2, uint8_t select) {

    //** center
    writeReg(0x7B, x);       //
    writeReg(0x7C, x >> 8);  //
    writeReg(0x7D, y);       //
    writeReg(0x7E, y >> 8);  //

    //** radii
    writeReg(0x77, radius2);       //
    writeReg(0x78, radius2 >> 8);  //
    writeReg(0x79, radius1);       //
    writeReg(0x7A, radius1 >> 8);  //

    writeReg(0x76, 0xD0 | select);  // filled arch
    Check_Busy_Draw();
  }

  // set the Text-cursor to x, y
  void goto_xy(uint16_t x, uint16_t y) {
    xBase = x;
    yBase = y;
    writeReg(0x63, x);       //
    writeReg(0x64, x >> 8);  //
    writeReg(0x65, y);       //
    writeReg(0x66, y >> 8);  //
  }


  void PlotChar(uint16_t x, uint16_t y, char c) {
    goto_xy(x, y);
    Text_Mode();
    cmdWrite(0x04);
    dataWrite(c);
    Graphic_Mode();
  }


  // we inherit from Print
  size_t write(uint8_t c) override {
    if (c == '\n') {
      // Handle newline: move to the next line
      xBase = 0;    // Reset xBase to the start of the line
      yBase += 30;  // Move down by the font height
      return 1;
    } else if (c == '\r') {
      // Ignore carriage return
      return 1;
    }

    // Ensure character is printable
    if (c < 32 || c > 254) return 0;  // need to adapt for Umlaute

    //** Plot the character using the Plotchar method
    PlotChar(xBase, yBase, c);

    //** Update xBase for the next character
    xBase += 20;          // Move by the character width (16 for Arial 16x24)
    if (xBase >= 1024) {  // Handle line wrapping
      xBase = 0;
      yBase += 30;  // Move to the next line
    }
    return 1;  // Indicate success
  }


  void Plot7Seg(uint16_t x, uint16_t y, uint8_t c) {

    switch (c) {

      case 0:
        //** build # 0 in 7-Seg
        Fill_rect(x + (2 * sc), y, x + (10 * sc), y + (2 * sc));               // top horizontal
        Fill_arc(x + (10 * sc), y + (2 * sc), 2 * sc, 2 * sc, 2);              // top right arc
        Fill_rect(x + (10 * sc), y + (2 * sc), x + (12 * sc), y + (22 * sc));  // right vertical
        Fill_arc(x + (10 * sc), y + (22 * sc), 2 * sc, 2 * sc, 3);             // bottom right arc
        Fill_rect(x + (2 * sc), y + (22 * sc), x + (10 * sc), y + (24 * sc));  // bottom hor
        Fill_arc(x + (2 * sc), y + (22 * sc), 2 * sc, 2 * sc, 0);              // bottom left arc
        Fill_rect(x, y + (2 * sc), x + (2 * sc), y + (22 * sc));               // lower left vertical
        Fill_arc(x + (2 * sc), y + (2 * sc), 2 * sc, 2 * sc, 1);               // top left arc
        break;

      case 1:
        //** build # 1 in 7-Seg
        Fill_circle(x + (11 * sc), y + sc, sc);                          // top right tip
        Fill_rect(x + (10 * sc), y + sc, x + (12 * sc), y + (23 * sc));  // right vert
        Fill_circle(x + (11 * sc), y + (23 * sc), sc);                   // bottom right tip
        break;

      case 2:
        //** build # 2 in 7-Seg
        Fill_rect(x + sc, y, x + (10 * sc), y + (2 * sc));                     // top hor
        Fill_circle(x + sc, y + sc, sc);                                       // top left tip
        Fill_arc(x + (10 * sc), y + (2 * sc), 2 * sc, 2 * sc, 2);              // top right arc
        Fill_rect(x + (10 * sc), y + (2 * sc), x + (12 * sc), y + (11 * sc));  // top right vert
        Fill_arc(x + (10 * sc), y + (11 * sc), 2 * sc, 2 * sc, 3);             // center right arc
        Fill_rect(x + (2 * sc), y + (11 * sc), x + (10 * sc), y + (13 * sc));  // center vert
        Fill_arc(x + (2 * sc), y + (13 * sc), 2 * sc, 2 * sc, 1);              // center left arc
        Fill_rect(x, y + (13 * sc), x + (2 * sc), y + (22 * sc));              // left bottom vert
        Fill_arc(x + (2 * sc), y + (22 * sc), 2 * sc, 2 * sc, 0);              // left bottom arc
        Fill_rect(x + (2 * sc), y + (22 * sc), x + (11 * sc), y + (24 * sc));  // bottom hor
        Fill_circle(x + (11 * sc), y + (23 * sc), sc);                         // bottom right tip
        break;

      case 3:
        //** build # 3 in 7-Seg
        Fill_circle(x + sc, y + sc, sc);                                       // top left tip
        Fill_rect(x + sc, y, x + (10 * sc), y + (2 * sc));                     // top hor
        Fill_arc(x + (10 * sc), y + (2 * sc), 2 * sc, 2 * sc, 2);              // right top arc
        Fill_rect(x + (10 * sc), y + (2 * sc), x + (12 * sc), y + (22 * sc));  // right vertical
        Fill_rect(x + (3 * sc), y + (11 * sc), x + (10 * sc), y + (13 * sc));  // center hor
        Fill_circle(x + (3 * sc), y + (12 * sc), sc);                          // center left tip
        Fill_arc(x + (10 * sc), y + (22 * sc), 2 * sc, 2 * sc, 3);             // bottom right arc
        Fill_rect(x + sc, y + (22 * sc), x + (10 * sc), y + (24 * sc));        // bottom hor
        Fill_circle(x + sc, y + (23 * sc), sc);                                // bottom left tip
        break;

      case 4:
        //** build # 4 in 7-Seg
        Fill_rect(x + (10 * sc), y + (4 * sc), x + (12 * sc), y + (12 * sc));   // right vertical
        Fill_circle(x + (11 * sc), y + (4 * sc), sc);                           // top right tip
        Fill_rect(x + (10 * sc), y + (12 * sc), x + (12 * sc), y + (23 * sc));  // right vertical
        Fill_circle(x + (11 * sc), y + (23 * sc), sc);                          // bottom right tip
        Fill_rect(x + (2 * sc), y + (11 * sc), x + (10 * sc), y + (13 * sc));   // center horizontal
        Fill_rect(x, y + sc, x + (2 * sc), y + (11 * sc));                      // left top vertical
        Fill_arc(x + (2 * sc), y + (11 * sc), 2 * sc, 2 * sc, 0);               // left center arc
        Fill_circle(x + sc, y + sc, sc);                                        // top left tip
        break;

      case 5:
        //** build # 5 in 7-Seg
        Fill_rect(x + (2 * sc), y, x + (11 * sc), y + (2 * sc));                // top hor
        Fill_circle(x + (11 * sc), y + sc, sc);                                 // top right tip
        Fill_arc(x + (2 * sc), y + (2 * sc), 2 * sc, 2 * sc, 1);                // top left arc
        Fill_rect(x, y + (2 * sc), x + (2 * sc), y + (11 * sc));                // upper left vertical
        Fill_arc(x + (2 * sc), y + (11 * sc), 2 * sc, 2 * sc, 0);               // center left arc
        Fill_rect(x + (2 * sc), y + (11 * sc), x + (10 * sc), y + (13 * sc));   // center hor
        Fill_arc(x + (10 * sc), y + (13 * sc), 2 * sc, 2 * sc, 2);              // center right arc
        Fill_arc(x + (10 * sc), y + (22 * sc), 2 * sc, 2 * sc, 3);              // bottom right arc
        Fill_rect(x + (10 * sc), y + (13 * sc), x + (12 * sc), y + (22 * sc));  // lower right vertical
        Fill_rect(x + (2 * sc), y + (22 * sc), x + (10 * sc), y + (24 * sc));   // bottom hor
        Fill_circle(x + (2 * sc), y + (23 * sc), sc);                           // bottom left tip
        break;

      case 6:
        //** build # 6 in 7-Seg
        Fill_rect(x + (2 * sc), y, x + (8 * sc), y + (2 * sc));                 // top hor
        Fill_circle(x + (8 * sc), y + sc, sc);                                  // top right tip
        Fill_arc(x + (2 * sc), y + (2 * sc), 2 * sc, 2 * sc, 1);                // top left arc
        Fill_rect(x, y + (2 * sc), x + (2 * sc), y + (22 * sc));                // left vertical
        Fill_arc(x + (2 * sc), y + (11 * sc), 2 * sc, 2 * sc, 0);               // center right arc
        Fill_rect(x + (2 * sc), y + (11 * sc), x + (10 * sc), y + (13 * sc));   // center hor
        Fill_arc(x + (10 * sc), y + (13 * sc), 2 * sc, 2 * sc, 2);              // left center arc
        Fill_arc(x + (10 * sc), y + (22 * sc), 2 * sc, 2 * sc, 3);              // bottom right arc
        Fill_rect(x + (10 * sc), y + (13 * sc), x + (12 * sc), y + (22 * sc));  // lower right vertical
        Fill_rect(x + (2 * sc), y + (22 * sc), x + (10 * sc), y + (24 * sc));   // bottom hor
        Fill_arc(x + (2 * sc), y + (22 * sc), 2 * sc, 2 * sc, 0);               // bottom left arc
        break;

      case 7:
        //** build # 7 in 7-Seg
        Fill_circle(x + (2 * sc), y + sc, sc);                                 // top tip
        Fill_rect(x + (2 * sc), y, x + (10 * sc), y + (2 * sc));               // top hor
        Fill_arc(x + (10 * sc), y + (2 * sc), 2 * sc, 2 * sc, 2);              // top right arc
        Fill_rect(x + (10 * sc), y + (2 * sc), x + (12 * sc), y + (23 * sc));  // right vert
        Fill_circle(x + (11 * sc), y + (23 * sc), sc);                         // bottom tip
        break;

      case 8:
        //** build # 8 in 7-Seg
        Fill_rect(x + (2 * sc), y, x + (10 * sc), y + (2 * sc));               // top horizontal
        Fill_arc(x + (10 * sc), y + (2 * sc), 2 * sc, 2 * sc, 2);              // top right arc
        Fill_rect(x + (10 * sc), y + (2 * sc), x + (12 * sc), y + (22 * sc));  // right vertical
        Fill_arc(x + (10 * sc), y + (22 * sc), 2 * sc, 2 * sc, 3);             // bottom right arc
        Fill_rect(x + (2 * sc), y + (22 * sc), x + (10 * sc), y + (24 * sc));  // bottom hor
        Fill_arc(x + (2 * sc), y + (22 * sc), 2 * sc, 2 * sc, 0);              // bottom left arc
        Fill_rect(x, y + (2 * sc), x + (2 * sc), y + (22 * sc));               // left vertical
        Fill_arc(x + (2 * sc), y + (2 * sc), 2 * sc, 2 * sc, 1);               // top left arc
        Fill_rect(x + (2 * sc), y + (11 * sc), x + (10 * sc), y + (13 * sc));  // center hor
        break;

      case 9:
        //** build 9 in 7-Seg
        Fill_rect(x + (2 * sc), y, x + (10 * sc), y + (2 * sc));               // top horizontal
        Fill_arc(x + (10 * sc), y + (2 * sc), 2 * sc, 2 * sc, 2);              // top right arc
        Fill_rect(x + (10 * sc), y + (2 * sc), x + (12 * sc), y + (22 * sc));  // right vertical
        Fill_arc(x + (10 * sc), y + (22 * sc), 2 * sc, 2 * sc, 3);             // bottom right arc
        Fill_rect(x + (4 * sc), y + (22 * sc), x + (10 * sc), y + (24 * sc));  // bottom hor
        Fill_circle(x + (4 * sc), y + (23 * sc), sc);                          // bottom left tip
        Fill_arc(x + (2 * sc), y + (11 * sc), 2 * sc, 2 * sc, 0);              // center left arc
        Fill_rect(x, y + (2 * sc), x + (2 * sc), y + (11 * sc));               // left vertical
        Fill_arc(x + (2 * sc), y + (2 * sc), 2 * sc, 2 * sc, 1);               // top left arc
        Fill_rect(x + (2 * sc), y + (11 * sc), x + (10 * sc), y + (13 * sc));  // center hor
        break;
    }
  }


  void clearScreen() {
    fore(0x00, 0x00, 0x00);
    Fill_rect(0, 0, 1023, 599);
  }

  void fore(uint8_t red, uint8_t green, uint8_t blue) {
    writeReg(0xD2, red);    //
    writeReg(0xD3, green);  //
    writeReg(0xD4, blue);   //
  }

  void back(uint8_t red, uint8_t green, uint8_t blue) {
    writeReg(0xD5, red);    //
    writeReg(0xD6, green);  //
    writeReg(0xD7, blue);   //
  }
};


//** Display Controller constructor
ToS_LT7683 tft;


//** Arduino Demo Program
void setup() {

  I2C.init(0x7E);
  LED.flash(4);

  tft.init_LT7683();
  LED.flash(3);

  tft.clearScreen();

  //** outer Frame
  tft.fore(0xFF, 0xFF, 0x00);  // ylw

  tft.line(1, 1, 1023, 1);
  tft.line(1, 1, 1, 599);
  tft.line(1, 599, 1023, 599);
  tft.line(1023, 1, 1023, 599);


  // x, y - arrows
  // 'x'
  tft.fore(0xFF, 0x00, 0x00);
  tft.line(10, 10, 40, 10);
  tft.line(35, 7, 40, 10);
  tft.line(35, 13, 40, 10);
  tft.line(46, 14, 54, 22);
  tft.line(46, 22, 54, 14);

  // 'y'
  tft.fore(0x00, 0xFF, 0x00);
  tft.line(10, 10, 10, 40);
  tft.line(7, 35, 10, 40);
  tft.line(13, 35, 10, 40);
  tft.line(13, 45, 17, 50);
  tft.line(21, 45, 17, 50);
  tft.line(17, 51, 17, 56);

  tft.textSize(3);
  tft.fore(0xFF, 0xFF, 0x00);
  tft.back(0x00, 0x00, 0x00);
  tft.goto_xy(350, 50);
  tft.print(F("ATtiny85 - LT7683"));

  // 7-Seg in increasing size
  tft.fore(0x00, 0x00, 0xFF);

  for (int i = 1; i < 9; i++) {
    sc = i;
    tft.Plot7Seg(36 + ((i - 1) * 120), 240 - (i * 13), i);
  }

  // Gyroscope
  // hor line
  tft.fore(0xFF, 0xFF, 0xFF);
  tft.line(150, 450, 350, 450);
  tft.fore(0x00, 0xFF, 0x00);

  //
  tft.Fill_ellipse(250, 450, 80, 30);


  tft.fore(0x00, 0x00, 0x00);
  tft.Fill_ellipse(250, 450, 79, 22);

  // vertical lines
  tft.fore(0xFF, 0xFF, 0xFF);
  tft.line(250, 360, 250, 420);
  tft.line(250, 430, 250, 540);

  tft.fore(0x00, 0xFF, 0x00);
  tft.circle(250, 450, 80);

  tft.fore(0xFF, 0x00, 0x00);
  tft.Fill_rounded_rect(600, 480, 700, 520, 20);

  tft.fore(0xFF, 0x00, 0xFF);
  tft.Fill_triangle(500, 380, 550, 410, 510, 420);
  tft.fore(0x00, 0xFF, 0xFF);
  tft.Fill_triangle(480, 450, 520, 460, 510, 420);
  tft.fore(0xFF, 0xFF, 0xFF);
  tft.circle(880, 460, 40);
  tft.Fill_arc(880, 460, 40, 40, 1);
  tft.Fill_arc(880, 460, 40, 40, 3);
}


void loop() {
}
