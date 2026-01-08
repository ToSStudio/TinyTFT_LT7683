// T.85 with TFT 1024 x 600 (LT7683 / RA8876) using barebone I2C (Technoblogy)
// connections: SDA PB0 = Pin7 (Display)
//              SCL PB2 = Pin8 (Display)
// connect the power supply of the Display to GND & 5 Volt.

// This is a demo-clock using compile-time as a basis

// Helper macro to convert ASCII to numeric
#define CHAR_TO_NUM(c) ((c) - '0')

// Extract hours and minutes at compile time
uint8_t hrs  = CHAR_TO_NUM (__TIME__[0]) * 10 + CHAR_TO_NUM (__TIME__[1]);
uint8_t mins = CHAR_TO_NUM (__TIME__[3]) * 10 + CHAR_TO_NUM (__TIME__[4]);
uint8_t secs = CHAR_TO_NUM (__TIME__[6]) * 10 + CHAR_TO_NUM (__TIME__[7]);

//** struct for x, y position on Display
struct xy {
  uint16_t x;
  uint16_t y;
};

// ======================================
// ****** I2C addresses of LT7683 *******
// ======================================
# define I2C_ADDR_CMD   0x7E  // write: command, read: status
# define I2C_ADDR_DATA  0x7F  // write: data, read: data

// ================================================
// ****** Array for Display-controller init *******
// ****** first number is the register ************
// ****** second is the register-value ************
// ================================================
const uint8_t initCode[][2] PROGMEM  = {

  {0x00, 0x01},  // SW reset
  {0x01, 0x01},  // System configuration: 16-bit data bus

  // --- SDRAM Initialization ---
  {0xE0, 0x29},  // SDRAM Attribute: 16-bit, 4 banks, CAS=3
  {0xE1, 0x03},  // SDRAM Mode Register ACAS latency = 3
  {0xE2, 0x0B},  // SDRAM Refresh Cycle Low Byte
  {0xE3, 0x06},  // SDRAM Refresh Cycle High Byte
  {0xE4, 0x01},  // Execute SDRAM Initialization

  // --- Graphic Mode Setup ---
  {0x02, 0x00},  // Graphic Mode Enable (0x00 = Graphic Mode)
  {0x03, 0x00},  // Rotation off

  // --- Window Geometry: Horizontal Display Period ---
  {0x10, 0x04},  // HDP: Horizontal Display Width (LSB) = 0x04
  {0x12, 0x80},  // HDP: Horizontal Display Width (MSB) = 0x80 → Total: 0x804 = 1024 px

  // --- Horizontal Non-Display Period (Back Porch + Front Porch) ---
  {0x13, 0xB0},  // HND = 0xB0 = 176 clocks

  // --- Horizontal Sync Pulse Width ---
  {0x14, 0x7F},  // HSW = 0x7F = 127 clocks

  // --- Vertical Display Period ---
  {0x15, 0x00},  // VDP (LSB) = 0x00
  {0x16, 0x12},  // VDP (MSB) = 0x12 → Total: 0x1200 = 4608 (but that’s too high → misconfigured?)
  {0x17, 0xF8},  // Actually LSB of 600 (0x0258): 0xF8 = 248 ?
  {0x18, 0x1A},  // VDP corrected: 0x07A8 = 1960? → double-check values later

  // --- Vertical Non-Display Period ---
  {0x19, 0x07},  // VND = 0x07 = 7 lines

  // --- Vertical Sync Pulse Width ---
  {0x1A, 0x57},  // VSW = 0x57 = 87 lines

  // --- Horizontal Start Position ---
  {0x1B, 0x02},  // HSP = 0x02 = 2 clocks

  // --- Horizontal Display Width ---
  {0x1C, 0x10},
  {0x1D, 0x80},  // HDW = 0x8010 = 1024 px → Confirm endianess

  // --- Vertical Start Position ---
  {0x1E, 0x0E},  // VSP = 0x0E = 14 lines

  // --- Vertical Display Height ---
  {0x1F, 0x04},  // VDW = 0x04 = 1024 ? (again endianess may need review)

  // --- Main Image Start Address ---
  // {0x20, 0x00},  // L0_START0
  // {0x21, 0x00},  // L0_START1
  // {0x22, 0x00},  // L0_START2
  // {0x23, 0x00},  // L0_START3
  // {0x24, 0x00},  // L1_START0
  {0x25, 0x04},  // L1_START1
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
  {0x55, 0x04},

  // --- Memory Geometry (Frame Buffer Stride, etc) ---
  {0x5B, 0x04},  // PPL (Pixels per line) low byte
  {0x5C, 0x58},  // PPL high byte (0x0458 = 1112 px)
  {0x5D, 0x02},  // LPF (Lines per frame)
  {0x5E, 0x01},  // XY mode enable, 16 BPP

  // --- PWM Setup (Backlight Control) ---
  {0x0B, 0x01},  // Enable PWM0_IRQ
  {0x0C, 0x01},  // Clear PWM0_IRQ (was: 0x11)
  {0x0D, 0x01},  // Mask PWM0_IRQ
  {0x85, 0x02},  // PWM Clock Divider: ÷2 (was: 0x12)
  {0x86, 0x0B},  // PWM Mode, Deadzone, Enable (was: 0x0B)
  {0x87, 0x00},  // Dead zone length = 0
  {0x88, 0xFF},  // PWM High Duration
  {0x89, 0x00},  // PWM Low Duration

  // --- Internal Font CGROM Selection ---
  {0xCC, 0x20},  // Internal CGROM, ISO 8859-1, 16x32 font
  {0xCD, 0x80}   // Text Align enable

};


