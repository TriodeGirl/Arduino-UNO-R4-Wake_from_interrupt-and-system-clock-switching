Arduino UNO R4 test-code to demonstrate:
  1. Wake-From-Interrupt
  2. Change System-Clock divisors 1/1, 1/16, and 1/64 settings 
  3. Switching between 48MHz HOCO and 8MHz MOCO internal oscillators (and PLL with XTAL if fitted)
  4. USB Module Disable (with code initiated Reset after timeout delay)

Unlike a linear regulator which adds to the current, the onboard DC/DC converter trades Volts for Current and therefor the readings when powered from an external 9.0V supply to Vin show a lower power-consumption than would be measured on the +5V rail input itself (I don't have a USB DVM module). 

Standard R4 Minima - LED series resistors all 330 ohm
=====================================================
Board running with +9.0V VIN at 48 MHz HOCO clock with default IDE settings:
  without sleep, USB active = c. 21.5 mA
  with WFI and USB active   = c. 13.3 mA
  with WFI but USB disabled = c. 11.2 mA

To properly get low-power operation some board mods are needed, as supplied the ON LED takes 8 to 9 mA from the +5V rail just by itself!
It is likely that low-power modes are mostly of interest for stand-alone setups which are not connected to a host USB. 

Board changes for Low_Power
===========================
  1. ON LED series resistor changed to 2.7K, and LED VCC connection changed to VUSB.
  2. L  LED series resistor changed to 1.7K (which = 1.8mA if 100% on).

Board running with +9.0V VIN at 48 MHz HOCO clock with default IDE settings:
  without sleep, USB active = c. 15.5 mA
  with WFI and USB active   = c.  7.5 mA
  with WFI but USB disabled = c.  5.3 mA

Board running with +9.0V VIN at 8 MHz MOCO clock with default IDE settings:
  without sleep, USB active = c.  6.2 mA - HOCO still running for USBFS Module
  with WFI and USB active   = c.  4.1 mA
  with WFI but USB disabled = c.  2.8 mA - HOCO stopped

Further measurments with 1/16 and 1/64 clock divisors in C code file.

![231216-susan-minima-board-mods-1](https://github.com/TriodeGirl/Arduino-UNO-R4-Wake_from_interrupt-and-system-clock-switching/assets/139503623/347589c4-d4c1-4e68-8b27-254f3dda9e30)

I have also added an external 12MHz XTAL.
