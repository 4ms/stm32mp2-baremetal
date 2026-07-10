#pragma once
#include <cstdint>

// Register map for the VeriSilicon/Vivante GPU (GCNanoUltra31) on the STM32MP25x.
// ST does not publish this register map (the CMSIS header declares GPU_BASE but no
// register struct), so these definitions come from the community-reverse-engineered
// etnaviv project (MIT license):
//   - Linux: drivers/gpu/drm/etnaviv/{state,state_hi,cmdstream}.xml.h
//   - Mesa:  src/etnaviv/hw/state_blt.xml.h
// Names below keep the etnaviv VIVS_* naming so they are easy to cross-reference.

namespace VivanteGpu
{

// ------------------- AHB ("Host Interface") registers -------------------

constexpr uint32_t HI_CLOCK_CONTROL = 0x0000;
constexpr uint32_t HI_IDLE_STATE = 0x0004;
constexpr uint32_t HI_AXI_CONFIG = 0x0008;
constexpr uint32_t HI_AXI_STATUS = 0x000C;
constexpr uint32_t HI_INTR_ACKNOWLEDGE = 0x0010; // read clears
constexpr uint32_t HI_INTR_ENBL = 0x0014;
constexpr uint32_t HI_CHIP_IDENTITY = 0x0018;
constexpr uint32_t HI_CHIP_FEATURE = 0x001C;
constexpr uint32_t HI_CHIP_MODEL = 0x0020;
constexpr uint32_t HI_CHIP_REV = 0x0024;
constexpr uint32_t HI_CHIP_DATE = 0x0028;
constexpr uint32_t HI_CHIP_TIME = 0x002C;
constexpr uint32_t HI_CHIP_CUSTOMER_ID = 0x0030;
constexpr uint32_t HI_CHIP_MINOR_FEATURE_0 = 0x0034;
constexpr uint32_t HI_CHIP_SPECS = 0x0048;
constexpr uint32_t HI_CHIP_MINOR_FEATURE_1 = 0x0074;
constexpr uint32_t HI_CHIP_MINOR_FEATURE_2 = 0x0084;
constexpr uint32_t HI_CHIP_MINOR_FEATURE_3 = 0x0088;
constexpr uint32_t HI_CHIP_MINOR_FEATURE_4 = 0x0094;
constexpr uint32_t HI_CHIP_MINOR_FEATURE_5 = 0x00A0;
constexpr uint32_t HI_CHIP_PRODUCT_ID = 0x00A8;
constexpr uint32_t HI_CHIP_ECO_ID = 0x00E8;

// HI_CLOCK_CONTROL bits
constexpr uint32_t CLK_FSCALE_VAL(uint32_t x)
{
	return (x << 2) & 0x1FC; // core clock scale: 0x40 = full speed
}
constexpr uint32_t CLK_FSCALE_CMD_LOAD = 1 << 9;
constexpr uint32_t CLK_DISABLE_DEBUG_REGISTERS = 1 << 11;
constexpr uint32_t CLK_SOFT_RESET = 1 << 12;
constexpr uint32_t CLK_IDLE_3D = 1 << 16;
constexpr uint32_t CLK_IDLE_2D = 1 << 17;
constexpr uint32_t CLK_ISOLATE_GPU = 1 << 19;

// HI_IDLE_STATE bits (1 = idle). More exist; these are the interesting ones.
constexpr uint32_t IDLE_FE = 1 << 0;
constexpr uint32_t IDLE_PE = 1 << 2;
constexpr uint32_t IDLE_BLT = 1 << 12;

// HI_INTR_ACKNOWLEDGE bits
constexpr uint32_t INTR_MMU_EXCEPTION = 1u << 30;
constexpr uint32_t INTR_AXI_BUS_ERROR = 1u << 31;

// HI_AXI_STATUS bits
constexpr uint32_t AXI_STATUS_DET_WR_ERR = 1 << 8;
constexpr uint32_t AXI_STATUS_DET_RD_ERR = 1 << 9;

// ------------------- FE (command stream front end) registers -------------------

constexpr uint32_t FE_COMMAND_ADDRESS = 0x0654;
constexpr uint32_t FE_COMMAND_CONTROL = 0x0658;
constexpr uint32_t FE_DMA_STATUS = 0x065C;
constexpr uint32_t FE_DMA_DEBUG_STATE = 0x0660;
constexpr uint32_t FE_DMA_ADDRESS = 0x0664;
constexpr uint32_t FE_DMA_LOW = 0x0668; // last fetched command word (low)
constexpr uint32_t FE_DMA_HIGH = 0x066C;

// FE_COMMAND_CONTROL: prefetch counts in 64-bit (2-dword) units
constexpr uint32_t FE_CONTROL_ENABLE = 1 << 16;

// ------------------- Command stream encoding (fed to the FE) -------------------
// Every command occupies a multiple of 64 bits (2 dwords).

// LOAD_STATE: header + N data words, writes N consecutive state registers
constexpr uint32_t cmd_load_state(uint32_t state_addr, uint32_t count = 1)
{
	return 0x0800'0000 | ((count & 0x3FF) << 16) | ((state_addr >> 2) & 0xFFFF);
}
constexpr uint32_t CMD_END = 0x1000'0000;
constexpr uint32_t CMD_NOP = 0x1800'0000;
constexpr uint32_t CMD_STALL = 0x4800'0000; // second dword is a sync token

// Sync token (both for the STALL command and the GL_SEMAPHORE_TOKEN state)
constexpr uint32_t SYNC_RECIPIENT_FE = 0x01;
constexpr uint32_t SYNC_RECIPIENT_BLT = 0x10;
constexpr uint32_t sync_token(uint32_t from, uint32_t to)
{
	return (from & 0x1F) | ((to & 0x1F) << 8);
}

// ------------------- State registers written via LOAD_STATE -------------------

constexpr uint32_t GL_SEMAPHORE_TOKEN = 0x3808;

// BLT engine (halti5 cores like this one; replaces the older RS/2D engines)
constexpr uint32_t BLT_SRC_ADDR = 0x1'4000;
constexpr uint32_t BLT_SRC_STRIDE = 0x1'4008;
constexpr uint32_t BLT_SRC_CONFIG = 0x1'400C;
constexpr uint32_t BLT_DEST_ADDR = 0x1'4018;
constexpr uint32_t BLT_DEST_STRIDE = 0x1'4024;
constexpr uint32_t BLT_DEST_CONFIG = 0x1'4028;
constexpr uint32_t BLT_DEST_POS = 0x1'402C;
constexpr uint32_t BLT_IMAGE_SIZE = 0x1'4030; // width | height << 16
constexpr uint32_t BLT_CLEAR_COLOR0 = 0x1'4044;
constexpr uint32_t BLT_CLEAR_COLOR1 = 0x1'4048;
constexpr uint32_t BLT_CLEAR_BITS0 = 0x1'404C; // mask of bits to write
constexpr uint32_t BLT_CLEAR_BITS1 = 0x1'4050;
constexpr uint32_t BLT_COMMAND = 0x1'4060;
constexpr uint32_t BLT_CONFIG = 0x1'4064;
constexpr uint32_t BLT_SET_COMMAND = 0x1'40AC;
constexpr uint32_t BLT_ENABLE = 0x1'40B8;

constexpr uint32_t BLT_COMMAND_CLEAR_IMAGE = 1;
constexpr uint32_t blt_config_clear_bpp(uint32_t bytes_per_pixel)
{
	return ((bytes_per_pixel - 1) & 7) << 7;
}
// DEST_STRIDE/SRC_STRIDE: stride in bytes | format << 21 | tiling << 29 (0 = linear)
constexpr uint32_t blt_stride(uint32_t stride_bytes)
{
	return stride_bytes & 0xF'FFFF;
}
// DEST_CONFIG/SRC_CONFIG: identity RGBA swizzle + UNK22 (set by all etnaviv BLT ops)
constexpr uint32_t BLT_IMAGE_CONFIG_PLAIN = (0 << 9) | (1 << 11) | (2 << 13) | (3 << 15) | (1 << 22);

} // namespace VivanteGpu