// 7-Seg scaling factor (Range: 1 -- 22)
uint8_t sc = 4;


// ================================
// ****** Class for our LED *******
// ================================
class ToS_LED {
  public:
    // constructor
    ToS_LED (uint8_t pin) : m_pin(pin)  {
      DDRB |= (1 << m_pin);
    }

    // flash-method
    void flash (int times = 2)  {
      for (int i = times * 2; i; i--)  {
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

ToS_LED LED (4);


//** Defines
// ================================
// DJD-I2C NOP-Based Timing (8 MHz)
// ================================

# define TWI_FAST_MODE  // Comment out for STANDARD mode (I2C 100 kHz)

# define NOP __asm__ __volatile__("nop")

# ifdef TWI_FAST_MODE
//** FAST Mode: SCL = 400 kHz
# define DELAY_T2TWI()  do { \
    NOP; NOP; NOP; NOP; NOP; NOP; NOP; NOP; \
    NOP; NOP; NOP; NOP; NOP; NOP; NOP; NOP; \
  } while (0)  // ≈ 2.0 µs

# define DELAY_T4TWI()  do { \
    NOP; NOP; NOP; NOP; NOP; NOP; NOP; NOP; \
  } while (0)  // ≈ 1.0 µs

# else
//** STANDARD Mode: SCL ≤ 100 kHz
# define DELAY_T2TWI()  do { \
    NOP; NOP; NOP; NOP; NOP; NOP; NOP; NOP; \
    NOP; NOP; NOP; NOP; NOP; NOP; NOP; NOP; \
    NOP; NOP; NOP; NOP; NOP; NOP; NOP; NOP; \
    NOP; NOP; NOP; NOP; NOP; NOP; NOP; NOP; \
    NOP; NOP; NOP; NOP; NOP; NOP; NOP; NOP; \
  } while (0)  // ≈ 5.0 µs

# define DELAY_T4TWI()  do { \
    NOP; NOP; NOP; NOP; NOP; NOP; NOP; NOP; \
    NOP; NOP; NOP; NOP; NOP; NOP; NOP; NOP; \
    NOP; NOP; NOP; NOP; NOP; NOP; NOP; NOP; \
    NOP; NOP; NOP; NOP; NOP; NOP; NOP; NOP; \
  } while (0)  // ≈ 4.0 µs
# endif


// ======================================================
// +++++ I2C Low Level Routines for Tiny 85 using the USI
// +++++ based on code developed by David Johnson-Davies
// +++++ www.technoblogy.com                     ********
// ======================================================
class DJD_I2C {

  public:

    //** Constants
    static constexpr uint8_t TWI_NACK_BIT = 0; // Bit position for (N)ACK bit.

    // Prepare register value to: Clear flags, and set USI to shift 8 bits i.e. count 16 clock edges.
    static constexpr uint8_t USISR_8bit = 1 << USISIF | 1 << USIOIF | 1 << USIPF | 1 << USIDC | 0x0 << USICNT0;

    // Prepare register value to: Clear flags, and set USI to shift 1 bit i.e. count 2 clock edges.
    static constexpr uint8_t USISR_1bit = 1 << USISIF | 1 << USIOIF | 1 << USIPF | 1 << USIDC | 0xE << USICNT0;

    static bool init (uint8_t probeAddress) {
      // Setup lines and USI registers
      PORT_USI |= 1 << PIN_USI_SDA;       // Pullup SDA
      PORT_USI_CL |= 1 << PIN_USI_SCL;    // Pullup SCL

      DDR_USI_CL |= 1 << PIN_USI_SCL;     // SCL as output
      DDR_USI |= 1 << PIN_USI_SDA;        // SDA as output

      USIDR = 0xFF;
      USICR = 0 << USISIE | 0 << USIOIE |
              1 << USIWM1 | 0 << USIWM0 |
              1 << USICS1 | 0 << USICS0 | 1 << USICLK |
              0 << USITC;
      USISR = 1 << USISIF | 1 << USIOIF | 1 << USIPF | 1 << USIDC | 0x0 << USICNT0;

      // === Probe device ===
      if (! start (probeAddress, 0)) {
        return false; // Probe failed — no ACK from slave
      }

      // send stop after probe
      stop();

      return true;  // All good
    }

