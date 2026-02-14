#include "drivers/aarch64_system_reg.hh"
#include <stddef.h>
#include <stdint.h>

// ==========================
// Region definitions (STM32MP25x)
// ==========================
#define DDR_BASE 0x80000000ULL
#define DDR_SIZE 0x40000000ULL // 1 GiB (ST common config)

#define PERIPH_BASE 0x40000000ULL
#define PERIPH_SIZE 0x40000000ULL // 1 GiB -> 0x4000_0000..0x7FFF_FFFF

// ROM checks SYSRAM branch targets within [0x0E00_0000, 0x0E03_FFFF]
// Map one 2 MiB block starting at 0x0E00_0000 as Normal cacheable.
#define IRAM2M_BASE 0x0E000000ULL // 2 MiB aligned
#define IRAM2M_SIZE 0x00200000ULL // 2 MiB

// ==========================
// AArch64 stage-1 descriptor bits
// ==========================
#define DESC_VALID (1ULL << 0)
#define DESC_TABLE (1ULL << 1) // for table descriptors
#define DESC_BLOCK (0ULL << 1) // for block descriptors at L1/L2

#define PTE_AF (1ULL << 10)
#define PTE_SH_INNER (3ULL << 8)  // Inner Shareable
#define PTE_AP_RW_EL1 (0ULL << 6) // EL1 RW, EL0 no access
#define PTE_NS (1ULL << 5)		  // Non-secure

#define PTE_ATTRINDX(n) ((uint64_t)(n) << 2) // bits[4:2]

// MAIR encodings:
//  AttrIdx0: Device-nGnRnE = 0x00
//  AttrIdx1: Normal WBWA   = 0xFF
//  AttrIdx2: NonCache      = 0x44
#define MAIR_ATTR_DEVICE_nGnRnE 0x00ULL
#define MAIR_ATTR_NORMAL_WBWA 0xFFULL
#define MAIR_ATTR_NORMAL_NC 0x44ULL

constexpr uint64_t mair =
	(MAIR_ATTR_DEVICE_nGnRnE << (8 * 0)) | (MAIR_ATTR_NORMAL_WBWA << (8 * 1)) | (MAIR_ATTR_NORMAL_NC << (8 * 2));

// MAIR idx 0: Device-nGnRnE
constexpr uint64_t attr_device = DESC_VALID | DESC_BLOCK | PTE_ATTRINDX(0) | PTE_AF | PTE_AP_RW_EL1 | PTE_NS;

// MAIR idx 1: Normal WBWA
constexpr uint64_t attr_normal =
	DESC_VALID | DESC_BLOCK | PTE_ATTRINDX(1) | PTE_SH_INNER | PTE_AF | PTE_AP_RW_EL1 | PTE_NS;

// MAIR idx 2: Non-cache
constexpr uint64_t attr_noncache =
	DESC_VALID | DESC_BLOCK | PTE_ATTRINDX(2) | PTE_SH_INNER | PTE_AF | PTE_AP_RW_EL1 | PTE_NS;

// TCR_EL1 (4KB granule, 32-bit VA, 32-bit PA)
#define TCR_TG0_4K (0ULL << 14)		// TG0: 4 kB granules
#define TCR_SH0_INNER (3ULL << 12)	// SH0: translation table walks are inner shareable
#define TCR_ORGN0_WBWA (1ULL << 10) // ORGN0: translation table walks are outer WB/WA
#define TCR_IRGN0_WBWA (1ULL << 8)	// IRGN0: translation table walks are inner WB/WA
// TODO: Don't we want walks enabled?
#define TCR_EPD1_DISABLE_EL1 (1ULL << 23) // disable TTBR1 walks
// #define TCR_T0SZ_48BIT (16ULL << 0)	  // 48-bit VA => T0SZ=16
#define TCR_T0SZ_32BIT (32ULL << 0)	   // 32-bit VA => T0SZ=32
#define TCR_IPS_32BIT_EL1 (0ULL << 32) // 0b000 => 32-bit PA (EL1)
#define TCR_PS_32BIT_EL3 (0ULL << 32)  // 0b000 => 32-bit PA (EL3)

// Page tables (4KB aligned)
// L0 -> L1 -> (optional) L2 for VA[0..1GiB)
static uint64_t tt_l0[512] __attribute__((aligned(4096)));
static uint64_t tt_l1[512] __attribute__((aligned(4096)));
static uint64_t tt_l2_low[512] __attribute__((aligned(4096))); // backs L1[0] (VA 0..1GiB)

// --------------------------
// System register helpers
// --------------------------

