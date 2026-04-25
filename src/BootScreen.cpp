#include "BootScreen.h"
#include "Tft.h"

// Layout (user frame 960x320):
//   Frame: 2px amber border, inset 8px from panel edges.
//   Wordmark "P-OBC"            16x32 zoom 4 → 64x128 per char (320x128 total).
//   Subtitle "PRECISION..."     12x24 zoom 2 → 24x48  per char.
//   Footer   "ASTRA"            16x32 zoom 2 → 32x64  per char.
//
// Hierarchy 128 → 64 → 48 puts the wordmark visually dominant, footer
// secondary as anchor, subtitle as the narrative line in between.

namespace {
constexpr int F_INSET = 8;
constexpr int F_THICK = 2;
}

static void drawFrame() {
    const int w = USR_W - 2 * F_INSET;
    const int h = USR_H - 2 * F_INSET;
    fillRectU(F_INSET,                       F_INSET,                       w,        F_THICK, COL_AMBER);
    fillRectU(F_INSET,                       USR_H - F_INSET - F_THICK,     w,        F_THICK, COL_AMBER);
    fillRectU(F_INSET,                       F_INSET,                       F_THICK,  h,       COL_AMBER);
    fillRectU(USR_W - F_INSET - F_THICK,     F_INSET,                       F_THICK,  h,       COL_AMBER);
}

void displayBoot() {
    targetBackBuffer();
    fillScreenU(COL_BG);
    drawFrame();
    drawCenteredU("P-OBC",                          30,  2, 4, COL_AMBER);
    drawCenteredU("PRECISION ON-BOARD COMPUTER",   174,  1, 2, COL_AMBER);
    drawCenteredU("ASTRA",                         242,  2, 2, COL_AMBER);
    flipBuffers();
}
