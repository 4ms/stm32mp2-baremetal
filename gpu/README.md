# GPU example

The STM32MP25x contains a VeriSilicon (Vivante) *GCNanoUltra31* 3D GPU.
This example brings it up in stages:

1. **Power**: VDDGPU is an independent supply: the STPMIC25's buck3 on the
   EV1 controls it. We have configured TF-A to turn this on, but just in case
   it will check and try to enable it via the PMIC (FIXME: PMIC configuration 
   via I2C doesn't work!)
   The example verifies the supply with the PWR voltage monitor and sets the
   "supply valid" bit (`PWR_CR12`) to remove the GPU domain's electrical
   isolation, like the other independent supplies.

2. **Clock**: PLL3 is dedicated to the GPU. We run it at 400 MHz, which is safe 
   for the 0.80 V that the PMIC provides on VDDGPU (the 800/900 MHz datasheet
   speeds need 0.90 V). `RCC_GPUCFGR` holds the GPU's clock-enable and reset bits.

3. **RIF**: The GPU's register port is RIFSC peripheral 79. It resets
     non-secure, so we set it to secure because our buffers are in secure regions
     of DDR4 memory. We also setup RIMC to allow secure and priv transactions.

4. **Ping**: read the chip identity registers (model, revision, feature bits)
   and confirm the core reports idle. This is the "is anybody home?" test:
   if power, clock or RIF are wrong, these reads return zero.

5. **Command stream**: the FE (front end) is the GPU's command processor: it
   DMAs a command buffer from memory. We feed it a trivial NOP+END stream,
   proving the GPU can master the bus and fetch from DDR.

6. **Fill a buffer**: the actual "basic 2D operation". This GPU has no 2D
   pipe, and -- despite its own feature registers claiming otherwise -- **no
   BLT engine** either (the authoritative feature list is the hardware
   database entry ST upstreamed to Linux: `etnaviv_hwdb.c`, customer_id 0x15;
   writing the nonexistent BLT registers wedges the FE with no fault). Fills
   and blits on this core go through the **RS ("resolve") engine**, the same
   path Mesa uses for clears here. We build a command stream that programs a
   solid-color RS fill of an RGBA8888 image in DDR, stall the FE on the PE
   pipeline, flush the color cache, then verify every pixel with the CPU.

7. **Blit + format-convert**: a fill is pure write bandwidth, and there the
   GPU has no edge over a CPU `memset` -- it wins on *work per byte* and on
   running async while the CPU does something else. So this step exercises a
   real per-pixel transform: the RS engine copies a source image into the
   destination while **swapping the red and blue channels** in flight
   (RGBA<->BGRA, via `VIVS_RS_CONFIG_SWAP_RB`). Same RS submit path as the
   fill, but with a real source, no clear mode, and the swap bit; the CPU
   verifies `dest[i] == swap_rb(source[i])`.

Expected output ends with:

```
GPU filled 1024x1024 with 0x4D5A11AC -- verified. \o/
GPU copied 1024x1024, swapping R<->B -- verified. \o/

SUCCESS
```

## The `etna` API

The example is built on a small reusable GPU API (`etna.hh` / `etna.cc`) rather
than open-coded command streams. It's modeled on the clean, MIT-licensed seam
in the middle of the Linux etnaviv stack -- libdrm's `etna_cmd_stream` /
`etna_bo` interface, the contract that Mesa's operation emitters are written
against. By providing that seam on baremetal, Mesa's MIT operation code
(`etnaviv_rs.c`, ...) ports near-mechanically; what can't come along is Mesa
itself (Gallium + the NIR shader compiler), so 3D would use offline-compiled
shaders.

The surface:

- `etna::Gpu` -- `init()` does all bring-up (power/clock/RIF/reset/identify);
  `alloc()` carves buffers from a DDR pool; `submit()`/`wait()`/`submit_and_wait()`
  run a command stream.
- `etna::Bo` -- a buffer object (physically-contiguous DDR; on this identity-map
  system, cpu pointer == physical == GPU address). `cpu_prep()`/`cpu_fini()`
  make the cache bracketing explicit so it can't be forgotten.
- `etna::CmdStream` -- a growable command buffer with libdrm/Mesa-shaped
  `emit`/`reserve`/`set_state`/`emit_reloc` helpers.
- `etna::clear()` / `etna::blit()` -- the RS fill and copy+convert operations.

Each `[HAVE]`/`[NEW]`/`[LATER]` tag in `etna.hh` marks the roadmap: the ring
buffer (WAIT/LINK) for throughput, IRQ-driven async completion, the GPU MMU,
and the 3D pipe are the deferred pieces. The whole `main.cc` is now just
`gpu.init()`, two `alloc()`s, and three API calls with CPU verification.

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

- **PLL3 is inside the GPU subsystem**, not the RCC (the RCC only feeds it a
  reference clock, `ck_gpuss_pll3_fref`). Touching the `RCC_PLL3CFGRx`
  registers while the GPU is unpowered or unclocked hangs the bus.
  Order of operations must be: VDDGPU valid, then `GPUEN`, then program PLL3,
  and pulse `GPURST` only after the kernel clock is running (RM0457 requires
  the ref clock to be slower than the GPU bus/kernel clocks during reset).
- **The VDDGPU voltage monitor must stay enabled.** Its live output is what
  releases the GPU power-domain reset which keeps PLL3 out of reset.
- **Caches**: the GPU reads and writes physical DDR directly (its own MMU is
  off, and GPU addresses are 32-bit bus addresses). The CPU must clean the
  dcache over the command buffer before kicking the FE, and invalidate over
  the image after the GPU writes it.
- **FE prefetch units**: `FE_COMMAND_CONTROL`'s prefetch count is in 64-bit
  units, so you have to add padding if needed.
- **Interrupts**: this example polls `HI_IDLE_STATE` instead of using the
  GPU's interrupt (GIC SPI 215). The FE-stalls-on-BLT-semaphore trick makes
  "FE idle" mean "everything done".
- If the identity reads come back zero, check if VDDGPU is
  valid, `RCC_GPUCFGR` enable/reset bits, PLL3 not locked, and RIF config.
- If identity reads work but the FE never goes idle and `FE_DMA_ADDRESS`
  never advances past the buffer address, the GPU's master transactions are
  being blocked, so check RIMC `ATTR[9]` and the `RISAF4` DDR regions.

## Running

Same flow as the other examples:

```bash
make
make flash SD=/dev/diskX   # or copy build/main.uimg to the app partition
```
