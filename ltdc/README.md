# LTDC + LVDS example

This requires the LVDS display for the STM32MP257F-EV1 board. The display 
is an optional add-on (B-LVDS7-WSVGA EDT ETML0700Z9NDHA, 1024x600@60).

This demo brings up the LTDC and LVDS peripherals and draws a test pattern
(using the CPU, not the GPU): RGB gradient, 1px white border, x^y hash on
green. Offset, wrap, and stride bugs are visible at a glance.

The register-level recipe was extracted from ST's v6.6 kernel
(`drivers/gpu/drm/stm/lvds.c`, `ltdc.c`, `clk-stm32mp25*.c`, and the EV1
device tree for the panel timings). Each write in `lvds.cc` / `ltdc.cc` cites
its source line. Unlike the GPU, the LTDC is documented in RM0457 — the kernel
driver is used for the *sequence* and the DT for the *numbers*.

## The interesting parts

- **The LVDS PHY generates the pixel clock.** Its PLL makes the 378 MHz bit
  clock (7 bits per lane per pixel), and the pixel clock is bit-clock/7,
  routed to the LTDC through a SYSCFG mux (`DISPLAYCLKCR = 1`).
  From the 40 MHz HSE: `40 MHz x 189 / (4*5) = 378 MHz` — 54.000 MHz pixel, exact.
- **RIF**: LTDC registers are RIFSC peripherals 80 (+119 for the
  layers), LVDS is 84 — and the LTDC is also a DMA master (RIMU index 11)
  that must issue secure reads to pass the RISAF DDR firewall.
- **Scanout is self-verifying without a panel**: `LTDC->CPSR` is the live line
  counter — advancing means the pixel clock and timings are running; the ISR
  flags distinguish FIFO underruns (DDR/RIF trouble) from a merely dark panel.

## Running

```bash
make UART=2        # EV1 console is UART2
```

You should see all 8 bring-up stages print, `LTDC line counter: N -> M (advancing \o/)`,
ISR flags 0x1 (line event only — no underruns), and the test pattern on the
panel.

