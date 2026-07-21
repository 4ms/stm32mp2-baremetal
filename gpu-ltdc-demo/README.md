# GPU LTDC Demo — spinning cubes

This project combines the `gpu/` and `ltdc/` projects by rendering 12
moving/spinning cubes at 60fps onto the 1024x600 LVDS display on the EV1. The
number of cubes can be increased in the source code. Sources are pulled from
the `gpu/` and `ltdc/` projects.

All cubes are rendered into one full-screen tiled render target with a
shared depth buffer, so the depth test resolves occlusion between them (cubes
pass in front of / behind each other by their z), though there is no collision 
detection and so cubes will pass through each other. 
Each cube has a world position, velocity, spin rate, and base hue (faces are shades of the
base hue).

The display is double-buffered, and the refresh is interrupt-driven via an ltdc callback.

## Performance

Overall performance is excellent: we easily hit 60 fps with up to around 100 cubes.
At 12 cubes, render time is 4-6ms.


## Expected output

You should see a color cube spinning on the screen.

![](docs/gpu-ltdc-demo-crop.gif)

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
60 fps, worst render 5207 us
60 fps, worst render 5819 us
60 fps, worst render 5858 us
```

You can 
