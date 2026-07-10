# GPU example

The STM32MP25x contains a VeriSilicon (Vivante) *GCNanoUltra31* 3D GPU, at
address 0x48280000. This example brings it up from cold and talks to it at the
register level, in stages that each prove one thing:

1. **Power**: VDDGPU is an independent supply: the STPMIC25's buck3 on the
   EV1, and it is **off after boot**. (The TF-A BL2 device tree marks it
   `regulator-always-on`, but that only prevents *disabling* -- BL2 registers
   the PMIC regulators and never enables ones it doesn't use itself.) So the
   example first talks to the PMIC over I2C7 (PD15=SCL, PD14=SDA, address
   0x33) and programs buck3 to 0.80 V + enabled. Then, like the other
   independent supplies, the PWR peripheral keeps the GPU domain electrically
   isolated until software verifies the supply with the voltage monitor and
   sets the "supply valid" bit (`PWR_CR12`).

2. **Clock**: PLL3 is dedicated to the GPU -- no flexgen channel is involved.
   We run it at 400 MHz, which is safe for the 0.80 V that the PMIC provides
   on VDDGPU (the 800/900 MHz datasheet speeds need 0.90 V).
   `RCC_GPUCFGR` holds the GPU's clock-enable and reset bits.

3. **RIF**: two sides to check whenever a peripheral "silently does nothing":
   - *Slave side*: the GPU's register port is RIFSC peripheral 79. It resets
     non-secure, which still admits our secure (EL3) CPU accesses, so nothing
     needs configuring. The example prints the state.
   - *Master side*: the GPU issues its own AXI transactions to DDR, and those
     pass through the DDR firewall (RISAF4), which our TF-A configures to
     admit only **secure** CID0/CID1 masters. The GPU's master attributes
     (RIMC ATTR entry 9) reset to non-secure, so without help every command
     fetch and pixel write would be *silently dropped*. The example sets the
     master's MSEC/MPRIV bits.

4. **Ping**: read the chip identity registers (model, revision, feature bits)
   and confirm the core reports idle. This is the "is anybody home?" test:
   if power, clock or RIF are wrong, these reads return zero.

5. **Command stream**: the FE (front end) is the GPU's command processor: it
   DMAs a command buffer from memory. We feed it a trivial NOP+END stream,
   proving the GPU can master the bus and fetch from DDR.

6. **Fill a buffer**: the actual "basic 2D operation". This GPU generation has
   no separate 2D blitter; 2D-style operations (clears, copies, mipmap
   generation) are done by the **BLT engine**. We build a command stream that
   programs a solid-color clear of a 64x64 RGBA8888 image in DDR, make the FE
   wait on a semaphore until the BLT engine finishes, then verify every pixel
   with the CPU.

Expected output ends with:

```
GPU filled a 64x64 RGBA image with 0x4D5A11AC -- verified by CPU. \o/

SUCCESS
```

## Where the register definitions come from

ST does not document the GPU register map (the reference manual stops at the
RCC/PWR/RIF plumbing, and the CMSIS device header declares `GPU_BASE` but no
register struct). Everything in `gpu_regs.hh` comes from the
reverse-engineered **etnaviv** project (MIT-licensed headers):

- Linux kernel: `drivers/gpu/drm/etnaviv/state_hi.xml.h`, `state.xml.h`,
  `cmdstream.xml.h`
- Mesa: `src/etnaviv/hw/state_blt.xml.h` (the BLT engine is not in the kernel
  headers), and the clear sequence mirrors `emit_blt_clearimage()` in Mesa's
  `src/gallium/drivers/etnaviv/etnaviv_blt.c`

The names keep etnaviv's `VIVS_*` structure so they are easy to
cross-reference against those sources and against `etnaviv_gpu.c` (whose
`etnaviv_hw_reset()` our soft-reset copies).

## Notes and gotchas

- **Caches**: the GPU reads and writes physical DDR directly (its own MMU is
  off, and GPU addresses are 32-bit bus addresses). The CPU must clean the
  dcache over the command buffer before kicking the FE, and invalidate over
  the image after the GPU writes it. Miss the invalidate and you'll "see" the
  pre-fill pattern even though DDR holds the filled pixels.
- **FE prefetch units**: `FE_COMMAND_CONTROL`'s prefetch count is in 64-bit
  units, and every command occupies a multiple of 64 bits -- a lone
  `LOAD_STATE` header is padded.
- **Interrupts**: this example polls `HI_IDLE_STATE` instead of using the
  GPU's interrupt (GIC SPI 215). The FE-stalls-on-BLT-semaphore trick makes
  "FE idle" mean "everything done".
- If the identity reads come back **zero**, suspect (in order): VDDGPU not
  valid, `RCC_GPUCFGR` enable/reset bits, PLL3 not locked, RIF slave config.
- If identity reads work but the FE never goes idle and `FE_DMA_ADDRESS`
  never advances past the buffer address, the GPU's *master* transactions are
  being blocked: RIMC ATTR[9] and the RISAF4 DDR regions are the suspects.

## Running

Same flow as the other examples:

```bash
make
make flash SD=/dev/diskX   # or copy build/main.uimg to the app partition
```
