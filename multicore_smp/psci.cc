#include "drivers/aarch64_system_reg.hh"
#include <cstdint>

#define PSCI_CPU_ON_AARCH64 0xC4000003ULL

asm(".global psci_cpu_on_smc\n"
	"psci_cpu_on_smc:\n"
	"	smc    #0\n"
	"	ret");

extern "C" uint64_t psci_cpu_on_smc(uint64_t fn, uint64_t target_cpu, uint64_t entry, uint64_t context);

int start_cpu1(void (*cpu1_entry)(void), uint64_t context)
{
	uint64_t mpidr0 = get_mpid();

	// Keep affinity levels except AFF0, then set AFF0=1
	// (AFF0 is bits[7:0] in MPIDR on Armv8-A)
	uint64_t target = (mpidr0 & ~0xFFULL) | 1ULL;

	uint64_t ret = psci_cpu_on_smc(PSCI_CPU_ON_AARCH64, target, (uint64_t)cpu1_entry, context);

	return (int)ret; // 0 means success
}
