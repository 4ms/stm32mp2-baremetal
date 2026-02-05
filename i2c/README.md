## I2C example

This example communicates with the on-board USBC PD controller IC (TCPP03-M20)
on the EV1 board.

This demonstrates reading and writing to an external chip via I2C in both
blocking and in interrupt mode. The interrupts fire correctly. 

It's not a very interesting demo since this chip only has 3 registers and they
usually return all 0's. The third register seems to return non-zero (0x20) if a
USB-C host with power capabilities is connected to the USB-DRD port on the EV1.

Using an oscilloscope, you can verify yourself that the I2C messages are indeed
being acknowledged, and the TCPP03 chip is responding to the requests to read
and write. If you change the address of the chip, then you'll get I2C errors,
which again confirms that I2C is working.

If you have another board with an I2C chip on it, you can change the device
address to match your chip, and use the I2C2 peripheral on the GPIO header
(header pins 27 and 28, port pins PB4 and PB5). The pin setup for these is
already being done. Don't forget to change all occurances of I2C1 to I2C2.
The Flexbar clock init is shared between I2C1 and I2C2, so no changes there are
needed.


```
``I2C Example
Setting I2C1 and I2C2 XBAR
Enable I2C1/2 clock
Init I2C1 pins
Init I2C2 pins
Init I2C1 periph
Reading memory, blocking
Read 0x0 from register 0x0
Read 0x0 from register 0x1
Read 0x20 from register 0x2
Writing 0x21 to register 0x0 in blocking mode
Reading memory via interrupt
I2C IRQ
I2C mem rx callback
I2C IRQ
Read 0x0 from register 0x0
Tick = 11333
Tick = 11442
....



TODO: verify if the TCPP03's registers read-outs make sense, based on its datasheet.
