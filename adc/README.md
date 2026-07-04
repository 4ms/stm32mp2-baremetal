## ADC example

This example demonstrates using the ADC to read analog voltages.

It uses the internal Voltage Reference, so make sure the VREF+ pin is not connected on your board.
TODO: check EV1 schematic before merging this into main!

The example runs through four phases:

1) Enables the VDDA18ADC supply (confirmed with the PWR voltage monitor) and the
   VREFBUF. Verify with a scope that the VREF+ pin is high (1.5V or 1.1V), and
   adjust `VrefMillivolts` in main.cc to match what you measure.

2) Takes single software-triggered conversions, polling for completion:
   - VREFINT (the internal ~1.21V bandgap): a sanity check that needs no
     wiring at all. Expect readings around 1210 mV.
   - ADC1 Channel 4 (PG4), which is on adaptor board pin B34. Connect it to a
     voltage divider between VREF+ and VREF- and the readings should track the
     divider voltage.

3) Converts CH4 continuously into a circular DMA buffer (HPDMA1 channel 0 in
   linked-list circular mode, hardware request from ADC1). Prints min/max/avg
   of the buffer once per second for 5 seconds.

4) Scans two channels (CH4 rank 1, VREFINT rank 2) into the same circular DMA
   buffer, interleaved. Prints per-channel stats once per second, forever.

Notes:

- The ADC is clocked from the ck_icn_ls_mcu bus clock divided by 4, so no
  flexgen kernel clock setup is needed.
- The sample buffer lives in RETRAM, which is cached (mapped Normal memory).
  DMA writes are not coherent with the dcache, so the CPU invalidates the
  buffer's cache lines before reading stats. Likewise the DMA linked-list node
  is fetched from memory by the DMA controller each loop, so it's cleaned to
  memory after setup (see dma.hh).
- The ADC1 global interrupt is enabled only to report overrun errors; the
  buffer-complete callback comes from the HPDMA channel interrupt.
