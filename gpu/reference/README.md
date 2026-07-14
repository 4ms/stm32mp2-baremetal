# reference/ — upstream etnaviv/Mesa sources

Read-only copies of the open-source files this example was derived from, kept in
the repo so the reasoning behind `gpu_regs.hh`, the RS command sequences, and
the "no BLT engine" finding survives independent of external checkouts. Nothing
here is compiled; these are documentation.

Licenses are **mixed** (as in `usb-drd/`): the kernel file is GPL-2.0, the Mesa
files and register headers are MIT. Per-file:

| File | Upstream | License |
|---|---|---|
| `etnaviv_hwdb_mainline.c` | Linux `drivers/gpu/drm/etnaviv/etnaviv_hwdb.c` (mainline) | **GPL-2.0** |
| `etnaviv_screen.c` | Mesa `src/gallium/drivers/etnaviv/etnaviv_screen.c` | MIT |
| `etnaviv_rs.c` | Mesa `src/gallium/drivers/etnaviv/etnaviv_rs.c` | MIT |
| `etnaviv_blt.c` | Mesa `src/gallium/drivers/etnaviv/etnaviv_blt.c` | MIT |
| `state_3d.xml.h` | Mesa `src/etnaviv/hw/state_3d.xml.h` | MIT |
| `state_blt.xml.h` | Mesa `src/etnaviv/hw/state_blt.xml.h` | MIT |

The register *definitions* we actually compile against live in `../gpu_regs.hh`
(hand-transcribed, keeping the etnaviv `VIVS_*` names). The RS clear/blit
sequences in `../etna.cc` are ports of `etnaviv_rs.c`. The `state_*.xml.h`
headers are auto-generated from the annotated XML in the etna_viv project
(<https://github.com/etnaviv/etna_viv>, `rnndb/*.xml`).

## Why "this GPU has no BLT engine despite the feature registers saying it does"

This is the single most load-bearing finding for the RS-vs-BLT decision, so the
evidence trail is recorded here. The MP25 core (GCNanoUltra31: model `0x8000`,
rev `0x6205`, product `0x80003`, customer `0x15`) reports a BLT engine in its
feature registers, but it does **not** have one, and driving the BLT registers
wedges the FE.

1. **The BLT feature bit** is minor-feature-5 bit 31:
   `linux/drivers/gpu/drm/etnaviv/common.xml.h:317`
   `#define chipMinorFeatures5_BLT_ENGINE  0x80000000`

2. **The silicon claims BLT.** On hardware, `HI_CHIP_MINOR_FEATURE_5` reads
   `0xBB5AC733` → bit 31 **set**.

3. **The hardware database says otherwise.** `etnaviv_hwdb_mainline.c` has an
   entry matching our exact chip (`.customer_id = 0x15` at line 203) with
   `.minor_features5 = 0x7b5ac333` (line 223) → bit 31 **clear**, i.e. no BLT.
   (`0xBB5AC733` vs `0x7B5AC333` disagree in `0xC0000400` — the BLT bit is not
   even the only wrong one.)

4. **The database is authoritative by design.** In the driver's identify path,
   `linux/drivers/gpu/drm/etnaviv/etnaviv_gpu.c:406-410`:
   ```c
   /*
    * If there is a match in the HWDB, we aren't interested in the
    * remaining register values, as they might be wrong.
    */
   if (etnaviv_fill_identity_from_hwdb(gpu))
       return;                                    // never reads the feature regs
   gpu->identity.features = gpu_read(gpu, VIVS_HI_CHIP_FEATURE);
   ```
   `etnaviv_fill_identity_from_hwdb()` (`etnaviv_hwdb.c:265`) `memcpy`s the whole
   identity — all `minor_features` — from the table and returns; on a match the
   feature registers are never read from hardware.

5. **That drives the fill/blit engine choice.** Mesa `etnaviv_screen.c:984`:
   `screen->specs.use_blt = VIV_FEATURE(screen, ETNA_FEATURE_BLT_ENGINE);`
   `VIV_FEATURE` reads the hwdb-corrected identity, so `use_blt` is false for
   this chip and Mesa routes clears/blits through the **RS engine** — which is
   exactly what `../etna.cc` does.

