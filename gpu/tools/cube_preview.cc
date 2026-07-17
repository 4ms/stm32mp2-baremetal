// =============================================================================
//  cube_preview.cc -- HOST tool: render the cube scene to image files
// =============================================================================
// Compiles on the development machine (clang++ -std=c++20), not the target.
// Renders the EXACT scene the GPU draws (cube_scene.hh) with the EXACT CPU
// reference renderer the GPU was verified against (cube_cpu_render.hh), and
// writes PPM images -- so human eyes can finally check that "GPU == CPU" also
// means "correct".
//
//   ./cube_preview <width> <height> <aspect> <angle> <tilt> <out.ppm>
//
// Build:  clang++ -std=c++20 -O2 -I.. cube_preview.cc -o cube_preview

#include "cube_cpu_render.hh"
#include <cstdio>
#include <cstdlib>
#include <vector>

int main(int argc, char **argv)
{
	if (argc != 7) {
		fprintf(stderr, "usage: %s <w> <h> <aspect> <angle> <tilt> <out.ppm>\n", argv[0]);
		return 1;
	}
	uint32_t w = atoi(argv[1]), h = atoi(argv[2]);
	float aspect = atof(argv[3]), angle = atof(argv[4]), tilt = atof(argv[5]);

	std::vector<uint32_t> img(w * h);
	std::vector<uint8_t> band(w * h);
	std::vector<float> zbuf(w * h);
	Mat4 m = cube_mvp(angle, tilt, aspect);
	cpu_render_cube(m, w, h, img, band, zbuf, 0xFF101828); // demo background

	FILE *f = fopen(argv[6], "wb");
	fprintf(f, "P6\n%u %u\n255\n", w, h);
	for (uint32_t p : img) {
		uint8_t rgb[3] = {uint8_t(p >> 16), uint8_t(p >> 8), uint8_t(p)};
		fwrite(rgb, 1, 3, f);
	}
	fclose(f);
	return 0;
}
