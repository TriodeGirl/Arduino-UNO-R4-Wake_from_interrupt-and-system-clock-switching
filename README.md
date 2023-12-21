Arduino UNO R4 test-code to demonstrate:
  1. Wake-From-Interrupt
  2. Change System-Clock divisors 1/1, 1/16, and 1/64 settings 
  3. Switching between 48MHz HOCO and 8MHz MOCO internal oscillators (and PLL with XTAL if fitted)
  4. USB Module Disable (with code initiated Reset after timeout delay)

To properly get low-power operation some board mods are needed, as supplied the ON LED takes 8 to 9 mA just by itself!
  1. ON LED series resistor changed to 2.7K, and LED VCC connection changed to VUSB.
  2. L  LED series resistor changed to 1.7K (which = 1.8mA if 100% on).

![231216-susan-minima-board-mods-1](https://github.com/TriodeGirl/Arduino-UNO-R4-Wake_from_interrupt-and-system-clock-switching/assets/139503623/347589c4-d4c1-4e68-8b27-254f3dda9e30)

I have also added an external 12MHz XTAL.
