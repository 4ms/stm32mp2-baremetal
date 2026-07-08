#include "aarch64/system_reg.hh"
#include "dma.hh"
#include "drivers/pin.hh"
#include "drivers/pwr_vdd.hh"
#include "drivers/rcc.hh"
#include "drivers/watchdog.hh"
#include "interrupt/interrupt.hh"
#include "print/print.hh"
#include "stm32mp2xx_hal.h"
#include <array>
#include <utility>

// This example reads analog voltages on ADC1 channel 4, which is PG4
// (pin B34 on the EV1 adaptor board). Connect PG4 to a voltage divider
// between VREF+ and VREF- to see readings.
//
// The reference voltage depends on the board (BOARD= in the Makefile):
// - devboard: VREF+ is unloaded, so the internal VREFBUF drives it and the
//   VREF+ pin must not be driven externally (see README).
// - ev1: VREF+ is wired to the 1.8V rail, so the VREFBUF is left in
//   external-reference mode (enabling it would fight the rail).
//
// The internal reference VREFINT (0.8V on MP2, per datasheet) is also
// converted, as a sanity check that works with no external wiring at all.

#if defined(BOARD_EV1)

// The EV1 wires VREF+ to its 1.8V analog rail.
constexpr inline uint32_t VrefMillivolts = 1800;

#else

// The MP25 VREFBUF supports two output voltages, selected by the VRS bit:
// VRS = 0: ~1212 mV (the raw bandgap voltage, measured on an EV1)
// VRS = 1: 1500 mV
enum class VrefScale : uint32_t { _1212mV = 0, _1500mV = VREFBUF_CSR_VRS };
constexpr inline VrefScale UseVrefScale = VrefScale::_1500mV;
constexpr inline uint32_t VrefMillivolts = (UseVrefScale == VrefScale::_1212mV) ? 1212 : 1500;

#endif

constexpr inline uint32_t NumSamples = 256;

// DMA writes samples here. The CPU must invalidate its dcache before reading them.
alignas(64) __attribute__((section(".s_retram"))) std::array<uint16_t, NumSamples> samples;

static ADC_HandleTypeDef hadc;
static DMA_HandleTypeDef hdma;
static DMA_QListTypeDef dma_queue;
static DMA_NodeTypeDef dma_node;
static DMA_NodeConfTypeDef dma_node_conf;

static volatile uint32_t buffers_done = 0;

static void init_vref();
static void adc_init(ADC_TypeDef *instance);
static void adc_config_channel(uint32_t channel, uint32_t rank, uint32_t sampling_time);
static void dma_init();
static void poll_channel(const char *label, uint32_t channel, uint32_t sampling_time, unsigned count);
static void print_sample_stats(unsigned num_chans);
static uint32_t poll_one_reading();

static constexpr uint32_t raw_to_mv(uint32_t raw)
{
	return raw * VrefMillivolts / 4095;
}

int main()
{
	print("ADC Example\n");

	HAL_Init();

	print("\n1) Configuring VREF+\n");
	init_vref();

	print("\n2) Single software-triggered conversions\n");
	adc_init(ADC1);
	print("Calibration factor: ", HAL_ADCEx_Calibration_GetValue(&hadc, ADC_SINGLE_ENDED), "\n");

	// Poll internal reference
	poll_channel("VREFINT: expect ~800 mV", ADC_CHANNEL_VREFINT, ADC_SAMPLETIME_1501CYCLES_5, 4);

	print("\n3) Continuous conversions of CH4 with circular DMA\n");
	dma_init();

	// Reconfigure for continuous conversions, results going to DMA.
	// HAL_ADC_Init() also requires the ADC to be disabled (see adc_config_channel)
	if (auto res = HAL_ADC_Stop(&hadc); res != HAL_OK)
		print("ERROR: HAL_ADC_Stop returned ", res, "\n");

	hadc.Init.ContinuousConvMode = ENABLE;
	hadc.Init.ConversionDataManagement = ADC_CONVERSIONDATA_DMA_CIRCULAR;
	if (auto res = HAL_ADC_Init(&hadc); res != HAL_OK)
		print("ERROR: HAL_ADC_Init returned ", res, "\n");

	adc_config_channel(ADC_CHANNEL_4, ADC_REGULAR_RANK_1, ADC_SAMPLETIME_247CYCLES_5);

	// Note: buffer address and length must match what was given to DmaHelper
	// via dma_node_conf (see comment in dma.hh)
	if (auto res = HAL_ADC_Start_DMA(&hadc, reinterpret_cast<const uint32_t *>(samples.data()), NumSamples);
		res != HAL_OK)
		print("ERROR: HAL_ADC_Start_DMA returned ", res, "\n");

	for (unsigned i = 0; i < 5; i++) {
		HAL_Delay(1000);
		print_sample_stats(1);
		watchdog_pet();
	}

	print("\n4) Scanning multiple channels (CH4 + VREFINT) with circular DMA\n");

	if (auto res = HAL_ADC_Stop_DMA(&hadc); res != HAL_OK)
		print("ERROR: HAL_ADC_Stop_DMA returned ", res, "\n");

	hadc.Init.ScanConvMode = ADC_SCAN_ENABLE;
	hadc.Init.NbrOfConversion = 2;
	hadc.Init.EOCSelection = ADC_EOC_SEQ_CONV;
	if (auto res = HAL_ADC_Init(&hadc); res != HAL_OK)
		print("ERROR: HAL_ADC_Init returned ", res, "\n");

	adc_config_channel(ADC_CHANNEL_4, ADC_REGULAR_RANK_1, ADC_SAMPLETIME_247CYCLES_5);
	adc_config_channel(ADC_CHANNEL_VREFINT, ADC_REGULAR_RANK_2, ADC_SAMPLETIME_1501CYCLES_5);

	// Samples are interleaved in the buffer: CH4, VREFINT, CH4, VREFINT, ...
	if (auto res = HAL_ADC_Start_DMA(&hadc, reinterpret_cast<const uint32_t *>(samples.data()), NumSamples);
		res != HAL_OK)
		print("ERROR: HAL_ADC_Start_DMA returned ", res, "\n");

	while (true) {
		HAL_Delay(1000);
		print_sample_stats(2);
		watchdog_pet();
	}
}

