# LTDC parallel-RGB example

This only runs on a custom devboard, but might be useful for porting to your
own custom board with a parallel RGB display.

The test panel is a BuyDisplay ER-TFT3.95-1, 720x720px with an NV3052C controller IC.
The registers are setup by bit-banging over a SPI-ish protocol. See reference/pinout
for the connections.

The example draws various test patterns. Pressing a key 1, 2, or 3 will select a pattern:
1: Red/Blue gradient: increasing red going down, increasing blue going right. Green does a 
"fractal" hashmap.
2: Color bars: red, green, blue, white, with black underneath.
3: 4 rows of 8 cells: Rows 1-3 fade r/g/b to black. Row 4 goes: white, grey, black, black->red.

Other keys were used for debugging and bring-up and produce broken results, but I've 
left them here because they're bound to be useful for future board bring-ups.

Since this only runs on the devboard, USART1 is the default.


```
LTDC parallel-RGB Example
Filled buffer with RGB565 test pattern #3 (720x720)
LTDC pins init
RIF setup: LTDC secure, LTDC DMA master (RIMU 11) MSEC
Init LTDC pixel clock = 33.3 MHz (PLL4 src=HSE x30/1/1/1)
   with porches: 812x756 -> 54 Hz refresh
NV3052C init (158 register writes)
   readback: RDDPM(0Ah)=0x9C (expect 0x9C)  RDDMADCTL(0Bh)=0xA  RDDCOLMOD(0Ch)=0x70  RDDIM(0Dh)=0x0  RDID1(DAh)=0x30
LTDC peripheral configured

LTDC line counter: 96 -> 304  (advancing)
LTDC ISR flags: 0x0  (bit1: FIFO-warn, bit2: xfer-err, bit6: FIFO-err)
vblank IRQ: ok

Look at the display!

keys:  1/2/3 = pattern (gradient / bars / bit ladder)
       6/8/4 = panel bus 16-bit (0x55) / 18-bit (0x66) / 24-bit (0x77)
       p = pad self-test (stuck / shorted header lines)
       s = status readback    ? = this help
```

## Other keys/testing/debugging:

6/8/4: select 16/18/24 bit mode (only 24 bit works beacuse we wired up right-justified pins)
p: pad self-test (can detect open lines and some shorts, but keep in mind the display has pull-ups/downs)
s: status

