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

// This example reads analog voltages on ADC1 channel 4, which is PG4
// (pin B34 on the EV1 adaptor board). Connect PG4 to a voltage divider
// between VREF+ and VREF- to see readings.
//
// VREF+ is driven by the internal VREFBUF, so the VREF+ pin must not be
// driven externally (see README).
//
// The internal reference VREFINT (~1.21V bandgap) is also converted, as a
// sanity check that works with no external wiring at all.

// VREFBUF scale 0. Measure the VREF+ pin and adjust if needed.
constexpr inline uint32_t VrefMillivolts = 1500;

constexpr inline uint32_t NumSamples = 256;

// DMA writes samples here. The CPU must invalidate its dcache before reading them.
alignas(64) __attribute__((section(".s_retram"))) std::array<uint16_t, NumSamples> samples;

static ADC_HandleTypeDef hadc;
static DMA_HandleTypeDef hdma;
static DMA_QListTypeDef dma_queue;
static DMA_NodeTypeDef dma_node;
static DMA_NodeConfTypeDef dma_node_conf;

static volatile uint32_t buffers_done = 0;

static void enable_vrefbuf();
static void adc_init();
static void adc_config_channel(uint32_t channel, uint32_t rank, uint32_t sampling_time);
static void polled_readings();
static void dma_init();
static void print_sample_stats(unsigned num_chans);

static constexpr uint32_t raw_to_mv(uint32_t raw)
{
	return raw * VrefMillivolts / 4095;
}

int main()
{
	print("ADC Example\n");

	HAL_Init();

	print("\n1) Enabling VREF+\n");
	enable_vrefbuf();

	print("\n2) Single software-triggered conversions\n");
	adc_init();
	polled_readings();

	print("\n3) Continuous conversions of CH4 with circular DMA\n");
	dma_init();

	// Reconfigure for continuous conversions, results going to DMA
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

void enable_vrefbuf()
{
	// Remove the power isolation of the VDDA18ADC-supplied analog domain.
	// Present::If uses the PWR voltage monitor to confirm the supply is present.
	PowerControl::enable_vdda18adc(PowerControl::Present::If);
	print("VDDA18ADC supply is valid\n");

	RCC_Enable::VREF_::set();

	// VRS = 0 is the highest scale. Verify the voltage on the VREF+ pin with
	// a scope (expect 1.5V or 1.1V) and update VrefMillivolts to match.
	uint32_t csr = VREFBUF->CSR;
	csr &= ~(VREFBUF_CSR_HIZ | VREFBUF_CSR_VRS_0 | VREFBUF_CSR_VRS_1 | VREFBUF_CSR_VRS_2);
	csr |= VREFBUF_CSR_ENVR;
	VREFBUF->CSR = csr;

	auto timeout = HAL_GetTick() + 100;
	while (!(VREFBUF->CSR & VREFBUF_CSR_VRR)) {
		if (HAL_GetTick() > timeout) {
			print("ERROR: VREFBUF not ready after 100ms (CSR = 0x", Hex{VREFBUF->CSR}, ")\n");
			return;
		}
	}

	print("VREFBUF enabled and ready. Verify VREF+ pin reads ", VrefMillivolts, " mV\n");
}

void adc_init()
{
	RCC_Enable::ADC12_::set();

	// PG4 => ADC1_INP4
	Pin adc_pin{GPIO::G, 4, PinMode::Analog};

	hadc.Instance = ADC1;
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

	print("Calibration factor: ", HAL_ADCEx_Calibration_GetValue(&hadc, ADC_SINGLE_ENDED), "\n");
}

void adc_config_channel(uint32_t channel, uint32_t rank, uint32_t sampling_time)
{
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

void polled_readings()
{
	print("Reading VREFINT (internal bandgap, expect ~1210 mV):\n");
	adc_config_channel(ADC_CHANNEL_VREFINT, ADC_REGULAR_RANK_1, ADC_SAMPLETIME_1501CYCLES_5);
	for (unsigned i = 0; i < 4; i++) {
		HAL_ADC_Start(&hadc);
		if (HAL_ADC_PollForConversion(&hadc, 100) != HAL_OK) {
			print("ERROR: poll for conversion timed out\n");
			break;
		}
		auto raw = HAL_ADC_GetValue(&hadc);
		print("  VREFINT raw: ", raw, " = ", raw_to_mv(raw), " mV\n");
	}

	print("Reading CH4 (PG4, adaptor board pin B34):\n");
	adc_config_channel(ADC_CHANNEL_4, ADC_REGULAR_RANK_1, ADC_SAMPLETIME_247CYCLES_5);
	for (unsigned i = 0; i < 8; i++) {
		HAL_ADC_Start(&hadc);
		if (HAL_ADC_PollForConversion(&hadc, 100) != HAL_OK) {
			print("ERROR: poll for conversion timed out\n");
			break;
		}
		auto raw = HAL_ADC_GetValue(&hadc);
		print("  CH4 raw: ", raw, " = ", raw_to_mv(raw), " mV\n");
		HAL_Delay(250);
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