void init_vref()
{
	// Remove the power isolation of the VDDA18ADC-supplied analog domain.
	// Present::If uses the PWR voltage monitor to confirm the supply is present.
	PowerControl::enable_vdda18adc(PowerControl::Present::If);
	print("VDDA18ADC supply is valid\n");

	RCC_Enable::VREF_::set();

#if defined(BOARD_EV1)
	// External voltage reference mode (ENVR=0, HIZ=1, the reset state): the
	// VREFBUF buffer stays off and VREF+ is an input, supplied by the EV1's
	// 1.8V rail.
	VREFBUF->CSR = (VREFBUF->CSR & ~VREFBUF_CSR_ENVR) | VREFBUF_CSR_HIZ;
	print("Using external reference on VREF+ pin (", VrefMillivolts, " mV rail on the EV1)\n");
#else
	uint32_t csr = VREFBUF->CSR;
	csr &= ~(VREFBUF_CSR_HIZ | VREFBUF_CSR_VRS_0 | VREFBUF_CSR_VRS_1 | VREFBUF_CSR_VRS_2);
	csr |= std::to_underlying(UseVrefScale) | VREFBUF_CSR_ENVR;
	VREFBUF->CSR = csr;

	auto timeout = HAL_GetTick() + 100;
	while (!(VREFBUF->CSR & VREFBUF_CSR_VRR)) {
		if (HAL_GetTick() > timeout) {
			print("ERROR: VREFBUF not ready after 100ms (CSR = 0x", Hex{VREFBUF->CSR}, ")\n");
			return;
		}
	}

	print("VREFBUF enabled and ready. Please use a meter to verify VREF+ pin reads ", VrefMillivolts, " mV\n");
#endif
}

void adc_init(ADC_TypeDef *instance)
{
	if (instance == ADC3)
		RCC_Enable::ADC3_::set();
	else
		RCC_Enable::ADC12_::set();

	Pin adc_pin{GPIO::G, 4, PinMode::Analog};
	// Pin must have secure state disabled or else ADC (which is non-secure) cannot reach the pad (which is by default
	// secure)
	LL_GPIO_DisablePinSecure(GPIOG, (1 << 4));

	hadc.Instance = instance;
	// Clock the ADC from the ck_icn_ls_mcu bus clock, so no kernel clock (flexgen) setup is needed
	hadc.Init.ClockPrescaler = ADC_CLOCK_CK_ICN_LS_MCU_DIV4;
	hadc.Init.Resolution = ADC_RESOLUTION_12B;
	hadc.Init.GainCompensation = 0;
	hadc.Init.ScanConvMode = ADC_SCAN_DISABLE;
	hadc.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
	hadc.Init.LowPowerAutoWait = DISABLE;
	hadc.Init.ContinuousConvMode = DISABLE;
	hadc.Init.NbrOfConversion = 1;
	hadc.Init.DiscontinuousConvMode = DISABLE;
	hadc.Init.ExternalTrigConv = ADC_SOFTWARE_START;
	hadc.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
	hadc.Init.SamplingMode = ADC_SAMPLING_MODE_NORMAL;
	hadc.Init.ConversionDataManagement = ADC_CONVERSIONDATA_DR;
	hadc.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
	hadc.Init.LeftBitShift = ADC_LEFTBITSHIFT_NONE;
	hadc.Init.OversamplingMode = DISABLE;

	if (auto res = HAL_ADC_Init(&hadc); res != HAL_OK)
		print("ERROR: HAL_ADC_Init returned ", res, "\n");

	if (auto res = HAL_ADCEx_Calibration_Start(&hadc, ADC_SINGLE_ENDED); res != HAL_OK)
		print("ERROR: HAL_ADCEx_Calibration_Start returned ", res, "\n");
}

