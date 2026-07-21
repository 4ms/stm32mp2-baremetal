#include "aarch64/system_reg.hh"
#include "gpu_io.hh"
#include "gpu_regs.hh"
#include "print/print.hh"
#include <array>
#include <cstdint>

namespace etna
{
// -------------------- GPU MMU --------------------
// The memory-touching engines (PE, BLT) do not work with the MMU in bypass on
// this core: the BLT accepts its state writes and then hangs on the first
// operation, with no fault anywhere. Both etnaviv and the vendor gcnano
// driver unconditionally enable the GPU MMU before running any engine, so we
// do the same, with an identity (virt == phys) mapping of all of DDR.
// This mirrors etnaviv_iommuv2_restore_sec() (etnaviv_iommu_v2.c).

namespace
{
constexpr uint32_t DdrMB = 512;			 // EV1: 512MB of DDR at 0x80000000
constexpr uint32_t NumStlbs = DdrMB / 4; // each STLB maps 4MB

alignas(4096) uint64_t mmu_pta[512];			 // page table array: 64-bit entry per context
alignas(4096) uint32_t mmu_mtlb[1024];			 // master TLB: 4MB per entry
alignas(4096) uint32_t mmu_stlb[NumStlbs][1024]; // slave TLBs: 4KB pages
alignas(4096) uint32_t mmu_safe_page[1024];		 // landing zone for faulting accesses
alignas(64) std::array<uint32_t, 4> pta_cmdbuf;
} // namespace

bool gpu_mmu_enable()
{
	using namespace VivanteGpu;

	// Identity-map DDR: MTLB index = addr[31:22], so DDR starts at entry 512
	for (auto &e : mmu_mtlb)
		e = 0; // not present
	for (uint32_t s = 0; s < NumStlbs; s++) {
		uint32_t base = 0x80000000 + s * 0x400000;
		for (uint32_t p = 0; p < 1024; p++)
			mmu_stlb[s][p] = (base + p * 4096) | MMU_PTE_PRESENT | MMU_PTE_WRITEABLE;
		mmu_mtlb[512 + s] = bus_addr(&mmu_stlb[s][0]) | MMU_PTE_PRESENT;
	}
	mmu_pta[0] = bus_addr(mmu_mtlb); // | mode: 0 = 4K pages

	clean_dcache_range(mmu_pta, sizeof(mmu_pta));
	clean_dcache_range(mmu_mtlb, sizeof(mmu_mtlb));
	clean_dcache_range(mmu_stlb, sizeof(mmu_stlb));

	gpu_write(MMUv2_PTA_ADDRESS_LOW, bus_addr(mmu_pta));
	gpu_write(MMUv2_PTA_ADDRESS_HIGH, 0);
	gpu_write(MMUv2_PTA_CONTROL, 1); // enable

	gpu_write(MMUv2_NONSEC_SAFE_ADDR_LOW, bus_addr(mmu_safe_page));
	gpu_write(MMUv2_SEC_SAFE_ADDR_LOW, bus_addr(mmu_safe_page));
	gpu_write(MMUv2_SAFE_ADDRESS_CONFIG, 0); // address bits 39:32 of both

	// The MMU loads its PTA entry via a command the FE executes (while
	// still in bypass), then the secure-bank control arms the MMU proper
	pta_cmdbuf[0] = cmd_load_state(MMUv2_PTA_CONFIG);
	pta_cmdbuf[1] = 0; // PTA index 0
	pta_cmdbuf[2] = CMD_END;
	pta_cmdbuf[3] = 0;
	clean_dcache_range(pta_cmdbuf.data(), sizeof(pta_cmdbuf));

	gpu_write(FE_COMMAND_ADDRESS, (uint32_t)(uintptr_t)pta_cmdbuf.data());
	gpu_write(FE_COMMAND_CONTROL, FE_CONTROL_ENABLE | (pta_cmdbuf.size() / 2));
	gpu_write(MMUv2_SEC_COMMAND_CONTROL, FE_CONTROL_ENABLE | (pta_cmdbuf.size() / 2));

	if (!wait_idle(IDLE_FE, 10'000)) {
		print("ERROR: FE did not go idle loading the MMU PTA\n");
		return false;
	}

	gpu_write(MMUv2_SEC_CONTROL, 1); // enable the MMU
	return true;
}

} // namespace etna
