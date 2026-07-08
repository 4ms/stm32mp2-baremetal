# Copro M33 (embedded technique)

This project demonstrates how the Cortex-M33 core of the STM32MP2 chip (the
"coprocessor", CPU2) can be started by the Cortex-A35 (CPU1) and run in parallel
with it.

Only loading, resetting, and starting the M33 is shown here.

TODO: Inter-core communication (IPCC/HSEM).

## The technique

The M33 firmware is embedded into the A35 firmware:

1. The M33 firmware is built on its own into an ELF, then flattened to a single
   binary (`build/corem33/firmware.bin`).
2. `xxd` converts that binary to a C array in `firmware_m33.h`.
3. The A35 firmware `#include`s that header, copies the array into SRAM2, tells
   the CA35 subsystem where the M33 vector table is, and releases the M33 from
   hold-boot.

Everything lives in one file (`build/corea35/main.elf/bin`).

### Memory layout

The whole M33 image (vector table + code + rodata + data) is placed contiguously
in **SRAM2**, at the M33 non-secure instruction-fetch alias **`0x0A060000`**
(128 kB). The A35 sees the same physical SRAM2 at the same address (see the SRAM
mappings in `shared/mmu/mmu.cc`), so it copies the blob straight there and writes
`0x0A060000` into the M33's vector-table register.

The M33 runs non-secure, matching how the A35 maps the peripheral space as
non-secure. This lets the M33 reuse the exact same peripheral addresses the A35
uses (`shared/print/uart_print.c`), so both cores share the console UART.

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
A35: tick 8
M33: tick 4
...
```

## Building

Requires both toolchains:
- `aarch64-none-elf-gcc` (A35)
- `arm-none-eabi-gcc` (M33)

```
make            # builds M33, embeds it, builds A35 -> build/corea35/main.uimg
```

The console UART defaults to USART2 (ST-LINK / USB-C). Override the A35 and M33 console
with e.g. `make UART_CHOICE=1`. 


## TODO

- Try running the M33 secure so it can drive secure GPIO pins
   - enable `CA35SYSCFG->M33_TZEN_CR` CFG_SECEXT
   - write `M33_INITSVTOR_CR` (not the NS one) with the secure fetch alias `0x0E060000`
   - link the M33 at 0x0E060000.

