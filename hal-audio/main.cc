#include "audio_generators.hh"
#include "dma.hh"
#include "drivers/pin.hh"
#include "interrupt.hh"
#include "print.hh"
#include "stm32mp2xx_hal.h"
#include "stm32mp2xx_hal_def.h"
#include "watchdog.hh"
#include <cmath>

static_assert(SAI2_BASE == 0x402a0000);
static_assert(SAI4_BASE == 0x40340000);
static_assert(RIFSC_BASE == 0x42080000);

static void verify_apb2_works_via_tim15();
static void print_security_settings();
static void dump_dma_info(DMA_NodeTypeDef &dma_node1, DMA_NodeConfTypeDef &dma_node_config);
static void dump_sai_registers();
static void test_pins();

constexpr uint32_t BufferWords = 64;
// In SRAM:
alignas(64) static __attribute__((section(".ddma"))) std::array<uint32_t, BufferWords> tx_buffer;
alignas(64) static __attribute__((section(".ddma"))) std::array<uint32_t, BufferWords> rx_buffer;
alignas(64) static __attribute__((section(".ddma"))) DMA_NodeTypeDef dma_tx_node1;
alignas(64) static __attribute__((section(".ddma"))) DMA_QListTypeDef dma_tx_queue;
alignas(64) static __attribute__((section(".ddma"))) DMA_NodeTypeDef dma_rx_node1;
alignas(64) static __attribute__((section(".ddma"))) DMA_QListTypeDef dma_rx_queue;
constexpr size_t BufferBytes = BufferWords * sizeof(tx_buffer[0]);

