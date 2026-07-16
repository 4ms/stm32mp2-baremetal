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
constexpr uint32_t INTR_FROM_PE = 1u << 2;
// constexpr uint32_t INTR_FROM_FE = 1u << 1;
constexpr uint32_t INTR_FROM_FE = 1u << 3;
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

// ------------------- Security-mode registers -------------------
// Cores with the SECURITY feature (like this one) ignore FE_COMMAND_CONTROL:
// the FE must (also) be started through the secure-bank mirror, and the core
// is reset through MMUv2_AHB_CONTROL instead of HI_CLOCK_CONTROL.SOFT_RESET
// (see ETNA_SEC_KERNEL handling in etnaviv_gpu.c).
constexpr uint32_t MMUv2_SEC_COMMAND_CONTROL = 0x03A4; // ENABLE | prefetch, like FE_COMMAND_CONTROL
constexpr uint32_t MMUv2_AHB_CONTROL = 0x03A8;
constexpr uint32_t MMUv2_AHB_CONTROL_RESET = 1 << 0;
constexpr uint32_t MMUv2_AHB_CONTROL_NONSEC_ACCESS = 1 << 1;

// GPU-MMU fault status, 4 bits per MMU: 1="slave not present", 2="page not
// present", 3="write violation", 4="out of bounds", 5="read security
// violation", 6="write security violation". 0 = no fault.
constexpr uint32_t MMUv2_STATUS = 0x0188;
constexpr uint32_t MMUv2_SEC_STATUS = 0x0384;
constexpr uint32_t MMUv2_SEC_EXCEPTION_ADDR = 0x0380;

// The GPU MMU proper. Both etnaviv and the vendor driver always enable it
// before running any engine -- the memory-touching engines (PE/BLT) do not
// reliably work in bypass on secure cores. On secure cores it is enabled
// through the "PTA" (page table array) + SEC_CONTROL (see
// etnaviv_iommuv2_restore_sec() in etnaviv_iommu_v2.c):
constexpr uint32_t MMUv2_SEC_CONTROL = 0x0388; // bit 0: enable
constexpr uint32_t MMUv2_PTA_ADDRESS_LOW = 0x038C;
constexpr uint32_t MMUv2_PTA_ADDRESS_HIGH = 0x0390;
constexpr uint32_t MMUv2_PTA_CONTROL = 0x0394; // bit 0: enable
constexpr uint32_t MMUv2_NONSEC_SAFE_ADDR_LOW = 0x0398;
constexpr uint32_t MMUv2_SEC_SAFE_ADDR_LOW = 0x039C;
constexpr uint32_t MMUv2_SAFE_ADDRESS_CONFIG = 0x03A0;

// State (written via LOAD_STATE) that makes the MMU load PTA entry N
constexpr uint32_t MMUv2_PTA_CONFIG = 0x01AC;

// Page table formats (MODE4_K): PTA is an array of 64-bit entries, one per
// context: MTLB physical address | mode (0 = 4K pages). The MTLB has 1024
// 32-bit entries, each mapping 4MB via one STLB page: STLB physical | PRESENT.
// Each STLB has 1024 32-bit entries: page physical | PRESENT | WRITEABLE.
constexpr uint32_t MMU_PTE_PRESENT = 1 << 0;
constexpr uint32_t MMU_PTE_EXCEPTION = 1 << 1;
constexpr uint32_t MMU_PTE_WRITEABLE = 1 << 2;

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

// WAIT: the FE waits `cycles` then advances to the next (64-bit-aligned)
// command. A WAIT immediately followed by a LINK back to the WAIT is the
// canonical FE idle loop the WAIT/LINK ring is built on.
constexpr uint32_t cmd_wait(uint32_t cycles)
{
	return 0x3800'0000 | (cycles & 0xFFFF);
}
// LINK: second dword is the target address. `prefetch` is how many 64-bit
// words to prefetch from the target (in qwords).
constexpr uint32_t cmd_link(uint32_t prefetch_qwords)
{
	return 0x4000'0000 | (prefetch_qwords & 0xFFFF);
}

// Sync token (both for the STALL command and the GL_SEMAPHORE_TOKEN state)
constexpr uint32_t SYNC_RECIPIENT_FE = 0x01;
constexpr uint32_t SYNC_RECIPIENT_PE = 0x07;
constexpr uint32_t SYNC_RECIPIENT_BLT = 0x10;
constexpr uint32_t sync_token(uint32_t from, uint32_t to)
{
	return (from & 0x1F) | ((to & 0x1F) << 8);
}

