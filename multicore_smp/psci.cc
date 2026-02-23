#include "aarch64/system_reg.hh"
#include "drivers/smc.hh"
#include "stm32mp257cxx_ca35.h"
#include <cstdint>

#define PSCI_CPU_ON_AARCH64 0xC4000003ULL

int start_cpu1(void (*cpu1_entry)(void), uint64_t context)
{
	uint64_t mpidr0 = get_mpid();

	// Keep affinity levels except AFF0, then set AFF0=1
	// (AFF0 is bits[7:0] in MPIDR on Armv8-A)
	uint64_t target = (mpidr0 & ~0xFFULL) | 1ULL;

	if (get_current_el() == 3) {
		// Set reset vector for CPU1 in 64-bit mode
		CA35SYSCFG->VBAR_CR = ((uint32_t)(uintptr_t)cpu1_entry) & ~0b11;

		// Reset CPU1 processor core 1
		RCC->C1P1RSTCSETR = RCC_C1P1RSTCSETR_C1P1RST;
		while (RCC->C1P1RSTCSETR & RCC_C1P1RSTCSETR_C1P1RST) {
			;
		}

		return 0;
	} else {
		auto ret = smc_call(PSCI_CPU_ON_AARCH64, target, (uint64_t)cpu1_entry, context, 0, 0, 0, 0);

		return ret.a0; // 0 means success
	}
}
