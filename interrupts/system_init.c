// #include <stdint.h>

void mmu_enable(void);

// static inline uint64_t read_sctlr_el1(void)
// {
// 	uint64_t v;
// 	__asm__ volatile("mrs %0, sctlr_el1" : "=r"(v));
// 	return v;
// }
// static inline void write_sctlr_el1(uint64_t v)
// {
// 	__asm__ volatile("msr sctlr_el1, %0" : : "r"(v) : "memory");
// }
// static inline void dsb_sy(void)
// {
// 	__asm__ volatile("dsb sy" ::: "memory");
// }
// static inline void isb(void)
// {
// 	__asm__ volatile("isb" ::: "memory");
// }
// static inline void tlbi_vmalle1is(void)
// {
// 	__asm__ volatile("tlbi vmalle1is" ::: "memory");
// }

// #define SCTLR_M (1ULL << 0)
// #define SCTLR_C (1ULL << 2)
// #define SCTLR_I (1ULL << 12)

void system_init(void)
{
	// Invalidate TLB after turning MMU off
	// dsb_sy();
	// tlbi_vmalle1is();
	// dsb_sy();
	// isb();

	mmu_enable();
}
