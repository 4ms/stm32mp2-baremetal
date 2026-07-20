#pragma once
#include "drivers/hal_cnt.hh"
#include "gpu_regs.hh"
#include "stm32mp2xx.h"

volatile uint32_t *const gpu = reinterpret_cast<volatile uint32_t *>(GPU_BASE);

inline uint32_t gpu_read(uint32_t offset)
{
	return gpu[offset / 4];
}

inline void gpu_write(uint32_t offset, uint32_t value)
{
	gpu[offset / 4] = value;
}

// GPU addresses are 32-bit bus addresses. We run with a flat (virt == phys)
// MMU mapping, so a pointer is already a bus address.
inline uint32_t bus_addr(const void *p)
{
	return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p));
}

inline bool wait_idle(uint32_t idle_bits, unsigned timeout_us)
{
	while (timeout_us--) {
		if ((gpu_read(VivanteGpu::HI_IDLE_STATE) & idle_bits) == idle_bits)
			return true;
		udelay(1);
	}
	return false;
}

// Wait for event bits to latch in HI_INTR_ACKNOWLEDGE. Unlike the idle bits
// (which also read "idle" if the FE never started!), an event is positive
// proof the command stream executed up to the EVENT command.
// Reading the register clears it, so accumulate into intr_acc.
inline bool wait_event(uint32_t event_mask, unsigned timeout_us, uint32_t &intr_acc)
{
	while (timeout_us--) {
		intr_acc |= gpu_read(VivanteGpu::HI_INTR_ACKNOWLEDGE);
		if ((intr_acc & event_mask) == event_mask)
			return true;
		// udelay(1);
	}
	return false;
}
