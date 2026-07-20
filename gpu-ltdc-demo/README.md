# gpu-ltdc-demo — the spinning cube, live on the LVDS panel

The `gpu/` and `ltdc/` bring-ups joined: the GPU renders a depth-tested,
matrix-transformed cube and the LTDC scans it out to the EV1's 1024x600 LVDS
display. Double-buffered and tear-free.

```
cube_scene.hh (verts + matrix VS, CPU-verified in gpu/etna_3d_tests)
     |  etna::emit_mesh          3D pipe: MUL/MAD vertex transform, D16 depth
     v
tiled render target 1024x600
     |  etna::resolve            RS engine untiles into the back buffer
     v
linear back buffer --ltdc_set_framebuffer()--> LTDC --> LVDS PHY --> panel
                        (latched at vblank; ltdc_wait_reload() paces the loop)
```

Per frame, one GPU submission: RS clear (color) + RS clear (depth to "far") +
draw + resolve. No CPU pixel work at all — the CPU computes one 4x4 matrix and
builds ~500 command words.

Sources are pulled from the two library projects (`../gpu/etna*.cc`,
`../ltdc/{display,ltdc,lvds}.cc`); this directory only has `main.cc`.

## Performance (first light)

~52 ms/frame (~19 fps) at 1024x600. Breakdown: the draw is ~1-2 ms; the
resolve (untile) of 2.4 MB dominates at ~35 ms; vblank quantization rounds the
result to 3 refresh periods. `ISR 0x1` throughout = zero FIFO underruns while
the GPU and LTDC share DDR.

Obvious levers if/when more fps is wanted: render into a smaller centered RT
and only resolve that window; profile whether the RS is faster with different
burst settings; or skip the clear of regions the cube never touches.

## Running

```bash
make UART=2        # EV1
make flash-t32
tail ~/minicom-ev1-cap.log   # prints ms/frame every 120 frames
```
