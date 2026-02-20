#include "drivers/rcc.hh"
#include "drivers/watchdog.hh"
#include "interrupt/interrupt.hh"
#include "print/print.hh"
#include "stm32mp2xx_hal.h"

constexpr inline uint32_t BufferWords = 8 * 1024;

alignas(64) __attribute__((section(".ddma"))) std::array<uint32_t, BufferWords> src_buffer;
alignas(64) __attribute__((section(".ddma"))) std::array<uint32_t, BufferWords> dst_buffer;
constexpr inline size_t BufferBytes = BufferWords * sizeof(src_buffer[0]);

static int check_data();

int main()
{
	print("HAL DMA Example\n");

	RCC_Enable::HPDMA1_::set();

	HAL_Init();

	print("Tick = ", HAL_GetTick(), "\n");

	DMA_HandleTypeDef hdma;

	hdma.Instance = HPDMA1_Channel0;
	hdma.Init.Request = DMA_REQUEST_SW;
	hdma.Init.BlkHWRequest = DMA_BREQ_SINGLE_BURST;
	hdma.Init.Direction = DMA_MEMORY_TO_MEMORY;
	hdma.Init.SrcInc = DMA_SINC_INCREMENTED;
	hdma.Init.DestInc = DMA_DINC_INCREMENTED;
	hdma.Init.SrcDataWidth = DMA_SRC_DATAWIDTH_WORD;
	hdma.Init.DestDataWidth = DMA_DEST_DATAWIDTH_WORD;
	hdma.Init.Priority = DMA_LOW_PRIORITY_HIGH_WEIGHT;
	hdma.Init.SrcBurstLength = 1;
	hdma.Init.DestBurstLength = 1;
	hdma.Init.TransferAllocatedPort = DMA_SRC_ALLOCATED_PORT0 | DMA_DEST_ALLOCATED_PORT1;
	hdma.Init.TransferEventMode = DMA_TCEM_BLOCK_TRANSFER;
	hdma.Init.Mode = DMA_NORMAL;

	print("Starting a DMA memory to memory transfer\n");

	if (auto res = HAL_DMA_Init(&hdma); res != HAL_OK) {
		print("ERROR: HAL_DMA_Init returned ", res, "\n");
	}

	// TODO: if secure
	if (auto res = HAL_DMA_ConfigChannelAttributes(
			&hdma, DMA_CHANNEL_PRIV | DMA_CHANNEL_SEC | DMA_CHANNEL_DEST_SEC | DMA_CHANNEL_SRC_SEC);
		res != HAL_OK)
	{
		print("ERROR: HAL_DMA_ConfigChannelAttributes returned ", res, "\n");
	}

	print("Preparing dst buffer: 0x", Hex((uintptr_t)(dst_buffer.data())), "\n");
	for (auto i = 0u; i < BufferWords; i++) {
		dst_buffer[i] = 0x5500AAFF;
	}

	// Write cache to memory
	// Cleaning dst_buffer makes sure we don't have any dirty lines that won't get invalidated later
	for (auto i = 0u; i < BufferWords; i += 16) {
		clean_dcache_address((uintptr_t)(&dst_buffer[i]));
	}

	print("Preparing src buffer: 0x", Hex((uintptr_t)(src_buffer.data())), "\n");
	for (auto i = 0u; i < BufferWords; i++) {
		src_buffer[i] = i * (i * (i * 7 + 3) + 1) + 43;
	}

	// Write cache to memory
	for (auto i = 0u; i < BufferWords; i += 16) {
		clean_dcache_address((uintptr_t)(&src_buffer[i]));
	}

	bool transfer_done = false;

	InterruptManager::register_and_start_isr(HPDMA1_Channel0_IRQn, 0, 0, [&transfer_done]() {
		transfer_done = true;
		print("Got DMA IRQ at tick ", HAL_GetTick(), "\n");

		// Check if src was copied to dst, and report errors or success
		if (check_data() == 0)
			print("DMA transfer success! dst_buffer matches src_buffer\n");
	});

	auto res = HAL_DMA_Start_IT(&hdma, (uintptr_t)src_buffer.data(), (uintptr_t)dst_buffer.data(), BufferBytes);

	if (res != HAL_OK) {
		print("ERROR: HAL_DMA_Start_IT returned ", res, "\n");
	}

	volatile int x = 0x10000000;

	while (true) {
		x = x + 1;
		if ((x % 10'000'000) == 0) {
			print("Tick = ", HAL_GetTick(), "\n");
			watchdog_pet();

			if (!transfer_done) {
				invalidate_dcache_address((uintptr_t)(&dst_buffer[0]));
				if (dst_buffer[0] == src_buffer[0]) {
					transfer_done = true;
					if (check_data() == 0) {
						print("Data transfered OK, but ISR did not fire\n");
					} else
						print("Data seems to have partially transfered, but not fully, and ISR did not fire\n");
				}
			}
		}

		asm("nop");
	}
}

int check_data()
{
	int misses = 0;
	for (auto i = 0u; i < BufferWords; i++) {

		// Invalidate it so cache contents will be replaced by memory when we read
		if (i % 16 == 0)
			invalidate_dcache_address((uintptr_t)(&dst_buffer[i]));

		if (dst_buffer[i] != src_buffer[i]) {
			misses++;
			print("[", i, "] ", Hex{dst_buffer[i]}, " != ", Hex{src_buffer[i]}, "\n");
		}
	}

	return misses;
}

extern "C" void assert_failed(uint8_t *file, uint32_t line)
{
	print("assert failed: ", file, ":", line, "\n");
}
