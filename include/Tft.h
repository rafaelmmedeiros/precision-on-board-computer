#pragma once

#include <Arduino.h>
#include "LCD.h"

// Landscape dimensions (user frame — panel is mounted rotated 90°).
constexpr int USR_W = LCD_YSIZE_TFT;   // 960
constexpr int USR_H = LCD_XSIZE_TFT;   // 320

// P-OBC palette (RGB565).
constexpr uint16_t COL_BG    = 0x0000;   // black
constexpr uint16_t COL_AMBER = 0xFC40;   // ~#FF8A00 dark orange — primary UI color
constexpr uint16_t COL_HOT   = 0xF800;   // red
constexpr uint16_t COL_COLD  = 0x449F;   // sky blue
constexpr uint16_t COL_GOOD  = 0x87F0;   // soft green

// One-time display bring-up: LT7680 + ST7701S init, font rotation, main window.
void tftInit();

// Geometry (user coords, 0..959 × 0..319).
void fillRectU(int ux, int uy, int uw, int uh, uint16_t color);
void fillScreenU(uint16_t color);

// Text: fontSel 0=8x16, 1=12x24, 2=16x32 CGROM; zoom 1..4.
void drawTextU(int ux, int uy, uint8_t fontSel, uint8_t zoom,
               uint16_t color, const char* s);
void drawCenteredU(const char* s, int uy,
                   uint8_t fontSel, uint8_t zoom, uint16_t color);
void drawCenteredInU(int ux_start, int ux_end, int uy,
                     uint8_t fontSel, uint8_t zoom,
                     uint16_t color, const char* s);

// Font metrics (after Font_90_degree rotation — the "tall" side becomes user-X).
int fontCellLong(uint8_t fontSel, uint8_t zoom);
int fontCellShort(uint8_t fontSel, uint8_t zoom);

// Double buffering. Draw into the back buffer, then flip to show it.
void targetBackBuffer();
void flipBuffers();

// Semantic color pickers used by the screens.
uint16_t tempColor(float t);       // <10°C blue, >30°C red, else amber
uint16_t voltageColor(float v);    // undervolt/overvolt red, nominal amber, charging green
