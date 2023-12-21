/*  Arduino UNO R4 test-code to demonstrate:
    1. Wake-From-Interrupt
    2. Change System-Clock divisors 1/1, 1/16, and 1/64 settings 
    3. Switching between 48MHz HOCO and 8MHz MOCO internal oscillators (and PLL with XTAL if fitted)
    4. USB Module Disable (with code initiated Reset after timeout delay)

NOTE: This code is experimental, don't use without reading RA4M1 documentation. 
      Clock, division, and low-power modes have lots of variations - not ALL will work! 

Susan Parker - 17th December 2023.
  First draft - WFI and USB disable

Susan Parker - 18th December 2023.
  Add processor clock-switching - not yet working.

Susan Parker - 19th December 2023.
  Add timeout delay SoftWare-Reset, to re-establish USB Serial connection to PC
  ... any application would need to do a tidyup before SW-Reset e.g. close files,
      save configurations into EEPROM, etc.
  Update processor clock-switching - now working.
  ... USBFS clock runs seperatly, direct from the 48MHz HOCO clock,
      USB serial still works at x1/16 and x1/64 sys-clock rates.

Susan Parker - 20th December 2023.
  Add additinal USB register clears - RA4M1 no longer backpowers ON led when USB removed.
  Update SysClk switching - restore IDE defaults before SW-Reset on "u" command.

Susan Parker - 21st December 2023.
  Add change of System Clock between HOCO and MOCO sources (and PLL if XTAL fitted)
  ... turn off HOCO clock when USBFS is disabled if using MOCO 8MHz clock,
      or MOSC and PLL with external XTAL (e.g. EK-RA4M1 dev-board).

  This code is "AS IS" without warranty or liability. 

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

======================================================

USB Serial input for very basic control of operations:

  s = Enable  Sleep / Wake From Interrupt.
  q = Disable Sleep / Wake From Interrupt.
  u = USB Module Disable - will perform a code function HW-Reset after 10s delay.
  0 = Set the System Clocks to IDE default.
  4 = Change all System Clocks to x1/16 rate.
  6 = Change all System Clocks to x1/64 rate.
  8 = Change System Clock from HOCO to MOCO.
  9 = Change System Clock from MOCO to HOCO.
  m = Report mode - Clock settings, WFI or Constant-Looping, etc.
  ? = Display all Char Commands (this list).

  7 = Change System Clock to PLL with XTAL. [ For boards with external XTAL fitted.]

Board changes for Low_Power:
1. ON LED series resistor changed to 2.7K, and LED VCC connection changed to VUSB.
2. L  LED series resistor changed to 1.7K (which = 1.8mA if 100% on).

Board running with +9.0V VIN at 48MHz HOCO clock with default IDE settings:
  without sleep, USB active = c. 15.5 mA
  with WFI and USB active   = c.  7.5 mA
  with WFI but USB disabled = c.  5.3 mA (Clear USBFS.USBMC and USBFS.USBBCCTRL0 to defaults)
  with WFI and USB unpluged = c.  5.3 mA (  "     "     "  ...)

(Without clearing USBFS.USBMC and USBFS.USBBCCTRL0 to defaults)
  with WFI but USB disabled = c.  5.8 mA (USB module still has power from PC conection).
  with WFI and USB unpluged = c.  5.9 mA (The ON led is partly backpowered through USB section).

With all SysClocks at 1/16th rate (3.0 MHz):
  without sleep, USB active = c.  5.3 mA
  with WFI and USB active   = c.  4.1 mA
  with WFI but USB disabled = c.  2.2 mA

With all SysClocks at 1/64th rate (750 kHz):
  without sleep, USB active = c.  4.7 mA
  with WFI and USB active   = c.  3.8 mA
  with WFI but USB disabled = c.  1.8 mA

Board running with +9.0V VIN at 8MHz MOCO clock with default IDE settings:
  without sleep, USB active = c.  6.2 mA - HOCO still running for USBFS Module
  with WFI and USB active   = c.  4.1 mA
  with WFI but USB disabled = c.  2.8 mA - HOCO stopped

With all SysClocks at 1/16th rate (500 kHz):
  without sleep, USB active = c.  4.5 mA
  with WFI and USB active   = c.  3.5 mA
  with WFI but USB disabled = c.  1.2 mA

With all SysClocks at 1/64th rate (125 kHz):
  without sleep, USB active = c.  4.4 mA
  with WFI and USB active   = c.  3.4 mA
  with WFI but USB disabled = c.  1.1 mA 

When board has USB disabled - a full power-cycle OFF and ON needed to reattach PC serial.
UPDATE: Code now can do a Software-Reset - saves on unplugging and plugging in cables.


### ARM-developer - Accessing memory-mapped peripherals ###
    https://developer.arm.com/documentation/102618/0100

Fastest possible pin writes and reads with the RA4M1 processor
  *PFS_P107PFS_BY = 0x05;      // Each Port Output bit clear > to > set   takes c. 83nS 
  *PFS_P107PFS_BY = 0x04;      // Each Port Output bit   set > to > clear takes c. 83nS 

  *PFS_P107PFS_BY = 0x05;      // Set HIGH
  char_val = *PFS_P107PFS_BY;  // Port State Input read - takes about 165nS
  *PFS_P107PFS_BY = 0x04;      // Read plus Set LOW = c. 250nS

*/


