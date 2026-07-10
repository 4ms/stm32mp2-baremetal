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
in **SRAM2** (128 kB). The M33 is linked at its *secure* instruction-fetch alias
of SRAM2, **`0x0E060000`**, which is also what goes into the M33's secure
vector-table register `M33_INITSVTOR_CR`. The A35 sees the same physical SRAM2
at **`0x0A060000`** (see the SRAM mappings in `shared/mmu/mmu.cc`), so it copies
the blob there — both addresses reach the same RAM.

### Security model

The M33 runs **secure**:

- The A35 sets `CA35SYSCFG->M33_TZEN_CR` `CFG_SECEXT` = 1 and writes the
  *secure* vector register `M33_INITSVTOR_CR` = `0x0E060000` (the secure
  instruction-fetch alias of SRAM2). The M33 image is linked at that address.
- A secure instruction fetch to a non-secure RISAB page is an illegal access
  (`SRWIAD` only forgives secure *data* accesses), and our EL3 startup
  (`shared/drivers/risab.cc`) cleared all page security bits — so the A35 sets
  SRAM2's RISAB4 pages back to secure before releasing the M33.
- The M33 leaves its SAU disabled, so the entire address map is Secure and all
  of its bus accesses are secure transactions. That lets it use the exact same
  peripheral addresses the A35 uses (`0x4xxxxxxx`, no `0x5xxxxxxx` secure
  aliases needed) — both cores share the console UART driver unchanged
  (`shared/print/uart_print.c`).
- GPIO pins reset to secure (`GPIOx_SECCFGR` = `0xFFFF`, RM0457 §24.4.12). A
  *non-secure* M33's GPIO writes would be silently ignored; the secure M33 can
  drive any pin directly, no per-pin handover needed.

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
with e.g. `make UART=6` (as discussed in the top-level [README](../README.md)). 


## Running the M33 non-secure instead

The demo previously ran the M33 non-secure; to go back:

- Clear `CA35SYSCFG->M33_TZEN_CR` `CFG_SECEXT` and write `M33_INITNSVTOR_CR`
  (not the secure one) with the non-secure fetch alias `0x0A060000`.
- Link the M33 at `0x0A060000` (and `_estack = 0x0A080000`).
- Leave SRAM2's RISAB pages non-secure (drop `sram2_set_secure()`).
- Hand over every GPIO pin the M33 will drive by clearing its bit in
  `GPIOx_SECCFGR` from the A35 (pins reset to secure, so a non-secure M33's
  GPIO writes are silently ignored otherwise).

