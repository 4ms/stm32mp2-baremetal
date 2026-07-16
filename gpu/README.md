# GPU example

This example brings up the GPU clocks/power/reset and shows three engines:
- RS: (Resolve Engine) to do solid fills and blits
- PPU: (Parallel Processing Unit) for running shaders that we generate. The
  shaders are small "programs" that run arbitrary per-pixel computations
- 3D graphics pipe: Drawing a triangle via vertex fetch -> vertex shader ->
  rasterizer -> fragment shader -> pixel engine

I knew very little about how GPUs work before doing this project,
so I learned a lot in getting the GPU to do some real graphics
processing, from the ground up. 

The GPU is a Vivante (VeriSilicon) GC8000 Nano Ultra VIP (rev 0x6205, "HALTI5"
generation). I believe it has:
  — 2 unified shader cores
  - 1 pixel pipe
  - full 3D graphics pipeline
  - 2 NN cores

The etnaviv project, the Mesa project, the Linux etnativ drivers, and the gcnano
sources were the main sources of knowledge, as there are no reference manuals
for this GPU like we have for the CPU. Everything is built on a small reusable
API (`etna.hh`).

## Bring-up (`etna::Gpu::init()`)

1. **Power**: VDDGPU is an independent supply, driven by the STPMIC25's buck3 on
   the EV1. If TF-A has not turned it on, `init()` enables it over I2C7. 

2. **Clock**: PLL3 is dedicated to the GPU; we run it at 800 MHz. That speed
   needs VDDGPU at 0.90 V. The PMIC init in this project sets buck3 to 0.90 V
   (400 MHz would run at the 0.80 V default).

3. **RIF**: the GPU's register port is RIFSC peripheral 79. We set it secure so
   the GPU issues secure transactions that pass the RISAF firewall to our DDR
   buffers.

At this point we are able to read the chip identity (model 0x8000, rev 0x6205,
product 0x80003, customer 0x15) to confirm the GPU is powered up and responding.

Next, we set up a ring buffer to hold our comannds. At the tail we put
WAIT/LINK commands so the GPU runs in an idle self-loop whenever it gets to
the end
processing commands we gave it. When we want to queue some commands we append
them and move the
WAIT/LINK to the end. That way the GPU doesn't need to be re-init each time we
use it.

## RS engine — 2D fill and blit

We use the Resolve engine (RS) to fill or do simple operations on areas of
pixels. We do this by putting a list of commands into the command buffer that
specify things like the input/output buffer address and size, pixel format,
color to fill, etc. Then we kick off the front end engine (FE) to process these
commands.

When reading the GPU's feature bits, the BLT bit is set. From what I understand,
the BLT engine would be more appropriate for this kind of operation, but the
etnaviv hardware database in mainline Linux says there is no
BLT engine for our chip. To support that, there's a comment that the database
takes precedence over the feature bits in the registers. On our chip, attempts
to drive BLT registers hangs the FE, and since the Mesa driver uses the RS
engine instead of the BLT for clears, we do that too. 

The RS engine gets its name "resolve" from its primary function of resolving
the tiled GPU-native pixel ordering into the linear ordering a display driver
would expect.

We do two tests:
- `test_fill()`: calls `etna::clear()` to solid-color fill of an RGBA8888 image
- `test_blit_convert()`: calls `etna::blit()` to copy with a per-pixel
  transform. The test swaps R<->B pixels (BlitFlags::BlitSwapRB)

The RS is very slow at filling bytes in DDR RAM: a 1024x1024 fill is 4x slower
than a CPU memset (about ~850k ticks vs. ~200k ticks). This is because GPUs are
good at doing parallel operations but a fill is a serial memory write (without
any benefit of caches). It's clearly not going to give us big wins to use the
GPU to fill large areas, but it runs asynchronously (so is basically "free") and
won't dirty our CPU's L1/L2 data cache if we only intend to fill an area of a
framebuffer that's directly displayed on a screen. Still, the HPDMA would
probably be much faster if you needed to fill a large area while the CPU does
other work.

A final test we do after `test_fill()` and `test_blit_convert()` is to test
the throughoutput of the ring buffer when using the WAIT/LINK scheme vs. when
just sending commands sequentially.

## Compute — running shaders on the PPU

The PPU is the Parallel Processing Unit, which runs shaders. Shaders are small
programs we generate that process pixels. The instruction format is documented in
reference/vivante_isa.xml and implemented in `gckPPU_*` encoders in gcnano.
The instruction set is fairly simple with fixed-width (4 words, or 128 bits)
instructions.

