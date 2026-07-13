#pragma once
#include <cstdint>

extern "C" uint32_t SystemA35_SYSTICK_Config(uint32_t timer_priority);
extern "C" void udelay(unsigned int us);
extern "C" void mdelay(unsigned int ms);
extern "C" uint32_t HAL_GetTick(void);
extern "C" unsigned long get_timer(unsigned long base);
extern "C" uint32_t SystemA35_TZ_STGEN_Start(uint32_t ck_ker_clk_freq);
extern "C" uint32_t SystemA35_SYSTICK_TimerSourceConfig(uint32_t timer_source);
extern "C" uint32_t SystemA35_ManageTick(uint32_t suspend_resume_stop_tick);
