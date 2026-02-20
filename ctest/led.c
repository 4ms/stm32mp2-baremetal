#include <stdint.h>

typedef struct {
	volatile uint32_t MODER;   /*!< GPIO mode register  Address offset: 0x000 */
	volatile uint32_t OTYPER;  /*!< GPIO output type register Address offset: 0x004 */
	volatile uint32_t OSPEEDR; /*!< GPIO output speed register Address offset: 0x008 */
	volatile uint32_t PUPDR;   /*!< GPIO port pull-up/pull-down register   Address offset: 0x00C */
	volatile uint32_t IDR;	   /*!< GPIO input data register Address offset: 0x010 */
	volatile uint32_t ODR;	   /*!< GPIO output data register Address offset: 0x014 */
	volatile uint32_t BSRR;	   /*!< GPIO bit set/reset register Address offset: 0x018 */
	volatile uint32_t LCKR;	   /*!< GPIO configuration lock register Address offset: 0x01C */
	volatile uint32_t AFR[2];  /*!< GPIO alternate function  registers Address offset: 0x020 */
	volatile uint32_t BRR;	   /*!< GPIO bit reset register Address offset: 0x028 */
} stm32_gpio_t;

#define STM32_GPIOJ_BASE 0x442D0000
#define GPIOJ ((stm32_gpio_t *)STM32_GPIOJ_BASE)

#define RCC_GPIOJCFGR_BASE 0x44200550

void led1_init(void)
{
	*(uintptr_t *)(RCC_GPIOJCFGR_BASE) = 0b110; // enable, lp enable, no reset
	// Pin 7 MODE = 0b01: Output
	GPIOJ->MODER = (GPIOJ->MODER & ~(0b11 << (7 * 2))) | (0b01 << (7 * 2));
}

void led1_off(void)
{
	// Pin 6 Reset
	GPIOJ->BSRR = 1 << (7 + 16);
}

void led1_on(void)
{
	// Pin 6 Set
	GPIOJ->BSRR = 1 << (7);
}

void led3_init(void)
{
	*(uintptr_t *)(RCC_GPIOJCFGR_BASE) = 0b110; // enable, lp enable, no reset
	// Pin 6 MODE = 0b01: Output
	GPIOJ->MODER = (GPIOJ->MODER & ~(0b11 << (6 * 2))) | (0b01 << (6 * 2));
}

void led3_off(void)
{
	// Pin 6 Reset
	GPIOJ->BSRR = 1 << (6 + 16);
}

void led3_on(void)
{
	// Pin 6 Set
	GPIOJ->BSRR = 1 << (6);
}
