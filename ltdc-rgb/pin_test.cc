#include "drivers/hal_cnt.hh"
#include "drivers/pin.hh"
#include "print/print.hh"
#include "stm32mp2xx.h"

// ---- pad self-test ------------------------------------------------------------
// Every header line is briefly taken away from the LTDC/SPI and used as a GPIO:
//  * all inputs with pull-down -> any HIGH  = driven/shorted high externally
//  * all inputs with pull-up   -> any LOW   = shorted low externally
//  * each pad driven HIGH in turn (others pull-down inputs) -> any other pad
//    reading HIGH = short between the two; the driven pad reading LOW = it
//    cannot drive (short to GND). Then driven LOW: reading HIGH = short to VCC.
// The LTDC keeps running; the panel shows garbage for a few ms.
struct Pad {
	GPIO port;
	uint8_t pin;
	const char *name;
	bool ltdc; // restore to AF13 (else: SPI idle state)
};
constexpr Pad pads[] = {
	{GPIO::F, 12, "CLK/PF12", true},	{GPIO::I, 7, "HSYNC/PI7", true},   {GPIO::I, 6, "VSYNC/PI6", true},
	{GPIO::I, 5, "DE/PI5", true},		{GPIO::F, 13, "R2/PF13", true},	   {GPIO::F, 14, "R3/PF14", true},
	{GPIO::F, 15, "R4/PF15", true},		{GPIO::G, 5, "R5/PG5", true},	   {GPIO::G, 6, "R6/PG6", true},
	{GPIO::G, 7, "R7/PG7", true},		{GPIO::G, 8, "G2/PG8", true},	   {GPIO::G, 9, "G3/PG9", true},
	{GPIO::G, 10, "G4/PG10", true},		{GPIO::G, 11, "G5/PG11", true},	   {GPIO::G, 12, "G6/PG12", true},
	{GPIO::G, 13, "G7/PG13", true},		{GPIO::G, 15, "B2/PG15", true},	   {GPIO::I, 0, "B3/PI0", true},
	{GPIO::I, 1, "B4/PI1", true},		{GPIO::I, 2, "B5/PI2", true},	   {GPIO::I, 3, "B6/PI3", true},
	{GPIO::I, 4, "B7/PI4", true},		{GPIO::F, 4, "SPI_CS/PF4", false}, {GPIO::A, 12, "SPI_MOSI/PA12", false},
	{GPIO::F, 5, "SPI_SCK/PF5", false},
};
constexpr unsigned NumPads = sizeof(pads) / sizeof(pads[0]);

void pad_selftest()
{
	Pin pin[NumPads];
	for (unsigned i = 0; i < NumPads; i++)
		pin[i].init({pads[i].port, PinNum(pads[i].pin), AFNone}, PinMode::Input, PinPull::Down);
	udelay(5000);
	bool rd_pd[NumPads], rd_pu[NumPads];
	for (unsigned i = 0; i < NumPads; i++)
		rd_pd[i] = pin[i].read_raw();
	for (unsigned i = 0; i < NumPads; i++)
		pin[i].set_pull(PinPull::Up);
	udelay(5000);
	for (unsigned i = 0; i < NumPads; i++)
		rd_pu[i] = pin[i].read_raw();
	for (unsigned i = 0; i < NumPads; i++)
		pin[i].set_pull(PinPull::Down);

	// Classify: a line that follows our weak internal pull has nothing external
	// on it (floating = open toward the panel, if its siblings are biased); one
	// that reads the same either way is biased by something stronger outside.
	print("pad self-test (", NumPads, " lines): state with internal pull-down / pull-up\n");
	for (unsigned i = 0; i < NumPads; i++) {
		print("  ", pads[i].name, ": ", rd_pd[i] ? "H" : "L", "/", rd_pu[i] ? "H" : "L", "  ");
		if (!rd_pd[i] && rd_pu[i])
			print("FLOATING (follows our pulls: nothing external, open?)\n");
		else if (rd_pd[i] && rd_pu[i])
			print("biased HIGH externally (pull-up or driven)\n");
		else if (!rd_pd[i] && !rd_pu[i])
			print("biased LOW externally (pull-down or driven)\n");
		else
			print("?? (reads opposite of our pulls)\n");
	}

	// Follow test: drive one line, watch the others. Only lines that read LOW
	// at rest (pull-down) can meaningfully be seen to follow a HIGH drive.
	unsigned faults = 0;
	for (unsigned i = 0; i < NumPads; i++) {
		pin[i].set_mode(PinMode::Output);
		pin[i].high();
		udelay(2000);
		if (!pin[i].read_raw()) {
			print("  ", pads[i].name, ": reads LOW while driven HIGH -> short to GND\n");
			faults++;
		}
		bool hi_follow[NumPads], lo_follow[NumPads];
		for (unsigned j = 0; j < NumPads; j++)
			hi_follow[j] = (j != i && !rd_pd[j] && pin[j].read_raw());
		pin[i].low();
		udelay(2000);
		if (pin[i].read_raw()) {
			print("  ", pads[i].name, ": reads HIGH while driven LOW -> short to VCC\n");
			faults++;
		}
		// and the mirror image for lines that rest HIGH: drive low, see who follows
		for (unsigned j = 0; j < NumPads; j++)
			lo_follow[j] = (j != i && rd_pu[j] && rd_pd[j] && !pin[j].read_raw());
		pin[i].set_mode(PinMode::Input);
		udelay(2000);
		// A real short tracks the driver both ways: the follower must return to
		// its rest state once the driver is released. A line that merely drifted
		// (slow RC, marginal external pull) stays put and is not reported.
		for (unsigned j = 0; j < NumPads; j++) {
			bool now = pin[j].read_raw();
			if (hi_follow[j] && !now) {
				print("  ", pads[i].name, " <-> ", pads[j].name, ": SHORT (follows high)\n");
				faults++;
			}
			if (lo_follow[j] && now) {
				print("  ", pads[i].name, " <-> ", pads[j].name, ": SHORT (follows low)\n");
				faults++;
			}
		}
	}
	// restore
	for (unsigned i = 0; i < NumPads; i++) {
		pin[i].set_pull(PinPull::None);
		if (pads[i].ltdc) {
			pin[i].set_alt(13);
			pin[i].set_mode(PinMode::Alt);
		} else {
			pin[i].set_mode(PinMode::Output);
			if (pads[i].pin == 4) // CS idles high
				pin[i].high();
			else
				pin[i].low();
		}
	}
	print("pad self-test done: ", faults, " short(s)\n");
}