void adc_config_channel(uint32_t channel, uint32_t rank, uint32_t sampling_time)
{
	// Unlike other STM32 HALs, the MP2 HAL_ADC_ConfigChannel() errors unless the
	// ADC is completely disabled -- and calibration and HAL_ADC_Start() both
	// leave it enabled. HAL_ADC_Stop() stops conversions and disables the ADC.
	if (auto res = HAL_ADC_Stop(&hadc); res != HAL_OK)
		print("ERROR: HAL_ADC_Stop returned ", res, "\n");

	ADC_ChannelConfTypeDef conf{};
	conf.Channel = channel;
	conf.Rank = rank;
	conf.SamplingTime = sampling_time;
	conf.SingleDiff = ADC_SINGLE_ENDED;
	conf.OffsetNumber = ADC_OFFSET_NONE;
	conf.Offset = 0;
	conf.OffsetRightShift = DISABLE;
	conf.OffsetSignedSaturation = DISABLE;
	conf.OffsetSaturation = DISABLE;
	conf.OffsetSign = ADC_OFFSET_SIGN_NEGATIVE;

	if (auto res = HAL_ADC_ConfigChannel(&hadc, &conf); res != HAL_OK)
		print("ERROR: HAL_ADC_ConfigChannel returned ", res, "\n");
}

uint32_t poll_one_reading()
{
	HAL_ADC_Start(&hadc);
	if (HAL_ADC_PollForConversion(&hadc, 100) != HAL_OK) {
		print("ERROR: poll for conversion timed out\n");
		return 0;
	}
	return HAL_ADC_GetValue(&hadc);
}

void poll_channel(const char *label, uint32_t channel, uint32_t sampling_time, unsigned count)
{
	print(label, ":\n");
	adc_config_channel(channel, ADC_REGULAR_RANK_1, sampling_time);
	HAL_Delay(1); // startup time for internal channel buffers
	for (unsigned i = 0; i < count; i++) {
		auto raw = poll_one_reading();
		print("  raw: ", raw, " = ", raw_to_mv(raw), " mV\n");
		watchdog_pet();
	}
}

void dma_init()
{
	RCC_Enable::HPDMA1_::set();

	hdma.Instance = HPDMA1_Channel0;
	hdma.Init.Request = HPDMA_REQUEST_ADC1;
	hdma.Init.Direction = DMA_PERIPH_TO_MEMORY;
	dma_node_conf.SrcAddress = (uint32_t)(uintptr_t)&ADC1->DR;
	dma_node_conf.DstAddress = (uint32_t)(uintptr_t)samples.data();
	dma_node_conf.DataSize = sizeof(samples);
	DmaHelper::setup_circular(hdma, dma_queue, dma_node, dma_node_conf);

	__HAL_LINKDMA(&hadc, DMA_Handle, hdma);

	InterruptManager::register_and_start_isr(HPDMA1_Channel0_IRQn, 0, 0, [] { HAL_DMA_IRQHandler(&hdma); });

	// The ADC IRQ is used to report overrun errors when converting via DMA
	InterruptManager::register_and_start_isr(ADC1_IRQn, 0, 0, [] { HAL_ADC_IRQHandler(&hadc); });
}

// Print min/max/average of the sample buffer, de-interleaving num_chans channels.
// The DMA keeps running while we read, so the buffer contents are a mix of old
// and new samples -- fine for displaying stats, but a real application would
// use the half/full-complete callbacks to process settled halves of the buffer.
void print_sample_stats(unsigned num_chans)
{
	auto start = reinterpret_cast<uintptr_t>(samples.data());
	for (auto addr = start; addr < start + sizeof(samples); addr += 64)
		invalidate_dcache_address(addr);

	for (unsigned chan = 0; chan < num_chans; chan++) {
		uint32_t min = 0xFFFF;
		uint32_t max = 0;
		uint32_t sum = 0;
		uint32_t count = 0;
		for (auto i = chan; i < NumSamples; i += num_chans) {
			auto s = samples[i];
			min = std::min<uint32_t>(min, s);
			max = std::max<uint32_t>(max, s);
			sum += s;
			count++;
		}
		auto avg = sum / count;
		print("  rank ",
			  chan + 1,
			  ": min ",
			  raw_to_mv(min),
			  " mV, max ",
			  raw_to_mv(max),
			  " mV, avg ",
			  raw_to_mv(avg),
			  " mV (raw ",
			  avg,
			  ")  [",
			  buffers_done,
			  " buffers]\n");
	}
}

extern "C" void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *)
{
	// Called from the HPDMA ISR each time the sample buffer has been filled
	buffers_done = buffers_done + 1;
}

extern "C" void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *h)
{
	print("ADC error: 0x", Hex{h->ErrorCode}, "\n");
}

extern "C" void assert_failed(uint8_t *file, uint32_t line)
{
	print("assert failed: ", file, ":", line, "\n");
}