// Low Power Mode Control
#define MSTP 0x40040000 
#define MSTP_MSTPCRB   ((volatile unsigned int   *)(MSTP + 0x7000))      // Module Stop Control Register B
#define MSTPB11 11 // USBFS

// ==== USB 2.0 Full-Speed Module ====
#define USBFSBASE  0x40090000
#define USBFS_SYSCFG     ((volatile unsigned short *)(USBFSBASE + 0x0000))
#define USBFS_USBMC      ((volatile unsigned short *)(USBFSBASE + 0x00CC))
#define USBFS_USBBCCTRL0 ((volatile unsigned short *)(USBFSBASE + 0x00B0))

// =========== Ports ============
#define PORTBASE 0x40040000
#define PFS_P111PFS_BY ((volatile unsigned char  *)(PORTBASE + 0x0843 + (11 * 4))) // D13 / SCLK

// Low Power Mode Control - See datasheet section 10
#define SYSTEM 0x40010000 // System Registers
#define SYSTEM_SBYCR    ((volatile unsigned short *)(SYSTEM + 0xE00C))  // Standby Control Register
#define SYSTEM_MSTPCRA  ((volatile unsigned int   *)(SYSTEM + 0xE01C))  // Module Stop Control Register A
#define SYSTEM_SCKDIVCR ((volatile unsigned int   *)(SYSTEM + 0xE020))  // System Clock Division Control Register
#define SYSTEM_PRCR     ((volatile unsigned short *)(SYSTEM + 0xE3FE))  // Protect Register
#define SYSTEM_SCKDIVCR ((volatile unsigned int   *)(SYSTEM + 0xE020))  // System Clock Division Control Register
                                // SYSTEM_SCKDIVCR = 10010100  Note: bit-7 is 1, and should be reading as 0
#define SCKDIVCR_PCKD_2_0   0   // Peripheral Module Clock D           = 4; 1/16
#define SCKDIVCR_PCKC_2_0   4   // Peripheral Module Clock C           = 1; 1/2
#define SCKDIVCR_PCKB_2_0   8   // Peripheral Module Clock B           = 0
#define SCKDIVCR_PCKA_2_0  12   // Peripheral Module Clock A           = 0
#define SCKDIVCR_ICK_2_0   24   // System Clock (ICLK) Select          = 0
#define SYSTEM_SCKSCR  ((volatile unsigned char *)(SYSTEM + 0xE026))  // System Clock Source Control Register
#define SCKSCR_CKSEL_2_0    0   // Clock Source Select - See section 8.2.2
#define SYSTEM_PLLCR   ((volatile unsigned char *)(SYSTEM + 0xE02A))  // PLL Control Register
#define PLLCR_PLLSTP        0   // PLL Stop Control; 0: PLL is operating, 1: PLL is stopped
#define SYSTEM_PLLCCR2 ((volatile unsigned char *)(SYSTEM + 0xE02B))  // PLL Clock Control Register 2
#define PLLCCR2_PLLMUL_4_0  0   // PLL Frequency Multiplication Factor Select
#define PLLCCR2_PLODIV_1_0  6   // PLL Output Frequency Division Ratio Select

