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

The etnaviv project, the Mesa project, the Linux etnaviv drivers, and the gcnano
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
WAIT/LINK commands so the GPU runs in an idle self-loop whenever it gets to the
end processing commands we gave it. When we want to queue some commands we
append them and move the WAIT/LINK to the end. That way the GPU doesn't need to
be re-init each time we use it.

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

The RS is slow at filling bytes in DDR RAM: a 1024x1024 fill is 4x slower
than a CPU memset (about ~830k ticks vs. ~220k ticks). It's not clear why --
this is still an open TODO. It seems as if we're limited to \~325MB/s max
bandwidth for DDR4 writes. GPUs are good at doing parallel operations but a
fill is a serial memory write (without any benefit of caches), so that might
just be what we're hitting.

Regardless, the GPU runs asynchronously (so is basically "free") and won't
dirty our CPU's L1/L2 data cache if we only intend to fill an area of a
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

The GPU is a little slower than the CPU, by around 1.6x (see above). That 
still seems useful for doing work in the background, and considering this is a
low-computation example, I think it proves the GPU's worth. The GPU runs at about
half the clock speed as the CPU, so that's actually quite good to see 1.6x the time.

## 3D — the graphics pipe (drawing triangles)

To test the 3D pipeline, we draw triangles and then check the frame buffer
to verify it has the expected pixels.

These tests all have a similar procedure: create an array of vertices (`vtx`),
get some pre-built simple shaders (`vs` and `ps`) that do something simple like
move the color arguments to the right engine. Then plug all these into a
command stream ported from the Mesa Gallium driver. If you want to see how the
porting happened, and where in the Mesa code everything came from, see the
header comments in `etna_3d.cc`.

Compared to other examples, the command stream is complex and runs through
several engines: vertex input (NFE) -> viewport / scissor / rasterizer / PS /
PE state -> shader ICACHE upload (VS + FS from BOs) -> inline uniform -> 
`RA->PE` stall → DRAW_INSTANCED -> drain. The shaders are trivial `MOV`s
(position pass-through VS, constant-color FS). I can't say I understand 
all the stages happening here, but the end result is tested, and draws the
expected triangles with the expected colors, textures and z-depth.

After drawing to the render target, the pixels are in a tiled format, meaning
they are arranged in an order that convenient for the GPU but not the same
order a display driver will want its framebuffer to be in. To resolve this, we
use the Resolver Engine. For one of the examples, we resolve the buffer
and then check if the shape is correct.

The tests are:
- triangle_test(): draw a simple triangle, check if pixels were set and if
  color is uniform. Then resolve it to a framebuffer and check if the shape is
  correct. 
- triangle_color_test(): draw an rgb gradient triangle (each vertex gets a
  color). Check that we see red, green, and blue pixels. Tests the rasterizer
  interpolation.
- triangle_depth_test(): draw two triangles at different z depths. The top
  one is red and the bottom one is green and flipped upside down. Count the 
  number of pixels of each color to check if the red triangle is blocking ~50%
  of the green one.
- triangle_texture_test(): draw a triangle and fill it with a texture. The
  texture is 64x64 with each quadrant a different color. The triangle's UVs
  span all four colors and we use a "NEAREST" filtering so that only exact
  colors from the texture will be used. Then we count how many pixels of 
  each color we found. Here we use a new shader opcode: TEXLD.


### Spinning cube

`spinning_cube_test` draws a cube with 36-vertices (six solid-colored faces),
at 8 rotation angles. Rotation is done by giving the GPU some CPU-calculated
transformation coefficients ("model-view-projection"). Snapshots at 8 different
angles are made and compared to a 100% CPU-rendered image.

Some new things checked here:
- Multi-instruction vertex shader (VS): `clip = M × position` as `MUL` +
  3×`MAD` accumulating the matrix columns, plus the color passthrough. This is
  built by a small `constexpr` instruction builder (`alu_inst()` in
  `etna_3d_tests.cc`). The builder is self-checking: `static_assert`s prove it
  reproduces the two hardware-verified `MOV` encodings bit-for-bit at compile
  time.
