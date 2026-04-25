#include "Tft.h"
#include "Features.h"
#include "LCD_init.h"   // ST7701S_Initial() — only included here (single TU)

// --- Double buffer state ---------------------------------------------------

static unsigned long g_frontFb = layer1_start_addr;
static unsigned long g_backFb  = layer2_start_addr;

// Anti-image-sticking shift. Advanced every SHIFT_INTERVAL_MS by tftTick().
static int g_shiftX = 0;
static int g_shiftY = 0;

// --- Font helpers ----------------------------------------------------------

// With Font_90_degree, the cursor advances along panel.y (= user.X) by the
// ORIGINAL glyph width (8/12/16). The glyph's extent in user.Y is the
// original height (16/24/32). Earlier version had these swapped, which
// caused every centered element to hug the left edge.
int fontCellLong(uint8_t fontSel, uint8_t zoom) {
    int wide = (fontSel == 0) ? 8 : (fontSel == 1) ? 12 : 16;
    return wide * zoom;
}

int fontCellShort(uint8_t fontSel, uint8_t zoom) {
    int tall = (fontSel == 0) ? 16 : (fontSel == 1) ? 24 : 32;
    return tall * zoom;
}

static void selectFont(uint8_t fontSel, uint8_t zoom) {
    switch (fontSel) {
        case 0:  ER_TFT.Font_Select_8x16_16x16();   break;
        case 1:  ER_TFT.Font_Select_12x24_24x24();  break;
        default: ER_TFT.Font_Select_16x32_32x32();  break;
    }
    switch (zoom) {
        case 1:  ER_TFT.Font_Width_X1(); ER_TFT.Font_Height_X1(); break;
        case 2:  ER_TFT.Font_Width_X2(); ER_TFT.Font_Height_X2(); break;
        case 3:  ER_TFT.Font_Width_X3(); ER_TFT.Font_Height_X3(); break;
        default: ER_TFT.Font_Width_X4(); ER_TFT.Font_Height_X4(); break;
    }
}

// --- Drawing ---------------------------------------------------------------

void fillRectU(int ux, int uy, int uw, int uh, uint16_t color) {
    ux += g_shiftX;
    uy += g_shiftY;
    ER_TFT.DrawSquare_Fill(uy, ux, uy + uh, ux + uw, color);
}

void fillScreenU(uint16_t color) {
    // Background covers the full panel WITHOUT the anti-stick shift so the
    // shifted edges never reveal stale framebuffer pixels.
    ER_TFT.DrawSquare_Fill(0, 0, USR_H, USR_W, color);
}

void drawTextU(int ux, int uy, uint8_t fontSel, uint8_t zoom,
               uint16_t color, const char* s) {
    ER_TFT.Foreground_color_65k(color);
    ER_TFT.Background_color_65k(COL_BG);
    ER_TFT.CGROM_Select_Internal_CGROM();
    selectFont(fontSel, zoom);
    ER_TFT.Goto_Text_XY(uy + g_shiftY, ux + g_shiftX);
    ER_TFT.Show_String((char*)s);
}

void drawCenteredU(const char* s, int uy, uint8_t fontSel, uint8_t zoom,
                   uint16_t color) {
    int w = (int)strlen(s) * fontCellLong(fontSel, zoom);
    int ux = (USR_W - w) / 2;
    if (ux < 0) ux = 0;
    drawTextU(ux, uy, fontSel, zoom, color, s);
}

void drawCenteredInU(int ux_start, int ux_end, int uy,
                     uint8_t fontSel, uint8_t zoom,
                     uint16_t color, const char* s) {
    int w = (int)strlen(s) * fontCellLong(fontSel, zoom);
    int ux = ux_start + (ux_end - ux_start - w) / 2;
    if (ux < ux_start) ux = ux_start;
    drawTextU(ux, uy, fontSel, zoom, color, s);
}

// --- Double buffer ---------------------------------------------------------

void targetBackBuffer() {
    ER_TFT.Canvas_Image_Start_address(g_backFb);
    ER_TFT.Canvas_image_width(LCD_XSIZE_TFT);
    ER_TFT.Active_Window_XY(0, 0);
    ER_TFT.Active_Window_WH(LCD_XSIZE_TFT, LCD_YSIZE_TFT);
}

void flipBuffers() {
    ER_TFT.Main_Image_Start_Address(g_backFb);
    // The LT7680 latches the new scanout address on its next vsync (~16 ms
    // at 60 Hz panel rate). Until then the panel is still reading from the
    // old buffer — which is about to become our new back buffer. Waiting
    // here guarantees the next draw mutates a buffer the panel is no longer
    // scanning, eliminating the partial-frame flicker on heavy screens
    // whose draws span multiple panel frames.
    delay(20);
    unsigned long tmp = g_frontFb;
    g_frontFb = g_backFb;
    g_backFb  = tmp;
    targetBackBuffer();
}