#define SYSTEM_MOSCCR   ((volatile unsigned char *)(SYSTEM + 0xE032))  // Main Clock Oscillator Control Register
#define MOSCCR_MOSTP        0   // Main Clock Oscillator Stop; 0: Main clock oscillator is operating, 1: MCO is stopped
#define SYSTEM_HOCOCR   ((volatile unsigned char *)(SYSTEM + 0xE036))  // High-Speed On-Chip Oscillator Control Register
#define HOCOCR_HCSTP        0   // HOCO Stop; 0: HOCO is operating, 1: HOCO is stopped
#define SYSTEM_MOCOCR   ((volatile unsigned char *)(SYSTEM + 0xE038))  // Middle-Speed On-Chip Oscillator Control Register
#define MOCOCR_MCSTP        0   // MOCO Stop; 0: MOCO is operating, 1: MOCO is stopped

#define SYSTEM_MOSCWTCR ((volatile unsigned char *)(SYSTEM + 0xE0A2))  // Main Clock Oscillator Wait Control Register
#define MOSCWTCR_MSTS_3_0   0   // Main Clock Oscillator Wait Time Setting
#define SYSTEM_HOCOWTCR ((volatile unsigned char *)(SYSTEM + 0xE0A5))  // High-Speed On-Chip Oscillator Wait Control Register
#define HOCOWTCR_MSTS_2_0   0   // HOCO Wait Time Setting

#define SYSTEM_MOMCR    ((volatile unsigned char *)(SYSTEM + 0xE413))  // Main Clock Oscillator Mode Oscillation Control Register
#define MOMCR_MODRV1        3   // Main Clock Oscillator Drive Capability 1 Switching; 0: 10 MHz to 20 MHz, 1: 1 MHz to 10 MHz
#define MOMCR_MOSEL         6   // Main Clock Oscillator Switching; 0: Resonator, 1: External clock input

// Reset Status
#define SYSTEM_RSTSR0  ((volatile unsigned char  *)(SYSTEM + 0xE410))  //
#define RSTSR0_PORF    0   // Power-On Reset Detect Flag; 1: Power-on reset detected
#define SYSTEM_RSTSR1  ((volatile unsigned short *)(SYSTEM + 0xE0C0))  //
#define RSTSR1_IWDTRF  0   // Independent Watchdog Timer Reset Detect Flag; 1: Independent watchdog timer reset detected
#define RSTSR1_WDTRF   1   // Watchdog Timer Reset Detect Flag; 1: Watchdog timer reset detected
#define RSTSR1_SWRF    2   // Software Reset Detect Flag;       1: Software reset detected
#define SYSTEM_RSTSR2  ((volatile unsigned char  *)(SYSTEM + 0xE411))  //
#define RSTSR2_CWSF    0   // Cold/Warm Start Determination Flag; 0: Cold start, 1: Warm start

// Flash Memory
#define FCACHE_FCACHEE  ((volatile unsigned short *)(SYSTEM + 0xC100))  // Flash Cache Enable Register
#define FCACHEE_FCACHEEN    0    // FCACHE Enable - 0: Disable FCACHE, 1: Enable FCACHE.
#define FCACHE_FCACHEIV ((volatile unsigned short *)(SYSTEM + 0xC100))  // Flash Cache Invalidate Register
#define FCACHEIV_FCACHEIV   0    // Flash Cache Invalidate - 0: Do not invalidate, 1: Invalidate

// System Control Block
#define SCBBASE 0xE0000000
#define SCB_AIRCR                  ((volatile unsigned int *)(SCBBASE + 0xED0C))
#define SCB_AIRCR_VECTKEY_Pos      16U                                   // SCB AIRCR: VECTKEY Position
#define SCB_AIRCR_SYSRESETREQ_Pos  2U                                    // SCB AIRCR: SYSRESETREQ Position
#define SCB_AIRCR_SYSRESETREQ_Msk  (1UL << SCB_AIRCR_SYSRESETREQ_Pos)    // SCB AIRCR: SYSRESETREQ Mask

// From: https://mcuoneclipse.com/2015/07/01/how-to-reset-an-arm-cortex-m-with-software/

#define WFI_MS_DOWNCOUNT 1000
#define WFI_SECONDS_DELAY  10

// #define CLOCK_PLL    // ONLY use when external 12MHz crystal fitted e.g. EK-RA4M1 Dev-board

bool wfi_flag = true;       // Start with WFI mode active
bool usb_flag = true;       // USB Module active
bool moco_flag = false;     // True when switched to MOCO 8MHz oscillator
bool  pll_flag = false;     // True when PLL and external XTAL

int sys_clk_div = 1;

void setup()
  {
  Serial.begin(115200);
  while (!Serial){};
  Serial.println("\nWFI, SysClk switching, and USB Disable demo code.");
  Serial.println(" - use 'q' command to halt WFI before an IDE Upload");
  print_command();
  }


