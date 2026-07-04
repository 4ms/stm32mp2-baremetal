# Copro M33 (embedded technique)

This project demonstrates how the Cortex-M33 core of the STM32MP2 chip (the
"coprocessor", CPU2) can be started by the Cortex-A35 (CPU1) and run in parallel
with it.

It is the STM32MP2 counterpart of the `copro_embedded` example in the
[stm32mp1-baremetal](https://github.com/4ms/stm32mp1-baremetal) repo (there the
A7 starts the M4). Only loading, resetting, and starting the M33 is shown here;
inter-core communication (IPCC/HSEM) is left for a different project.

## The technique

The M33 firmware is embedded into the A35 firmware:

1. The M33 firmware is built on its own into an ELF, then flattened to a single
   binary (`build/corem33/firmware.bin`).
2. `xxd` converts that binary to a C array in `firmware_m33.h`.
3. The A35 firmware `#include`s that header, copies the array into SRAM2, tells
   the CA35 subsystem where the M33 vector table is, and releases the M33 from
   hold-boot.

Everything lives in one file (`build/corea35/main.uimg`) that u-boot can load,
exactly like the other projects in this repo.

### Memory layout

The whole M33 image (vector table + code + rodata + data) is placed contiguously
in **SRAM2**, at the M33 non-secure instruction-fetch alias **`0x0A060000`**
(128 kB). The A35 sees the same physical SRAM2 at the same address (see the SRAM
mappings in `shared/mmu/mmu.cc`), so it copies the blob straight there and writes
`0x0A060000` into the M33's vector-table register.

The M33 runs **non-secure**, matching how the A35 maps the peripheral space as
non-secure. This lets the M33 reuse the exact same peripheral addresses the A35
uses (`shared/print/uart_print.c`), so both cores share the console UART.

### Start sequence (A35 side, `main_a35.cc`)

Follows OP-TEE's `stm32_remoteproc` driver:

1. `block_ram_enable_el3()` (called from `startup_a35.s`) opens all SRAM blocks
   to every CID / non-secure via RISAB, so the A35 can load SRAM2 and the M33
   can fetch from it.
2. Park CPU2 for a clean state: clear `RCC->CPUBOOTCR.BOOT_CPU2` (hold-boot) and
   set `RCC->C2RSTCSETR.C2RST` (reset).
3. Copy the M33 blob to `0x0A060000` and clean the A35 data cache over it.
4. `CA35SYSCFG->M33_TZEN_CR` &= ~CFG_SECEXT (run non-secure) and
   `CA35SYSCFG->M33_INITNSVTOR_CR = 0x0A060000`.
5. Set `RCC->CPUBOOTCR.BOOT_CPU2` to release hold-boot. The M33 boots from
   `INITNSVTOR`; the hardware auto-releases the M33 reset.

## What it does

- **A35** prints from `Cortex-A35` and toggles **PH8**.
- **M33** prints from `Cortex-M33` and toggles **PF9**.

Both print an occasional "tick" so you can see them running concurrently over
the shared console UART.

Expected output (interleaving depends on timing):

```
A35: Hello from Cortex-A35!
A35: Loading M33 firmware (1808 bytes) into SRAM2
A35: Starting M33 core after LED goes off in 3 2 1 now
A35: M33 released from hold-boot
M33: * yawn *
M33: Hello from Cortex-M33!
M33: I will toggle PF9 and print occasionally.
A35: tick 4
M33: tick 4
...
```

## Building

Requires both toolchains:
- `aarch64-none-elf-gcc` (A35)
- `arm-none-eabi-gcc` (M33)

```
make            # builds M33, embeds it, builds A35 -> build/corea35/main.uimg
make clean
```

Copy `build/corea35/main.uimg` to the SD card as usual (see the repo root
README), or `make install` to copy it to a mounted `BAREAPP` partition.

The console UART defaults to USART2 (ST-LINK / USB-C). Override the A35 console
with e.g. `make UART_CHOICE=1`. The M33 console is set in `makefile_m33.mk`
(`-DUART=2`).

## Notes / assumptions

- The M33 boot registers (`CA35SYSCFG->M33_*`) and `RCC->CPUBOOTCR` must be
  writable by the A35. The board's TF-A/RIF configuration leaves `CPUBOOTCR`
  assigned to the A35 (CID1, non-secure), and this repo's A35 already drives
  RCC/GPIO/UART non-secure, so no extra RIF setup is done here. If you retarget
  to a locked-down secure configuration you may need to adjust this.
- To run the M33 **secure** instead: enable `CA35SYSCFG->M33_TZEN_CR` CFG_SECEXT,
  write `M33_INITSVTOR_CR` (not the NS one) with the secure fetch alias
  `0x0E060000`, and link the M33 there.
