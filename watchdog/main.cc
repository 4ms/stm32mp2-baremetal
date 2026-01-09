#include "drivers/aarch64_system_reg.hh"
#include "print.hh"
#include "watchdog.hh"

int main()
{
	print("Watchdog example\n");

	const auto cntperiod = read_cntfreq();
	cntp_enable(true);

	const auto cnt_start = read_cntpct();
	bool did_pet = false;

	while (true) {
		auto cnt = read_cntpct() - cnt_start;
		auto cnt_sec = cnt / cntperiod;
		if (cnt_sec % 3 == 0) {
			if (!did_pet) {
				print("Petting the watchdog at ", cnt_sec, " sec.\n");
				watchdog_pet();
				did_pet = true;
			}
		} else {
			did_pet = false;
		}
	}
}