void loop()
  {
  static uint32_t loop_delay = 1000;
  static uint32_t seconds_count = 0;
  static uint32_t sys_clk_div_shift = 0;

  if(wfi_flag)                     // For testing, make sure sleep mode isn't on at bootup
    {
    *PFS_P111PFS_BY = 0x04;        // D13 - CLEAR LED
  	asm volatile("wfi");           // Stop here and wait for an interrupt
    *PFS_P111PFS_BY = 0x05;        // D13 - Set LED
    loop_delay--;
    if(loop_delay == 0)
      {
      loop_delay = (WFI_MS_DOWNCOUNT >> sys_clk_div_shift);
      seconds_count++;
      if(usb_flag == true)
        {
        Serial.print(".");          // print a "." once per second 
        }
      }
    }

  if(usb_flag == true)
    {
    if(Serial.available())           // If anything comes in on the USB Serial (which is done with USB interrupts)
      {
      char inbyte = Serial.read();
      Serial.write(inbyte);          // ... send it back out 

      switch (inbyte)                // Very basic control of operations
        {
        case 's':
          {
          wfi_flag = true;
          Serial.println("\nEnable Sleep");
          break;
          }
        case 'q':
          {
          wfi_flag = false;
          Serial.println("\nDisable Sleep");
         *PFS_P111PFS_BY = 0x04;        // D13 - CLEAR LED
          break;
          }
        case 'u':
          {
          usb_flag = false;
          seconds_count = 0;
          Serial.println("\nDisable USBFS module");
          delayMicroseconds(900);                        // Wait for USB serial print transaction...
          Serial.end();                                  // Probably does something?
          *USBFS_USBMC      = 0x0002;                    // Turn off USB power circuits
          *USBFS_USBBCCTRL0 = 0x0000;                    // Turn off Pin Functions
          *USBFS_SYSCFG     = 0x0000;                    // USBFS Operation Disable
          delayMicroseconds(1);                          // Wait to ensure module stopped
          *MSTP_MSTPCRB |= (0x01 << MSTPB11);            // Disable USBFS module
          if((moco_flag == true) || (pll_flag == true))  // If MOCO or PLL clock in use
            {
            disable_sys_clock_hoco();
            }
          break;
          }
        case 'm':
          {
          print_status();
          break;
          }
        case '?':
          {
          print_command();
          break;
          }
        case '0':
          {
          if(moco_flag == true)                 // If MOCO clock in use
            sys_clk_div_shift = 2;
          else
            sys_clk_div_shift = 0;              // PCLKB is already at x1/2
          loop_delay = 1;
          sys_clk_div = 1;
          Serial.println("\nSYSclk = 1x1");
          setup_sys_clock_divider(0x0);
          Serial.println(*SYSTEM_SCKDIVCR, BIN);
          break;
          }
        case '4':
          {
          if(moco_flag == true)                // If MOCO clock in use
            sys_clk_div_shift = 3 + 2;
          else
            sys_clk_div_shift = 3;
          loop_delay = 1;
          sys_clk_div = 16;
          Serial.println("\nSYSclk = 1/16");
          setup_sys_clock_divider(0x4);
          Serial.println(*SYSTEM_SCKDIVCR, BIN);
          break;
          }
        case '6':
          {
          if(moco_flag == true)                // If MOCO clock in use
            sys_clk_div_shift = 5 + 2;
          else
            sys_clk_div_shift = 5;
          loop_delay = 1;
          sys_clk_div = 64;
          Serial.println("\nSYSclk = 1/64");
          setup_sys_clock_divider(0x6);
          Serial.println(*SYSTEM_SCKDIVCR, BIN);
          break;
          }
#ifdef CLOCK_PLL            // Only use when external XTAL is fitted
        case '7':
          {
          if(moco_flag == true)
            {
            moco_flag = false;
            sys_clk_div_shift -= 2;
            }
          loop_delay = 1;
          Serial.println("\n Change to MOSC - XTAL & PLL"); 
          setup_sys_clock_pll();        
          pll_flag = true;
          break;
          }
#endif
        case '8':
          {
          sys_clk_div_shift += 2;
          loop_delay = 1;
          setup_sys_clock_moco();
          if(pll_flag == true)
            {
            pll_flag = false;
            Serial.println("\nChange from PLL to 8MHz MOCO"); 
            disable_sys_clock_pll();
            }
          else
            Serial.println("\nChange from HOCO to 8MHz MOCO"); 
            // Don't disable HOCO as used for USB-serial
          moco_flag = true;
          break;
          }
        case '9':
          {
          loop_delay = 1;
          setup_sys_clock_hoco();
          if(moco_flag == true)
            {
            moco_flag = false;
            sys_clk_div_shift -= 2;
            Serial.println("\nChange from MOCO to 48MHz HOCO");
            disable_sys_clock_moco();
            }
          else
            {
            pll_flag = false;
            Serial.println("\nChange from XTAL PLL to 48MHz HOCO");
            disable_sys_clock_pll();
            }
          break;
          }
        default:
          break;
        }
      }
    }
  else
    {
    if(seconds_count >= WFI_SECONDS_DELAY)  // Sortware reset needed to reinitialse USB with PC  
      { 
      *SCB_AIRCR = ((0x5FA << SCB_AIRCR_VECTKEY_Pos) | SCB_AIRCR_SYSRESETREQ_Msk);
      }
    }
  }

