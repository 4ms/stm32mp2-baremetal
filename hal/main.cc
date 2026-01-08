#include "interrupt.hh"
#include "print.hh"
#include "stm32mp2xx_hal.h"

constexpr inline uint32_t BufferSize = 8 * 1024;

__attribute__((section(".ddma"))) std::array<uint32_t, BufferSize> src_buffer;
__attribute__((section(".ddma"))) std::array<uint32_t, BufferSize> dst_buffer;

int main()
{
	print("HAL Example\n");
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

	if (auto res = HAL_DMA_ConfigChannelAttributes(&hdma, DMA_CHANNEL_NPRIV); res != HAL_OK) {
		print("ERROR: HAL_DMA_ConfigChannelAttributes returned ", res, "\n");
	}

	for (auto i = 0u; i < BufferSize; i++) {
		// print("W:");
		src_buffer[i] = i * (i * (i * 7 + 3) + 1) + 43;
		// print(src_buffer[i], "\n");
		dst_buffer[i] = 0x5500AAFF;
		// print("X\n");
	}

	InterruptManager::register_and_start_isr(HPDMA1_Channel0_IRQn, 0, 0, []() {
		print("Tick = ", HAL_GetTick(), "\n");
		print("DMA IRQ\n");

		for (auto i = 0u; i < BufferSize; i++) {
			if ((i % 8) == 0)
				print("\n");
			print(Hex{dst_buffer[i]}, " ");
		}
	});

	auto res = HAL_DMA_Start_IT(&hdma, (uintptr_t)src_buffer.data(), (uintptr_t)dst_buffer.data(), BufferSize);
	if (res != HAL_OK) {
		print("ERROR: HAL_DMA_Start_IT returned ", res, "\n");
	}

	// HAL_DMA_RegisterCallback(&hdma, HAL_DMA_XFER_CPLT_CB_ID, TransferComplete);
	// HAL_DMA_RegisterCallback(&hdma, HAL_DMA_XFER_ERROR_CB_ID, TransferError);

	volatile int x = 0x10000000;

	while (true) {
		x = x + 1;
		if ((x % 10'000'000) == 0) {
			print("Tick = ", HAL_GetTick(), "\n");
			// ???
			// IWDG1->KR = 0x5555;
			// IWDG1->KR = 0xAAAA;
		}

		asm("nop");
	}
}

extern "C" void assert_failed(uint8_t *file, uint32_t line)
{
	print("assert failed: ", file, ":", line, "\n");
}