- `emit_mesh()` is a general helper in the `etna_3d.hh` that draws a `MeshDraw`
  struct — any vertex count, parametric shader sizes, optional uniforms and
  depth.

## The `etna` API

The "API" is modeled on libdrm's `etna_cmd_stream` / `etna_bo` interface, 
which goes between Gallium and the kernel drivers.

seam in the middle of the Linux etnaviv stack that Mesa's operation emitters are
written against. Providing it on baremetal means Mesa's MIT emitter code ports
without much friction (what *can't* port easily is Mesa itself: Gallium + the
NIR shader compiler).

- **`etna::Gpu`**  
    - `init()`: bring-up + ring buffer setup-
    - `alloc()` DDR pool
    - `submit()` appends to the ring and patches the idle WAIT into a LINK so ops queue back-to-back (ops must not emit
  `END` — that halts the ring). 
    - `wait()` sleeps on `WFE` until the GPU interrupt fires
- **`etna::Bo`** 
    — a physically-contiguous buffer
    - `cpu_prep()`/`cpu_fini()` need to be used before/after reading/writing because the buffer is cached 
- **`etna::CmdStream`** 
    — a growable command buffer with helpers similar to libdrm/Mesa (`emit`/`reserve`/`set_state`/`emit_reloc`/`stall`)
- **Operations** 
    — `clear()`/`blit()` (RS)
    - `make_kernel()`/`compute()` (PPU),


## How this was written/ported

ST does not document the GPU register map. Everything comes from the
reverse-engineered **etnaviv** project and Mesa's etnaviv/gallium drivers.
All sources we consulted as licensed MIT or GPL-v2.0.
I used an LLM extensively to do things like mechanically port the command
sequences from the Mesa driver and etnaviv project into the unified
step-by-step sequences you see in etna_3d.cc, and to test the behavior of each
opcode and refine the bit-level assembly of shaders until the results were as
expected.

- Register values (`gpu_regs.hh`): Mostly copied from etnaviv `state.xml.h`,
  `state_hi.xml.h`, `cmdstream.xml.h`. 
    - RS command stream from `etnaviv_rs.c`
- Shader ISA (`ppu_asm.hh`): the gcnano vendor `gckPPU_*` encoders and the
  Vivante `rnndb/isa.xml` opcode table from etna_viv.
- **3D pipe** (`gpu_regs_3d.hh`, `etna_3d.cc`): Mesa's
  `src/etnaviv/hw/state_3d.xml.h` and the Gallium `etnaviv_emit.c` and
  `etnaviv_context.c` draw path.