    // Start transmission by sending address
    static bool start (uint8_t address, int32_t readcount) {
      if (readcount != 0) {
        I2Ccount = readcount;
        readcount = 1;
      }
      uint8_t addressRW = (address << 1) | (readcount & 1);

      /* Release SCL to ensure that (repeated) Start can be performed */
      PORT_USI_CL |= 1 << PIN_USI_SCL;                                // Release SCL.
      while (!(PIN_USI_CL & 1 << PIN_USI_SCL));                       // Verify that SCL becomes high.
#ifdef TWI_FAST_MODE
      DELAY_T4TWI();
#else
      DELAY_T2TWI();
#endif

      /* Generate Start Condition */
      PORT_USI &= ~(1 << PIN_USI_SDA);                                // Force SDA LOW.
      DELAY_T4TWI ();
      PORT_USI_CL &= ~(1 << PIN_USI_SCL);                             // Pull SCL LOW.
      PORT_USI |= 1 << PIN_USI_SDA;                                   // Release SDA.

      if (!(USISR & 1 << USISIF)) return false;

      /*Write address */
      PORT_USI_CL &= ~(1 << PIN_USI_SCL);                             // Pull SCL LOW.
      USIDR = addressRW;                                              // Setup data.
      transfer (USISR_8bit);                                          // Send 8 bits on bus.

      /* Clock and verify (N)ACK from slave */
      DDR_USI &= ~(1 << PIN_USI_SDA);                                 // Enable SDA as input.
      if (transfer(USISR_1bit) & 1 << TWI_NACK_BIT) return false;     // No ACK

      return true;                                                    // Start successfully completed
    }

    static uint8_t transfer (uint8_t data) {
      USISR = data;                                                   // Set USISR according to data.
      // Prepare clocking.
      data = 0 << USISIE | 0 << USIOIE |                              // Interrupts disabled
             1 << USIWM1 | 0 << USIWM0 |                              // Set USI in Two-wire mode.
             1 << USICS1 | 0 << USICS0 | 1 << USICLK |                // Software clock strobe as source.
             1 << USITC;                                              // Toggle Clock Port.
      do {
        DELAY_T2TWI();
        USICR = data;                                                 // Generate positive SCL edge.
        while (!(PIN_USI_CL & 1 << PIN_USI_SCL));                     // Wait for SCL to go high.
        DELAY_T4TWI();
        USICR = data;                                                 // Generate negative SCL edge.
      } while (!(USISR & 1 << USIOIF));                               // Wait for USIOIF (USI Overflow Interrupt Flag) to indicate end of 8 or 1 bit transfer


      DELAY_T2TWI();
      data = USIDR;                                                   // Read out data.
      USIDR = 0xFF;                                                   // Release SDA.
      DDR_USI |= (1 << PIN_USI_SDA);                                  // Enable SDA as output.

      return data;                                                    // Return the data from the USIDR
    }

    static uint8_t read (void) {
      if ((I2Ccount != 0) && (I2Ccount != -1)) I2Ccount--;

      /* Read a byte */
      DDR_USI &= ~(1 << PIN_USI_SDA);                                 // Enable SDA as input.
      uint8_t data = transfer(USISR_8bit);

      /* Prepare to generate ACK (or NACK in case of End Of Transmission) */
      if (I2Ccount == 0) USIDR = 0xFF; else USIDR = 0x00;
      transfer(USISR_1bit);                                          // Generate ACK/NACK.

      return data;                                                   // Read successfully completed
    }

    static uint8_t readLast (void) {
      I2Ccount = 0;
      return read();
    }

    static bool write (uint8_t data) {
      /* Write a byte */
      PORT_USI_CL &= ~(1 << PIN_USI_SCL);                             // Pull SCL LOW.
      USIDR = data;                                                   // Setup data.
      transfer(USISR_8bit);                                       // Send 8 bits on bus.

      /* Clock and verify (N)ACK from slave */
      DDR_USI &= ~(1 << PIN_USI_SDA);                                 // Enable SDA as input.
      if ( transfer (USISR_1bit) & 1 << TWI_NACK_BIT) return false;

      return true;                                                    // Write successfully completed
    }


    static bool restart (uint8_t address, int32_t readcount) {
      return start (address, readcount);
    }


