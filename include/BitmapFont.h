#pragma once

#include <Arduino.h>
#include <stdint.h>

// 1-bpp bitmap font renderer — for glyphs the LT7680 CGROM cannot produce
// (arbitrary sizes, custom styles like seven-segment). Glyph bitmaps live in
// flash (PROGMEM); rendering walks rows and emits horizontal pixel runs as
// fillRectU calls, which the LT7680 executes as hardware fills.

struct BitmapGlyph {
    uint8_t width;           // glyph bitmap width in pixels
    uint8_t height;          // glyph bitmap height in pixels
    uint8_t advance;         // x-advance after drawing (monospace: == font cell width)
    int8_t  offsetX;         // bearing from pen to bitmap top-left
    int8_t  offsetY;
    const uint8_t* bitmap;   // 1bpp, MSB first, row-major, stride = (width+7)/8
};

struct BitmapFont {
    uint8_t height;              // line height (for vertical layout)
    uint8_t firstChar;           // ASCII code of glyphs[0]
    uint8_t lastChar;            // inclusive
    const BitmapGlyph* glyphs;   // size = lastChar - firstChar + 1
};

// Draw one glyph. Background is NOT cleared — caller fills the area first if
// needed. Out-of-range characters are silently skipped.
void drawGlyph(int ux, int uy, const BitmapFont& font, char c, uint16_t color);

// Draw a null-terminated string, advancing by glyph.advance per character.
void drawBitmapText(int ux, int uy, const BitmapFont& font,
                    const char* s, uint16_t color);

// Total advance width of the string (for centering / right-aligning).
int measureBitmapText(const BitmapFont& font, const char* s);

// Horizontally center text between [ux_start, ux_end).
void drawBitmapTextCenteredIn(int ux_start, int ux_end, int uy,
                              const BitmapFont& font,
                              const char* s, uint16_t color);