// Clean+invalidate all data cache to PoC
static inline void dcache_civac_all(void)
{
	// Iterate by set/way using CCSIDR. This is the standard ARM pattern.
	// Assumes data cache is at least present at EL1. Safe even if D-cache is off.
	uint64_t clidr;
	asm volatile("mrs %0, clidr_el1" : "=r"(clidr));

	for (unsigned level = 0; level < 7; level++) {
		unsigned ctype = (clidr >> (level * 3)) & 0x7U;
		if (ctype < 2)
			continue; // no data/unified cache at this level

		uint64_t csselr = (uint64_t)(level << 1); // select data/unified
		asm volatile("msr csselr_el1, %0" : : "r"(csselr));
		isb();

		uint64_t ccsidr;
		asm volatile("mrs %0, ccsidr_el1" : "=r"(ccsidr));

		unsigned line_len = ((ccsidr & 0x7) + 4); // log2(words) + 2 => log2(bytes)
		unsigned ways = ((ccsidr >> 3) & 0x3FF) + 1;
		unsigned sets = ((ccsidr >> 13) & 0x7FFF) + 1;

		// way_shift = 32 - log2(ways)
		unsigned way_shift = 32;
		unsigned tmp = ways - 1;
		while (tmp) {
			way_shift--;
			tmp >>= 1;
		}

		for (unsigned way = 0; way < ways; way++) {
			for (unsigned set = 0; set < sets; set++) {
				uint64_t sw = ((uint64_t)level << 1) | ((uint64_t)set << line_len) | ((uint64_t)way << way_shift);
				asm volatile("dc cisw, %0" : : "r"(sw));
			}
		}
	}

	dsb_sy();
}

static void zero_tables(void)
{
	for (size_t i = 0; i < 512; i++) {
		tt_l0[i] = 0;
		tt_l1[i] = 0;
		tt_l2_low[i] = 0;
	}
}

static void map_l1_block(uint64_t va, uint64_t pa, uint64_t attr)
{
	// L1 index: VA[38:30] (1 GiB)
	const uint64_t idx = (va >> 30) & 0x1FFULL;
	tt_l1[idx] = (pa & 0xFFFFFC0000000ULL) | attr;
}

static void map_l2_block_low_2m(uint64_t va, uint64_t pa, uint64_t attr)
{
	// L2 index: VA[29:21] (2 MiB) within the L2 table that backs L1[0] (VA 0..1GiB)
	const uint64_t idx = (va >> 21) & 0x1FFULL;
	tt_l2_low[idx] = (pa & 0xFFFFFFE00000ULL) | attr;
}

static void populate_tables()
{
	zero_tables();

	// L0[0] -> L1
	tt_l0[0] = ((uint64_t)tt_l1) | DESC_VALID | DESC_TABLE;

	// L1[0] -> L2 for VA 0..1GiB so we can place a precise SRAM mapping in low space.
	tt_l1[0] = ((uint64_t)tt_l2_low) | DESC_VALID | DESC_TABLE;

	// SRAM/SYSRAM region (2 MiB window at 0x0E00_0000) cacheable
	map_l2_block_low_2m(IRAM2M_BASE, IRAM2M_BASE, attr_normal);

	// Peripherals 0x4000_0000..0x7FFF_FFFF as Device
	map_l1_block(PERIPH_BASE, PERIPH_BASE, attr_device);

	// DDR 1GiB @ 0x8000_0000 as Normal cacheable
	map_l1_block(DDR_BASE, DDR_BASE, attr_normal);

	dsb_sy();
}

extern "C" void mmu_enable()
{
	// If caches/MMU are already enabled, clean+invalidate before changing attributes/tables.
	// This avoids “stale” lines or aliasing surprises
	dcache_civac_all();
	ic_iallu();
	dsb_sy();
	isb();

	populate_tables();

	// Point TTBR0 to the top level page table
	//   TTBR0 -> L0
	write_ttbr0_el1((uint64_t)tt_l0);

	// MAIR: idx0 Device, idx1 Normal WBWA, idx2 NonCache
	write_mair_el1(mair);

	// Translation regime configuration
	constexpr uint64_t tcr_el1 = TCR_T0SZ_32BIT | TCR_TG0_4K | TCR_SH0_INNER | TCR_ORGN0_WBWA | TCR_IRGN0_WBWA |
								 TCR_EPD1_DISABLE_EL1 | TCR_IPS_32BIT_EL1;

	write_tcr_el1(tcr_el1);

	//  Invalidate TLBs for the new regime
	dsb_sy();
	isb();
	tlbi_vmalle1is();
	dsb_sy();
	isb();

	// Enable MMU + caches
	uint64_t sctlr = read_sctlr_el1();
	sctlr |= (SCTLR_M | SCTLR_C | SCTLR_I);
	write_sctlr_el1(sctlr);
	isb();
}

extern "C" void mmu_enable_el3()
{
	dcache_civac_all();
	ic_iallu();
	dsb_sy();
	isb();

	populate_tables();

	// Initialize MAIR_EL3 to have attributes matching MAIR_IDX
	write_mair_el3(mair);

	// Translation regime configuration
	constexpr uint64_t tcr_el3 =
		TCR_T0SZ_32BIT | TCR_TG0_4K | TCR_SH0_INNER | TCR_ORGN0_WBWA | TCR_IRGN0_WBWA | TCR_PS_32BIT_EL3;
	write_tcr_el3(tcr_el3);

	// Point TTBR0 to the top level page table
	write_ttbr0_el3((uint64_t)tt_l0);

	//  Invalidate TLBs for the new regime
	dsb_sy();
	isb();
	tlbi_all_e3();
	dsb_sy();
	isb();

	// Enable MMU + caches
	uint64_t sctlr = read_sctlr_el3();
	sctlr |= (SCTLR_M | SCTLR_C | SCTLR_I);
	write_sctlr_el3(sctlr);
	isb();
}
