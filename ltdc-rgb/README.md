# LTDC parallel-RGB example

This only runs on a custom devboard, but might be useful for porting to your
own custom board with a parallel RGB display.

The test panel is a BuyDisplay ER-TFT3.95-1, 720x720px with NV3052C.
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
LTDC parallel-RGB Example (NV3052C / ER-TFT3.95-1)
==================================================

1. RGB565 test pattern at 0x90000000 (720x720)
2. 22 LTDC pads -> AF13 (R/G/B[7:2], CLK/HSYNC/VSYNC/DE)
3. RIF: LTDC secure, LTDC DMA master (RIMU 11) MSEC
4. LTDC clock on, reset pulsed, pixel clock = ck_ker_ltdc = 33.3 MHz (PLL4 src=HSE x30/1/1/1)
   812x756 total -> 54 Hz refresh
   LTDC version 0x40101
5. NV3052C: SWRESET + vendor init over bit-banged SPI (158 writes), 24-bit bus, sleep out, display on
   readback: RDDPM(0Ah)=0x9C (expect 0x9C)  RDDMADCTL(0Bh)=0xA  RDDCOLMOD(0Ch)=0x70  RDDIM(0Dh)=0x0  RDID1(DAh)=0x30
6. LTDC configured + enabled (RGB565 layer, DE mode, HS/VS low, DE high, data on falling CLK)

LTDC line counter: 338 -> 546  (advancing \o/)
LTDC ISR flags: 0x0  (bit1 FIFO-warn, bit2 xfer-err, bit6 FIFO-err)
vblank IRQ: ok

Scanout running -- look at the panel!
```

## Other keys/testing/debugging:

6/8/4: select 16/18/24 bit mode (only 24 bit works beacuse we wired up right-justified pins)
p: pad self-test (not reliable, but can detect some shorts)
s: status

