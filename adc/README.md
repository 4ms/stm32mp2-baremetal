## ADC example

This example demonstrates using the ADC to read analog voltages.

It uses the internal Voltage Reference, so make sure VREF+ pin in not connected on your board.
TODO: check EV1 schematic before merging this into main!

TODO:
1) Enable VREF+, verify with scope VREF+ pin is high (1.5V or 1.1V)
2) Take ADC readings on a pin while human connects an ADC pin to voltage divider between VREF+ and VREF-.
   Suggest: ADC1 Channel 4 (PG4) which is on adapator board pin B34
3) Setup ADC DMA to continually read ADC
4) Setup multiple channels of ADC readings