int main()
{
	DMA_HandleTypeDef hdma_tx;
	DMA_HandleTypeDef hdma_rx;

	struct AudioGen {
		SineGen L{48000};
		SineGen R{48000};
	};
	AudioGen sines{};

	print("HAL Audio Example\n");
	HAL_Init();

	print("Setting SAI2 XBAR\n");
	RCC->PREDIVxCFGR[24] = 0;
	RCC->XBARxCFGR[24] = RCC_XBARxCFGR_XBARxEN | RCC_XBAR_CLKSRC_PLL4;
	while (RCC->XBARxCFGR[24] & RCC_XBARxCFGR_XBARxSTS) // 1 = ongoing change
		;
	RCC->FINDIVxCFGR[24] = RCC_FINDIVxCFGR_FINDIVxEN | 0x30;
	while (RCC->FINDIVSR1 & (1 << 24))
		;

	print("PLL4 freq = ", HAL_RCCEx_GetPLL4ClockFreq(), "\n");
	uint32_t freq = HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_SAI2);
	print("SAI2 kernel clock freq = ", freq, "\n");

	print("Enable SAI clock\n");
	__HAL_RCC_SAI2_CLK_ENABLE();
	__HAL_RCC_SAI2_FORCE_RESET();
	__HAL_RCC_SAI2_RELEASE_RESET();
	__HAL_RCC_HPDMA1_CLK_ENABLE();

	print("Init SAI pins\n");
	Pin scka{GPIO::J, PinNum::_11, PinMode::Alt, AltFunc3, PinPull::Up}; // header pin 12
	Pin sda{GPIO::J, PinNum::_12, PinMode::Alt, AltFunc3, PinPull::Up};	 // header pin 38
	Pin sdb{GPIO::J, PinNum::_2, PinMode::Alt, AltFunc4, PinPull::Up};	 // header pin 40
	Pin mclk{GPIO::J, PinNum::_13, PinMode::Alt, AltFunc3, PinPull::Up}; // R34
	Pin fsa{GPIO::J, PinNum::_3, PinMode::Alt, AltFunc3, PinPull::Up};	 // header pin 35

	print("Init SAI TX periph\n");
	SAI_HandleTypeDef hsai_tx;
	hsai_tx.Instance = SAI2_Block_A;
	hsai_tx.Init.AudioMode = SAI_MODEMASTER_TX;
	hsai_tx.Init.Synchro = SAI_ASYNCHRONOUS;
	hsai_tx.Init.SynchroExt = SAI_SYNCEXT_DISABLE;
	hsai_tx.Init.MckOutput = SAI_MCK_OUTPUT_ENABLE;
	hsai_tx.Init.OutputDrive = SAI_OUTPUTDRIVE_DISABLE;
	hsai_tx.Init.NoDivider = SAI_MASTERDIVIDER_ENABLE;
	hsai_tx.Init.FIFOThreshold = SAI_FIFOTHRESHOLD_EMPTY;
	hsai_tx.Init.AudioFrequency = 48000;
	hsai_tx.Init.MckOverSampling = SAI_MCK_OVERSAMPLING_ENABLE;
	hsai_tx.Init.MonoStereoMode = SAI_STEREOMODE;
	hsai_tx.Init.CompandingMode = SAI_NOCOMPANDING;
	hsai_tx.Init.TriState = SAI_OUTPUT_NOTRELEASED;
	hsai_tx.Init.PdmInit.Activation = DISABLE;

	if (auto res = HAL_SAI_DeInit(&hsai_tx); res != HAL_OK) {
		print("HAL_SAI_DeInit failed ", res, "\n");
	}

	if (auto res = HAL_SAI_InitProtocol(&hsai_tx, SAI_I2S_STANDARD, SAI_PROTOCOL_DATASIZE_24BIT, 2); res != HAL_OK) {
		print("ERROR: HAL_SAI_InitProtocol returned ", res, "\n");
	}

	print("Init SAI RX periph\n");
	SAI_HandleTypeDef hsai_rx;
	hsai_rx.Instance = SAI2_Block_B;
	hsai_rx.Init.AudioMode = SAI_MODESLAVE_RX;
	hsai_rx.Init.Synchro = SAI_SYNCHRONOUS;
	hsai_rx.Init.SynchroExt = SAI_SYNCEXT_DISABLE;
	hsai_rx.Init.MckOutput = SAI_MCK_OUTPUT_DISABLE;
	hsai_rx.Init.OutputDrive = SAI_OUTPUTDRIVE_DISABLE;
	hsai_rx.Init.NoDivider = SAI_MASTERDIVIDER_ENABLE;
	hsai_rx.Init.FIFOThreshold = SAI_FIFOTHRESHOLD_EMPTY;
	hsai_rx.Init.AudioFrequency = 48000;
	hsai_rx.Init.MckOverSampling = SAI_MCK_OVERSAMPLING_ENABLE;
	hsai_rx.Init.MonoStereoMode = SAI_STEREOMODE;
	hsai_rx.Init.CompandingMode = SAI_NOCOMPANDING;
	hsai_rx.Init.TriState = SAI_OUTPUT_NOTRELEASED;
	hsai_rx.Init.PdmInit.Activation = DISABLE;

	if (auto res = HAL_SAI_DeInit(&hsai_rx); res != HAL_OK) {
		print("HAL_SAI_DeInit failed ", res, "\n");
	}

	if (auto res = HAL_SAI_InitProtocol(&hsai_rx, SAI_I2S_STANDARD, SAI_PROTOCOL_DATASIZE_24BIT, 2); res != HAL_OK) {
		print("ERROR: HAL_SAI_InitProtocol returned ", res, "\n");
	}

	// DMA TX Setup
	hdma_tx.Instance = HPDMA1_Channel0;
	hdma_tx.Init.Request = HPDMA_REQUEST_SAI2_A;
	hdma_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;

	DMA_NodeConfTypeDef dma_node_config_tx;
	dma_node_config_tx.SrcAddress = (uintptr_t)tx_buffer.data();
	dma_node_config_tx.DstAddress = reinterpret_cast<uintptr_t>(&SAI2_Block_A->DR);
	dma_node_config_tx.DataSize = BufferBytes;

	DmaHelper::setup_circular(hdma_tx, dma_tx_queue, dma_tx_node1, dma_node_config_tx);
	dump_dma_info(dma_tx_node1, dma_node_config_tx);

	print("Link DMA TX and SAI TX\n");
	__HAL_LINKDMA(&hsai_tx, hdmatx, hdma_tx);

	// DMA RX Setup
	hdma_rx.Instance = HPDMA1_Channel1;
	hdma_rx.Init.Request = HPDMA_REQUEST_SAI2_B;
	hdma_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;

	DMA_NodeConfTypeDef dma_node_config_rx;
	dma_node_config_rx.SrcAddress = reinterpret_cast<uintptr_t>(&SAI2_Block_B->DR);
	dma_node_config_rx.DstAddress = (uintptr_t)rx_buffer.data();
	dma_node_config_rx.DataSize = BufferBytes;

	DmaHelper::setup_circular(hdma_rx, dma_rx_queue, dma_rx_node1, dma_node_config_rx);
	dump_dma_info(dma_rx_node1, dma_node_config_rx);

	print("Link DMA RX and SAI RX\n");
	__HAL_LINKDMA(&hsai_rx, hdmarx, hdma_rx);

	InterruptManager::register_and_start_isr(HPDMA1_Channel0_IRQn, 1, 1, [&hdma_tx, &sines]() {
		for (auto i = 0u; i < tx_buffer.size(); i += 2) {
			tx_buffer[i] = sines.L.sample(150);
			tx_buffer[i + 1] = sines.R.sample(500);
		}

		for (auto i = 0u; i < BufferWords; i += 16) {
			clean_dcache_address((uintptr_t)(&tx_buffer[i]));
		}

		HAL_DMA_IRQHandler(&hdma_tx);

		if (hdma_tx.ErrorCode != 0)
			print("DMA err = ", hdma_tx.ErrorCode, " state=", hdma_tx.State, "\n");
	});

	for (auto i = 0u; i < tx_buffer.size(); i += 2) {
		tx_buffer[i] = sines.L.sample(150);
		tx_buffer[i + 1] = sines.R.sample(500);
	}

	// Write tx_buffer cache to memory
	// Cleaning rx_buffer makes sure we don't have any dirty lines that won't get invalidated later
	for (auto i = 0u; i < BufferWords; i += 16) {
		clean_dcache_address((uintptr_t)(&tx_buffer[i]));
		clean_dcache_address((uintptr_t)(&rx_buffer[i]));
	}

	print("Start transmitting/receiving\n");
	if (auto res = HAL_SAI_Receive_DMA(&hsai_rx, (uint8_t *)rx_buffer.data(), rx_buffer.size()); res != HAL_OK) {
		print("ERROR: HAL_SAI_Receive_DMA returned ", res, "\n");
	}

	if (auto res = HAL_SAI_Transmit_DMA(&hsai_tx, (uint8_t *)tx_buffer.data(), tx_buffer.size()); res != HAL_OK) {
		print("ERROR: HAL_SAI_Transmit_DMA returned ", res, "\n");
	}

	// Endless loop
	volatile int x = 0x10000000;
	while (true) {
		x = x + 1;
		if ((x % 10'000'000) == 0) {
			print("Tick = ", HAL_GetTick(), "\n");

			watchdog_pet();
		}

		asm("nop");
	}
}

