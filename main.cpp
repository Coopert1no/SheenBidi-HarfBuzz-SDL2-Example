#include <cassert>
#include <codecvt>
#include <fstream>
#include <locale>
#include <string>

#include <SDL2/SDL.h>
#undef main

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#endif

#include <harfbuzz/hb-ft.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include "Layout.h"

// Globals
FT_Library ft_lib;
FT_Face ft_face;
hb_font_t* hb_font;
SDL_Renderer* renderer;
int total_glyphs_shaped;
int total_glyphs_unshaped;
bool printed_info;

std::wstring init(
	int window_width,
	int window_height,
	int font_size,
	const char* font_path,
	const char* text_path
);

SDL_Texture* create_glyph_texture(uint32_t glyph_index, int* out_texture_width, int* out_texture_height);
void draw_glyph(uint32_t glyph_index, float x, float y);
void draw_bidi_shaped(const TextLayout& layout, float x, float y);
void draw_bidi_unshaped(const std::wstring& utf16_text, float x, float y);
std::wstring utf8_to_utf16(const char* utf8, size_t utf8_size);

int main() {
	int font_size = 70;
	std::wstring utf16_text =
		init(1280, 720, font_size, "C:/Windows/Fonts/arial.ttf", "utf8_text.txt");

	TextLayout text_layout(
		hb_font,
		SBStringEncodingUTF16,
		(void*)utf16_text.c_str(),
		utf16_text.length()
	);

	text_layout.layout();

	bool running = true;
	while (running) {
		// Events
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT) {
				running = false;
			}
		}

		// Clear
		SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
		SDL_RenderClear(renderer);

		// Render stuff
		float offset = 50;
		draw_bidi_shaped(text_layout, 0, offset);
		draw_bidi_unshaped(utf16_text, 0, offset + 20 + (float)font_size);

		if (!printed_info) {
			printed_info = true;
			printf("glyphs shaped: %d, unshaped: %d\n", total_glyphs_shaped, total_glyphs_unshaped);
		}

		//
		SDL_RenderPresent(renderer);
	}
	return 0;
}

std::wstring init(
	int window_width,
	int window_height,
	int font_size,
	const char* font_path,
	const char* text_path
) {
	// Font stuff
	{
		assert(FT_Init_FreeType(&ft_lib) == 0);
		assert(FT_New_Face(ft_lib, font_path, 0, &ft_face) == 0);
		assert(FT_Set_Char_Size(ft_face, 0, font_size * 64, 0, 0) == 0);
		hb_font = hb_ft_font_create_referenced(ft_face);
		assert(hb_font);
	}

	// SDL Stuff
	{
		assert(SDL_Init(SDL_INIT_EVERYTHING) == 0);
		SDL_Window* window = SDL_CreateWindow(
			"SheenBidi HarfBuzz SDL2 Example",
			SDL_WINDOWPOS_CENTERED,
			SDL_WINDOWPOS_CENTERED,
			window_width,
			window_height,
			0
		);

		int renderer_flags = SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC;
		renderer = SDL_CreateRenderer(window, -1, renderer_flags);
		assert(renderer);
	}

	std::wstring utf16_text;
	// Load text from a file
	{
		std::ifstream file(text_path);
		assert(file.is_open());
		std::string str(
			(std::istreambuf_iterator(file)),
			std::istreambuf_iterator<char>()
		);
		utf16_text = utf8_to_utf16(str.c_str(), str.size());
	}

	return utf16_text;
}

SDL_Texture* create_glyph_texture(uint32_t glyph_index, int* out_texture_width, int* out_texture_height) {
	assert(FT_Load_Glyph(ft_face, glyph_index, FT_LOAD_RENDER) == 0);
	if (ft_face->glyph->bitmap.buffer == nullptr) {
		return nullptr;
	}

	SDL_Surface* glyph_surface = SDL_CreateRGBSurfaceFrom(
		ft_face->glyph->bitmap.buffer,
		(int)ft_face->glyph->bitmap.width,
		(int)ft_face->glyph->bitmap.rows,
		8,
		ft_face->glyph->bitmap.pitch,
		0,
		0,
		0,
		0xFF
	);
	assert(glyph_surface != nullptr);

	SDL_Color colors[256];
	for (int i = 0; i < 256; i++) {
		colors[i].r = (Uint8)i;
		colors[i].g = (Uint8)i;
		colors[i].b = (Uint8)i;
	}
	SDL_SetPaletteColors(glyph_surface->format->palette, colors, 0, 256);
	SDL_SetSurfaceBlendMode(glyph_surface, SDL_BLENDMODE_ADD);

	SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, glyph_surface);
	assert(texture != nullptr);
	SDL_FreeSurface(glyph_surface);

	assert(SDL_QueryTexture(texture, nullptr, nullptr, out_texture_width, out_texture_height) == 0);
	return texture;
}

