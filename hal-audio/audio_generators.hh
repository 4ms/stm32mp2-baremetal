#pragma once
#include <cmath>
#include <cstdint>

#ifndef M_PI
#define M_PI 3.141592653589793238462643f
#endif

struct SineGen {
	float phase = 0; // 0..1
	const float sample_rate;

	SineGen(float samplerate)
		: sample_rate{samplerate}
	{}

	int32_t sample(float freq)
	{
		float freq_inc = freq / sample_rate;

		phase += freq_inc;
		if (phase >= 1.f)
			phase -= 1.f;

		// float -1..1 => 24-bit signed -8M..8M
		return 0x7FFFFFL * std::sin(2.f * M_PI * phase);
	};
};