void test_pins()
{
	Pin scka{GPIO::J, PinNum::_11, PinMode::Output};  // pin 12
	Pin fsa{GPIO::J, PinNum::_3, PinMode::Output};	  // pin 27
	Pin sda{GPIO::J, PinNum::_12, PinMode::Output};	  // pin 38
	Pin sdb{GPIO::J, PinNum::_2, PinMode::Output};	  // pin 40
	Pin mclka{GPIO::J, PinNum::_13, PinMode::Output}; // LED4

	scka.high();
	fsa.high();
	sda.high();
	sdb.high();
	mclka.high();

	scka.low();
	fsa.low();
	sda.low();
	sdb.low();
	mclka.low();
}

void verify_apb2_works_via_tim15()
{
	__HAL_RCC_TIM15_CLK_ENABLE();
	HAL_Delay(1);
	TIM15->CNT = 0x5000;
	TIM15->PSC = 0x1;
	TIM15->ARR = 0x7777;
	TIM15->CR1 = (1 << 7) | (0b01 << 8) | (1 << 0);
	print("TIM15->CNT = ", Hex{TIM15->CNT}, "\n");
	HAL_Delay(10);
	// This prints a lower value than the previous print
	print("TIM15->CNT = ", Hex{TIM15->CNT}, "\n");
}

