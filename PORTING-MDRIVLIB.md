# Porting status: stm32mp2-baremetal drivers to use mdrivlib

The goal is to port the drivers in this repo to mdrivlib, so we have a
more-or-less unified API for our projects on various stm32 chips (F0, F4, G4,
F7, MP1, MP2).

## Status

The first pieces of `shared/` are ported to mdrivlib (`target/stm32mp2/`,
`target/stm32mp2_ca35/`): 
  - AArch64 vectors + exception dump
  - interrupt control and handler
  - GIC init
  - MMU/page tables
  - system_reg accessors
  - cache maintenance
  - RCC enable/reset/PLL/XBAR
  - SAI TDM

To port:
  - DMA
  - ADC
  - I2C
  - UART
  - GPIO

To write examples:
  - LTDC
  - HSEM/IPCC
  - SDMMC
  - NOR flash (QSPI)

