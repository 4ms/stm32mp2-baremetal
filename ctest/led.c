#include <stdint.h>

#define STM32_GPIOH_BASE 0x400B0000UL
#define STM32_GPIOH_MODER 0x400B0000UL
#define STM32_GPIOH_BSRR 0x400B0018UL

// Green LD3 is PD8
#define STM32_GPIOD_BASE 0x44270000

// Blue LED1 is PJ7
#define STM32_GPIOJ_BASE 0x442D0000

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

#define GPIOD ((stm32_gpio_t *)STM32_GPIOD_BASE)
#define GPIOJ ((stm32_gpio_t *)STM32_GPIOJ_BASE)

void led1_init(void)
{
	GPIOJ->MODER = (GPIOJ->MODER & ~(0b11 << (7 * 2))) | (0b01 << (7 * 2));
}

void led1_off(void)
{
	GPIOJ->BSRR = 1 << (7 + 16);
}

void led1_on(void)
{
	GPIOJ->BSRR = 1 << (7);
}

void led3_init(void)
{
	GPIOD->MODER = (GPIOD->MODER & ~(0b11 << (8 * 2))) | (0b01 << (8 * 2));
}

void led3_off(void)
{
	GPIOD->BSRR = 1 << (8 + 16);
}

void led3_on(void)
{
	GPIOD->BSRR = 1 << (8);
}
