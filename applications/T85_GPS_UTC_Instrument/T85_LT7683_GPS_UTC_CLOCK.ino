// T.85 with TFT 1024 x 600 (LT7683 / RA8876) using barebone I2C (Technoblogy)
// Display: ER-TFT101B4-1-5553 available from BUYDISPLAY / Eastrising
// connections: SDA PB0 = Pin7 (Display)
//              SCL PB2 = Pin8 (Display)
//              Blue LED = PB4
// connect the power supply of the Display to GND & 5 Volt.

// This is an app which will display GPS-utc-time derived from RMC & GGA-sentences on a 10" TFT

// 7 Segments de-coder   /*0*/  /*1*/ /*2*/ /*3*/ /*4*/ /*5*/ /*6*/ /*7*/ /*8*/ /*9*/ /*A*/ /*B*/ /*C*/ /*D*/ /*E*/ /*F*/ /*spc*/ /*-*/  /*.*/
const uint8_t digits[] = { 0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f, 0x77, 0x7c, 0x39, 0x5E, 0x79, 0x71, 0x00, 0x40, 0x80 };

// Labels on top of TFT:
static const char lbl1[] PROGMEM = "ATtiny85 - GPS";
static const char lbl2[] PROGMEM = "UTC-clock";

// Labels on bottom:
static const char sPWR[] PROGMEM = "Power";
static const char sGPS[] PROGMEM = "GPS";
static const char sUTC[] PROGMEM = "TIME";
static const char sFIX[] PROGMEM = "FIX";
static const char sHDOP[] PROGMEM = "HDOP";
static const char sALT[] PROGMEM = "ALT";
static const char sSAT[] PROGMEM = "SATS";
static const char sLAT[] PROGMEM = "Lat";
static const char sLON[] PROGMEM = "Lon";
static const char sMTR[] PROGMEM = "Mtr";

// Days of week
const char dow3[] PROGMEM = "SUNMONTUEWEDTHUFRISAT";

// Date format selection
// 0 = DD.MM.YYYY (Europe, default)
// 1 = MM/DD/YYYY (USA)
#define DATE_FORMAT_US 0

// Dash strings for showDate() in PROGMEM (fixed length, no terminator needed)
#if DATE_FORMAT_US == 0
static const char dashStr[] PROGMEM = "--- --.--.---- ";  // note trailing space to overwrite leftovers
#else
static const char dashStr[] PROGMEM = "--- --/--/---- ";
#endif

// watchdog-time-out in loop in case the GPS-module is not connected:
constexpr uint32_t GPS_IDLE_MAX = 600000UL;  // tune experimentally
static uint32_t gps_idle = 0;                // watchdog-timer in case the connection to the GPS-module is lost

// Globals for RGB (fore & back)
const uint8_t rf = 0xFF;
const uint8_t gf = 180;
const uint8_t bf = 40;  // ylw

const uint8_t rb = 20;
const uint8_t gb = 24;
const uint8_t bb = 28;  // 0x0F, 0x88, 0xCF ... kinda magenta ... was 0x12, 0x15, 0xA6

// GPS state machine
enum gps_state_t : uint8_t {
  GPS_NO_DATA = 0,
  GPS_TIME_ONLY,
  GPS_FIX
};

volatile gps_state_t gps_state = GPS_NO_DATA;

// Global Variables for the NMEA-decoder
volatile uint8_t utc_hh = 0;
volatile uint8_t utc_mm = 0;
volatile uint8_t utc_ss = 0;

volatile uint8_t gga_sats;       // 0..99 typical
volatile uint16_t gga_hdop_x10;  // hdop * 10
volatile int16_t gga_alt_m;      // integer meters (rounded down, decimals ignored)
volatile uint8_t gga_new;        // optional "fresh GGA" flag

volatile uint8_t lat_deg = 0;         // 0..90
volatile uint16_t lat_min_x1000 = 0;  // minutes * 1000 (MM.MMM)
volatile char lat_hemi = 'N';         // 'N' or 'S'

volatile uint8_t lon_deg = 0;         // 0..180 (uint8_t also ok, but uint16_t is fine)
volatile uint16_t lon_min_x1000 = 0;  // minutes * 1000
volatile char lon_hemi = 'E';         // 'E' or 'W'

volatile uint8_t utc_dd = 0;
volatile uint8_t utc_mo = 0;
volatile uint8_t utc_yy = 0;  // 00..99

// Text-position
uint16_t xBase = 0;
uint16_t yBase = 0;

// volatiles for the bitbang UART-Receiver
volatile uint8_t bitcount = 0;
volatile uint8_t rxByte = 0;
volatile bool rxReady = false;

volatile uint8_t toc = 0;
volatile bool burst_is_done = false;

// 7-Seg scaling factor (Range: 1 -- 22)
uint8_t sc = 10;

#define DIG_W (14 * sc)  // make this 7 and double in show_utc()

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
  { 0x25, 0x04 },  // L1_START1
  { 0x26, 0x00 },  // L1_START2
  { 0x27, 0x00 },  // L1_START3
  { 0x28, 0x00 },  // L2_START0
  { 0x29, 0x00 },  // L2_START1

  // --- PIP Address Setup (if used) ---
  { 0x50, 0x00 },  // PIP1_ADDR0
  { 0x51, 0x00 },
  { 0x52, 0x00 },
  { 0x53, 0x00 },
  { 0x54, 0x00 },
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



// ================================
// ****** Class for the LED *******
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
      delay_ms_busy(85);
      // delay(85);
    }
  }

private:
  int m_pin;

  static inline void delay_ms_busy(uint8_t ms) {
    while (ms--) {
      // ~1 ms delay @ 8 MHz
      for (uint16_t i = 0; i < 800; i++) {
        __asm__ __volatile__("nop");
      }
    }
  }

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
class ToS_LT7683 {
private:

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

  void dataWrite2(uint8_t b0, uint8_t b1) {
    I2C.start(I2C_ADDR_DATA, 0);
    I2C.write(b0);
    I2C.write(b1);
    I2C.stop();
  }

  void dataWriteBurst(const uint8_t *buf, uint8_t n) {
    I2C.start(I2C_ADDR_DATA, 0);
    for (uint8_t i = 0; i < n; i++) I2C.write(buf[i]);
    I2C.stop();
  }

