# GPU LTDC Demo — spinning cube

This project combines the `gpu/` and `ltdc/` projects by rendering a spinning
cube onto the LVDS display on the EV1. Double-buffered and tear-free.

Per frame, one GPU submission: RS clear (color) + RS clear (depth set to "far") +
draw + resolve. No CPU pixel work at all — the CPU computes one 4x4 matrix and
builds ~500 command words.

Sources are pulled from the `gpu/` and `ltdc/` projects.

## Performance

We easily hit 60fps, which is the maximum rate of the LVDS display.
Draw time is about 1.1ms, so we operate at around 7% load.

## Expected output

You should see a color cube spinning on the screen.

```
GPU -> LTDC demo: composited spinning cube
==========================================

etna: bringing up GPU
etna: VDDGPU not present (CR12 = 0x0)
etna: VDDGPU off -- trying to enable buck3 over I2C7...
pmic: product ID 0x20, version 0x11
pmic: Buck3 (VDDGPU) was: voltage code 0, control 0x0
pmic: Buck3 (VDDGPU) now: voltage code 40 (900mV), control 0x1
etna: gpu pll set to 800 MHz
etna: GPU mem-clock ~600 MHz

etna: GC model 0x8000 rev 0x6205 (product 0x80003, customer 0x15)
verify: 0 mismatches vs CPU reference  \o/
Display up: bg on layer 1, cube on layer 2 (512x512 at 256,44)
spinning...
61 fps
61 fps
61 fps
60 fps
60 fps
60 fps
```