void draw_glyph(uint32_t glyph_index, float x, float y) {
	uint8_t r, g, b;
	{
		r = (uint8_t)rand();
		g = (uint8_t)rand();
		b = (uint8_t)rand();
		SDL_clamp(r, 200, 255);
		SDL_clamp(g, 200, 255);
		SDL_clamp(b, 200, 255);
	}

	int texture_width, texture_height;
	SDL_Texture* glyph_texture = create_glyph_texture(glyph_index, &texture_width, &texture_height);
	if (glyph_texture == nullptr) {
		return;
	}
	SDL_SetTextureColorMod(glyph_texture, r, g, b);

	SDL_FRect dest;
	{
		dest.x = x;
		dest.y = y;
		dest.w = (float)texture_width;
		dest.h = (float)texture_height;

		// Bearings and ascender
		dest.x += (float)ft_face->glyph->metrics.horiBearingX / 64.0f;
		dest.y -= (float)ft_face->glyph->metrics.horiBearingY / 64.0f;
		dest.y += (float)ft_face->size->metrics.ascender / 64.0f;
	}

	SDL_RenderCopyF(renderer, glyph_texture, nullptr, &dest);
	SDL_DestroyTexture(glyph_texture);
}

void draw_bidi_shaped(const TextLayout& layout, float x, float y) {
	srand(0);

	float cursor_x = x;
	float cursor_y = y;
	for (const TextLayout::Glyph& glyph : layout.get_glyphs()) {
		draw_glyph(glyph.codepoint, cursor_x + glyph.x_offset, cursor_y - glyph.y_offset);
		cursor_x += glyph.x_advance;
		cursor_y += glyph.y_advance;
	}
}

void draw_bidi_unshaped(const std::wstring& utf16_text, float x, float y) {
	srand(0);

	float cursor_x = x;
	float cursor_y = y;
	uint32_t prev_glyph = 0;
	int glyph_count = 0;
	for (char32_t character : utf16_text) {
		uint32_t glyph_index = FT_Get_Char_Index(ft_face, character);

		// Kerning
		{
			FT_Vector kerning;
			FT_Get_Kerning(ft_face, prev_glyph, glyph_index, FT_KERNING_DEFAULT, &kerning);
			cursor_x += (float)kerning.x / 64.0f;
		}

		// Skip whitespace
		if (glyph_index != 3) {
			draw_glyph(glyph_index, cursor_x, cursor_y);
		} else {
			// We need to load the glyph even if we don't want to render it, to get advances for whitespace.
			FT_Load_Glyph(ft_face, glyph_index, FT_LOAD_ADVANCE_ONLY);
		}

		cursor_x += (float)ft_face->glyph->advance.x / 64.0f;
		cursor_y += (float)ft_face->glyph->advance.y / 64.0f;

		prev_glyph = glyph_index;
		++glyph_count;
	}

	total_glyphs_unshaped = glyph_count;
}

std::wstring utf8_to_utf16(const char* utf8, size_t utf8_size) {
	std::wstring result;

#ifdef _WIN32
	// utf8 -> utf16
	int utf16_size =
		MultiByteToWideChar(
			CP_UTF8,
			MB_PRECOMPOSED,
			utf8,
			(int)utf8_size,
			nullptr,
			0
		);

	wchar_t* buffer = (wchar_t*)malloc((utf16_size + 1) * sizeof(wchar_t));
	buffer[utf16_size] = 0;

	utf16_size = MultiByteToWideChar(
		CP_UTF8,
		MB_PRECOMPOSED,
		utf8,
		(int)utf8_size,
		buffer,
		utf16_size
	);
	assert(utf16_size);

	result = buffer;
	free(buffer);

#else
#error Please convert utf8 to utf16 for your platform.
#endif

	return result;
}