// --- Color logic -----------------------------------------------------------

uint16_t tempColor(float t) {
    if (t < 10.0f) return COL_COLD;
    if (t > 30.0f) return COL_HOT;
    return COL_AMBER;
}

uint16_t voltageColor(float v) {
    if (v < 12.0f)  return COL_HOT;    // undervoltage
    if (v < 13.5f)  return COL_AMBER;  // idle / no alternator
    if (v <= 14.8f) return COL_GOOD;   // alternator charging
    return COL_HOT;                     // overvoltage
}

uint16_t consumptionColor(float kmL) {
    // Same thresholds as the consumption history bars — keeps every
    // view of the same physical quantity visually consistent.
    if (kmL < 10.0f) return COL_HOT;
    if (kmL > 14.0f) return COL_GOOD;
    return COL_AMBER;
}

// --- Backlight (PWM on GPIO 2) ---------------------------------------------
//
// LEDC channel 0, ~5 kHz, 8-bit resolution (0..255). Wiring intent: GPIO 2
// drives the LT7680 BL_CONTROL input (datasheet pin 9). Until the 3.3V
// hard-tie at BL_CONTROL is cut and rerouted, the PWM only blinks the
// onboard LED — useful as bench debug feedback.

static constexpr int      BACKLIGHT_PIN     = 2;
static constexpr int      BACKLIGHT_CHANNEL = 0;
static constexpr uint32_t BACKLIGHT_FREQ_HZ = 5000;
static constexpr uint8_t  BACKLIGHT_RES_BITS = 8;

void tftBacklightInit() {
    ledcSetup(BACKLIGHT_CHANNEL, BACKLIGHT_FREQ_HZ, BACKLIGHT_RES_BITS);
    ledcAttachPin(BACKLIGHT_PIN, BACKLIGHT_CHANNEL);
    tftBacklight(255);   // start at full brightness through the same path
}

void tftBacklight(uint8_t level) {
    // The API is intuitive — 0 dark, 255 bright. The current breakout
    // happens to drive the panel inverted (high duty → dim), so flip
    // before writing when BACKLIGHT_ACTIVE_LOW is set.
    const uint8_t pwm = pobc::BACKLIGHT_ACTIVE_LOW ? (uint8_t)(255 - level) : level;
    ledcWrite(BACKLIGHT_CHANNEL, pwm);
}

// --- Anti-image-sticking ---------------------------------------------------

void tftTick() {
    static uint32_t lastShift = 0;
    static uint8_t  phase     = 0;
    constexpr uint32_t SHIFT_INTERVAL_MS = 5UL * 60UL * 1000UL;  // 5 min

    if (millis() - lastShift < SHIFT_INTERVAL_MS) return;

    // 4-corner cycle of 2-pixel shifts: no static pixel stays lit for more
    // than one 5-minute window.
    phase = (phase + 1) & 0x03;
    switch (phase) {
        default:
        case 0: g_shiftX = 0; g_shiftY = 0; break;
        case 1: g_shiftX = 2; g_shiftY = 0; break;
        case 2: g_shiftX = 2; g_shiftY = 2; break;
        case 3: g_shiftX = 0; g_shiftY = 2; break;
    }
    lastShift = millis();
}

// --- Bring-up --------------------------------------------------------------

void tftInit() {
    ER_TFT.Parallel_Init();
    ER_TFT.HW_Reset();
    ER_TFT.System_Check_Temp();
    delay(100);
    while (ER_TFT.LCD_StatusRead() & 0x02) { /* wait TASK_BUSY */ }
    ER_TFT.initial();
    ST7701S_Initial();
    ER_TFT.Display_ON();

    ER_TFT.Select_Main_Window_16bpp();
    ER_TFT.Main_Image_Start_Address(g_frontFb);
    ER_TFT.Main_Image_Width(LCD_XSIZE_TFT);
    ER_TFT.Main_Window_Start_XY(0, 0);

    // Rotated text for landscape mount. VDIR=1 (reg 0x12 bit 3) is the only
    // scan flip that's compatible with the SDRAM framebuffer. Combined with
    // physical 180° rotation of the display, text comes out legible.
    ER_TFT.Font_90_degree();
    ER_TFT.LCD_CmdWrite(0x12);
    uint8_t reg12 = ER_TFT.LCD_DataRead();
    reg12 |= cSetb3;
    ER_TFT.LCD_DataWrite(reg12);

    targetBackBuffer();
    fillScreenU(COL_BG);
    flipBuffers();
    fillScreenU(COL_BG);
}
