## ADC example

This example demonstrates using the ADC to read analog voltages.

It uses the internal Voltage Reference, so make sure the VREF+ pin is not connected on your board.
TODO: check EV1 schematic before merging this into main!

The example runs through four phases:

1) Enables the VDDA18ADC supply (confirmed with the PWR voltage monitor) and the
   VREFBUF. The MP25 VREFBUF supports two output voltages selected by the VRS
   bit: ~1.212V (VRS=0, the raw bandgap) or 1.5V (VRS=1). Select with
   `UseVrefScale` in main.cc (default 1.5V) and verify the VREF+ pin with a
   scope.

2) Takes single software-triggered conversions, polling for completion:
   - VREFINT: the internal reference. On MP2 this is 0.8V typical (0.792V to
     0.808V per the datasheet) -- NOT the 1.21V bandgap of older STM32
     families. Expect readings around 800 mV. A sanity check needing no wiring.
   - A CH4 (PG4) self-test: the pin is driven low, then pulled up, by its own
     GPIO cell while the ADC samples it, verifying the pad-to-ADC path without
     any external wiring.
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
- MP2 HAL quirk: HAL_ADC_Init() and HAL_ADC_ConfigChannel() return HAL_ERROR
  if the ADC is enabled at all (stricter than other STM32 HALs), and both
  calibration and HAL_ADC_Start() leave it enabled. So the ADC must be stopped
  (HAL_ADC_Stop() disables it) before any init or channel config -- see
  adc_config_channel() in main.cc.