void print_security_settings()
{
	print("RCC_SECCFGR0 = ", Hex{RCC->SECCFGR[0]}, "\n");
	print("RCC_PRIVCFGR0 = ", Hex{RCC->PRIVCFGR[0]}, "\n");

	// RM Table 38:
	// SAI2 RISC index is 50. *CFGR[0] spans 0-31, *CFGR[1] spans 32-63. So we check [1] for bit 50-32=18
	print("RISC->SECCFGR[1] = ", Hex{RISC->SECCFGR[1]}, " (bit 18 = SAI2)\n");
	print("RISC->PRIVCFGR[1] = ", Hex{RISC->PRIVCFGR[1]}, " (bit 18 = SAI2)\n");
	print("RISC->PER50_CIDCFGR = ", Hex{RISC->PER[50].CIDCFGR}, "\n");

	print("GPIOJ->SECCFGR = ", Hex{GPIOJ->SECCFGR}, "\n");
}

void dump_dma_info(DMA_NodeTypeDef &dma_node1, DMA_NodeConfTypeDef &dma_node_config)
{
	print("Node 1 contents:\n");
	print("  LinkRegisters[0] (CTR1) = ", Hex{dma_node1.LinkRegisters[0]}, "\n");
	print("  LinkRegisters[1] (CTR2) = ", Hex{dma_node1.LinkRegisters[1]}, "\n");
	print("  LinkRegisters[2] (CBR1) = ", Hex{dma_node1.LinkRegisters[2]}, "\n");
	print("  LinkRegisters[3] (CSAR) = ", Hex{dma_node1.LinkRegisters[3]}, "\n");
	print("  LinkRegisters[4] (CDAR) = ", Hex{dma_node1.LinkRegisters[4]}, "\n");
	print("  LinkRegisters[5] (CLLR) = ", Hex{dma_node1.LinkRegisters[5]}, "\n");

	print("  dma_node1 addr = ", Hex{(uint32_t)(uintptr_t)&dma_node1}, "\n");
	print("  DMA src: ", Hex{dma_node_config.SrcAddress}, "\n");
	print("  DMA dst: ", Hex{dma_node_config.DstAddress}, "\n");

	// These get set after DMA has been started
	print("HPDMA1_Channel0 registers:\n");
	print("  CSR  = ", Hex{HPDMA1_Channel0->CSR}, "\n");
	print("  CSAR = ", Hex{HPDMA1_Channel0->CSAR}, "\n");
	print("  CDAR = ", Hex{HPDMA1_Channel0->CDAR}, "\n");
	print("  CTR1 = ", Hex{HPDMA1_Channel0->CTR1}, "\n");
	print("  CTR2 = ", Hex{HPDMA1_Channel0->CTR2}, "\n");
	print("  CBR1 = ", Hex{HPDMA1_Channel0->CBR1}, "\n");
	print("  CLBAR = ", Hex{HPDMA1_Channel0->CLBAR}, "\n");
	print("  CLLR = ", Hex{HPDMA1_Channel0->CLLR}, "\n");
}

void dump_sai_registers()
{
	print("CR1 = ", Hex{SAI2_Block_A->CR1}, "\n");
	print("CR2 = ", Hex{SAI2_Block_A->CR2}, "\n");
	print("FRCR = ", Hex{SAI2_Block_A->FRCR}, "\n");
	print("SLOTR = ", Hex{SAI2_Block_A->SLOTR}, "\n");
	print("IMR = ", Hex{SAI2_Block_A->IMR}, "\n");
	print("SR = ", Hex{SAI2_Block_A->SR}, "\n");
}
extern "C" void assert_failed(uint8_t *file, uint32_t line)
{
	print("assert failed: ", file, ":", line, "\n");
}
