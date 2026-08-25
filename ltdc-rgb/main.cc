#include "aarch64/system_reg.hh" // clean_dcache_range
#include "display.hh"
#include "drivers/hal_cnt.hh" // SystemA35_SYSTICK_Config, udelay
#include "drivers/pin.hh"
#include "drivers/rcc_pll.hh"
#include "ltdc.hh"
#include "nv3052c.hh"
#include "panel_nv3052c.hh"
#include "print/print.hh"
#include "stm32mp2xx.h"
#include <cstdint>
#include <span>

void pad_selftest();

namespace
{

constexpr uint32_t FbAddr = 0x90000000;
static_assert(Panel::BytesPerPixel == 2, "this example draws RGB565");

constexpr uint16_t rgb565(uint32_t r, uint32_t g, uint32_t b)
{
	return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

// Test patterns:
//  1: RGB gradient field (red ->, blue v), x^y hash on green, 1px white border
//  2: Solid bars R G B W, black underneath, white border
//  3: Each color gets a row, then a row for black/white/grey/dark-red
void fill_test_pattern(std::span<uint16_t> fb, int pattern)
{
	using namespace Panel;
	for (uint32_t y = 0; y < VActive; y++)
		for (uint32_t x = 0; x < HActive; x++) {
			uint16_t px = 0;
			bool border = (x == 0 || y == 0 || x == HActive - 1 || y == VActive - 1);

			if (pattern == 1) {
				uint32_t r = (x * 255) / (HActive - 1);
				uint32_t b = (y * 255) / (VActive - 1);
				px = rgb565(r, (x ^ y) & 0xFF, b);
				if (border)
					px = 0xFFFF;

			} else if (pattern == 2) {
				if (y >= VActive - 2 || y <= 1 || x <= 1 || x >= HActive - 2)
					px = 0xFFFF;
				else if (y > VActive * 3 / 4)
					px = 0x0000;
				else if (x < HActive / 4)
					px = rgb565(255, 0, 0);
				else if (x < HActive / 2)
					px = rgb565(0, 255, 0);
				else if (x < HActive * 3 / 4)
					px = rgb565(0, 0, 255);
				else
					px = 0xFFFF;

			} else {
				if (border)
					px = 0xFFFF;
				else {
					uint32_t row = y / (VActive / 4), col = x / (HActive / 8);
					if (row < 3) {
						uint32_t v = 0;
						if (col < 6)
							v = (col < 5 || row == 1) ? (0x80 >> col) : 0; // single bit
						else if (col == 7)
							v = 0xFF;
						px = row == 0 ? rgb565(v, 0, 0) : row == 1 ? rgb565(0, v, 0) : rgb565(0, 0, v);
					} else {
						px = col == 0 ? 0xFFFF : col == 1 ? 0x7BEF : col == 2 ? 0 : rgb565((col - 2) << 3, 0, 0);
					}
					if (x % (HActive / 8) == 0 || y % (VActive / 4) == 0)
						px = 0; // thin black gridlines between cells
				}
			}
			fb[y * HActive + x] = px;
		}
	clean_dcache_range(reinterpret_cast<void *>(FbAddr), FbBytes);
}

#if UART == 1
constexpr uint64_t ConsoleUsart = USART1_BASE;
#elif UART == 6
constexpr uint64_t ConsoleUsart = USART6_BASE;
#else
constexpr uint64_t ConsoleUsart = USART2_BASE;
#endif

int console_getc()
{
	auto *isr = reinterpret_cast<volatile uint32_t *>(ConsoleUsart + 0x1C);
	auto *icr = reinterpret_cast<volatile uint32_t *>(ConsoleUsart + 0x20);
	auto *rdr = reinterpret_cast<volatile uint32_t *>(ConsoleUsart + 0x24);
	if (*isr & (1u << 3))
		*icr = 1u << 3; // clear overrun so RX keeps going
	if (*isr & (1u << 5))
		return *rdr & 0xFF;
	return -1;
}

void set_colmod(uint8_t code)
{
	// page 0 is selected at the end of the init table
	nv3052c_write_reg(0x3A, code);
	nv3052c_write_reg(0x0C, code);
	print("COLMOD <- 0x", Hex{code}, "  readback RDDCOLMOD=0x", Hex{nv3052c_read_reg(0x0C)}, "\n");
}

void help()
{
	print("\nkeys:  1/2/3 = pattern (gradient / bars / bit ladder)\n");
	print("       6/8/4 = panel bus 16-bit (0x55) / 18-bit (0x66) / 24-bit (0x77)\n");
	print("       p = pad self-test (stuck / shorted header lines)\n");
	print("       s = status readback    ? = this help\n\n");
}

void status()
{
	print("RDDPM=0x", Hex{nv3052c_read_reg(0x0A)});
	print(" RDDMADCTL=0x", Hex{nv3052c_read_reg(0x0B)});
	print(" RDDCOLMOD=0x", Hex{nv3052c_read_reg(0x0C)});
	print(" RDDIM=0x", Hex{nv3052c_read_reg(0x0D)});
	print(" RDID1=0x", Hex{nv3052c_read_reg(0xDA)});
	print("  LTDC line=", ltdc_current_line(), " ISR=0x", Hex{ltdc_isr()}, "\n");
}

} // namespace

int main()
{
	print("\nLTDC parallel-RGB Example\n");

	SystemA35_SYSTICK_Config(0);

	auto fb = std::span<uint16_t>{reinterpret_cast<uint16_t *>(FbAddr), Panel::HActive * Panel::VActive};
	int pattern = 3;
	fill_test_pattern(fb, pattern);
	print("Filled buffer with RGB565 test pattern #", pattern, " (", Panel::HActive, "x", Panel::VActive, ")\n");

	display_pins_setup();
	print("LTDC pins init\n");

	display_rif_setup();
	print("RIF setup: LTDC secure, LTDC DMA master (RIMU 11) MSEC\n");

	uint32_t pixel_clk = display_clocks_setup();
	auto s4 = RCC_Clocks::get_pll_settings<RCC_Clocks::PLL4>();
	print("Init LTDC pixel clock = ");
	print(pixel_clk / 1'000'000, ".", (pixel_clk / 100'000) % 10, " MHz");
	print(" (PLL4 src=", s4.src == RCC_Clocks::MuxSelSource::hse ? "HSE" : "HSI");
	print(" x", s4.mult, "/", s4.refdiv, "/", s4.postdiv1, "/", s4.postdiv2, ")\n");

	print("   with porches: ", Panel::HTotal, "x", Panel::VTotal, " -> ");
	print(pixel_clk / (Panel::HTotal * Panel::VTotal), " Hz refresh\n");

	uint32_t writes = nv3052c_init();
	print("NV3052C init (", writes, " register writes)\n");
	print("   readback: RDDPM(0Ah)=0x", Hex{nv3052c_read_reg(0x0A)}, " (expect 0x9C)");
	print("  RDDMADCTL(0Bh)=0x", Hex{nv3052c_read_reg(0x0B)});
	print("  RDDCOLMOD(0Ch)=0x", Hex{nv3052c_read_reg(0x0C)});
	print("  RDDIM(0Dh)=0x", Hex{nv3052c_read_reg(0x0D)});
	print("  RDID1(DAh)=0x", Hex{nv3052c_read_reg(0xDA)}, "\n");

	ltdc_init(FbAddr);
	print("LTDC peripheral configured\n");

	uint32_t l0 = ltdc_current_line();
	udelay(5000);
	uint32_t l1 = ltdc_current_line();
	print("\nLTDC line counter: ", l0, " -> ", l1, l0 != l1 ? "  (advancing)\n" : "  (FROZEN!)\n");
	print("LTDC ISR flags: 0x", Hex{ltdc_isr()}, "  (bit1: FIFO-warn, bit2: xfer-err, bit6: FIFO-err)\n");
	print("vblank IRQ: ", ltdc_wait_vblank() ? "ok" : "TIMEOUT", "\n");

	if (l0 == l1)
		print("\nFAILED: pixel clock dead\n");
	else
		print("\nLook at the display!\n");
	help();

	while (true) {
		int c = console_getc();
		if (c < 0) {
			udelay(1000);
			continue;
		}
		switch (c) {
			case '1':
			case '2':
			case '3':
				pattern = c - '0';
				fill_test_pattern(fb, pattern);
				print("pattern ", pattern, "\n");
				break;
			case '6':
				set_colmod(0x55);
				break;
			case '8':
				set_colmod(0x66);
				break;
			case '4':
				set_colmod(0x77);
				break;
			case 's':
				status();
				break;
			case 'p':
				pad_selftest();
				break;
			case '?':
				help();
				break;
			default:
				print("key '", char(c), "'?\n");
				break;
		}
	}
}

extern "C" void assert_failed(uint8_t *file, uint32_t line)
{
	print("assert failed: ", file, ":", int(line), "\n");
}
