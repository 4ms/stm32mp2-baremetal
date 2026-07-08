// Cortex-M33 coprocessor firmware for the STM32MP257F.
//
// Runs in parallel with the A35 after the A35 loads this image into SRAM2 and
// releases CPU2 from hold-boot. Prints occasionally over the shared console
// UART and toggles PF9.
//
// The M33 runs *secure* (the A35 sets CFG_SECEXT before releasing it), with
// the SAU left disabled -- so the whole address map is Secure and every bus
// access the M33 makes is a secure transaction. That means:
//  - it can use the same peripheral addresses the A35 uses (0x4xxxxxxx),
//    no need for the 0x5xxxxxxx secure aliases;
//  - it can drive GPIO pins directly, since pins reset to secure
//    (GPIOx_SECCFGR = 0xFFFF) and secure accesses are always allowed.

#include "print/print.hh"
#include <cstdint>

extern "C" void init_uart(void);

namespace
{
// RCC and GPIOF (same addresses the A35 uses; see comment at top of file).
constexpr uintptr_t RCC_GPIOFCFGR = 0x44200540UL; // RCC->GPIOFCFGR
constexpr uint32_t RCC_GPIOxEN = (1u << 1);

constexpr uintptr_t GPIOF_BASE = 0x44290000UL;
constexpr uintptr_t GPIOF_MODER = GPIOF_BASE + 0x00;
constexpr uintptr_t GPIOF_OTYPER = GPIOF_BASE + 0x04;
constexpr uintptr_t GPIOF_OSPEEDR = GPIOF_BASE + 0x08;
constexpr uintptr_t GPIOF_PUPDR = GPIOF_BASE + 0x0C;
constexpr uintptr_t GPIOF_BSRR = GPIOF_BASE + 0x18;

constexpr unsigned LED_PIN = 9; // PF9

inline volatile uint32_t &reg(uintptr_t addr)
{
	return *reinterpret_cast<volatile uint32_t *>(addr);
}

void pf9_init()
{
	reg(RCC_GPIOFCFGR) |= RCC_GPIOxEN; // enable GPIOF clock

	unsigned s2 = LED_PIN * 2u;
	reg(GPIOF_MODER) = (reg(GPIOF_MODER) & ~(3u << s2)) | (1u << s2); // output
	reg(GPIOF_OTYPER) &= ~(1u << LED_PIN);							 // push-pull
	reg(GPIOF_OSPEEDR) = (reg(GPIOF_OSPEEDR) & ~(3u << s2)) | (2u << s2);
	reg(GPIOF_PUPDR) &= ~(3u << s2); // no pull
}

void pf9_set(bool on)
{
	reg(GPIOF_BSRR) = on ? (1u << LED_PIN) : (1u << (LED_PIN + 16));
}

void delay(unsigned n)
{
	for (volatile unsigned i = 0; i < n; i++)
		asm volatile("nop");
}
} // namespace

int main()
{
	// The A35 has already brought up the console UART, but calling init_uart()
	// again is safe and makes the M33 independent of A35 timing.
	init_uart();

	print("\nM33: * yawn *\nM33: Hello from Cortex-M33!\n");
	print("M33: I will toggle PF9 and print occasionally.\n");

	pf9_init();

	unsigned count = 0;
	while (true) {
		pf9_set(true);
		delay(2'000'000);
		pf9_set(false);
		delay(2'000'000);

		if ((++count % 4) == 0)
			print("M33: tick ", (int)count, "\n");
	}
}
