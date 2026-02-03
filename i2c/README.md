## I2C example

This example uses the on-board USBC PD controller IC (TCPP03-M20) which is already connected via I2C. 

It's not a very interesting demo since this chip only has 3 registers and they appear to be protected(?)

However, it demonstrates reading and writing to an external chip via I2C in both blocking and in interrupt mode.
The interrupts fire correctly. 

Using an oscilloscope, you can verify yourself that the I2C messages are indeed being acknowledged, and the
TCPP03 chip is responding to the requests to read and write (however, it just always returns 0's).
If you change the address of the chip, then you'll get I2C errors, which again confirms that I2C is working.

If you have your own dev board with an I2C chip on it, you can change the device address to match your chip,
and use the I2C2 peripheral on the GPIO header (header pins 27 and 28, port
pins PB4 and PB5). The pin setup for these is already being done. Also don't forget
to change all occurances of I2C1 to I2C2. The Flexbar clock init is shared between I2C1 and I2C2,
so no changes there are needed.


```
I2C Example
Setting I2C1 XBAR
PLL7 freq = 835511719
I2C1 kernel clock freq = 13054870
Enable I2C1 clock
Init I2C1 pins
Init I2C1 periph
Reading memory, blocking
Read 0x0 from register 0x0
Read 0x0 from register 0x1
Read 0x0 from register 0x2
Writing 0x21 to register 0x0 in blocking mode
Reading memory via interrupt
I2C IRQ
I2C mem rx callback
I2C IRQ
Read 0x0 from register 0x0
```

TODO: verify if indeed the TCPP03's registers are protected.
