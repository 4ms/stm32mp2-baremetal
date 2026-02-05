## HAL Audio example

This example demonstrates using the HAL SAI and DMA drivers to send/receive audio from a codec with I2S.

The SAI peripheral is fairly straight-forward. The DMA setup is not: the HPDMA
driver does not have a native circular mode, so we are using linked-lists with
a circular queue to make this work. ST's HAL has an interface for setting this
up, which we are using here. The DMA queue and DMA list nodes are used by the
DMA peripheral, which cannot see the cache. Therefore you must clean the linked
lists from the cache after configuring them, but before using them.

The SAI DMA writes and reads from a buffer you provide. This must also be made
cache-consistant, so invalidating the rx_buffer before reading it, and cleaning
the tx_buffer after writing it will ensure the DMA and our app "see" the same
data.

There is no on-board codec on the EV1, so this example requires an external
board with the PCM3168 codec. You can easily change this to another codec since
I2S is a standard protocol. The I2C register setup is in i2c_codec.hh, so
adjust those register values to match what your codec needs.

Connect the 5 I2S lines and 2 I2C lines from the EV1 to your board. See
gpio_header_pinout.txt. If your codec requires MCLK, then you have to get that
from R34, or pin 8 of U23. I was able to use a micro-grabber to connect
reliably to pin 8 of this IC since it's SOIC-8, the pins are fairly large.

If your codec needs a reset pin, then also connect that.

The example produces two sine waves on the left channel output. The right input
channel is copied to the right output channel.



```
HAL Audio Example
tx buffer: 90004140
Setting SAI2 XBAR to use PLL8
PLL8 freq = 307200000
SAI2 kernel clock freq = 12288000
Enable SAI clock
Init SAI pins
Init SAI TX periph
Init SAI RX periph
Link DMA TX and SAI TX
Link DMA RX and SAI RX
Setting I2C2 XBAR
Enable I2C2 clock
Init I2C2 pins
Init I2C2 periph
Start transmitting/receiving
Tick = 9858
Tick = 12858
```

