#pragma once
#include <cstdint>
#include <vector>

#include <SheenBidi.h>
#include <harfbuzz/hb.h>

// NOTE: Works only for one line
struct TextLayout {
	struct Glyph {
		uint32_t codepoint;
		uint32_t cluster;

		float x_offset;
		float y_offset;

		float x_advance;
		float y_advance;
	};

	struct Run {
		hb_buffer_t* buffer;
		hb_script_t script;
		hb_direction_t direction;

		uint32_t offset;
		uint32_t length;
	};

public:
	TextLayout(hb_font_t* font, SBStringEncoding encoding, void* string, size_t length);
	~TextLayout();

	void layout();

	float get_caret_pos_from_index(uint32_t index);
	uint32_t get_caret_index_from_pos(float pos);

	const std::vector<Glyph>& get_glyphs() const {
		return glyphs;
	}

	const std::vector<Run>& get_runs() const {
		return runs;
	}

private:
	void split_into_runs(
		const SBRun* sb_runs,
		SBUInteger run_count
	);

	void shape();

private:
	SBCodepointSequence text = {};
	hb_font_t* hb_font = nullptr;

	std::vector<Run> runs;
	std::vector<Glyph> glyphs;
};

