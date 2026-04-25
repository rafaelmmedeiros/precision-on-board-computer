#include "BootScreen.h"
#include "Tft.h"

// Just "ASTRA" centered on black — biggest CGROM (16x32 zoom 4 = 64x128 per
// char). 5 chars × 64 = 320 wide, vertical-centered in 320-tall user frame.
void displayBoot() {
    targetBackBuffer();
    fillScreenU(COL_BG);
    constexpr int CHAR_H = 32 * 4;                    // 128
    constexpr int Y      = (USR_H - CHAR_H) / 2;      // 96
    drawCenteredU("ASTRA", Y, 2, 4, COL_AMBER);
    flipBuffers();
}
