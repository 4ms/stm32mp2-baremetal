#include "../aarch64/system_reg.hh"
#include <cstdint>

static uint32_t cntfreq_ms;
extern "C" uint32_t SystemA35_SYSTICK_Config(uint32_t timer_priority)
{
	auto cntfreq = read_cntfreq();
	cntfreq_ms = cntfreq / 1000;
	// print("CNT Freq = ", hz, "\n");

	cntp_enable(false);
	cntp_irq_enable(false);

	// Clear stale compare by reading cval
	auto now = read_cntpct();

	cntp_enable(true);
	return 0;
}

extern "C" uint32_t HAL_GetTick(void)
{
	return read_cntpct() / cntfreq_ms;
}

extern "C" uint32_t SystemA35_TZ_STGEN_Start(uint32_t ck_ker_clk_freq)
{
	return 0;
}

extern "C" uint32_t SystemA35_SYSTICK_TimerSourceConfig(uint32_t timer_source)
{
	return 0;
}

extern "C" uint32_t SystemA35_ManageTick(uint32_t suspend_resume_stop_tick)
{
	return 0;
}
