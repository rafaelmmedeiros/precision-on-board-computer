#include "Tft.h"
#include "LCD_init.h"   // ST7701S_Initial() — only included here (single TU)

// --- Double buffer state ---------------------------------------------------

static unsigned long g_frontFb = layer1_start_addr;
static unsigned long g_backFb  = layer2_start_addr;

// --- Font helpers ----------------------------------------------------------

int fontCellLong(uint8_t fontSel, uint8_t zoom) {
    int tall = (fontSel == 0) ? 16 : (fontSel == 1) ? 24 : 32;
    return tall * zoom;
}

int fontCellShort(uint8_t fontSel, uint8_t zoom) {
    int wide = (fontSel == 0) ? 8 : (fontSel == 1) ? 12 : 16;
    return wide * zoom;
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
    ER_TFT.DrawSquare_Fill(uy, ux, uy + uh, ux + uw, color);
}

void fillScreenU(uint16_t color) {
    fillRectU(0, 0, USR_W, USR_H, color);
}

void drawTextU(int ux, int uy, uint8_t fontSel, uint8_t zoom,
               uint16_t color, const char* s) {
    ER_TFT.Foreground_color_65k(color);
    ER_TFT.Background_color_65k(COL_BG);
    ER_TFT.CGROM_Select_Internal_CGROM();
    selectFont(fontSel, zoom);
    ER_TFT.Goto_Text_XY(uy, ux);
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
