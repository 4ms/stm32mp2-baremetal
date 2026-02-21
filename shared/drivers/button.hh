#include "stm32mp2xx.h"

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
