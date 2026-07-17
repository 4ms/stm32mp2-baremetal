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
  routed *back* to the LTDC through a SYSCFG mux (`DISPLAYCLKCR = 1`). So the
  bring-up order is forced: LVDS PLL first, LTDC second. From the 40 MHz HSE:
  `40 MHz x 189 / (4*5) = 378 MHz` — 54.000 MHz pixel, exact.
- **RIF, as always**: LTDC registers are RIFSC peripherals 80 (+119 for the
  layers), LVDS is 84 — and the LTDC is also a *DMA master* (RIMU index 11)
  that must issue secure reads to pass the RISAF DDR firewall, exactly like
  the GPU. Miss that and the layer FIFO starves: black screen + underrun flags.
- **MP25's LTDC latches layer registers per-layer** (`L1RCR` = immediate or
  at-vblank), not via the classic global `SRCR`. Forget the reload and you get
  perfect timings and a black screen while every register reads back correct.
- **One gate bit per IP**: `RCC_LTDCCFGR`/`RCC_LVDSCFGR` bit 1 enables both the
  bus and kernel clocks; bit 0 is the reset; `LVDSCFGR` bit 15 selects HSE as
  the PHY PLL reference (no flexgen involved at all).
- **Scanout is self-verifying without a panel**: `LTDC->CPSR` is the live line
  counter — advancing means the pixel clock and timings are running; the ISR
  flags distinguish FIFO underruns (DDR/RIF trouble) from a merely dark panel.

## Files

| file | what |
|------|------|
| `panel_etml0700z9.hh` | panel timings (from the EV1 DT) + derived LTDC values + PLL solution |
| `lvds.cc/hh` | LVDS PHY PLL + host (vesa-24 mapping), ported 1:1 from lvds.c |
| `ltdc.cc/hh` | timings, one ARGB8888 layer, per-layer reload, diagnostics |
| `main.cc` | the 8-step bring-up ladder with staged prints |

## Running

```bash
make UART=2        # EV1 console is UART2
make flash-t32
tail ~/minicom-ev1-cap.log
```

Expected: all 8 stages print, `LTDC line counter: N -> M (advancing \o/)`,
ISR flags 0x1 (line event only — no underruns), and the test pattern on the
panel.

## Next

Point the GPU at it: render the spinning cube (see `gpu/`) into a tiled render
target, `etna::resolve()` into this framebuffer, flip with
`ltdc_set_framebuffer()` (vblank-latched) — a live spinning cube on glass.
