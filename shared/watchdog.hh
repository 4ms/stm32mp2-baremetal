#include "smc.hh"
#include <cstdint>

constexpr uint64_t SMC_WD_ID = 0xB200005A;

enum smc_wd_call {
	SMCWD_INIT = 0,
	SMCWD_SET_TIMEOUT = 1,
	SMCWD_ENABLE = 2,
	SMCWD_PET = 3,
	SMCWD_GET_TIMELEFT = 4,
};

inline int watchdog_pet()
{
	auto res = smc_call(SMC_WD_ID, SMCWD_PET, 0, 0, 0, 0, 0, 0);
	return res.a0;
}
