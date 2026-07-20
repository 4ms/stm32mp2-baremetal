# gcnano flop_reset -- compute (PPU) reference

Vendor source (dual MIT/GPL) for the PPU/NN/TP "flop reset" -- the closest thing
to a documented, chip-specific example of running a shader on this exact GPU
(GCNANOULTRA31_VIP2, model 0x8000 rev 0x6205 customer 0x15). `etna_compute.cc`
is derived from this.

- `gc_hal_kernel_hardware_func_flop_reset.c` -- the assembler (gckPPU_*) + the
  PPU shader (`_ProgramPPUInstruction`, "out = in + in") + the dispatch
  (`_ProgramPPUCommand`) + orchestration (`gckHARDWARE_ResetFlopWithPPU`).
- `gc_hal_kernel_hardware_func_flop_reset_config.h` -- per-chip NN/TP data.

## How etna_compute.cc's dispatch array was derived (reproducible)

- `extract2.py` -- parses `_ProgramPPUCommand`, evaluates the vendor's bitfield
  macros, resolves the `HALTI5` branch (taken) and drops the `wlFE` END (our
  ring's submit() adds the trailer), and emits the 118-dword command sequence
  with the variable slots (addresses, dims) named.
- `gen_cpp.py` -- resolves those variables to our chip's concrete values
  (dataType 0x7, NumShaderCores 2, groupSize 1x1, globalScale 4x1,
  groupCount 16x6, RegCount 3, InstCount 16) and prints the C++ array, marking
  the 3 runtime address patch slots (dword 7=input, 13=output, 53=shader).

The shader encoder (`gpu/ppu_asm.hh`) is a separate faithful port of the
gckPPU_* functions, host-tested in isolation.
