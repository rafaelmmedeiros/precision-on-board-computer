#include "BitmapFont.h"
#include "Tft.h"

// Renderer strategy: for each row, scan bits left-to-right and batch contiguous
// "on" bits into one fillRectU call. Seven-segment-style glyphs have few, long
// runs per row, so we get close to one fill per segment. Over SPI at 8 MHz one
// DrawSquare_Fill is ~27 µs, so a 96-px-tall digit with ~2 runs/row finishes in
// ~5 ms — fine for the 10 Hz UI loop.

static void drawGlyphBitmap(int ux, int uy, const BitmapGlyph& g, uint16_t color) {
    const int stride = (g.width + 7) / 8;
    const uint8_t* row = g.bitmap;
    if (!row) return;

    for (int y = 0; y < g.height; ++y) {
        int x = 0;
        while (x < g.width) {
            // skip run of 0s
            while (x < g.width) {
                uint8_t byte = pgm_read_byte(row + (x >> 3));
                if (byte & (0x80 >> (x & 7))) break;
                ++x;
            }
            if (x >= g.width) break;
            int runStart = x;
            // scan run of 1s
            while (x < g.width) {
                uint8_t byte = pgm_read_byte(row + (x >> 3));
                if (!(byte & (0x80 >> (x & 7)))) break;
                ++x;
            }
            fillRectU(ux + runStart, uy + y, x - runStart, 1, color);
        }
        row += stride;
    }
}

static const BitmapGlyph* lookup(const BitmapFont& font, char c) {
    uint8_t uc = (uint8_t)c;
    if (uc < font.firstChar || uc > font.lastChar) return nullptr;
    return &font.glyphs[uc - font.firstChar];
}

void drawGlyph(int ux, int uy, const BitmapFont& font, char c, uint16_t color) {
    const BitmapGlyph* g = lookup(font, c);
    if (!g) return;
    drawGlyphBitmap(ux + g->offsetX, uy + g->offsetY, *g, color);
}

void drawBitmapText(int ux, int uy, const BitmapFont& font,
                    const char* s, uint16_t color) {
    for (; *s; ++s) {
        const BitmapGlyph* g = lookup(font, *s);
        if (g) {
            drawGlyphBitmap(ux + g->offsetX, uy + g->offsetY, *g, color);
            ux += g->advance;
        } else {
            ux += font.height / 2;   // fallback advance for unknown glyphs
        }
    }
}

int measureBitmapText(const BitmapFont& font, const char* s) {
    int w = 0;
    for (; *s; ++s) {
        const BitmapGlyph* g = lookup(font, *s);
        w += g ? g->advance : (font.height / 2);
    }
    return w;
}

void drawBitmapTextCenteredIn(int ux_start, int ux_end, int uy,
                              const BitmapFont& font,
                              const char* s, uint16_t color) {
    int w = measureBitmapText(font, s);
    int ux = ux_start + (ux_end - ux_start - w) / 2;
    if (ux < ux_start) ux = ux_start;
    drawBitmapText(ux, uy, font, s, color);
}
