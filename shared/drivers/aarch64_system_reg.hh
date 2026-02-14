#pragma once
#include <cstdint>

inline void dsb_sy()
{
	asm volatile("dsb sy" ::: "memory");
}

inline void isb()
{
	asm volatile("isb" ::: "memory");
}

inline uint64_t get_core_id()
{
	uint64_t core_id;
	asm volatile("mrs %0, mpidr_el1\n\t" : "=r"(core_id) : : "memory");
	uint64_t aff3 = (core_id >> 32) & 0xFF;
	return aff3;
}

inline uint64_t get_mpid()
{
	uint64_t core_id;
	asm volatile("mrs %0, mpidr_el1\n\t" : "=r"(core_id) : : "memory");
	uint64_t aff3 = (core_id >> 8) & 0xFF00'0000;
	return (core_id & 0x00FF'FFFF) | aff3;
}

// Return raw CurrentEL register (0b1100 - 0b0000)
inline uint32_t read_current_el()
{
	uint64_t current_el;
	asm volatile("mrs %0, CurrentEL\n\t" : "=r"(current_el) : : "memory");
	return current_el;
}

// Returns current EL level (0-3)
inline uint32_t get_current_el()
{
	constexpr uint64_t CURRENT_EL_MASK = 0x3;
	constexpr uint64_t CURRENT_EL_SHIFT = 2;

	uint32_t current_el = read_current_el();
	return ((current_el >> CURRENT_EL_SHIFT) & CURRENT_EL_MASK);
}

// HCR
inline uint64_t read_hcr_el2()
{
	uint64_t reg;
	asm volatile("mrs %0, HCR_EL2\n\t" : "=r"(reg) : : "memory");
	return reg;
}

// SPSR
inline uint64_t read_spsr_el1()
{
	uint64_t spsr_el1;
	asm volatile("mrs %0, SPSR_EL1\n\t" : "=r"(spsr_el1) : : "memory");
	return spsr_el1;
}

inline void write_spsr_el1(uint64_t spsr_el1)
{
	asm volatile("msr SPSR_EL1, %0\n\t" : : "r"(spsr_el1) : "memory");
}

inline uint64_t read_spsr_el2()
{
	uint64_t spsr_el2;
	asm volatile("mrs %0, SPSR_EL2\n\t" : "=r"(spsr_el2) : : "memory");
	return spsr_el2;
}

inline void write_spsr_el2(uint64_t spsr_el2)
{
	asm volatile("msr SPSR_EL2, %0\n\t" : : "r"(spsr_el2) : "memory");
}

// VBAR

inline uint64_t read_vbar_el2()
{
	uint64_t vbar_el2;

	asm volatile("mrs %0, VBAR_EL2\n\t" : "=r"(vbar_el2) : : "memory");
	return vbar_el2;
}

inline uint64_t read_vbar_el1()
{
	uint64_t vbar_el1;

	asm volatile("mrs %0, VBAR_EL1\n\t" : "=r"(vbar_el1) : : "memory");
	return vbar_el1;
}

inline void write_vbar_el1(uint64_t vbar_el1)
{
	asm volatile("msr VBAR_EL1, %0\n\t" : : "r"(vbar_el1) : "memory");
}

// DAIF

constexpr uint64_t DAIF_DBG_BIT = (1 << 3); // Debug mask bit
constexpr uint64_t DAIF_ABT_BIT = (1 << 2); // Asynchronous abort mask bit
constexpr uint64_t DAIF_IRQ_BIT = (1 << 1); // IRQ mask bit
constexpr uint64_t DAIF_FIQ_BIT = (1 << 0); // FIQ mask bit

inline uint32_t read_daif()
{
	uint32_t daif;

	asm volatile("mrs %0, DAIF\n\t" : "=r"(daif) : : "memory");
	return daif;
}

inline void write_daif(uint32_t daif)
{
	asm volatile("msr DAIF, %0\n\t" : : "r"(daif) : "memory");
}

inline void enable_debug_exceptions()
{
	asm volatile("msr DAIFClr, %0\n\t" : : "i"(DAIF_DBG_BIT) : "memory");
}

inline void enable_serror_exceptions()
{
	asm volatile("msr DAIFClr, %0\n\t" : : "i"(DAIF_ABT_BIT) : "memory");
}

inline void enable_irq()
{
	asm volatile("msr DAIFClr, %0\n\t" : : "i"(DAIF_IRQ_BIT) : "memory");
}

inline void enable_fiq()
{
	asm volatile("msr DAIFClr, %0\n\t" : : "i"(DAIF_FIQ_BIT) : "memory");
}

inline void disable_debug_exceptions()
{
	asm volatile("msr DAIFSet, %0\n\t" : : "i"(DAIF_DBG_BIT) : "memory");
}

inline void disable_serror_exceptions()
{
	asm volatile("msr DAIFSet, %0\n\t" : : "i"(DAIF_ABT_BIT) : "memory");
}

inline void disable_irq()
{
	asm volatile("msr DAIFSet, %0\n\t" : : "i"(DAIF_IRQ_BIT) : "memory");
}

inline void disable_fiq()
{
	asm volatile("msr DAIFSet, %0\n\t" : : "i"(DAIF_FIQ_BIT) : "memory");
}

// SCTLR

#define SCTLR_M (1ULL << 0)
#define SCTLR_C (1ULL << 2)
#define SCTLR_I (1ULL << 12)

inline uint64_t read_sctlr_el1()
{
	uint64_t v;
	asm volatile("mrs %0, sctlr_el1" : "=r"(v));
	return v;
}
inline uint64_t read_sctlr_el3()
{
	uint64_t v;
	asm volatile("mrs %0, sctlr_el3" : "=r"(v));
	return v;
}
inline void write_sctlr_el1(uint64_t v)
{
	asm volatile("msr sctlr_el1, %0" ::"r"(v));
}
inline void write_sctlr_el3(uint64_t v)
{
	asm volatile("msr sctlr_el3, %0" ::"r"(v));
}

// Read counter freq
inline uint64_t read_cntfreq()
{
	uint64_t v;
	asm volatile("mrs %0, cntfrq_el0" : "=r"(v));
	return v;
}

// Read physical counter ticks (cntpct)
inline uint64_t read_cntpct()
{
	uint64_t v;
	asm volatile("mrs %0, cntpct_el0" : "=r"(v));
	return v;
}

//
// CNTP control:
//  bit0 ENABLE
//  bit1 IMASK (1 masks interrupt)
//  bit2 ISTATUS (read-only, 1 if interrupt condition met)

inline uint32_t read_cntp_ctl()
{
	uint64_t v;
	asm volatile("mrs %0, cntp_ctl_el0" : "=r"(v));
	return (uint32_t)v;
}

inline void write_cntp_ctl(uint32_t v)
{
	asm volatile("msr cntp_ctl_el0, %0" ::"r"((uint64_t)v));
}

inline void cntp_enable(bool en)
{
	uint32_t ctl = read_cntp_ctl();
	if (en)
		ctl |= (1u << 0);
	else
		ctl &= ~(1u << 0);
	write_cntp_ctl(ctl);
}

inline void cntp_irq_enable(bool en)
{
	uint32_t ctl = read_cntp_ctl();
	if (en)
		ctl &= ~(1u << 1); /* IMASK=0 => unmasked */
	else
		ctl |= (1u << 1); /* IMASK=1 => masked */
	write_cntp_ctl(ctl);
}

// Relative timer value (32-bit ticks from now)
inline void set_cntp_tval_ticks(uint32_t ticks)
{
	asm volatile("msr cntp_tval_el0, %0" ::"r"((uint64_t)ticks));
}

inline bool cntp_irq_is_pending()
{
	return (read_cntp_ctl() & (1u << 2)) != 0;
}

// Absolute compare value (64-bit ticks)
inline void set_cntp_cval(uint64_t cval)
{
	asm volatile("msr cntp_cval_el0, %0" ::"r"(cval));
}

// Caches

inline void clean_dcache_address_u(uintptr_t addr)
{
	asm volatile("dc cvau, %0" ::"r"(addr));
}

inline void clean_dcache_address(uintptr_t addr)
{
	asm volatile("dc cvac, %0" ::"r"(addr));
}

inline void invalidate_dcache_address(uintptr_t addr)
{
	asm volatile("dc ivac, %0" ::"r"(addr));
}

inline void clean_invalidate_dcache_address(uintptr_t addr)
{
	asm volatile("dc civac, %0" ::"r"(addr));
}

inline void zero_dcache_address(uintptr_t addr)
{
	asm volatile("dc zva, %0" ::"r"(addr));
}

inline void tlbi_vmalle1is()
{
	asm volatile("tlbi vmalle1is" ::: "memory");
}

inline void tlbi_all_e3()
{
	asm volatile("tlbi alle3" ::: "memory");
}

inline void ic_iallu()
{
	asm volatile("ic iallu" ::: "memory");
}

//
// MMU
//

// MAIR
inline void write_mair_el1(uint64_t v)
{
	asm volatile("msr mair_el1, %0" ::"r"(v) : "memory");
}
inline void write_mair_el3(uint64_t v)
{
	asm volatile("msr mair_el3, %0" ::"r"(v) : "memory");
}

// TCR
inline void write_tcr_el1(uint64_t v)
{
	asm volatile("msr tcr_el1, %0" ::"r"(v) : "memory");
}
inline void write_tcr_el3(uint64_t v)
{
	asm volatile("msr tcr_el3, %0" ::"r"(v) : "memory");
}

// TTBR0
inline void write_ttbr0_el1(uint64_t v)
{
	asm volatile("msr ttbr0_el1, %0" ::"r"(v) : "memory");
}
inline void write_ttbr0_el3(uint64_t v)
{
	asm volatile("msr ttbr0_el3, %0" ::"r"(v) : "memory");
}
