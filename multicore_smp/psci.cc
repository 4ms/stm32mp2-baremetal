#include "drivers/aarch64_system_reg.hh"
#include "smc.hh"
#include <cstdint>

#define PSCI_CPU_ON_AARCH64 0xC4000003ULL

int start_cpu1(void (*cpu1_entry)(void), uint64_t context)
{
	uint64_t mpidr0 = get_mpid();

	// Keep affinity levels except AFF0, then set AFF0=1
	// (AFF0 is bits[7:0] in MPIDR on Armv8-A)
	uint64_t target = (mpidr0 & ~0xFFULL) | 1ULL;

	auto ret = smc_call(PSCI_CPU_ON_AARCH64, target, (uint64_t)cpu1_entry, context, 0, 0, 0, 0);

	return ret.a0; // 0 means success
}
