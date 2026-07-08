// Cortex-A35 firmware that embeds, loads, and starts the
// Cortex-M33 coprocessor, then runs in parallel with it.
//
// Both cores share the console UART and print occasionally; the A35 toggles
// PH8 and the M33 toggles PF9.

#include "aarch64/system_reg.hh"
#include "print/print.hh"
#include "stm32mp2xx.h"
#include <cstdint>

#include "firmware_m33.h" // provides m33_firmware[] and m33_firmware_len

namespace
{
// SRAM2 non-secure instruction-fetch alias. Must match linkscript_m33.ld and
// startup's vector-table location.
constexpr uintptr_t M33_LOAD_ADDR = 0x0A060000UL;

// PH8, toggled by the A35:
constexpr uintptr_t RCC_GPIOHCFGR = 0x44200548UL;
constexpr uint32_t RCC_GPIOxEN = (1u << 1);
constexpr uintptr_t GPIOH_ADDR = 0x442B0000UL;
constexpr unsigned LED_PIN = 8; // PH8

inline volatile uint32_t &reg(uintptr_t addr)
{
	return *reinterpret_cast<volatile uint32_t *>(addr);
}

void ph8_init()
{
	reg(RCC_GPIOHCFGR) |= RCC_GPIOxEN;
	unsigned s2 = LED_PIN * 2u;
	reg(GPIOH_ADDR + 0x00) = (reg(GPIOH_ADDR + 0x00) & ~(3u << s2)) | (1u << s2); // output
	reg(GPIOH_ADDR + 0x04) &= ~(1u << LED_PIN);									  // push-pull
	reg(GPIOH_ADDR + 0x08) = (reg(GPIOH_ADDR + 0x08) & ~(3u << s2)) | (2u << s2);
	reg(GPIOH_ADDR + 0x0C) &= ~(3u << s2); // no pull
}

void ph8_set(bool on)
{
	reg(GPIOH_ADDR + 0x18) = on ? (1u << LED_PIN) : (1u << (LED_PIN + 16));
}

void delay(unsigned n)
{
	for (volatile unsigned i = 0; i < n; i++)
		asm volatile("nop");
}

// Every GPIO pin resets to secure on the MP2 (GPIOx_SECCFGR reset value is
// 0xFFFF, see RM0457 sec. 24.4.12). The M33 runs non-secure, so its writes to
// a secure pin's MODER/BSRR/... bits are silently ignored and reads return 0.
// The A35 (secure) must hand over each pin the M33 will drive by clearing that
// pin's SEC bit.
void gpio_pin_set_nonsecure(uintptr_t rcc_gpiocfgr, uintptr_t gpio_base, unsigned pin)
{
	reg(rcc_gpiocfgr) |= RCC_GPIOxEN;
	reg(gpio_base + 0x30) &= ~(1u << pin); // GPIOx_SECCFGR
}

void load_m33_firmware()
{
	// Copy the embedded blob into SRAM2
	auto *dst = reinterpret_cast<volatile uint8_t *>(M33_LOAD_ADDR);
	for (unsigned i = 0; i < m33_firmware_len; i++)
		dst[i] = m33_firmware[i];

	// Clean the cache so a write to memory is ensured
	for (uintptr_t a = M33_LOAD_ADDR; a < M33_LOAD_ADDR + m33_firmware_len; a += 64)
		clean_dcache_address(a);

	dsb_sy();
	isb();
}

void start_m33()
{
	// Park CPU2 first for a known-clean state: assert hold-boot (BOOT_CPU2=0)
	// and assert the M33 reset. Mirrors OP-TEE rproc_stop().
	RCC->CPUBOOTCR &= ~RCC_CPUBOOTCR_BOOT_CPU2;
	RCC->C2RSTCSETR = RCC_C2RSTCSETR_C2RST;

	load_m33_firmware();

	// Make the LED pins non-secure so the non-secure M33 can drive them:
	// PH8 (shared debug LED) and PF9 (the demo's M33 pin).
	gpio_pin_set_nonsecure(RCC_GPIOHCFGR, GPIOH_ADDR, LED_PIN); // PH8
	gpio_pin_set_nonsecure(0x44200540UL, 0x44290000UL, 9);		// GPIOF: PF9

	// Run the M33 non-secure: make sure TrustZone is not enabled and point the
	// non-secure vector table at the loaded image.
	CA35SYSCFG->M33_TZEN_CR &= ~CA35SYSCFG_M33_TZEN_CR_CFG_SECEXT;
	CA35SYSCFG->M33_INITNSVTOR_CR = (uint32_t)M33_LOAD_ADDR & CA35SYSCFG_M33_INITNSVTOR_CR_INITNSVTOR_Msk;
	dsb_sy();
	isb();

	// Release hold-boot -> the M33 boots from INITNSVTOR. The hardware
	// automatically releases the M33 reset (see OP-TEE stm32_rproc_start()).
	RCC->CPUBOOTCR |= RCC_CPUBOOTCR_BOOT_CPU2;
}

void report_m33_boot_state()
{
	print("A35: --- M33 boot diagnostics ---\n");
	print("A35: M33_ACCESS_CR     = ", Hex{CA35SYSCFG->M33_ACCESS_CR}, " (reset: 0x3 = secure+priv only)\n");
	print("A35: M33_TZEN_CR       = ", Hex{CA35SYSCFG->M33_TZEN_CR}, " (want 0; 1 = write was blocked)\n");
	print("A35: M33_INITNSVTOR_CR = ",
		  Hex{CA35SYSCFG->M33_INITNSVTOR_CR},
		  " (want ",
		  Hex{(unsigned)M33_LOAD_ADDR},
		  ")\n");
	print("A35: RCC CPUBOOTCR     = ", Hex{RCC->CPUBOOTCR}, " (want bit0 BOOT_CPU2 = 1)\n");
	print("A35: RCC C2RSTCSETR    = ", Hex{RCC->C2RSTCSETR}, " (want 0 = CPU2 reset released)\n");

	// PWR->CPU2D2SR: bit0 HOLD_BOOT (1 = still held), bits[3:2] CSTATE
	// (0 = in reset, 1 = CRun), bits[10:8] DSTATE (0 = Run)
	print("A35: PWR CPU2D2SR      = ", Hex{PWR->CPU2D2SR}, " (bit0 HOLD_BOOT, CSTATE[3:2]=01 CRun)\n");

	// Breadcrumbs: M33 startup writes 0xB007C0DE into vector slot 8 as its
	// first instructions, and 0xB007C0DF into slot 9 right before main().
	// Poll for a while, invalidating our dcache so we see the M33's writes.
	volatile uint32_t *crumb0 = reinterpret_cast<volatile uint32_t *>(M33_LOAD_ADDR + 0x20);
	volatile uint32_t *crumb1 = reinterpret_cast<volatile uint32_t *>(M33_LOAD_ADDR + 0x24);
	for (unsigned i = 0; i < 100; i++) {
		invalidate_dcache_address(M33_LOAD_ADDR + 0x20);
		dsb_sy();
		if (*crumb0 == 0xB007C0DE && *crumb1 == 0xB007C0DF)
			break;
		delay(100'000);
	}
	invalidate_dcache_address(M33_LOAD_ADDR + 0x20);
	dsb_sy();
	print("A35: M33 breadcrumb[0] = ", Hex{*crumb0}, " (0xB007C0DE = M33 reset handler ran)\n");
	print("A35: M33 breadcrumb[1] = ", Hex{*crumb1}, " (0xB007C0DF = M33 reached main)\n");
	print("A35: PWR CPU2D2SR      = ", Hex{PWR->CPU2D2SR}, " (after wait)\n");
	print("A35: -------------------------\n");
}
} // namespace

int main()
{
	print("\nA35: Hello from Cortex-A35!\n");

	ph8_init();

	print("A35: Loading M33 firmware (", (int)m33_firmware_len, " bytes) into SRAM2\n");

	print("A35: Starting M33 core after LED goes off in 3 ");
	ph8_set(true);
	delay(8'000'000);
	print("2 ");
	delay(8'000'000);
	print("1 ");
	ph8_set(false);
	delay(8'000'000);
	print("now\n");

	start_m33();
	print("A35: M33 released from hold-boot\n");

	report_m33_boot_state();

	unsigned count = 0;
	while (true) {
		ph8_set(true);
		delay(3'000'000);
		ph8_set(false);
		delay(3'000'000);

		if ((++count % 4) == 0)
			print("A35: tick ", (int)count, "\n");
	}
}