Reference repos:
- [Mesa](https://gitlab.freedesktop.org/mesa/mesa):
    - [src/gallium/drivers/etnaviv](https://gitlab.freedesktop.org/mesa/mesa/-/tree/main/src/gallium/drivers/etnaviv?ref_type=heads)
    - [src/etnaviv/hw](https://gitlab.freedesktop.org/mesa/mesa/-/tree/main/src/etnaviv/hw?ref_type=heads)
- [Mesa Libdrm](https://gitlab.freedesktop.org/mesa/libdrm)
- [ST's Linux fork, v6.6-stm32mp](https://github.com/STMicroelectronics/linux/tree/v6.6-stm32mp)
    - [drivers/gpu/drm/etnaviv](https://github.com/STMicroelectronics/linux/tree/v6.6-stm32mp/drivers/gpu/drm/etnaviv)
- (gcnano 6.4.19 sources)[https://github.com/STMicroelectronics/gcnano-binaries]
- [etna_viv](https://github.com/etnaviv/etna_viv)


## Expected output

```
GPU Example (etna API)
======================

etna: bringing up GPU
etna: VDDGPU not present (CR12 = 0x0)
etna: VDDGPU off -- trying to enable buck3 over I2C7...
PMIC product ID 0x20, version 0x11
Buck3 (VDDGPU) was: voltage code 0, control 0x0
Buck3 (VDDGPU) now: voltage code 40 (900mV), control 0x1
etna: GPU mem-clock ~600 MHz

etna: GC model 0x8000 rev 0x6205 (product 0x80003, customer 0x15)

RS Engine tests:
RS fill (1024x1024) in 823327 ticks (326 MB/s)
GPU filled 1024x1024 with 0x4D5A11AC -- verified. \o/
RS blit+convert (1024x1024) in 3260037 ticks
GPU copied 1024x1024, swapping R<->B -- verified. \o/
Ring throughput (16 x 64x64 clears): sequential 62973 ticks, pipelined 58191 ticks

PPU compute/shader tests:
GPU copy over 64x6 (384 bytes) in 2062 ticks -- verified. \o/
GPU copy over 128x32 (4096 bytes) in 5648 ticks -- verified. \o/
GPU copy over 256x64 (16384 bytes) in 16201 ticks -- verified. \o/
GPU copy over 32x4 (128 bytes) in 1752 ticks -- verified. \o/
GPU add(out=in+in) over 128x32 (4096 bytes) in 5684 ticks -- verified. \o/
GPU addsat(min(in+in,255)) over 128x32 (4096 bytes) in 5564 ticks -- verified. \o/
GPU add2(A+B, ramp+inv=255) over 128x32 (4096 bytes) in 5712 ticks -- verified. \o/
GPU blend-add(sat(A+B)) over 128x32 (4096 bytes) in 5551 ticks -- verified. \o/
GPU and(in & 0x0F, imm) over 64x6 (384 bytes) in 2335 ticks -- verified. \o/
GPU flop-reset(dp2x8, const in) over 64x6 (384 bytes) in 2215 ticks -- verified. \o/
GPU mul2((A*B)&0xFF) over 64x6 (384 bytes) in 2380 ticks -- verified. \o/
GPU mulhi2(mul_hi(A,B)) over 64x6 (384 bytes) in 2176 ticks -- verified. \o/
GPU not(~in) over 64x6 (384 bytes) in 2042 ticks -- verified. \o/
GPU blend-lerp(a*A + b*(1-A)) over 128x32 (4096 bytes) in 8421 ticks -- verified. \o/
Alpha-blended two 512x512 ARGB images (GPU) in 1648645 ticks
GPU alpha-blended 512x512 ARGB (per-channel lerp) -- verified. \o/
Same blend on the CPU in 1083585 ticks (GPU / CPU = 1.5x)

3D tests:
3D triangle drawn in 4660 ticks
RT: 1301 of 4096 pixels drawn, color 0xFFFF0000 (uniform)
GPU drew a solid triangle in 0xFFFF0000 -- 3D pipe verified. \o/
RS resolve (untile 64x64) in 4334 ticks
shape: exact (NDC +Y = increasing framebuffer rows)
resolved image matches the expected triangle -- shape verified. \o/
gradient triangle drawn in 6901 ticks
RT: 1301 of 4096 pixels drawn. corners seen R=1 G=1 B=1 (varied)
GPU interpolated a per-vertex-color varying across the triangle. \o/
depth: two triangles drawn in 8951 ticks
depth test: 1951 drawn -> 1301 red (near), 650 green (far)
GPU depth test occluded the farther triangle -- depth buffer works. \o/
textured triangle drawn in 7264 ticks
texture test: 1301 drawn -> R=469 G=494 B=156 W=182 other=0
GPU sampled a 2D texture across the triangle -- texturing works. \o/
cube frame 0: 658 px drawn, 0 mismatches (562 edge px ignored)
cube frame 1: 761 px drawn, 0 mismatches (687 edge px ignored)
cube frame 2: 721 px drawn, 0 mismatches (613 edge px ignored)
cube frame 3: 715 px drawn, 0 mismatches (623 edge px ignored)
cube frame 4: 635 px drawn, 0 mismatches (558 edge px ignored)
cube frame 5: 771 px drawn, 0 mismatches (686 edge px ignored)
cube frame 6: 739 px drawn, 0 mismatches (628 edge px ignored)
cube frame 7: 735 px drawn, 0 mismatches (645 edge px ignored)
spinning cube: 8 frames avg 12655 ticks (draw+resolve), faces seen 0x3F
GPU spun a cube: VS matrix transform + depth + rasterization all match the CPU. \o/

SUCCESS 
```

## Running

```bash
make
make flash SD=/dev/diskX   # or copy build/main.uimg to the app partition
```