  void writeRegs(uint8_t firstReg, const uint8_t *buf, uint8_t n) {
    cmdWrite(firstReg);      // select start register
    dataWriteBurst(buf, n);  // writes firstReg, firstReg+1, ... sequentially
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
      temp = readStatus();   // Get status byte
      delayMicroseconds(4);  // Small delay to prevent bus overload
    } while (temp & 0x08);   // Wait until Bit 3 clears (DRAW engine idle)
  }

  static inline uint16_t RGB565(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint16_t)(r & 0xF8) << 8) | ((uint16_t)(g & 0xFC) << 3) | (b >> 3);
  }

  // used in 7-Seg Font to draw repetetive patterns of the segments
  inline void drawH(uint16_t xL, uint16_t xR, uint16_t yT, uint16_t sc) {
    uint16_t yB = yT + 2 * sc;
    Fill_triangle(xL - sc, yT + sc, xL, yT, xL, yB);
    Fill_rect(xL, yT, xR, yB);
    Fill_triangle(xR, yT, xR + sc, yT + sc, xR, yB);
  }

  inline void drawV(uint16_t xL, uint16_t yT, uint16_t yB, uint16_t sc) {
    uint16_t xR = xL + 2 * sc;
    Fill_triangle(xL, yT, xL + sc, yT - sc, xR, yT);
    Fill_rect(xL, yT, xR, yB);
    Fill_triangle(xL, yB, xL + sc, yB + sc, xR, yB);
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

  void textSize(uint8_t size) {
    if ((size < 1) || (size > 3)) return;

    switch (size) {

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

  void setCursorXY(uint16_t x, uint16_t y) {

    // Write X low (0x5F)
    I2C.start(I2C_ADDR_CMD, 0);
    I2C.write(0x5F);
    I2C.restart(I2C_ADDR_DATA, 0);
    I2C.write((uint8_t)(x & 0xFF));

    // Write X high (0x60)
    I2C.restart(I2C_ADDR_CMD, 0);
    I2C.write(0x60);
    I2C.restart(I2C_ADDR_DATA, 0);
    I2C.write((uint8_t)(x >> 8));

    // Write Y low (0x61)
    I2C.restart(I2C_ADDR_CMD, 0);
    I2C.write(0x61);
    I2C.restart(I2C_ADDR_DATA, 0);
    I2C.write((uint8_t)(y & 0xFF));

    // Write Y high (0x62)
    I2C.restart(I2C_ADDR_CMD, 0);
    I2C.write(0x62);
    I2C.restart(I2C_ADDR_DATA, 0);
    I2C.write((uint8_t)(y >> 8));

    I2C.stop();
  }

  void putPixel565(uint16_t x, uint16_t y, uint16_t clr565) {

    // Cursor X/Y using restarts (no STOP)
    I2C.start(I2C_ADDR_CMD, 0);
    I2C.write(0x5F);
    I2C.restart(I2C_ADDR_DATA, 0);
    I2C.write((uint8_t)(x & 0xFF));

    I2C.restart(I2C_ADDR_CMD, 0);
    I2C.write(0x60);
    I2C.restart(I2C_ADDR_DATA, 0);
    I2C.write((uint8_t)(x >> 8));

    I2C.restart(I2C_ADDR_CMD, 0);
    I2C.write(0x61);
    I2C.restart(I2C_ADDR_DATA, 0);
    I2C.write((uint8_t)(y & 0xFF));

    I2C.restart(I2C_ADDR_CMD, 0);
    I2C.write(0x62);
    I2C.restart(I2C_ADDR_DATA, 0);
    I2C.write((uint8_t)(y >> 8));

    // GRAM write command
    I2C.restart(I2C_ADDR_CMD, 0);
    I2C.write(0x04);

    // Pixel data (2 bytes) in same transaction
    I2C.restart(I2C_ADDR_DATA, 0);
    I2C.write((uint8_t)(clr565 & 0xFF));
    I2C.write((uint8_t)(clr565 >> 8));

    I2C.stop();
  }

  void putPixel(uint16_t x, uint16_t y, uint8_t r, uint8_t g, uint8_t b) {
    putPixel565(x, y, RGB565(r, g, b));
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

  void goto_xy(uint16_t x, uint16_t y) {
    // xBase = x;
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

  // 1) Enter/exit text write mode once per batch
  inline void TextBegin() {
    Text_Mode();
    cmdWrite(0x04);  // your "write char" command
  }

  inline void TextEnd() {
    Graphic_Mode();
  }

  // 2) Send one character (assumes controller auto-advances cursor in text mode)
  inline void Putc(char c) {
    dataWrite((uint8_t)c);
  }

  // 3) Print exactly N chars from PROGMEM (no terminator)
  inline void PrintP_N(const char *p, uint8_t n) {
    TextBegin();
    while (n--) {
      Putc((char)pgm_read_byte(p++));
    }
    TextEnd();
  }

  void plot_digit(char d) {
    PlotChar(xBase, yBase, d);
    xBase += 20;
  }

  void PutC(char c) {
    PlotChar(xBase, yBase, c);
    xBase += 20;
  }

  void put_u8(uint8_t v) {  // prints 0..255 without leading zeros
    if (v >= 100) {
      PutC('0' + (v / 100));
      v %= 100;
      PutC('0' + (v / 10));
      PutC('0' + (v % 10));
    } else if (v >= 10) {
      PutC('0' + (v / 10));
      PutC('0' + (v % 10));
    } else {
      PutC('0' + v);
    }
  }

  void put4(uint16_t v) {  // prints 0000..9999
    PutC('0' + (v / 1000));
    v %= 1000;
    PutC('0' + (v / 100));
    v %= 100;
    PutC('0' + (v / 10));
    PutC('0' + (v % 10));
  }

  void put2(uint8_t v) {  // always prints 00..99
    PutC('0' + (v / 10));
    PutC('0' + (v % 10));
  }

  // scaleable 7-Seg numbers made of rectangles and triangles
  void Plot7Seg(uint16_t x, uint16_t y, uint8_t number) {  // progmem:
    // Clamp: unknown -> space (index 17)
    if (number >= (sizeof(digits) / sizeof(digits[0]))) number = 17;

    uint8_t seg = digits[number];

    // Precompute common coordinates (saves flash vs repeated (N*sc))
    uint16_t x0 = x;
    uint16_t x1 = x + 1 * sc;
    uint16_t x2 = x + 2 * sc;
    uint16_t x10 = x + 10 * sc;
    uint16_t x12 = x + 12 * sc;

    uint16_t y0 = y;
    uint16_t y1 = y + 1 * sc;
    uint16_t y2 = y + 2 * sc;
    uint16_t y11 = y + 11 * sc;
    uint16_t y12 = y + 12 * sc;
    uint16_t y13 = y + 13 * sc;
    uint16_t y22 = y + 22 * sc;
    uint16_t y23 = y + 23 * sc;
    uint16_t y24 = y + 24 * sc;

    // Segment 1: top horizontal (x2..x10, y0)
    if (seg & 0x01) fore(rf, gf, bf);
    else fore(rb, gb, bb);
    drawH(x2, x10, y0, sc);

    // Segment 2
    if (seg & 0x02) fore(rf, gf, bf);
    else fore(rb, gb, bb);
    drawV(x10, y2, y11, sc);

    // Segment 3: bottom right vertical (x10..x12, y13..y22)
    if (seg & 0x04) fore(rf, gf, bf);
    else fore(rb, gb, bb);
    drawV(x10, y13, y22, sc);

    // Segment 4: bottom horizontal (x2..x10, y22)
    if (seg & 0x08) fore(rf, gf, bf);
    else fore(rb, gb, bb);
    drawH(x2, x10, y22, sc);

    // Segment 5: bottom left vertical (x0..x2, y13..y22)
    if (seg & 0x10) fore(rf, gf, bf);
    else fore(rb, gb, bb);
    drawV(x0, y13, y22, sc);

    // Segment 6: top left vertical (x0..x2, y2..y11)
    if (seg & 0x20) fore(rf, gf, bf);
    else fore(rb, gb, bb);
    drawV(x0, y2, y11, sc);

    // Segment 7: middle horizontal (x2..x10, y11)
    if (seg & 0x40) fore(rf, gf, bf);
    else fore(rb, gb, bb);
    drawH(x2, x10, y11, sc);

    // Segment 8: decimal point (bit 7) as a filled circle bottom-right
    if (seg & 0x80) fore(rf, gf, bf);
    else fore(rb, gb, bb);

    // Segment 8: decimal point (bit 7) as a filled circle bottom-right
    if (seg & 0x80) {
      uint16_t dx = x12 - 1 * sc;
      uint16_t dy = y24 - 1 * sc;
      uint8_t dr = (sc > 1) ? (sc - 1) : 1;

      Fill_circle(dx, dy, dr);
    }
  }

  void drawColon(uint16_t x, uint16_t y, bool on) {

    // Compute dot centers
    uint16_t cx = x + 6 * sc;
    uint16_t cy1 = y + 8 * sc;
    uint16_t cy2 = y + 16 * sc;

    // Set color for ON/OFF
    if (on) fore(rf, gf, bf);  // yellow
    else fore(rb, gb, bb);     // background

    Fill_circle(cx, cy1, 10);
    Fill_circle(cx, cy2, 10);

    // Restore foreground color
    fore(rf, gf, bf);
  }

  void clearScreen() {

    fore(0x00, 0x00, 0x00);
    Fill_rect(0, 0, 1023, 599);
  }

  inline void fore(uint8_t red, uint8_t green, uint8_t blue) {  // was setDrawColor
    writeReg(0xD2, red);
    writeReg(0xD3, green);
    writeReg(0xD4, blue);
  }

  void back(uint8_t red, uint8_t green, uint8_t blue) {
    writeReg(0xD5, red);    //
    writeReg(0xD6, green);  //
    writeReg(0xD7, blue);   //
  }
};

//** Display Controller constructor
ToS_LT7683 tft;

// ==========================================

// helpers to disable IRQs while drawing to the TFT:
static inline void PCINT_off() {
  GIMSK &= ~_BV(PCIE);  // PCINT off
}

static inline void PCINT_on() {
  GIFR = _BV(PCIF);    // clear any pending PCINT
  GIMSK |= _BV(PCIE);  // PCINT on
}


// Decode the RMC-NMEA-sentence (robust, sentence-level commit)
volatile bool rmc_seen = false;  // set when header seen (optional debug)

static inline void nmeaDecode(uint8_t c) {

  enum { MSG_NONE = 0,
         MSG_RMC = 1,
         MSG_GGA = 2 };

  // header/router
  static uint8_t hs = 0;
  static uint8_t msg = MSG_NONE;

  // payload
  static uint8_t field = 0;
  static uint8_t in_checksum = 0;

  // -------- sentence-local results --------
  static uint8_t tmp_hh, tmp_mm, tmp_ss, tmp_time_digits, tmp_time_ok;

  // RMC
  static uint8_t tmp_fixA;
  static uint8_t tmp_dd, tmp_mo, tmp_yy, tmp_have_date;

  // RMC Lat/Lon
  static uint8_t tmp_lat_deg, tmp_lon_deg;
  static uint16_t tmp_lat_min_x1000, tmp_lon_min_x1000;
  static char tmp_lat_hemi, tmp_lon_hemi;

  // GGA
  static uint8_t tmp_fixq;
  static uint8_t tmp_sats, tmp_have_sats;
  static uint16_t tmp_hdop_x10;
  static uint8_t tmp_have_hdop;
  static int16_t tmp_alt_m;
  static uint8_t tmp_have_alt;

  // -------- per-field parsers --------
  // date ddmmyy
  static uint8_t date_digit, date_seen;
  static uint8_t dd_acc, mo_acc, yy_acc;

  // RMC lat: ddmm.mmmm -> keep mm + first 3 decimals
  static uint8_t lat_dig, lat_after_dot, lat_frac_dig;
  static uint8_t lat_deg_acc, lat_min_int;
  static uint16_t lat_frac3;

  // RMC lon: dddmm.mmmm -> keep mm + first 3 decimals
  static uint8_t lon_dig, lon_after_dot, lon_frac_dig;
  static uint16_t lon_deg_acc;
  static uint8_t lon_min_int;
  static uint16_t lon_frac3;

  // GGA sats/hdop
  static uint8_t sats_seen;
  static uint16_t sats_acc;
  static uint8_t hd_seen, num_after_dot;
  static uint16_t hd_ip;
  static uint8_t hd_fp;

  // GGA alt field 9 (integer meters, ignore decimals)
  static uint8_t alt_seen, alt_neg;
  static int16_t alt_acc;

  // -------- header/router --------
  switch (hs) {
    case 0: hs = (c == '$') ? 1 : 0; return;
    case 1: hs = (c == 'G') ? 2 : (c == '$' ? 1 : 0); return;
    case 2: hs = (c == 'P' || c == 'N') ? 3 : (c == '$' ? 1 : 0); return;

    case 3:
      msg = MSG_NONE;
      if (c == 'R') {
        msg = MSG_RMC;
        hs = 4;
        return;
      }
      if (c == 'G') {
        msg = MSG_GGA;
        hs = 4;
        return;
      }
      hs = (c == '$') ? 1 : 0;
      return;

    case 4:
      if (msg == MSG_RMC) {
        if (c == 'M') {
          hs = 5;
          return;
        }
      }
      if (msg == MSG_GGA) {
        if (c == 'G') {
          hs = 5;
          return;
        }
      }
      hs = (c == '$') ? 1 : 0;
      return;

    case 5:
      if (msg == MSG_RMC) {
        if (c == 'C') hs = 7;
        else hs = (c == '$') ? 1 : 0;
      }
      if (msg == MSG_GGA) {
        if (c == 'A') hs = 7;
        else hs = (c == '$') ? 1 : 0;
      }

      if (hs == 7) {
        // optional debug
        if (msg == MSG_RMC) rmc_seen = true;

        field = 0;
        in_checksum = 0;

        tmp_hh = tmp_mm = tmp_ss = 0;
        tmp_time_digits = 0;
        tmp_time_ok = 0;

        tmp_fixA = 0;
        tmp_dd = tmp_mo = tmp_yy = 0;
        tmp_have_date = 0;

        tmp_lat_deg = 0;
        tmp_lon_deg = 0;
        tmp_lat_min_x1000 = 0;
        tmp_lon_min_x1000 = 0;
        tmp_lat_hemi = 'N';
        tmp_lon_hemi = 'E';

        tmp_fixq = 0;
        tmp_sats = 0;
        tmp_have_sats = 0;
        tmp_hdop_x10 = 0;
        tmp_have_hdop = 0;
        tmp_alt_m = 0;
        tmp_have_alt = 0;

        // parsers reset
        date_digit = 0;
        date_seen = 0;
        dd_acc = mo_acc = yy_acc = 0;

        lat_dig = 0;
        lat_after_dot = 0;
        lat_frac_dig = 0;
        lat_deg_acc = 0;
        lat_min_int = 0;
        lat_frac3 = 0;

        lon_dig = 0;
        lon_after_dot = 0;
        lon_frac_dig = 0;
        lon_deg_acc = 0;
        lon_min_int = 0;
        lon_frac3 = 0;

        sats_seen = 0;
        sats_acc = 0;

        hd_seen = 0;
        num_after_dot = 0;
        hd_ip = 0;
        hd_fp = 0;

        alt_seen = 0;
        alt_neg = 0;
        alt_acc = 0;
      }
      return;
  }

  // -------- payload --------
  if (hs != 7) return;

  if (c == '$') {
    hs = 1;
    return;
  }
  if (c == '*') {
    in_checksum = 1;
    return;
  }

  if (c == '\n' || c == '\r') {

    uint8_t have_fix_rmc = (msg == MSG_RMC && tmp_time_ok && tmp_fixA) ? 1 : 0;
    uint8_t have_fix_gga = (msg == MSG_GGA && tmp_time_ok && (tmp_fixq > 0)) ? 1 : 0;

    cli();

    if (tmp_time_ok) {
      utc_hh = tmp_hh;
      utc_mm = tmp_mm;
      utc_ss = tmp_ss;
    }

    if (msg == MSG_RMC) {
      if (tmp_have_date) {
        utc_dd = tmp_dd;
        utc_mo = tmp_mo;
        utc_yy = tmp_yy;
      }

      // Commit Lat/Lon from RMC only if RMC says "A"
      if (have_fix_rmc) {
        lat_deg = tmp_lat_deg;
        lat_min_x1000 = tmp_lat_min_x1000;
        lat_hemi = tmp_lat_hemi;

        lon_deg = tmp_lon_deg;
        lon_min_x1000 = tmp_lon_min_x1000;
        lon_hemi = tmp_lon_hemi;
      }

      // IMPORTANT: do not downgrade FIX here (GGA is authoritative for FIX)
      if (!tmp_time_ok) gps_state = GPS_NO_DATA;
      else if (gps_state == GPS_NO_DATA) gps_state = GPS_TIME_ONLY;
    }

    if (msg == MSG_GGA) {
      if (have_fix_gga) {
        if (tmp_have_sats) gga_sats = tmp_sats;
        if (tmp_have_hdop) gga_hdop_x10 = tmp_hdop_x10;
        if (tmp_have_alt) gga_alt_m = tmp_alt_m;
        gga_new = 1;
        gps_state = GPS_FIX;
      } else {
        if (tmp_time_ok && gps_state == GPS_NO_DATA) gps_state = GPS_TIME_ONLY;
      }
    }

    sei();

    hs = 0;
    msg = MSG_NONE;
    return;
  }

  if (in_checksum) return;

  if (c == ',') {
    field++;
    num_after_dot = 0;

    // RMC finalize date when leaving field 9 (->10)
    if (msg == MSG_RMC && field == 10) {
      if (date_seen && date_digit >= 6) {
        tmp_dd = dd_acc;
        tmp_mo = mo_acc;
        tmp_yy = yy_acc;
        tmp_have_date = 1;
      } else tmp_have_date = 0;
    }

    // RMC finalize LAT when leaving field 3 -> field becomes 4
    if (msg == MSG_RMC && field == 4) {
      tmp_lat_deg = lat_deg_acc;
      tmp_lat_min_x1000 = (uint16_t)((uint16_t)lat_min_int * 1000u + lat_frac3);
    }

    // RMC finalize LON when leaving field 5 -> field becomes 6
    if (msg == MSG_RMC && field == 6) {
      tmp_lon_deg = (uint8_t)lon_deg_acc;
      tmp_lon_min_x1000 = (uint16_t)((uint16_t)lon_min_int * 1000u + lon_frac3);
    }

    // GGA finalize sats when leaving field 7 -> field becomes 8
    if (msg == MSG_GGA && field == 8) {
      if (sats_seen) {
        tmp_sats = (uint8_t)(sats_acc > 99 ? 99 : sats_acc);
        tmp_have_sats = 1;
      } else tmp_have_sats = 0;
    }

    // GGA finalize hdop when leaving field 8 -> field becomes 9
    if (msg == MSG_GGA && field == 9) {
      if (hd_seen) {
        tmp_hdop_x10 = (uint16_t)(hd_ip * 10u + hd_fp);
        tmp_have_hdop = 1;
      } else tmp_have_hdop = 0;
    }

    // GGA finalize alt when leaving field 9 -> field becomes 10
    if (msg == MSG_GGA && field == 10) {
      if (alt_seen) {
        tmp_alt_m = alt_neg ? (int16_t)(-alt_acc) : alt_acc;
        tmp_have_alt = 1;
      } else tmp_have_alt = 0;
    }

    // per-field resets (only what we parse)

    if (field == 1) {
      tmp_hh = tmp_mm = tmp_ss = 0;
      tmp_time_digits = 0;
      tmp_time_ok = 0;
    }

    // RMC: entering LAT numeric field 3
    if (msg == MSG_RMC && field == 3) {
      lat_dig = 0;
      lat_after_dot = 0;
      lat_frac_dig = 0;
      lat_deg_acc = 0;
      lat_min_int = 0;
      lat_frac3 = 0;
    }

    // RMC: entering LON numeric field 5
    if (msg == MSG_RMC && field == 5) {
      lon_dig = 0;
      lon_after_dot = 0;
      lon_frac_dig = 0;
      lon_deg_acc = 0;
      lon_min_int = 0;
      lon_frac3 = 0;
    }

    // RMC: entering date field 9
    if (msg == MSG_RMC && field == 9) {
      date_digit = 0;
      date_seen = 0;
      dd_acc = mo_acc = yy_acc = 0;
    }

    // GGA: entering sats field 7
    if (msg == MSG_GGA && field == 7) {
      sats_seen = 0;
      sats_acc = 0;
    }

    // GGA: entering hdop field 8
    if (msg == MSG_GGA && field == 8) {
      hd_seen = 0;
      hd_ip = 0;
      hd_fp = 0;
      num_after_dot = 0;
    }

    // GGA: entering alt field 9
    if (msg == MSG_GGA && field == 9) {
      alt_seen = 0;
      alt_neg = 0;
      alt_acc = 0;
      num_after_dot = 0;
    }

    return;
  }

  // -------- field parsing --------

  // TIME field 1 for RMC & GGA
  if (field == 1 && c >= '0' && c <= '9') {
    uint8_t d = (uint8_t)(c - '0');
    if (tmp_time_digits < 2) tmp_hh = (uint8_t)(tmp_hh * 10u + d);
    else if (tmp_time_digits < 4) tmp_mm = (uint8_t)(tmp_mm * 10u + d);
    else if (tmp_time_digits < 6) tmp_ss = (uint8_t)(tmp_ss * 10u + d);
    tmp_time_digits++;
    if (tmp_time_digits >= 6) tmp_time_ok = 1;
  }

  // RMC status field 2
  if (msg == MSG_RMC && field == 2) {
    if (c == 'A') tmp_fixA = 1;
  }

  // RMC LAT numeric field 3: ddmm.mmmm (grab dd, mm, first 3 decimals)
  if (msg == MSG_RMC && field == 3) {
    if (c >= '0' && c <= '9') {
      uint8_t d = (uint8_t)(c - '0');
      if (!lat_after_dot) {
        if (lat_dig < 2) lat_deg_acc = (uint8_t)(lat_deg_acc * 10u + d);
        else if (lat_dig < 4) lat_min_int = (uint8_t)(lat_min_int * 10u + d);
        lat_dig++;
      } else {
        if (lat_frac_dig < 3) {
          lat_frac3 = (uint16_t)(lat_frac3 * 10u + d);
          lat_frac_dig++;
        }
      }
    } else if (c == '.') {
      lat_after_dot = 1;
    }
  }

  // RMC LAT hemi field 4
  if (msg == MSG_RMC && field == 4) {
    if (c == 'N' || c == 'S') tmp_lat_hemi = (char)c;
  }

  // RMC LON numeric field 5: dddmm.mmmm
  if (msg == MSG_RMC && field == 5) {
    if (c >= '0' && c <= '9') {
      uint8_t d = (uint8_t)(c - '0');
      if (!lon_after_dot) {
        if (lon_dig < 3) lon_deg_acc = (uint16_t)(lon_deg_acc * 10u + d);
        else if (lon_dig < 5) lon_min_int = (uint8_t)(lon_min_int * 10u + d);
        lon_dig++;
      } else {
        if (lon_frac_dig < 3) {
          lon_frac3 = (uint16_t)(lon_frac3 * 10u + d);
          lon_frac_dig++;
        }
      }
    } else if (c == '.') {
      lon_after_dot = 1;
    }
  }

  // RMC LON hemi field 6
  if (msg == MSG_RMC && field == 6) {
    if (c == 'E' || c == 'W') tmp_lon_hemi = (char)c;
  }

  // RMC date field 9
  if (msg == MSG_RMC && field == 9 && c >= '0' && c <= '9') {
    uint8_t d = (uint8_t)(c - '0');
    date_seen = 1;
    if (date_digit < 2) dd_acc = (uint8_t)(dd_acc * 10u + d);
    else if (date_digit < 4) mo_acc = (uint8_t)(mo_acc * 10u + d);
    else if (date_digit < 6) yy_acc = (uint8_t)(yy_acc * 10u + d);
    date_digit++;
  }

  // GGA fix quality field 6
  if (msg == MSG_GGA && field == 6 && c >= '0' && c <= '9') {
    tmp_fixq = (uint8_t)(c - '0');
  }

  // GGA sats field 7
  if (msg == MSG_GGA && field == 7 && c >= '0' && c <= '9') {
    sats_seen = 1;
    sats_acc = (uint16_t)(sats_acc * 10u + (uint8_t)(c - '0'));
  }

  // GGA hdop field 8
  if (msg == MSG_GGA && field == 8) {
    if (c >= '0' && c <= '9') {
      uint8_t d = (uint8_t)(c - '0');
      hd_seen = 1;
      if (!num_after_dot) hd_ip = (uint16_t)(hd_ip * 10u + d);
      else hd_fp = d;
    } else if (c == '.') num_after_dot = 1;
  }

  // GGA alt field 9 (integer meters, ignore decimals)
  if (msg == MSG_GGA && field == 9) {
    if (c == '-') alt_neg = 1;
    else if (c == '.') num_after_dot = 1;
    else if (!num_after_dot && c >= '0' && c <= '9') {
      alt_seen = 1;
      if (alt_acc < 30000)
        alt_acc = (int16_t)(alt_acc * 10 + (int16_t)(c - '0'));
    }
  }
}

static inline void showTime7Seg() {
  const uint16_t x0 = 30;
  const uint16_t y0 = 180;

  // one "cell" pitch for a 7-seg glyph
  const uint16_t step = DIG_W;  // e.g. #define DIG_W (14 * sc)

  static uint8_t last_hh = 255;
  static uint8_t last_mm = 255;
  static uint8_t last_ss = 255;
  static uint8_t shown_dashes = 0;
  static bool last_colon = true;

  gps_state_t st;
  uint8_t hh, mm, ss;

  cli();
  st = gps_state;
  hh = utc_hh;
  mm = utc_mm;
  ss = utc_ss;
  sei();

  // --- NO DATA: show --:--:-- once ---
  if (st == GPS_NO_DATA) {
    if (!shown_dashes) {
      shown_dashes = 1;

      tft.Plot7Seg(x0 + step * 0, y0, 17);
      tft.Plot7Seg(x0 + step * 1, y0, 17);
      tft.Plot7Seg(x0 + (step * 2) + (step / 2), y0, 17);
      tft.Plot7Seg(x0 + (step * 3) + (step / 2), y0, 17);
      tft.Plot7Seg(x0 + step * 5, y0, 17);
      tft.Plot7Seg(x0 + step * 6, y0, 17);
    }
    return;
  }

  // --- TIME AVAILABLE ---
  if (shown_dashes) {
    last_hh = last_mm = last_ss = 255;  // force redraw once
    shown_dashes = 0;
  }

  // HH
  if (hh != last_hh) {
    last_hh = hh;
    tft.Plot7Seg(x0 + step * 0, y0, (hh / 10));
    tft.Plot7Seg(x0 + step * 1, y0, (hh % 10));
  }

  // MM
  if (mm != last_mm) {
    last_mm = mm;
    tft.Plot7Seg(x0 + (step * 2) + (step / 2), y0, (mm / 10));
    tft.Plot7Seg(x0 + (step * 3) + (step / 2), y0, (mm % 10));
  }

  // SS (tens only every 10 s, units every second)
  if ((ss / 10) != (last_ss / 10)) {
    tft.Plot7Seg(x0 + step * 5, y0, (ss / 10));
  }
  if (ss != last_ss) {
    last_ss = ss;

    bool colon_on = (ss & 1) == 0;  // 1 second on, 1 second off

    if (colon_on != last_colon) {
      last_colon = colon_on;

      tft.drawColon(276, 186, colon_on);
      tft.drawColon(626, 186, colon_on);
    }

    tft.Plot7Seg(x0 + step * 6, y0, (ss % 10));
  }
}

// helpers for Lat/Lon
static inline void put2_u8(uint8_t v) {  // 00..99
  tft.Putc((char)('0' + (v / 10)));
  tft.Putc((char)('0' + (v % 10)));
}

static inline void put3_u16(uint16_t v) {  // 000..999
  tft.Putc((char)('0' + (v / 100)));
  v %= 100;
  tft.Putc((char)('0' + (v / 10)));
  tft.Putc((char)('0' + (v % 10)));
}

static inline void showLat() {
  // ---- widget origin ----
  const uint16_t y0 = 50;
  const uint16_t xDeg = 800;  // degrees field starts here (2 chars)
  const uint16_t xMin = 850;  // minutes field starts here
  uint8_t st, deg, hemi;
  uint16_t mx;

  cli();
  st = (uint8_t)gps_state;
  deg = lat_deg;
  mx = lat_min_x1000;
  hemi = (uint8_t)lat_hemi;
  sei();

  // --- change detection ---
  static uint8_t last_st = 255;
  static uint8_t last_deg = 255;
  static uint8_t last_hemi = 0;
  static uint16_t last_mx = 0xFFFF;
  if (st == last_st && deg == last_deg && hemi == last_hemi && mx == last_mx) return;
  last_st = st;
  last_deg = deg;
  last_hemi = hemi;
  last_mx = mx;

  uint8_t mm = (uint8_t)(mx / 1000u);
  uint16_t mmm = (uint16_t)(mx - (uint16_t)mm * 1000u);

  // -------- degrees field (no space printed after it) --------
  tft.goto_xy(xDeg, y0);
  tft.TextBegin();

  if (st != GPS_FIX) {
    // degrees: "--"
    tft.Putc('-');
    tft.Putc('-');
    tft.TextEnd();

    // minutes: "--.--- -"
    tft.goto_xy(xMin, y0);
    tft.TextBegin();
    tft.Putc('-');
    tft.Putc('-');
    tft.Putc('.');
    tft.Putc('-');
    tft.Putc('-');
    tft.Putc('-');
    tft.Putc(' ');
    tft.Putc('-');
    tft.TextEnd();
    return;
  }

  // degrees: fixed 2 chars, leading space if <10
  if (deg >= 10) {
    tft.Putc((char)('0' + (deg / 10)));
    tft.Putc((char)('0' + (deg % 10)));
  } else {
    tft.Putc(' ');
    tft.Putc((char)('0' + deg));
  }
  tft.TextEnd();

  // -------- minutes field --------
  tft.goto_xy(xMin, y0);
  tft.TextBegin();

  put2_u8(mm);
  tft.Putc('.');
  put3_u16(mmm);
  tft.Putc(' ');
  tft.Putc((char)hemi);

  tft.TextEnd();
}

static inline void showLon() {
  // ---- widget origin ----
  const uint16_t y0 = 88;

  const uint16_t xDeg = 780;  // degrees field starts here (3 chars)
  const uint16_t xMin = 850;  // minutes field starts here (tweak!)
  //           ^^^ adjust so your degree circle sits between xDeg and xMin

  uint8_t st, deg, hemi;
  uint16_t mx;

  cli();
  st = (uint8_t)gps_state;
  deg = lon_deg;
  mx = lon_min_x1000;
  hemi = (uint8_t)lon_hemi;
  sei();

  // --- change detection ---
  static uint8_t last_st = 255;
  static uint8_t last_deg = 255;
  static uint8_t last_hemi = 0;
  static uint16_t last_mx = 0xFFFF;
  if (st == last_st && deg == last_deg && hemi == last_hemi && mx == last_mx) return;
  last_st = st;
  last_deg = deg;
  last_hemi = hemi;
  last_mx = mx;

  uint8_t mm = (uint8_t)(mx / 1000u);
  uint16_t mmm = (uint16_t)(mx - (uint16_t)mm * 1000u);

  // -------- degrees field (no space printed after it) --------
  tft.goto_xy(xDeg, y0);
  tft.TextBegin();

  if (st != GPS_FIX) {
    // degrees: "---"
    tft.Putc('-');
    tft.Putc('-');
    tft.Putc('-');
    tft.TextEnd();

    // minutes: "--.--- -"
    tft.goto_xy(xMin, y0);
    tft.TextBegin();
    tft.Putc('-');
    tft.Putc('-');
    tft.Putc('.');
    tft.Putc('-');
    tft.Putc('-');
    tft.Putc('-');
    tft.Putc(' ');
    tft.Putc('-');
    tft.TextEnd();
    return;
  }

  // degrees: fixed 3 chars with leading spaces
  if (deg >= 100) {
    tft.Putc((char)('0' + (deg / 100)));
    put2_u8((uint8_t)(deg % 100));
  } else {
    tft.Putc(' ');
    if (deg >= 10) {
      put2_u8(deg);  // "13"
    } else {
      tft.Putc(' ');
      tft.Putc((char)('0' + deg));  // " 3"
    }
  }

  tft.TextEnd();

  // -------- minutes field --------
  tft.goto_xy(xMin, y0);
  tft.TextBegin();

  put2_u8(mm);
  tft.Putc('.');
  put3_u16(mmm);
  tft.Putc(' ');
  tft.Putc((char)hemi);

  tft.TextEnd();
}

// compute Monday ... Sunday from date
uint8_t DayOfWeek(uint8_t y, uint8_t m, uint8_t d) {
  // y = 0..99 for years 2000..2099
  static const uint8_t mo[12] = { 0, 3, 3, 6, 1, 4, 6, 2, 5, 0, 3, 5 };
  uint8_t w = y + (y >> 2) + mo[m - 1] + d + 6;  // +6: 1 Jan 2000 was Saturday
  if ((y & 3) == 0 && m < 3) w--;                // leap-year correction for Jan/Feb
  return (w % 7);                                // 0..6 (Sun..Sat)
}

static inline void showDate() {
  tft.fore(rf, gf, bf);

  const uint16_t x0 = 400;  // position on TFT
  const uint16_t y0 = 450;  // adjust

  static uint8_t last_dd = 255;
  static uint8_t last_mo = 255;
  static uint8_t last_yy = 255;
  static uint8_t last_dow = 255;
  static uint8_t shown_dashes = 0;

  gps_state_t st;
  uint8_t dd, mo, yy;

  cli();
  st = gps_state;
  dd = utc_dd;
  mo = utc_mo;
  yy = utc_yy;
  sei();

  // ---- NO DATA ----
  if (st == GPS_NO_DATA) {
    if (!shown_dashes) {
      shown_dashes = 1;

      tft.goto_xy(x0, y0);
      tft.TextBegin();
      for (uint8_t i = 0; i < (uint8_t)(sizeof(dashStr) - 1); i++) {
        tft.Putc((char)pgm_read_byte(dashStr + i));
      }
      tft.TextEnd();
    }
    return;
  }

  // ---- DATA AVAILABLE ----
  if (shown_dashes) {
    shown_dashes = 0;
    last_dd = last_mo = last_yy = last_dow = 255;  // force redraw once
  }

  uint8_t dow = DayOfWeek(yy, mo, dd);  // you expect 0..6
  if (dow > 6) dow = 0;                 // safety clamp

  if (dd == last_dd && mo == last_mo && yy == last_yy && dow == last_dow) return;

  last_dd = dd;
  last_mo = mo;
  last_yy = yy;
  last_dow = dow;

  tft.goto_xy(x0, y0);
  tft.TextBegin();

  // DOW (3 letters) from your PROGMEM table: "SUNMONTUEWEDTHUFRISAT"
  const char *p = dow3 + (uint8_t)(dow * 3);
  tft.Putc((char)pgm_read_byte(p + 0));
  tft.Putc((char)pgm_read_byte(p + 1));
  tft.Putc((char)pgm_read_byte(p + 2));
  tft.Putc(' ');

#if DATE_FORMAT_US == 0
  // DD.MM.YYYY
  tft.Putc((char)('0' + (dd / 10)));
  tft.Putc((char)('0' + (dd % 10)));
  tft.Putc('.');

  tft.Putc((char)('0' + (mo / 10)));
  tft.Putc((char)('0' + (mo % 10)));
  tft.Putc('.');
#else
  // MM/DD/YYYY
  tft.Putc((char)('0' + (mo / 10)));
  tft.Putc((char)('0' + (mo % 10)));
  tft.Putc('/');

  tft.Putc((char)('0' + (dd / 10)));
  tft.Putc((char)('0' + (dd % 10)));
  tft.Putc('/');
#endif

  // YYYY (always 20yy)
  tft.Putc('2');
  tft.Putc('0');
  tft.Putc((char)('0' + (yy / 10)));
  tft.Putc((char)('0' + (yy % 10)));

  tft.TextEnd();
}

static inline void showSats() {
  tft.fore(rf, gf, bf);

  const uint16_t x0 = 490;
  const uint16_t y0 = 548;

  static uint8_t last_sats = 255;
  uint8_t s = gga_sats;
  if (s == last_sats) return;
  last_sats = s;

  tft.goto_xy(x0, y0);
  tft.TextBegin();

  if (s < 10) {
    tft.Putc(' ');
    tft.Putc('0' + s);
  } else {
    tft.Putc('0' + (s / 10));
    tft.Putc('0' + (s % 10));
  }

  tft.TextEnd();
}

static inline void showAlt() {
  const uint16_t x0 = 600;
  const uint16_t y0 = 548;

  uint8_t st;
  int16_t alt;

  cli();
  st = (uint8_t)gps_state;
  alt = gga_alt_m;
  sei();

  static uint8_t last_st = 255;
  static int16_t last_alt = (int16_t)0x7FFF;
  if (st == last_st && alt == last_alt) return;
  last_st = st;
  last_alt = alt;

  tft.goto_xy(x0, y0);
  tft.TextBegin();

  if (st != GPS_FIX) {
    tft.Putc('-');
    tft.Putc('-');
    tft.Putc('-');
    tft.Putc('-');
    tft.Putc('-');
    // tft.Putc('m');
    tft.TextEnd();
    return;
  }

  // sign column
  if (alt < 0) {
    tft.Putc('-');
    alt = -alt;
  } else {
    tft.Putc(' ');
  }

  uint16_t v = (uint16_t)alt;

  // compute digits without printing yet
  uint8_t d1 = (uint8_t)(v / 1000);
  v %= 1000;
  uint8_t d2 = (uint8_t)(v / 100);
  v %= 100;
  uint8_t d3 = (uint8_t)(v / 10);
  uint8_t d4 = (uint8_t)(v % 10);

  // right alignment inside 4 digit positions
  if (d1) {
    tft.Putc('0' + d1);
  } else {
    tft.Putc(' ');
  }

  if (d1 || d2) {
    tft.Putc('0' + d2);
  } else {
    tft.Putc(' ');
  }

  if (d1 || d2 || d3) {
    tft.Putc('0' + d3);
  } else {
    tft.Putc(' ');
  }

  tft.Putc('0' + d4);

  // tft.Putc('m');

  tft.TextEnd();
}

static inline uint8_t hdopLevel5(uint16_t hdop_x10, uint8_t fix_ok) {
  if (!fix_ok) return 0;
  if (hdop_x10 <= 10) return 5;   // <=1.0
  if (hdop_x10 <= 20) return 4;   // <=2.0
  if (hdop_x10 <= 50) return 3;   // <=5.0
  if (hdop_x10 <= 100) return 2;  // <=10
  return 1;
}

void showHdop() {
  // widget origin (top-left reference for bars)
  const uint16_t x0 = 60;
  const uint16_t y0 = 80;   // bottom baseline
  const uint8_t w = 6;      // bar width
  const uint8_t gap = 3;    // spacing
  const uint8_t stepH = 6;  // height increment per bar (bar i has (i+1)*stepH)
  //colors:
  // uint8_t r0 = rf, g0 = gf, b0 = bf;
  // tft.fore(0xFF, 0xFF, 0x00);
  // tft.back(0x21, 0x00, 0xDD);

  gps_state_t st;

  // cli();
  st = gps_state;
  // sei();

  static uint8_t last = 255;

  uint8_t fix_ok = (st == GPS_FIX);                // do we need this ?
  uint8_t lvl = hdopLevel5(gga_hdop_x10, fix_ok);  // 0..5

  if (lvl == last) return;
  uint8_t old = last;
  last = lvl;

  // If level decreased, erase bars that should be off
  if (old != 255 && lvl < old) {
    tft.fore(rb, gb, bb);  // background
    for (uint8_t i = lvl; i < old; i++) {
      uint16_t x = x0 + i * (w + gap);
      uint8_t h = (uint8_t)((i + 1) * stepH);
      tft.Fill_rect(x, (uint16_t)(y0 - h), (uint16_t)(x + w), y0);
    }
  }

  // If level increased, draw new bars
  if (old == 255 || lvl > old) {
    tft.fore(rf, gf, bf);  // foreground (yellow)
    uint8_t start = (old == 255) ? 0 : old;
    for (uint8_t i = start; i < lvl; i++) {
      uint16_t x = x0 + i * (w + gap);
      uint8_t h = (uint8_t)((i + 1) * stepH);
      tft.Fill_rect(x, (uint16_t)(y0 - h), (uint16_t)(x + w), y0);
    }
  }

  // If no fix, optionally erase all bars (lvl==0 handled by decrease path when old>0)
  if (!fix_ok && old == 255) {
    // first call case: ensure area is blank
    tft.fore(rb, gb, bb);
    for (uint8_t i = 0; i < 5; i++) {
      uint16_t x = x0 + i * (w + gap);
      uint8_t h = (uint8_t)((i + 1) * stepH);
      tft.Fill_rect(x, (uint16_t)(y0 - h), (uint16_t)(x + w), y0);
    }
  }
}

static inline void showLeds() {
  static uint8_t last_gps = 255;
  static uint8_t last_utc = 255;
  static uint8_t last_fix = 255;

  uint8_t gps_on = (gps_idle < GPS_IDLE_MAX);
  uint8_t utc_on = (gps_state != GPS_NO_DATA);
  uint8_t fix_on = (gps_state == GPS_FIX);

  if (gps_on != last_gps) {
    last_gps = gps_on;
    if (gps_on) tft.fore(0, 60, 255);
    else tft.fore(0xFF, 0x00, 0x00);
    tft.Fill_circle(180, 564, 7);
  }

  if (utc_on != last_utc) {
    last_utc = utc_on;
    if (utc_on) tft.fore(0, 60, 255);
    else tft.fore(0xFF, 0x00, 0x00);
    tft.Fill_circle(290, 564, 7);
  }
  if (fix_on != last_fix) {
    last_fix = fix_on;
    if (fix_on) tft.fore(0, 60, 255);
    else tft.fore(0xFF, 0x00, 0x00);
    tft.Fill_circle(400, 564, 7);
  }
  tft.fore(rf, gf, bf);
}



//** Arduino GPS-Program
void setup() {

  I2C.init(0x7E);
  tft.init_LT7683();
  LED.flash(3);
  tft.clearScreen();

  //** outer Frame
  tft.fore(rb, gb, bb);                        // ylw
  tft.Fill_rounded_rect(3, 3, 1021, 597, 10);  // inner background
  tft.fore(rf, gf, bf);                        // ylw
  tft.rounded_rect(2, 2, 1022, 598, 10);       // outer frame
  tft.rounded_rect(15, 20, 1008, 140, 10);     // around Text on Top
  tft.rounded_rect(15, 150, 1008, 510, 10);    // around UTC
  tft.rounded_rect(360, 440, 678, 492, 10);    // around Date

  tft.textSize(3);
  tft.back(rb, gb, bb);
  tft.goto_xy(374, 34);
  tft.PrintP_N(lbl1, sizeof(lbl1) - 1);
  tft.goto_xy(426, 82);
  tft.PrintP_N(lbl2, sizeof(lbl2) - 1);
  tft.rounded_rect(25, 30, 135, 130, 10);    // rounded rect around HDOP
  tft.rounded_rect(725, 30, 999, 130, 10);   // rounded rect around Lat/Lon
  tft.rounded_rect(15, 520, 120, 588, 10);   // rounded rect around the Power-LED
  tft.rounded_rect(125, 520, 230, 588, 10);  // rounded rect around the GPS-LED
  tft.rounded_rect(235, 520, 340, 588, 10);  // rounded rect around the Time-LED
  tft.rounded_rect(345, 520, 450, 588, 10);  // rounded rect around the Fix-LED
  tft.rounded_rect(455, 520, 560, 588, 10);  // rounded rect around Nr.-of-Sats
  tft.rounded_rect(565, 520, 740, 588, 10);  // rounded rect around Altitude

  // display the '°' sign made out of two circles for the Lat/Lon-display:
  tft.Fill_circle(841, 53, 5);
  tft.Fill_circle(841, 91, 5);
  tft.fore(rb, gb, bb);
  tft.Fill_circle(841, 53, 3);
  tft.Fill_circle(841, 91, 3);

  // frames for LEDs
  tft.fore(200, 200, 200);       // silver / grey
  tft.Fill_circle(70, 564, 9);   // LED-frame for Pwr
  tft.Fill_circle(180, 564, 9);  // LED-frame for GPS
  tft.Fill_circle(290, 564, 9);  // LED-frame for TIME
  tft.Fill_circle(400, 564, 9);  // LED-frame for FIX

  tft.fore(0, 60, 255);         // blue
  tft.Fill_circle(70, 564, 7);  // Pwr-LED always on as long as program runs

  tft.fore(0xFF, 0x30, 0x00);  // kinda red
  tft.textSize(2);
  tft.goto_xy(735, 52);
  tft.PrintP_N(sLAT, 3);
  tft.goto_xy(735, 92);
  tft.PrintP_N(sLON, 3);
  tft.goto_xy(56, 96);
  tft.PrintP_N(sHDOP, 4);
  tft.goto_xy(40, 525);
  tft.PrintP_N(sPWR, 5);
  tft.goto_xy(162, 525);
  tft.PrintP_N(sGPS, 3);
  tft.goto_xy(264, 525);
  tft.PrintP_N(sUTC, 4);
  tft.goto_xy(380, 525);
  tft.PrintP_N(sFIX, 3);
  tft.goto_xy(485, 525);
  tft.PrintP_N(sSAT, 4);
  tft.goto_xy(695, 554);
  tft.PrintP_N(sMTR, 3);
  tft.goto_xy(636, 525);
  tft.PrintP_N(sALT, 3);
  tft.textSize(3);

  // arm the PCINT IRQ
  pinMode(3, INPUT);
  cli();
  GIMSK = 0b00100000;  // turns on pin change interrupts
  PCMSK = 0b00001000;  // turn on interrupt on pin PB3
  GIFR = 0b00100000;   // clear PCIF
  sei();

  tft.fore(rf, gf, bf);  // ylw
  tft.back(rb, gb, bb);  // background color
}


void loop() {

  if (rxReady) {
    uint8_t b;
    cli();
    b = rxByte;
    rxReady = false;
    gps_idle = 0;  //
    sei();
    nmeaDecode(b);  // feed our NMEA-decoder
  } else {
    if (gps_idle < GPS_IDLE_MAX) { gps_idle++; }
  }

  if (burst_is_done) {
    cli();
    burst_is_done = false;
    sei();

    if (rmc_seen) {
      cli();
      rmc_seen = 0;
      sei();
    }

    PCINT_off();
    showDate();
    showLeds();
    showHdop();
    showLat();
    showLon();
    showTime7Seg();  //
    showSats();
    showAlt();
    LED.flash(1);  // debug
    PCINT_on();
  }
}


// ================================================
// ****** IRQ-Section to receive GPS-data *********
// ****** first is the Pin Change IRQ  ************
// ****** second is Timer-IRQ          ************
// ================================================

// detect the Start-Bit:
ISR(PCINT0_vect) {  // only start on falling edge (RX low)
  if (!(PINB & 0x08)) {
    bitcount = 0;
    rxByte = 0;
    toc = 0;  // reset the detect burst-is-done time-out-counter to zero

    // PCINT off during frame
    GIMSK &= ~0b00100000;
    GIFR = 0b00100000;

    // turn on the timer1 IRQ to sample first Bit:
    TCNT1 = 0;
    TCCR1 = 4;              // Normal Mode, prescaler 8
    TIMSK = (1 << OCIE1A);  // interrupt on OCR1A match
    OCR1A = 152;            // 1/2 of Bit-length
    OCR1C = 152;            //      ""
  }
}


// Sample 8 Bits and stuff into rxByte or detect end-of-burst
ISR(TIMER1_COMPA_vect) {
  // find out why we are inside the iSR by looking at OCR1A:
  if (OCR1A == 200) {  // we are here to check for end-of-burst
    if (toc++ > 10) {
      TCCR1 = 0;  // turn off the timer
      burst_is_done = true;
    }
  } else {  // we are here to stuff a Byte
    // PINB = 0x10;  // signal Bit-stuffing is happening to LA
    if (PINB & 0x08) rxByte |= (1 << bitcount);
    bitcount++;
    OCR1A = 102;
    OCR1C = 102;
    if (bitcount >= 8) {  // we have a Byte
                          // keep timer running after 8 samples as clock to detect end-of-burst
      OCR1A = 200;
      OCR1C = 200;
      // TCCR1 = 0;
      rxReady = true;

      // re-enable PCINT for next start bit
      GIFR = 0b00100000;
      GIMSK = 0b00100000;
    }
    // PINB = 0x10;  // signal Bit-read is done to LA
  }
}