    static void stop (void) {
      PORT_USI &= ~(1 << PIN_USI_SDA);                                // Pull SDA low.
      PORT_USI_CL |= 1 << PIN_USI_SCL;                                // Release SCL.
      while (!(PIN_USI_CL & 1 << PIN_USI_SCL));                       // Wait for SCL to go high.
      DELAY_T4TWI ();
      PORT_USI |= 1 << PIN_USI_SDA;                                   // Release SDA.
      DELAY_T2TWI ();
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
class ToS_LT7683 : public Print  {
  private:

    //** Text Cursor Position
    uint16_t xBase = 0;
    uint16_t yBase = 0;

    //***********************************************//
    //******** Medium Level I2C routines ************//
    //***********************************************//

    void cmdWrite (uint8_t cmd) {

      I2C.start (I2C_ADDR_CMD, 0);
      I2C.write (cmd);
      I2C.stop  ();

    }


    void dataWrite (uint8_t data) {

      I2C.start (I2C_ADDR_DATA, 0);
      I2C.write (data);
      I2C.stop  ();

    }

    uint8_t dataRead () {

      I2C.start (I2C_ADDR_DATA, 1);
      return (I2C.read());

    }

    uint8_t readStatus () {

      I2C.start (I2C_ADDR_CMD, 1);
      return (I2C.read());

    }


    //***************************************************//
    //********  Top Level I2C routines ******************//
    //***************************************************//

    void writeReg (uint8_t reg, uint8_t data)  {

      cmdWrite (reg);
      dataWrite (data);

    }


    uint8_t readReg (uint8_t reg) {

      cmdWrite(reg);
      return dataRead();

    }


    void Check_Busy_Draw () {
      uint8_t temp;
      do {
        temp = readStatus ();        // Get status byte
        // delayMicroseconds (20);      // Small delay to prevent bus overload
      } while (temp & 0x08);         // Wait until Bit 3 clears (DRAW engine idle)
    }


    //** Display control
  public:
    void init_LT7683 () {

      for (uint16_t i = 0; i < sizeof (initCode) / sizeof (initCode[0]); i++) {
        uint8_t reg  = pgm_read_byte (&initCode[i][0]);
        uint8_t val  = pgm_read_byte (&initCode[i][1]);
        writeReg(reg, val);
        delay(10);
      }

      Graphic_Mode ();
      Display_ON   ();          // affects R.0x12 B.7

    }


    void Graphic_Mode () {

      uint8_t temp;
      temp = readReg (0x03);
      temp &= 0xFB;  //
      writeReg (0x03, temp);    //

    }


    void Text_Mode () {

      uint8_t temp;
      temp = readReg (0x03);
      temp |= 0x04;  //
      writeReg (0x03, temp);    //

    }


    void textSize (uint8_t height)  {

      if ((height < 1) || (height > 3)) return;

      switch (height)  {

        case 1 : writeReg (0xCC, 0x00); break;
        case 2 : writeReg (0xCC, 0x10); break;
        case 3 : writeReg (0xCC, 0x20); break;

      }

    }


    void Display_ON (void) {

      uint8_t temp;
      temp = readReg (0x12);
      temp |= 0x40;
      writeReg (0x12, temp);

    }

  
    void setFrameAdr (uint16_t x_start, uint16_t y_start, uint16_t x_end, uint16_t y_end)  {

      // start
      writeReg (0x68, x_start);         //
      writeReg (0x69, x_start >> 8);    //
      writeReg (0x6A, y_start);         //
      writeReg (0x6B, y_start >> 8);    //

      // end
      writeReg (0x6C, x_end);         //
      writeReg (0x6D, x_end >> 8);    //
      writeReg (0x6E, y_end);         //
      writeReg (0x6F, y_end >> 8);    //

    }

    void line (uint16_t x_start, uint16_t y_start, uint16_t x_end, uint16_t y_end)  {

      setFrameAdr (x_start, y_start, x_end, y_end);
      writeReg (0x67, 0x80);    // line
      Check_Busy_Draw();

    }


    void rounded_rect (uint16_t x_start, uint16_t y_start, uint16_t x_end, uint16_t y_end, uint8_t radius)  {

      setFrameAdr (x_start, y_start, x_end, y_end);
      writeReg (0x77, radius);
      writeReg (0x79, radius);
      writeReg (0x76, 0xB0);            // rounded rectangle
      Check_Busy_Draw();

    }


    void Fill_rounded_rect (uint16_t x_start, uint16_t y_start, uint16_t x_end, uint16_t y_end, uint8_t radius)  {

      setFrameAdr (x_start, y_start, x_end, y_end);
      writeReg (0x77, radius);
      writeReg (0x79, radius);
      writeReg (0x76, 0xF0);            // rounded rectangle
      Check_Busy_Draw();

    }


    void Fill_rect (uint16_t x_start, uint16_t y_start, uint16_t x_end, uint16_t y_end)  {

      setFrameAdr (x_start, y_start, x_end, y_end);
      writeReg (0x76, 0xE0);    // filled square
      Check_Busy_Draw();

    }

    void Fill_ellipse (uint16_t x, uint16_t y, uint16_t radius1, uint16_t radius2)  {

      //** circle center
      writeReg (0x7B, x);               //
      writeReg (0x7C, x >> 8);          //
      writeReg (0x7D, y);               //
      writeReg (0x7E, y >> 8);          //

      //** radii
      writeReg (0x77, radius1);         //
      writeReg (0x78, radius1 >> 8);    //
      writeReg (0x79, radius2);         //
      writeReg (0x7A, radius2 >> 8);    //

      // draw Ellipse
      writeReg (0x76, 0xC0);    // filled ellipse
      Check_Busy_Draw();

    }


    void Fill_circle (uint16_t x, uint16_t y, uint16_t radius)  {

      Fill_ellipse (x, y, radius, radius);

    }


    void Fill_triangle (uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t x3, uint16_t y3)  {

      //** point1
      writeReg (0x68, x1);         //
      writeReg (0x69, x1 >> 8);    //
      writeReg (0x6A, y1);         //
      writeReg (0x6B, y1 >> 8);    //

      //** point2
      writeReg (0x6C, x2);         //
      writeReg (0x6D, x2 >> 8);    //
      writeReg (0x6E, y2);         //
      writeReg (0x6F, y2 >> 8);    //

      //** point3
      writeReg (0x70, x3);         //
      writeReg (0x71, x3 >> 8);    //
      writeReg (0x72, y3);         //
      writeReg (0x73, y3 >> 8);    //

      //** draw
      writeReg (0x67, 0xA2);    // filled triangle
      Check_Busy_Draw();

    }

    void Fill_arc (uint16_t x, uint16_t y, uint16_t radius1, uint16_t radius2, uint8_t select)  {

      //** center
      writeReg (0x7B, x);         //
      writeReg (0x7C, x >> 8);    //
      writeReg (0x7D, y);         //
      writeReg (0x7E, y >> 8);    //

      //** radii
      writeReg (0x77, radius2);         //
      writeReg (0x78, radius2 >> 8);    //
      writeReg (0x79, radius1);         //
      writeReg (0x7A, radius1 >> 8);    //

      writeReg (0x76, 0xD0 | select);   // filled arch
      Check_Busy_Draw();

    }


    void goto_xy (uint16_t x, uint16_t y) {

      xBase = x;
      yBase = y;
      writeReg (0x63, x);         //
      writeReg (0x64, x >> 8);    //
      writeReg (0x65, y);         //
      writeReg (0x66, y >> 8);    //

    }


    void PlotChar (uint16_t x, uint16_t y, char c)   {

      goto_xy (x, y);
      Text_Mode();
      cmdWrite (0x04);
      dataWrite (c);
      Graphic_Mode ();

    }


    // we inherit from Print
    size_t write (uint8_t c) override {
      if (c == '\n') {
        // Handle newline: move to the next line
        xBase = 0; // Reset xBase to the start of the line
        yBase += 30; // Move down by the font height
        return 1;
      } else if (c == '\r') {
        // Ignore carriage return
        return 1;
      }

      // Ensure character is printable
      if (c < 32 || c > 254) return 0;    // need to adapt for Umlaute

      //** Plot the character using the Plotchar method
      PlotChar (xBase, yBase, c);

      //** Update xBase for the next character
      xBase += 20; // Move by the character width (16 for Arial 16x24)
      if (xBase >= 1024) { // Handle line wrapping
        xBase = 0;
        yBase += 30; // Move to the next line
      }
      return 1; // Indicate success

    }


    void Plot7Seg (uint16_t x, uint16_t y, uint8_t c)  {

      switch (c)  {

        case 0:
          //** build # 0 in 7-Seg
          Fill_rect (x + (2 * sc), y, x + (10 * sc), y + (2 * sc));               // top horizontal
          Fill_arc  (x + (10 * sc), y + (2 * sc), 2 * sc, 2 * sc, 2);             // top right arc
          Fill_rect (x + (10 * sc), y + (2 * sc), x + (12 * sc), y + (22 * sc));  // right vertical
          Fill_arc  (x + (10 * sc), y + (22 * sc), 2 * sc, 2 * sc, 3);            // bottom right arc
          Fill_rect (x + (2 * sc), y + (22 * sc), x + (10 * sc), y + (24 * sc));  // bottom hor
          Fill_arc  (x + (2 * sc), y + (22 * sc), 2 * sc, 2 * sc, 0);             // bottom left arc
          Fill_rect (x, y + (2 * sc), x + (2 * sc), y + (22 * sc));               // lower left vertical
          Fill_arc  (x + (2 * sc), y + (2 * sc), 2 * sc, 2 * sc, 1);              // top left arc
          break;

        case 1:
          //** build # 1 in 7-Seg
          Fill_circle (x + (11 * sc), y + sc, sc);                                // top right tip
          Fill_rect (x + (10 * sc), y + sc, x + (12 * sc), y + (23 * sc));        // right vert
          Fill_circle (x + (11 * sc), y + (23 * sc), sc);                         // bottom right tip
          break;

        case 2:
          //** build # 2 in 7-Seg
          Fill_rect (x + sc, y, x + (10 * sc), y + (2 * sc));                     // top hor
          Fill_circle (x + sc, y + sc, sc);                                       // top left tip
          Fill_arc  (x + (10 * sc), y + (2 * sc), 2 * sc, 2 * sc, 2);             // top right arc
          Fill_rect (x + (10 * sc), y + (2 * sc), x + (12 * sc), y + (11 * sc));  // top right vert
          Fill_arc  (x + (10 * sc), y + (11 * sc), 2 * sc, 2 * sc, 3);            // center right arc
          Fill_rect (x + (2 * sc), y + (11 * sc), x + (10 * sc), y + (13 * sc));  // center vert
          Fill_arc  (x + (2 * sc), y + (13 * sc), 2 * sc, 2 * sc, 1);             // center left arc
          Fill_rect (x, y + (13 * sc), x + (2 * sc), y + (22 * sc));              // left bottom vert
          Fill_arc  (x + (2 * sc), y + (22 * sc), 2 * sc, 2 * sc, 0);             // left bottom arc
          Fill_rect (x + (2 * sc), y + (22 * sc), x + (11 * sc), y + (24 * sc));  // bottom hor
          Fill_circle (x + (11 * sc), y + (23 * sc), sc);                         // bottom right tip
          break;

        case 3:
          //** build # 3 in 7-Seg
          Fill_circle (x + sc, y + sc, sc);                                      // top left tip
          Fill_rect (x + sc, y, x + (10 * sc), y + (2 * sc));                    // top hor
          Fill_arc  (x + (10 * sc), y + (2 * sc), 2 * sc, 2 * sc, 2);            // right top arc
          Fill_rect (x + (10 * sc), y + (2 * sc), x + (12 * sc), y + (22 * sc)); // right vertical
          Fill_rect (x + (3 * sc), y + (11 * sc), x + (10 * sc), y + (13 * sc)); // center hor
          Fill_circle (x + (3 * sc), y + (12 * sc), sc);                         // center left tip
          Fill_arc  (x + (10 * sc), y + (22 * sc), 2 * sc, 2 * sc, 3);           // bottom right arc
          Fill_rect (x + sc, y + (22 * sc), x + (10 * sc), y + (24 * sc));       // bottom hor
          Fill_circle (x + sc, y + (23 * sc), sc);                               // bottom left tip
          break;

        case 4:
          //** build # 4 in 7-Seg
          Fill_rect (x + (10 * sc), y + (4 * sc), x + (12 * sc), y + (12 * sc));  // right vertical
          Fill_circle (x + (11 * sc), y + (4 * sc), sc);                          // top right tip
          Fill_rect (x + (10 * sc), y + (12 * sc), x + (12 * sc), y + (23 * sc)); // right vertical
          Fill_circle (x + (11 * sc), y + (23 * sc), sc);                         // bottom right tip
          Fill_rect (x + (2 * sc), y + (11 * sc), x + (10 * sc), y + (13 * sc));  // center horizontal
          Fill_rect (x, y + sc, x + (2 * sc), y + (11 * sc));                     // left top vertical
          Fill_arc  (x + (2 * sc), y + (11 * sc), 2 * sc, 2 * sc, 0);             // left center arc
          Fill_circle (x + sc, y + sc, sc);                                       // top left tip
          break;

        case 5:
          //** build # 5 in 7-Seg
          Fill_rect (x + (2 * sc), y, x + (11 * sc), y + (2 * sc));               // top hor
          Fill_circle (x + (11 * sc), y + sc, sc);                                // top right tip
          Fill_arc (x + (2 * sc), y + (2 * sc), 2 * sc, 2 * sc, 1);               // top left arc
          Fill_rect (x, y + (2 * sc), x + (2 * sc), y + (11 * sc));               // upper left vertical
          Fill_arc (x + (2 * sc) , y + (11 * sc), 2 * sc, 2 * sc, 0);             // center left arc
          Fill_rect (x + (2 * sc), y + (11 * sc), x + (10 * sc), y + (13 * sc));  // center hor
          Fill_arc (x + (10 * sc), y + (13 * sc), 2 * sc, 2 * sc, 2);             // center right arc
          Fill_arc (x + (10 * sc), y + (22 * sc), 2 * sc, 2 * sc, 3);             // bottom right arc
          Fill_rect (x + (10 * sc), y + (13 * sc), x + (12 * sc), y + (22 * sc)); // lower right vertical
          Fill_rect (x + (2 * sc), y + (22 * sc), x + (10 * sc), y + (24 * sc));  // bottom hor
          Fill_circle (x + (2 * sc), y + (23 * sc), sc);                          // bottom left tip
          break;

        case 6:
          //** build # 6 in 7-Seg
          Fill_rect (x + (2 * sc), y, x + (8 * sc), y + (2 * sc));                // top hor
          Fill_circle (x + (8 * sc), y + sc, sc);                                 // top right tip
          Fill_arc  (x + (2 * sc), y + (2 * sc), 2 * sc, 2 * sc, 1);              // top left arc
          Fill_rect (x, y + (2 * sc), x + (2 * sc), y + (22 * sc));               // left vertical
          Fill_arc  (x + (2 * sc), y + (11 * sc), 2 * sc, 2 * sc, 0);             // center right arc
          Fill_rect (x + (2 * sc), y + (11 * sc), x + (10 * sc), y + (13 * sc));  // center hor
          Fill_arc  (x + (10 * sc), y + (13 * sc), 2 * sc, 2 * sc, 2);            // left center arc
          Fill_arc  (x + (10 * sc), y + (22 * sc), 2 * sc, 2 * sc, 3);            // bottom right arc
          Fill_rect (x + (10 * sc), y + (13 * sc), x + (12 * sc), y + (22 * sc)); // lower right vertical
          Fill_rect (x + (2 * sc), y + (22 * sc), x + (10 * sc), y + (24 * sc));  // bottom hor
          Fill_arc  (x + (2 * sc), y + (22 * sc), 2 * sc, 2 * sc, 0);             // bottom left arc
          break;

        case 7:
          //** build # 7 in 7-Seg
          Fill_circle (x + (2 * sc), y + sc, sc);                                // top tip
          Fill_rect (x + (2 * sc), y, x + (10 * sc), y + (2 * sc));              // top hor
          Fill_arc (x + (10 * sc), y + (2 * sc), 2 * sc, 2 * sc, 2);             // top right arc
          Fill_rect (x + (10 * sc), y + (2 * sc), x + (12 * sc), y + (23 * sc)); // right vert
          Fill_circle (x + (11 * sc), y + (23 * sc), sc);                        // bottom tip
          break;

        case 8:
          //** build # 8 in 7-Seg
          Fill_rect (x + (2 * sc), y, x + (10 * sc), y + (2 * sc));              // top horizontal
          Fill_arc  (x + (10 * sc), y + (2 * sc), 2 * sc, 2 * sc, 2);            // top right arc
          Fill_rect (x + (10 * sc), y + (2 * sc), x + (12 * sc), y + (22 * sc)); // right vertical
          Fill_arc  (x + (10 * sc), y + (22 * sc), 2 * sc, 2 * sc, 3);           // bottom right arc
          Fill_rect (x + (2 * sc), y + (22 * sc), x + (10 * sc), y + (24 * sc)); // bottom hor
          Fill_arc  (x + (2 * sc), y + (22 * sc), 2 * sc, 2 * sc, 0);            // bottom left arc
          Fill_rect (x, y + (2 * sc), x + (2 * sc), y + (22 * sc));              // left vertical
          Fill_arc  (x + (2 * sc), y + (2 * sc), 2 * sc, 2 * sc, 1);             // top left arc
          Fill_rect (x + (2 * sc), y + (11 * sc), x + (10 * sc), y + (13 * sc)); // center hor
          break;

        case 9:
          //** build 9 in 7-Seg
          Fill_rect (x + (2 * sc), y, x + (10 * sc), y + (2 * sc));              // top horizontal
          Fill_arc  (x + (10 * sc), y + (2 * sc), 2 * sc, 2 * sc, 2);            // top right arc
          Fill_rect (x + (10 * sc), y + (2 * sc), x + (12 * sc), y + (22 * sc)); // right vertical
          Fill_arc  (x + (10 * sc), y + (22 * sc), 2 * sc, 2 * sc, 3);           // bottom right arc
          Fill_rect (x + (4 * sc), y + (22 * sc), x + (10 * sc), y + (24 * sc)); // bottom hor
          Fill_circle (x + (4 * sc), y + (23 * sc), sc);                         // bottom left tip
          Fill_arc  (x + (2 * sc), y + (11 * sc), 2 * sc, 2 * sc, 0);            // center left arc
          Fill_rect (x, y + (2 * sc), x + (2 * sc), y + (11 * sc));              // left vertical
          Fill_arc  (x + (2 * sc), y + (2 * sc), 2 * sc, 2 * sc, 1);             // top left arc
          Fill_rect (x + (2 * sc), y + (11 * sc), x + (10 * sc), y + (13 * sc)); // center hor
          break;

      }
    }


    void clearScreen ()  {

      fore (0x00, 0x00, 0x00);
      Fill_rect (0, 0, 1023, 599);

    }

    void fore (uint8_t red, uint8_t green, uint8_t blue) {

      writeReg (0xD2, red);      //
      writeReg (0xD3, green);    //
      writeReg (0xD4, blue);     //

    }

    void back (uint8_t red, uint8_t green, uint8_t blue) {

      writeReg (0xD5, red);      //
      writeReg (0xD6, green);    //
      writeReg (0xD7, blue);     //

    }
};


//** Display Controller constructor
ToS_LT7683 tft;



// =================================================
// +++++++++  Trigonometric Computation  +++++++++++
// =================================================

xy trig (int radius, int alpha)  {

  if (alpha < 0) alpha += 360;
  alpha = alpha % 360;
  xy Pos;

  // compute absolute x, y - Position within center of 1024 x 600 pixel Frame

  if ((alpha >= 0) && (alpha < 90))  {
    Pos.x = 512 + radius * sin ( radians ( alpha ));        // inversion of cos <> sin (x <> y) is ok because of Symmetry
    Pos.y = 300 - radius * cos ( radians ( alpha ));

  }

  if ((alpha >= 90) && (alpha < 180))  {
    Pos.x = 512 + radius * cos ( radians ( alpha - 90 ));
    Pos.y = 300 + radius * sin ( radians ( alpha - 90 ));
  }

  if (( alpha >= 180) && ( alpha < 270))  {
    Pos.x = 512 - radius * cos ( radians ( 90 - ( alpha % 90 )));
    Pos.y = 300 + radius * sin ( radians ( 90 - ( alpha % 90 )));
  }

  if (( alpha >= 270) && (alpha <= 360))  {
    Pos.x = 512 - radius * cos ( radians ( alpha % 90 ));    // inversion of cos <> sin is ok because of Symmetry
    Pos.y = 300 - radius * sin ( radians ( alpha % 90 ));

  }

  return Pos;

}


void ShowPointer (uint16_t alpha)  {

  // tip
  xy tip = trig (159, alpha);
  // left
  xy left = trig (133, (alpha - 4));
  // right
  xy right = trig (133, (alpha + 4));
  // draw
  tft.Fill_triangle (left.x, left.y, tip.x, tip.y, right.x, right.y);

}


//** Arduino Demo Program
void setup () {

  I2C.init (0x7E);
  LED.flash (4);
 
  tft.init_LT7683();
  LED.flash (3);
 
  tft.clearScreen ();

  //** outer Frame
  tft.fore (0xFF, 0xFF, 0x00);      // ylw
  tft.rounded_rect (4, 5, 1019, 595, 10);

  tft.fore (0x00, 0x0A, 0xD9);
  tft.Fill_rounded_rect (5, 6, 1018, 594, 10);

  tft.fore (0x21, 0x00, 0xAA);
  tft.Fill_circle (512, 300, 176);

  tft.fore (0x21, 0x00, 0xCC);
  tft.Fill_circle (512, 300, 160);

  tft.fore (0x00, 0x00, 0x88);
  tft.Fill_circle (512, 300, 130);

  tft.fore (0xFF, 0xFF, 0x00);
  tft.back (0x00, 0x0A, 0xD9);
  
  tft.goto_xy (136, 124);
  tft.print (F("Hr"));
  tft.goto_xy (852, 124);
  tft.print (F("Min"));

  //** bottom line
  tft.fore (0x00, 0x00, 0x00);
  // delay (444);
  tft.goto_xy (314, 546);
  tft.print ("ToSStudio - Software");

  tft.fore (0xFF, 0xFF, 0x00);
  /*
    //** hour marks
    for (int alpha = 0; alpha <= 360; alpha += 30)  {

    xy Pos1 = trig (162, alpha);
    xy Pos2 = (alpha == 0 || alpha == 90 || alpha == 180 || alpha == 270) ? trig (182, alpha) : trig (176, alpha);
    tft.line (Pos1.x, Pos1.y, Pos2.x, Pos2.y);

    }
  */

  //** emphasize cardinal hr marks
  //** 12 hr
  tft.line (511, 138, 511, 118);
  tft.line (513, 138, 513, 118);

  //** 3 hr
  tft.line (674, 299, 694, 299);
  tft.line (674, 301, 694, 301);

  //** 6 hr
  tft.line (511, 462, 511, 482);
  tft.line (513, 462, 513, 482);

  //** 9 hr
  tft.line (350, 299, 330, 299);
  tft.line (350, 301, 330, 301);

  tft.textSize (1);
  tft.PlotChar (503, 92,  '1');
  tft.PlotChar (514, 92,  '2');
  tft.PlotChar (708, 293, '3');
  tft.PlotChar (509, 496, '6');
  tft.PlotChar (310, 293, '9');

  tft.rounded_rect (290, 76,  740,  526, 25);
  tft.rounded_rect (20,  76,  280,  526, 25);
  tft.rounded_rect (750, 76,  1005, 526, 25);
  tft.rounded_rect (440, 235, 585,  360, 10);

  tft.fore (0x00, 0x00, 0x66);

  tft.Fill_rounded_rect (20,  16,  280,  65,  15);
  tft.Fill_rounded_rect (20,  540, 280,  582, 15);
  tft.Fill_rounded_rect (750, 20,  1005, 58,  15);
  tft.Fill_rounded_rect (750, 540, 1005, 582, 15);

  tft.fore (0xFF, 0xFF, 0x00);
  tft.back (0x00, 0x00, 0x66);
  tft.textSize (3);
  tft.goto_xy (63, 24);
  tft.print ("ATTINY 85");
  tft.goto_xy (814, 24);
  tft.print ("LT 7683");
  tft.textSize (2);
  tft.goto_xy (36, 548);
  tft.print ("17 Aug. 2025");
  tft.goto_xy (766, 548);
  tft.print (F("CPU:"));
  tft.textSize (1);
  tft.goto_xy (856, 544);
  tft.print ("Tmp:");
  tft.PlotChar (932, 544, '2');
  tft.PlotChar (942, 544, '1');
  tft.PlotChar (952, 544, 0xB0);   // °-sign
  tft.PlotChar (962, 544, 'C');
  tft.goto_xy  (856, 565);
  tft.print ("Vcc:");
  tft.PlotChar (932, 565, '3');
  tft.PlotChar (942, 565, '.');
  tft.PlotChar (950, 565, '3');
  tft.PlotChar (962, 565, 'V');

  sc = 8;
  tft.Plot7Seg (38,  208, (hrs / 10));
  tft.Plot7Seg (162, 208, (hrs % 10));

  tft.Plot7Seg (766, 208, (mins / 10));
  tft.Plot7Seg (888, 208, (mins % 10));
  sc = 4;

  secs += 55;             // typical time it takes to upload in Seconds
  if (secs > 60)  {
    secs -= 60;
  }

}


void loop() {

  static uint32_t mil_last_secs;

  if ((millis() - mil_last_secs) >= 1000)  {

    mil_last_secs = millis ();

    //** remove previous pointer
    tft.fore (0x21, 0x00, 0xCC);
    ShowPointer (secs * 6);

    if (secs++ >= 59)  {
      secs = 0;
      mins ++;
      sc = 8;
      tft.fore (0x00, 0x0A, 0xD9);
      tft.Fill_rect (766, 208, 988, 400);
      tft.fore (0xFF, 0xFF, 0x00);
      tft.Plot7Seg (766, 208, (mins / 10));
      tft.Plot7Seg (888, 208, (mins % 10));
      if (mins >= 59)  {
        mins = 0;
        hrs ++;
        if (hrs >= 23)  {
          hrs = 0;
        }
        tft.fore (0x00, 0x0A, 0xD9);
        tft.Fill_rect (38, 208, 260, 400);
        tft.fore (0xFF, 0xFF, 0x00);
        tft.Plot7Seg (38,  208, (hrs / 10));
        tft.Plot7Seg (162, 208, (hrs % 10));

      }

      sc = 4;
    }

    // show new Pointer and display seconds in 7_Seg
    tft.fore (0xFF, 0xFF, 0x00);
    ShowPointer (secs * 6);

    tft.fore (0x00, 0x00, 0x88);
    tft.Fill_rect (455, 249, 568, 345);
    tft.fore (0xFF, 0xFF, 0x00);
    tft.Plot7Seg (455, 249, (secs / 10));
    tft.Plot7Seg (520, 249, (secs % 10));
    LED.flash (1);
  }

  delay (10);    // avoid overheating the µC

}