void print_status(void)
  {
  Serial.print("\nSysClk source  = ");
  if(pll_flag == true)                      // If external XTAL and PLL clock in use
    Serial.println("48 MHz PLL from XTAL");   
  else if(moco_flag == true)                // If MOCO clock in use
    Serial.println("8 MHz MOCO");
  else
    Serial.println("48 MHz HOCO");

  Serial.print("SysClk divider = 1/");
  Serial.print(sys_clk_div);

  Serial.print("\nMode = ");
  if(wfi_flag == false)
    Serial.println("No Sleeping");
  else
    Serial.println("Wake from IRQs");
  }

void print_command(void)
  {
  Serial.println("\nSingle Char Commands:");
  Serial.println("s = Enable  Sleep / Wake From Interrupt.");
  Serial.println("q = Disable Sleep / Wake From Interrupt.");
  Serial.println("u = USB Module Disable - code function HW-Reset after 10s delay.");
  Serial.println("0 = Set the System Clocks to IDE default.");
  Serial.println("4 = Change all System Clocks to x1/16 rate.");
  Serial.println("6 = Change all System Clocks to x1/64 rate.");
#ifdef CLOCK_PLL
  Serial.println("7 = Change System Clock to PLL with XTAL.");
  Serial.println("8 = Change System Clock to MOCO.");
  Serial.println("9 = Change System Clock to HOCO.");
#else
  Serial.println("8 = Change System Clock from HOCO to MOCO.");
  Serial.println("9 = Change System Clock from MOCO to HOCO.");
#endif
  Serial.println("m = Report mode - Clock settings, WFI or Constant-Looping, etc.");
  Serial.println("? = Display all Char Commands (this list).");
  }

/*
Restrictions on setting the clock frequency: ICLK ≥ PCLKA ≥ PCLKB, PCLKD ≥ PCLKA ≥ PCLKB, ICLK ≥ FCLKA
Restrictions on the clock frequency ratio: (N: integer, and up to 64)
ICLK:FCLK = N:1, ICLK:PCLKA = N: 1, ICLK:PCLKB = N: 1
ICLK:PCLKC = N:1 or 1:N, ICLK:PCLKD= N:1 or 1:N
PCLKB:PCLKC = 1:1 or 1:2 or 1:4 or 2:1 or 4:1 or 8:1
*/
// After reset:
// 0100 0100 0000 0100 MSBs
// 0100 0100 0100 0100 LSBs
// 
// From IDE
// 0001 0000 0000 0001 MSBs
// 0000 0001 0000 0000 LSBs

void setup_sys_clock_divider(int divider) // Divider for HOCO On-Chip Oscillator
  {
  *SYSTEM_PRCR      = 0xA501;            // Enable writing to the clock registers
  if(divider == 4)
    *SYSTEM_SCKDIVCR = 0x44044444;  // Set the clock dividerers to 1/16th rate
  else if(divider == 6)
    *SYSTEM_SCKDIVCR = 0x66066666;  // Set the clock dividerers to 1/64th rate
  else
    if(moco_flag == true)             // If MOCO clock in use, don't need wait on FLASH 
      *SYSTEM_SCKDIVCR = 0x00010100;  // Set the clock dividerers to main speeds
    else
      *SYSTEM_SCKDIVCR = 0x10010100;  // Set the clock dividerers to main speeds
    
  *SYSTEM_PRCR      = 0xA500;            // Disable writing to the clock registers
  }


