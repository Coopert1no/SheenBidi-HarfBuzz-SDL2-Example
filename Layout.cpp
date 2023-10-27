#include "Layout.h"

#include <assert.h>

TextLayout::TextLayout(hb_font_t* font, SBStringEncoding encoding, void* string, size_t length) :
	text{
		.stringEncoding = encoding,
		.stringBuffer = string,
		.stringLength = length
	},
	hb_font(font) {}

TextLayout::~TextLayout() {
	for (auto& run : runs) {
		if (run.buffer) {
			hb_buffer_destroy(run.buffer);
		}
	}
}

void TextLayout::layout() {
	assert(hb_font);
	assert(text.stringBuffer);

	// 0. Init SheenBidi's stuff
	SBAlgorithmRef bidi_algorithm = SBAlgorithmCreate(&text);
	SBParagraphRef paragraph = SBAlgorithmCreateParagraph(bidi_algorithm, 0, INT32_MAX, SBLevelDefaultLTR);
	SBUInteger paragraph_length = SBParagraphGetLength(paragraph);
	SBLineRef paragraph_line = SBParagraphCreateLine(paragraph, 0, paragraph_length);

	// 1. Split text into runs
	split_into_runs(
		SBLineGetRunsPtr(paragraph_line),
		SBLineGetRunCount(paragraph_line)
	);

	// 2. Get the glyphs
	shape();

	// 3. Cleanup SheenBidi's stuff
	SBLineRelease(paragraph_line);
	SBParagraphRelease(paragraph);
	SBAlgorithmRelease(bidi_algorithm);
}

float TextLayout::get_caret_pos_from_index(uint32_t index) {
	// TODO:
	return 0;
}

uint32_t TextLayout::get_caret_index_from_pos(float pos) {
	// TODO:
	return 0;
}

void TextLayout::split_into_runs(const SBRun* sb_runs, SBUInteger run_count) {
	SBScriptLocatorRef locator = SBScriptLocatorCreate();

	// First, we go through each run and then split that run into multiple runs, based on script.
	for (SBUInteger i = 0; i < run_count; ++i) {
		const SBRun* sb_run = &sb_runs[i];

		SBCodepointSequence cp_seq = {
			.stringEncoding = text.stringEncoding,
			.stringBuffer = nullptr,
			.stringLength = sb_run->length
		};

		if (cp_seq.stringEncoding == SBStringEncodingUTF32) {
			cp_seq.stringBuffer = (uint32_t*)text.stringBuffer + sb_run->offset;
		} else if (cp_seq.stringEncoding == SBStringEncodingUTF16) {
			cp_seq.stringBuffer = (uint16_t*)text.stringBuffer + sb_run->offset;
		} else if (cp_seq.stringEncoding == SBStringEncodingUTF8) {
			cp_seq.stringBuffer = (char*)text.stringBuffer + sb_run->offset;
		} else {
			printf("invalid string encoding.\n");
			abort();
		}

		SBScriptLocatorLoadCodepoints(locator, &cp_seq);

		// Split bidi runs into more runs based on script.
		while (SBScriptLocatorMoveNext(locator)) {
			const SBScriptAgent* agent = SBScriptLocatorGetAgent(locator);
			const uint32_t script_tag = SBScriptGetOpenTypeTag(agent->script);

			Run run = {
				nullptr,
				hb_script_from_iso15924_tag(script_tag),
				hb_script_get_horizontal_direction(run.script),
				(uint32_t)sb_run->offset + (uint32_t)agent->offset,
				(uint32_t)agent->length
			};

			runs.push_back(run);
		}

		SBScriptLocatorReset(locator);
	}

	SBScriptLocatorRelease(locator);
}

void TextLayout::shape() {
	std::vector<Glyph> result;

	for (Run& run : runs) {
		if (run.buffer == nullptr) {
			run.buffer = hb_buffer_create();
		}

		if (text.stringEncoding == SBStringEncodingUTF32) {
			hb_buffer_add_utf32(
				run.buffer,
				(const uint32_t*)text.stringBuffer,
				(int)text.stringLength,
				(int)run.offset,
				(int)run.length
			);
		} else if (text.stringEncoding == SBStringEncodingUTF16) {
			hb_buffer_add_utf16(
				run.buffer,
				(const uint16_t*)text.stringBuffer,
				(int)text.stringLength,
				(int)run.offset,
				(int)run.length
			);
		} else if (text.stringEncoding == SBStringEncodingUTF8) {
			hb_buffer_add_utf8(
				run.buffer,
				(const char*)text.stringBuffer,
				(int)text.stringLength,
				(int)run.offset,
				(int)run.length
			);
		} else {
			printf("invalid string encoding.\n");
			abort();
		}

		hb_buffer_set_direction(run.buffer, run.direction);
		hb_buffer_set_script(run.buffer, run.script);
		hb_buffer_set_language(run.buffer, hb_language_get_default());

		hb_shape(hb_font, run.buffer, nullptr, 0);

		uint32_t glyph_count;
		hb_glyph_info_t* glyph_info = hb_buffer_get_glyph_infos(run.buffer, &glyph_count);
		hb_glyph_position_t* glyph_pos = hb_buffer_get_glyph_positions(run.buffer, &glyph_count);

		for (uint32_t i = 0; i < glyph_count; ++i) {
			Glyph glyph;

			glyph.codepoint = glyph_info[i].codepoint;
			glyph.cluster = glyph_info[i].cluster;

			glyph.x_offset = (float)glyph_pos[i].x_offset / 64.0f;
			glyph.y_offset = (float)glyph_pos[i].y_offset / 64.0f;
			glyph.x_advance = (float)glyph_pos[i].x_advance / 64.0f;
			glyph.y_advance = (float)glyph_pos[i].y_advance / 64.0f;

			result.push_back(glyph);
		}

		hb_buffer_clear_contents(run.buffer);
	}

	glyphs = result;
}