To run a test, we build a shader (which is done by emitting a series of 4-word
instructions) and put it somewhere in RAM that the GPU can access. This happens
in `build_*_shader()` in `ppu_asm.hh`. 

Then we create a command stream similar to the RS command streams, but these
commands are designed to execute ("dispatch") the shader. The command stream is
built in `emit_ppu_dispatch()` in `etna_compute.cc`. The dispatch command stream
is a 118-word binary blob that we extracted from the gcnano sources
"flop-reset". We use it as a template and fill in the addresses to our buffers,
buffer sizes, etc.

In `compute_test()` various shaders are built and then tested with some simple images
(usually solid color areas).

The simplest shader is just a copy operation: it has two instructions: load a
pixel, then store the value in another pixel.

In addition to that, `compute_test()` tests add, saturating add, multiply, bitwise ops,
two-image add/blend, and a per-pixel fractional alpha blend (`out =
mul_hi(a,alpha) + mul_hi(b,255−alpha)`).

These are all tested with small buffers (<16k bytes).

Finally, we do `test_image_blend` which alpha-blends two 512×512 ARGB images
(treated as u8). We measure the time to do that, and compare that to the time
to do the same operation on the CPU.

Again, the GPU is much slower than the CPU, by around 125x. The reasons are similar
to why the RS engine was slower: the majority of the time is spent waiting for the
DDR4 RAM to read or write. We can show this is the case because adding an extra 
instruction (e.g. AND) doesn't add any extra time. So doing simple math on pixels
does not warrent using the GPU. Doing high-arithmetic-intensity work (or just needing
to offload the task to run in the background) is the best use case for the PPU.

## 3D — the graphics pipe (triangle)

`etna_3d.cc` + `gpu_regs_3d.hh` drive the full graphics pipeline, hand-ported
from Mesa's etnaviv draw path. `triangle_test` clears a tiled render target,
draws one triangle, and checks the pixels changed.

The draw sequence: one-time HALTI5 init → vertex input (NFE) → viewport /
scissor / rasterizer / PS / PE state → shader **instruction-cache** upload (VS +
FS from BOs — HALTI5 has no inline shader load) → inline uniform → `RA→PE` stall
→ **`DRAW_INSTANCED`** → drain. The shaders are trivial `MOV`s (position
pass-through VS, constant-color FS).

**Status: working** — it draws a solid-color triangle (correct shape and the
color the fragment shader pulls from its uniform), verified by reading the render
target back. Two subtle bugs, both wrong constants in the ported sequence, cost
the debug cycles:

- The `LOAD_STATE` command has a **FIXP header bit** (`0x04000000`) that tells the
  FE to convert a 16.16 fixed-point data word to float. The viewport and scissor
  registers need it; without it the viewport is garbage and every triangle lands
  off-screen (draw runs clean, zero fragments, *no faults*).
- **`PE_LOGIC_OP`**'s low nibble is the logic op: `COPY` is `0xC` (dest ← source),
  `CLEAR` is `0x0` (dest ← 0). With `0` there, the pixel engine blanks every
  fragment to zero — correct triangle shape, but always black, ignoring the
  shader color entirely.

The debug lesson worth keeping: "draw runs, zero pixels, no MMU/RISAF/AXI faults"
means the geometry was rejected (viewport/clip), not that a write was blocked
(that shows a firewall/IAC fault); and a triangle that's the *right shape but
wrong color* points downstream of the shader, at the PE.

### Per-vertex color (varyings)

`triangle_color_test` (in the same file) is the next step up: a proper
**varying**. Each vertex carries its own color; the vertex shader passes it
through as a smooth `vec4`, the **rasterizer interpolates** it per fragment, and
the fragment shader just outputs the interpolated value — a red/green/blue
gradient triangle instead of a flat fill.

The render target is still tiled, so rather than resolve it we verify something
tiling-invariant: that a red-dominant, a green-dominant, *and* a blue-dominant
fragment all appear, and the color isn't uniform — which only happens if the
varying really interpolated the three vertex colors. (The clear is black, since
the gradient never produces black.)

This one landed first try, because the two fiddly parts were worked out from
Mesa before writing code:

- **The `MOV` encoding**, decoded from the two known-good triangle shaders:
  `word0 = 0x07801009 | (dst_reg << 16)`, `word3 = 0x00390008 | (src_reg << 4)`
  (`| 0x20000000` to read a uniform instead of a temp). `MOV` reads *src2*, which
  is why the middle two instruction words are zero.