char setup_sys_clock_moco(void)
  {
  *FCACHE_FCACHEE    = 0x0000;        // Disable the flash cache
  *SYSTEM_PRCR       = 0xA501;        // Enable writing to the clock registers
  *SYSTEM_MOCOCR     = 0x00;          // Start Middle-Speed On-Chip Oscillator Control Register
	asm volatile("dsb");                // Data bus Synchronization instruction
  delayMicroseconds(1);               // wait for MOCO to stabilise
  *SYSTEM_SCKSCR     = 0x01;          // Select MOCO as the system clock 
  *SYSTEM_PRCR       = 0xA500;        // Disable writing to the clock registers
  *FCACHE_FCACHEIV   = 0x0001;        // Invalidate the flash cache
  delayMicroseconds(1);               // wait for a moment
  char test = *FCACHE_FCACHEIV;       // Check that FCACHEIV.FCACHEIV is 0
  *FCACHE_FCACHEE    = 0x0001;        // Enable the flash cache
  return(test);
  }

char setup_sys_clock_hoco(void)
  {
  *FCACHE_FCACHEE    = 0x0000;        // Disable the flash cache
  *SYSTEM_PRCR       = 0xA501;        // Enable writing to the clock registers
// This part only needed is USB has been disable as otherwise HOCO is still running
  *SYSTEM_HOCOCR     = 0x00;          // Start High-Speed On-Chip Oscillator Control Register
	asm volatile("dsb");                // Data bus Synchronization instruction
  delayMicroseconds(1);               // wait for HOCO to stabilise

  *SYSTEM_SCKSCR     = 0x00;          // Select HOCO as the system clock 
  *SYSTEM_PRCR       = 0xA500;        // Disable writing to the clock registers
  *FCACHE_FCACHEIV   = 0x0001;        // Invalidate the flash cache
  delayMicroseconds(1);               // wait for a moment
  char test = *FCACHE_FCACHEIV;       // Check that FCACHEIV.FCACHEIV is 0
  *FCACHE_FCACHEE    = 0x0001;        // Enable the flash cache
  return(test);
  }

void setup_sys_clock_pll(void)        // Only use when external XTAL is fitted
  {
  *SYSTEM_PRCR     = 0xA501;          // Enable writing to the clock registers
  *SYSTEM_MOSCCR   = 0x01;            // Make sure XTAL is stopped
  *SYSTEM_MOMCR    = 0x00;            // MODRV1 = 0 (10 MHz to 20 MHz); MOSEL = 0 (Resonator)
  *SYSTEM_MOSCWTCR = 0x07;            // Set stability timeout period 
  *SYSTEM_MOSCCR   = 0x00;            // Enable XTAL
	asm volatile("dsb");                // Data bus Synchronization instruction
  char enable_ok = *SYSTEM_MOSCCR;    // Check bit 
  delay(100);                         // wait for XTAL to stabilise  
  *SYSTEM_PLLCR    = 0x01;            // Disable PLL
  *SYSTEM_PLLCCR2  = 0x07;            // Setup PLLCCR2_PLLMUL_4_0 PLL PLL Frequency Multiplication to 8x
  *SYSTEM_PLLCCR2 |= 0x40;            // Setup PLLCCR2_PLODIV_1_0 PLL Output Frequency Division to /2
  *SYSTEM_PLLCR    = 0x00;            // Enable PLL
  delayMicroseconds(1000);            // wait for PLL to stabilise
  *SYSTEM_SCKSCR   = 0x05;            // Select PLL as the system clock 
  *SYSTEM_PRCR     = 0xA500;          // Disable writing to the clock registers
  }

void disable_sys_clock_hoco(void)
  {
  *SYSTEM_PRCR   = 0xA501;       // Enable writing to the clock registers
  *SYSTEM_HOCOCR = 0x01;         // Stop the HOCO clock
  *SYSTEM_PRCR   = 0xA500;       // Disable writing to the clock registers
  }

void disable_sys_clock_moco(void)
  {
  *SYSTEM_PRCR   = 0xA501;       // Enable writing to the clock registers
  *SYSTEM_MOCOCR = 0x01;         // Stop the MOCO clock
  *SYSTEM_PRCR   = 0xA500;       // Disable writing to the clock registers
  }

void disable_sys_clock_pll(void)
  {
  *SYSTEM_PRCR   = 0xA501;       // Enable writing to the clock registers
  *SYSTEM_MOSCCR = 0x01;         // Stop the Main clock
  *SYSTEM_PRCR   = 0xA500;       // Disable writing to the clock registers
  }

