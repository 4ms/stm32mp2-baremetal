#include "drivers/pwr_vdd.hh"
#include "stm32mp2xx.h"

inline void button_user1_init()
{
	PowerControl::enable_vddio3(PowerControl::Present::Always);
	// Enable RCC for GPIOD
	RCC->GPIODCFGR |= RCC_GPIODCFGR_GPIOxEN;
	// PD2 mode = input (0b00)
	GPIOD->MODER = (GPIOD->MODER & ~(0b11 << (2 * 2))) | (0b00 << (2 * 2));
}

inline bool button_user1_pressed()
{
	return (GPIOD->IDR & (1 << 2));
}

inline void button_user2_init()
{
	// Enable RCC for GPIOG
	RCC->GPIOGCFGR |= RCC_GPIOGCFGR_GPIOxEN;
	// PG8 mode = input (0b00)
	GPIOG->MODER = (GPIOG->MODER & ~(0b11 << (8 * 2))) | (0b00 << (8 * 2));
}

inline bool button_user2_pressed()
{
	return (GPIOG->IDR & (1 << 8));
}