// ------------------- State registers written via LOAD_STATE -------------------

constexpr uint32_t GL_SEMAPHORE_TOKEN = 0x3808;
// Writing an event ID latches that bit in HI_INTR_ACKNOWLEDGE when the given
// engine processes it -- the canonical "did my command stream get here" signal
constexpr uint32_t GL_EVENT = 0x3804;
constexpr uint32_t GL_EVENT_FROM_FE = 0x20;
constexpr uint32_t GL_EVENT_FROM_PE = 0x40;
constexpr uint32_t GL_EVENT_FROM_BLT = 0x80;
constexpr uint32_t GL_FLUSH_CACHE = 0x380C;
// The flush bits the etnaviv driver uses when ending 3D-pipe work:
// DEPTH | COLOR | TEXTURE | TEXTUREVS | SHADER_L2
constexpr uint32_t GL_FLUSH_CACHE_PIPE3D = 0x57;

// RS ("resolve") engine: the fill/blit engine on this core. Note the MP25's
// AHB feature registers falsely advertise a BLT engine (minor feature 5 bit
// 31 reads 1); the authoritative hardware database entry ST upstreamed to
// Linux (etnaviv_hwdb.c, customer_id 0x15) says there is NO BLT -- and
// indeed writes to BLT registers hang the FE. Mesa uses the RS engine for
// clears/blits on this core (see etna_clear_blit_rs_init / etnaviv_rs.c).
constexpr uint32_t RS_KICKER = 0x1600; // write 0xbeebbeeb to start
constexpr uint32_t RS_CONFIG = 0x1604; // src format | dest format << 8
constexpr uint32_t RS_SOURCE_STRIDE = 0x160C;
constexpr uint32_t RS_DEST_STRIDE = 0x1614;
constexpr uint32_t RS_WINDOW_SIZE = 0x1620; // width | height << 16
constexpr uint32_t RS_DITHER0 = 0x1630;
constexpr uint32_t RS_DITHER1 = 0x1634;
constexpr uint32_t RS_CLEAR_CONTROL = 0x163C; // bits[15:0] mask, bit16: enable
constexpr uint32_t RS_FILL_VALUE0 = 0x1640;	  // ..4 consecutive dwords
constexpr uint32_t RS_EXTRA_CONFIG = 0x16A0;
constexpr uint32_t RS_SINGLE_BUFFER = 0x16B8;
constexpr uint32_t RS_PIPE_SOURCE_ADDR0 = 0x16C0;
constexpr uint32_t RS_PIPE_DEST_ADDR0 = 0x16E0;
constexpr uint32_t RS_PIPE_OFFSET0 = 0x1700; // x | y << 16
constexpr uint32_t RS_PIPE_OFFSET1 = 0x1704;

constexpr uint32_t RS_FORMAT_A8R8G8B8 = 6;
constexpr uint32_t RS_CLEAR_CONTROL_ENABLED1 = 1 << 16;
constexpr uint32_t RS_KICK = 0xBEEBBEEB;

// RS_CONFIG bits: the RS datapath can swap the R and B channels (RGBA<->BGRA)
// and vertically flip while it copies -- a genuine per-pixel transform. See
// VIVS_RS_CONFIG_SWAP_RB / _FLIP in Mesa's state_3d.xml.h.
constexpr uint32_t RS_CONFIG_SWAP_RB = 0x20000000;
constexpr uint32_t RS_CONFIG_FLIP = 0x40000000;
// Source is a (basic-)tiled surface: the RS untiles as it copies ("resolve").
// Pair with RS_SOURCE_STRIDE = tiled_stride << 2 (a tile row = 4 pixel rows).
// The 0x80000000 stride TILING bit is only for SUPER-tiled sources -- not us.
constexpr uint32_t RS_CONFIG_SOURCE_TILED = 0x00000080;

// GL_FLUSH_CACHE bits for finishing RS/PE work
constexpr uint32_t GL_FLUSH_CACHE_COLOR = 1 << 1;
constexpr uint32_t GL_FLUSH_CACHE_DEPTH = 1 << 0;

// BLT engine (halti5 cores; NOT PRESENT on the MP25 despite the feature
// registers claiming otherwise -- kept for reference/other chips)
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
