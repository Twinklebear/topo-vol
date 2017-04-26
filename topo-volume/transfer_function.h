#pragma once

#include <set>
#include <memory>
#include <vector>
#include <array>
#include <glm/glm.hpp>
#include "glt/gl_core_4_5.h"
#include "glt/buffer_allocator.h"

class TransferFunction {
	// A line is made up of points sorted by x, its coordinates are
	// on the range [0, 1]
	struct Line {
		std::vector<glm::vec2> line;
		int color;

		// TODO: Constructor that takes an existing line
		// Construct a new diagonal line: [(0, 0), (1, 1)]
		Line();
		/* Move a point on the line from start to end, if the line is
		 * not split at 'start' it will be split then moved
		 * TODO: Should we have some cap on the number of points? We should
		 * also track if you're actively dragging a point so we don't recreate
		 * points if you move the mouse too fast
		 */
		void move_point(const float &start_x, const glm::vec2 &end);
		// Remove a point from the line, merging the two segments on either side
		void remove_point(const float &x);
	};

	struct Palette {
		// The line currently being edited
		int active_line;
		// Lines for RGBA transfer function controls
		std::array<Line, 4> rgba_lines;
		// The segments that this palette is applied to
		std::set<int> segments;

		Palette();
	};

	std::vector<Palette> palettes;
	int active_palette;

	// Track if the function changed and must be re-uploaded.
	// We start by marking it changed to upload the initial palette
	bool fcn_changed, active_fcn_changed;
	GLint max_palettes;

	/* The 1d palette texture on the GPU (0) for coloring
	 * the volume and the 2d one (1) for displaying the color map
	 */
	std::array<GLuint, 2> palette_tex;

public:
	// The histogram for the volume data
	std::vector<size_t> *histogram;

	TransferFunction();
	~TransferFunction();
	TransferFunction(const TransferFunction&) = delete;
	TransferFunction& operator=(const TransferFunction&) = delete;
	/* Draw the transfer function editor widget
	 */
	void draw_ui();
	/* Render the transfer function to a 1D texture that can
	 * be applied to volume data
	 */
	void render();

private:
	void render_palette_ui(Palette &p); 
	void resample_palette(const Palette &p, std::vector<uint8_t> &out);
};