- **The HALTI5 varying linkage** — the magic values that change with varying
  count (`VS_HALTI5_OUTPUT_COUNT`, `…UNK008A0`, the per-varying component counts,
  `GL_HALTI5_SHADER_ATTRIBUTES`), computed the way Mesa's `etna_link_shaders` /
  `emit_halti5_only_state` do for one 4-component smooth varying. Plus the
  two-attribute vertex stream (position + color interleaved), where Mesa groups
  byte-consecutive attributes and only the last one is flagged non-consecutive.

### Depth test

`triangle_depth_test` proves the depth buffer occludes correctly. It draws the
*same* triangle twice into one render target sharing a D16 depth buffer — near
(z=0.3) in red first, then far (z=0.7) in green — with the depth test set to
`LESS` and depth writes on. The near red triangle writes depth 0.65; the far
green triangle's 0.85 fails `LESS` everywhere the red already drew, so it's
rejected. Result: **1301 red, 0 green** — the near triangle occludes the far one.

It's an unambiguous pass/fail: with depth *off* this would be painter's order
(green drawn last wins) → all green. Because the fills are solid, the tiled RT
reads back tiling-invariant, so we just count red vs green fragments.

Three things made it land first try:

- **`PE_DEPTH_CONFIG`** enabled is `0x00041101` (`D16 | MODE_Z | UNK18 |
  FUNC(LESS)<<8 | WRITE_ENABLE`), late-z (no early-Z, no `DISABLE_ZS`). The
  *disabled* value we already had (`0x01000700`) decodes as
  `MODE_NONE | FUNC(ALWAYS) | DISABLE_ZS` — decoding it confirmed the field
  layout before writing the enabled one. `RA_EARLY_DEPTH` stays `0x15000030`
  (its forward-Z / late-z write-disable bits are exactly right for a PE-side test).
- **The depth buffer**: `PE_PIPE_DEPTH_ADDR0` (reloc), `PE_DEPTH_STRIDE`, and
  `PE_DEPTH_NORMALIZE = 65535.0` (`2^16-1` for D16 — easy to leave at 0 and get
  garbage depth). Tiled like the color target but 2 bytes/pixel.
- **Coherency**: both draws go in *one* command stream so the pixel engine's
  depth cache stays hot and the second draw sees the first's depth writes. The
  depth buffer is cleared to `0xFFFF` (far); being uniform, its tiling is moot.

Both draws share `emit_triangle`, which grew an optional depth-buffer argument
(absent ⇒ depth off, as before).

### Texturing

`triangle_texture_test` samples a real 2D texture in the fragment shader — the
last big fixed-function block, completing the classic pipeline (vertex fetch →
VS → varyings → rasterizer → FS with uniforms *and* texture sampling → depth →
PE). A 64×64 tiled A8R8G8B8 texture holds four solid quadrants (red / green /
blue / white); the triangle's UVs span all four; NEAREST filtering means every
drawn pixel must be *exactly* one of the four texel colors. The result —
`R=469 G=494 B=156 W=182 other=0` — is pixel-exact sampling.

How HALTI5 texturing works (all values in `gpu_regs_3d.hh`, extracted from
Mesa's `etnaviv_texture_desc.c`):

- **A texture is a 256-byte in-memory descriptor** (TXDESC): texel base address,
  size, format (`A8R8G8B8 = 7` — same memory layout our render targets use, so
  render-to-texture is within reach), tiling config, plus a couple of
  always-set magic dwords (`CONFIG2 = 0x00030000`, `ASTC0 = 0x0C0C0C00`).
  Everything else must be zeroed.
- **Sampler state lives in registers, not the descriptor**: per-slot
  `NTE_DESCRIPTOR_ADDR` points at the TXDESC, and `NTE_DESCRIPTOR_SAMP_CTRL0/1`
  + LOD registers hold wrap/filter (clamp-to-edge + nearest here). The FS
  `TEXLD` instruction's sampler id selects the slot directly.
- **Coherency is the silent-black trap**: the texture caches want *two*
  separate `GL_FLUSH_CACHE` writes (`TEXTURE` then `TEXTUREVS`), the descriptor
  cache wants `NTE_DESCRIPTOR_FLUSH` + its own flush bits, and re-pointing a
  descriptor needs `NTE_DESCRIPTOR_INVALIDATE`.
- **`TEXLD` (opcode 0x18)** was hand-encoded like the MOVs and verified against
  the ISA before flashing: `texld t2, tex0, t1` = `{0x07821018, 0x39001F20, 0, 0}`.
  A fragment-shader `TEXLD` needs no explicit LOD.
- **Texel data is written directly in the sampler's 4×4-tile layout**
  (`word = (y/4)·stride_px·4 + (y%4)·4 + (x/4)·16 + (x%4)`), and the UV rides
  the same proven vec4 varying linkage as the color triangle (`u,v,0,1` — so no
  new 2-component-varying plumbing), with `TEXLD` consuming just `.xy`.

### RS resolve (untile) + exact shape verification

All the checks above were *tiling-invariant* (counting colors, never asking
*where* a pixel is) because the render target is tiled. `etna::resolve()` closes
that gap: it uses the RS engine for its namesake job — copying a tiled surface
out as **linear** — so pixel (x,y) is simply at `y*stride + x*4`. It's the
`blit()` recipe with two changes from Mesa's `etnaviv_rs.c`: `RS_CONFIG` gains
`SOURCE_TILED` (0x80), and the source-stride register holds the tiled stride
`<< 2` (one RS source row = a row of 4×4 tiles = 4 pixel rows).

`triangle_test` now finishes by resolving the RT and verifying the **shape**:
every pixel more than 1 px inside the expected window-space triangle must be
the draw color, everything more than 1 px outside must be the clear color (the
1 px band belongs to the hardware's fill rule). Result: an exact match, zero
mismatches — and it settled the window-Y question: with our positive viewport
scale, **NDC +Y maps to increasing framebuffer rows** (a GL-style y-up flip
would use a negative `PA_VIEWPORT_SCALE_Y`, which is how Mesa does it).

The resolve is also the render→display link: a display controller scans out
linear framebuffers, so "render tiled, resolve linear" is the shape of every
future frame.

### Spinning cube

`spinning_cube_test` is the first thing that looks like real graphics: a
36-vertex cube with six solid-colored faces, rotated by a model-view-projection
matrix in the **vertex shader**, depth-tested, drawn at 8 rotation angles
(~30k ticks per frame including the resolve, at 64×64). New ground it covers:

- **The first multi-instruction VS**: `clip = M × position` as
  `MUL` + 3×`MAD` accumulating the matrix columns, plus the color passthrough —
  5 instructions, built by a small `constexpr` instruction builder
  (`alu_inst()` in `etna_3d_tests.cc`). The builder is self-checking:
  `static_assert`s prove it reproduces the two hardware-verified `MOV`
  encodings bit-for-bit at compile time.
- **VS uniforms**: the matrix rides in `u0..u3` as four column vec4s. The
  gotcha that cost a flash cycle: on HALTI5 the *vertex* stage's uniforms are
  written through the **mirror window at `0x34000`** — `0x36000`, where the
  fragment color uniform had happily worked, is the *PS* stage's window.
  Upload to the wrong one and the VS reads zeros: degenerate clip positions,
  "draw runs clean, zero fragments, no faults" (the same silent signature as
  the FIXP viewport bug).
- **Perspective divide**: the first draw with w ≠ 1 — the PA's divide and the
  perspective foreshortening match CPU math exactly.
- `emit_mesh()` joins the library (`etna_3d.hh`): a generalized draw taking a
  `MeshDraw` struct — any vertex count, parametric shader sizes, optional
  uniforms and depth.

With no display yet, verification is numeric like the shape test, but now
against a full **CPU reference renderer**: the same 36 vertices, the same
matrix values, a float z-buffer, rasterized at pixel centers. Every frame must
match the resolved GPU image pixel-for-pixel outside a ~1.25 px band around
projected triangle edges (the hardware's fixed-point rasterizer owns the
edges), and all six face colors must appear over the spin. Result: **8/8 frames,
zero mismatches**.

## The `etna` API

Modeled on libdrm's `etna_cmd_stream` / `etna_bo` interface — the MIT-licensed
seam in the middle of the Linux etnaviv stack that Mesa's operation emitters are
written against. Providing it on baremetal means Mesa's MIT emitter code ports
without much friction (what *can't* port easily is Mesa itself: Gallium + the
NIR shader compiler).

- **`etna::Gpu`** — `init()` (bring-up + ring), `alloc()` (DDR pool),
  `submit()`/`wait()`/`submit_and_wait()`. `submit()` appends to the ring and
  patches the idle WAIT into a LINK so ops queue back-to-back (ops must not emit
  `END` — that halts the ring). `wait()` is IRQ-driven: it sleeps on `WFE` until
  the GPU interrupt fires rather than busy-polling.
- **`etna::Bo`** — a physically-contiguous buffer. Identity map: CPU pointer =
  physical = GPU address. `cpu_prep()`/`cpu_fini()` make cache bracketing
  explicit so it can't be forgotten.
- **`etna::CmdStream`** — a growable command buffer with libdrm/Mesa-shaped
  `emit`/`reserve`/`set_state`/`emit_reloc`/`stall` helpers.
- **Operations** — `clear()`/`blit()` (RS), `make_kernel()`/`compute()` (PPU),
  `triangle_test()` (3D).

## Source files

| File | What |
|---|---|
| `etna.hh` / `etna.cc` | the API + bring-up, DDR pool, ring, submit/wait, RS clear/blit |
| `gpu_regs.hh` | RS / MMU / compute-dispatch registers + command encoding |
| `etna_compute.cc` | PPU dispatch template + `make_kernel`/`compute` + kernel suite |
| `ppu_asm.hh` | halti5 shader assembler + kernel builders |
| `gpu_regs_3d.hh` | 3D graphics-pipe registers (PA/SE/RA/PE/VS/PS/SH/FE-draw) |
| `etna_3d.cc` | the 3D draw (state + ICACHE shaders + `DRAW_INSTANCED`) |
| `pmic.cc` | buck3 (VDDGPU) enable over I2C7 |
| `main.cc` | test harness |
| `reference/` | the upstream etnaviv/Mesa sources the above are derived from |

## Where the register definitions come from

ST does not document the GPU register map. Everything comes from the
reverse-engineered **etnaviv** project and Mesa's etnaviv driver (MIT; the
kernel hwdb file is GPL-2.0), keeping the `VIVS_*` names for cross-reference:

- **RS / MMU / compute** (`gpu_regs.hh`): etnaviv `state.xml.h` /
  `state_hi.xml.h` / `cmdstream.xml.h`; the RS sequences mirror `etnaviv_rs.c`.
- **Shader ISA** (`ppu_asm.hh`): the gcnano vendor `gckPPU_*` encoders + the
  Vivante `rnndb/isa.xml` opcode table (copied to `reference/vivante_isa.xml`).
- **3D pipe** (`gpu_regs_3d.hh`, `etna_3d.cc`): Mesa's `src/etnaviv/hw/
  state_3d.xml.h` and the `etnaviv_emit.c` / `etnaviv_context.c` draw path.

The `reference/` directory holds read-only copies of the load-bearing sources,
including the evidence chain for "this chip has no BLT engine."

## Notes and gotchas

- **PLL3 lives inside the GPU subsystem**, not the RCC. Touching `RCC_PLL3CFGRx`
  while the GPU is unpowered/unclocked hangs the bus. Order: VDDGPU valid →
  `GPUEN` → program PLL3 → pulse `GPURST` last (ref clock must be slower than the
  kernel clock during reset). The **VDDGPU voltage monitor must stay enabled** —
  its live output releases the GPU-domain reset that keeps PLL3 out of reset.
- **Security-enabled core**: the FE is started via the secure command-control
  bank (`MMUv2_SEC_COMMAND_CONTROL`) and reset via `MMUv2_AHB_CONTROL`, not the
  plain `FE_COMMAND_CONTROL` / `CLOCK_CONTROL.SOFT_RESET`.
- **Caches**: the GPU reads/writes physical DDR directly (its MMU is off; RS and
  PPU master DDR fine in bypass). Clean the dcache over command/data buffers
  before a kick; invalidate over the result before the CPU reads it.
- **Completion** is the `FROM_PE` event (pipelined behind writes + a cache
  flush), delivered by the GPU interrupt (GIC SPI 215). The ISR is the *sole*
  reader of `HI_INTR_ACKNOWLEDGE` (read-to-clear, also de-asserts the GIC line);
  a polling reader would steal event bits.
- **Compute is memory-latency bound**, not launch- or compute-bound; workgroup
  tiling does not help (see above). Use the right engine for the workload.
- **3D `LOAD_STATE` FIXP bit**: viewport/scissor registers must be emitted with
  the FIXP header flag; without it the viewport is garbage and nothing draws.
- **HALTI5 shaders load via the instruction cache** from a memory BO — there is
  no inline shader upload.
- If identity reads come back zero: check VDDGPU valid, `RCC_GPUCFGR`, PLL3 lock,
  RIF. If reads work but the FE never advances past the buffer address, master
  transactions are blocked — check RIMC `ATTR[9]` and the `RISAF4` DDR regions.

## Expected output

```
GPU Example (etna API)
======================

etna: bringing up GPU
etna: VDDGPU not present (CR12 = 0x0)
etna: VDDGPU off -- trying to enable buck3 over I2C7...
PMIC product ID 0x21, version 0x11
Buck3 (VDDGPU) was: voltage code 0, control 0x0
Buck3 (VDDGPU) now: voltage code 40 (900mV), control 0x1

etna: GC model 0x8000 rev 0x6205 (product 0x80003, customer 0x15)

RS fill (1024x1024) in 842517 ticks
GPU filled 1024x1024 with 0x4D5A11AC -- verified. \o/
RS blit+convert (1024x1024) in 3763949 ticks
GPU copied 1024x1024, swapping R<->B -- verified. \o/
Ring throughput (16 x 64x64 clears): sequential 74485 ticks, pipelined 59042 ticks

GPU copy over 64x6 (384 bytes) in 24707 ticks -- verified. \o/
GPU copy over 128x32 (4096 bytes) in 27271 ticks -- verified. \o/
GPU copy over 256x64 (16384 bytes) in 27899 ticks -- verified. \o/
GPU copy over 32x4 (128 bytes) in 25685 ticks -- verified. \o/
GPU add(out=in+in) over 128x32 (4096 bytes) in 23706 ticks -- verified. \o/
GPU addsat(min(in+in,255)) over 128x32 (4096 bytes) in 10693 ticks -- verified. \o/
GPU add2(A+B, ramp+inv=255) over 128x32 (4096 bytes) in 14537 ticks -- verified. \o/
GPU blend-add(sat(A+B)) over 128x32 (4096 bytes) in 15185 ticks -- verified. \o/
GPU and(in & 0x0F, imm) over 64x6 (384 bytes) in 18509 ticks -- verified. \o/
GPU flop-reset(dp2x8, const in) over 64x6 (384 bytes) in 13450 ticks -- verified. \o/
GPU mul2((A*B)&0xFF) over 64x6 (384 bytes) in 17780 ticks -- verified. \o/
GPU mulhi2(mul_hi(A,B)) over 64x6 (384 bytes) in 26071 ticks -- verified. \o/
GPU not(~in) over 64x6 (384 bytes) in 15187 ticks -- verified. \o/
GPU blend-lerp(a*A + b*(1-A)) over 128x32 (4096 bytes) in 30343 ticks -- verified. \o/
Alpha-blended two 512x512 ARGB images (GPU) in 104904488 ticks
GPU alpha-blended 512x512 ARGB (per-channel lerp) -- verified. \o/
Same blend on the CPU in 1078384 ticks   (GPU / CPU = 97x)
3D triangle drawn in 27235 ticks
RT: 1301 of 4096 pixels drawn, color 0xFFFF0000 (uniform)
GPU drew a solid triangle in 0xFFFF0000 -- 3D pipe verified. \o/
RS resolve (untile 64x64) in 24594 ticks
shape: exact (NDC +Y = increasing framebuffer rows)
resolved image matches the expected triangle -- shape verified. \o/
gradient triangle drawn in 27151 ticks
RT: 1301 of 4096 pixels drawn; corners seen R=1 G=1 B=1 (varied)
GPU interpolated a per-vertex-color varying across the triangle. \o/
depth: two triangles drawn in 32141 ticks
depth test: 1951 drawn -> 1301 red (near), 650 green (far)
GPU depth test occluded the farther triangle -- depth buffer works. \o/
textured triangle drawn in 16864 ticks
texture test: 1301 drawn -> R=469 G=494 B=156 W=182 other=0
GPU sampled a 2D texture across the triangle -- texturing works. \o/
cube frame 0: 658 px drawn, 0 mismatches (562 edge px ignored)
  ... 8 frames, all 0 mismatches ...
spinning cube: 8 frames avg 30168 ticks (draw+resolve), faces seen 0x3F
GPU spun a cube: VS matrix transform + depth + rasterization all match the CPU. \o/
SUCCESS
```

## Running

```bash
make
make flash SD=/dev/diskX   # or copy build/main.uimg to the app partition
```
