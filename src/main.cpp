#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <SPI.h>

#include "LCD.h"
#include "LCD_init.h"   // contains ST7701S_Initial() — include in exactly one TU
#include "config.h"

extern "C" {
    uint8_t temprature_sens_read();
}

#define NTP_SERVER "pool.ntp.org"
#define GMT_OFFSET_SEC (-3 * 3600)  // Brasilia UTC-3
#define DAYLIGHT_OFFSET_SEC 0

// Panel physical: 320 wide x 960 tall (portrait).
// Mount rotated 90° so the long axis is horizontal: user sees 960x320 landscape.
// Font_90_degree() + axis swap translates user (ux, uy) → panel (panel_x=uy, panel_y=ux).
// Rectangles are symmetric under coord swap so DrawSquare_Fill args just swap XY roles.
static constexpr int USR_W = LCD_YSIZE_TFT;   // 960 horizontal (user)
static constexpr int USR_H = LCD_XSIZE_TFT;   // 320 vertical   (user)

// Double-buffer: two framebuffers (layer1 = 0, layer2 = 614400 from LCD.h).
// We draw into the back buffer, then flip Main_Image_Start_Address to show it.
static unsigned long g_frontFb = layer1_start_addr;
static unsigned long g_backFb  = layer2_start_addr;

// P-OBC UI palette (RGB565)
static constexpr uint16_t COL_BG    = Black;
static constexpr uint16_t COL_AMBER = 0xFD02;   // approx #FCA017
static constexpr uint16_t COL_WARN  = Red;

static char     g_city[32] = "---";
static float    g_weatherTemp = NAN;
static float    g_lat = 0.0f;
static float    g_lon = 0.0f;
static uint32_t g_lastWeatherFetch = 0;

// --- TFT helpers (user-space: 960x320 landscape) ---------------------------

// Font cells, in panel pixels. With Font_90_degree, text advances along panel Y
// (= user X). Cell width on the glyph side becomes the user-X size after rotation.
static int fontCellLong(uint8_t fontSel, uint8_t zoom) {
    // 8x16, 12x24, 16x32 → the "tall" side is what ends up along user-X when rotated
    int tall = (fontSel == 0) ? 16 : (fontSel == 1) ? 24 : 32;
    return tall * zoom;
}

static void selectFont(uint8_t fontSel, uint8_t zoom) {
    switch (fontSel) {
        case 0: ER_TFT.Font_Select_8x16_16x16();   break;
        case 1: ER_TFT.Font_Select_12x24_24x24();  break;
        default: ER_TFT.Font_Select_16x32_32x32(); break;
    }
    switch (zoom) {
        case 1: ER_TFT.Font_Width_X1(); ER_TFT.Font_Height_X1(); break;
        case 2: ER_TFT.Font_Width_X2(); ER_TFT.Font_Height_X2(); break;
        case 3: ER_TFT.Font_Width_X3(); ER_TFT.Font_Height_X3(); break;
        default: ER_TFT.Font_Width_X4(); ER_TFT.Font_Height_X4(); break;
    }
}

// Draw a filled rect in user coords (ux, uy, uw, uh). Axis swap → panel call.
static void fillRectU(int ux, int uy, int uw, int uh, uint16_t color) {
    ER_TFT.DrawSquare_Fill(uy, ux, uy + uh, ux + uw, color);
}

static void fillScreenU(uint16_t color) {
    fillRectU(0, 0, USR_W, USR_H, color);
}

// Draw text in user coords, top-left of the text cell at (ux, uy).
static void drawTextU(int ux, int uy, uint8_t fontSel, uint8_t zoom,
                      uint16_t color, const char* s) {
    ER_TFT.Foreground_color_65k(color);
    ER_TFT.Background_color_65k(COL_BG);
    ER_TFT.CGROM_Select_Internal_CGROM();
    selectFont(fontSel, zoom);
    // Panel coord: X = uy, Y = ux. Font_90 advances along panel Y = user X.
    ER_TFT.Goto_Text_XY(uy, ux);
    ER_TFT.Show_String((char*)s);
}

static void drawCenteredU(const char* s, int uy, uint8_t fontSel, uint8_t zoom,
                          uint16_t color) {
    int w = (int)strlen(s) * fontCellLong(fontSel, zoom);
    int ux = (USR_W - w) / 2;
    if (ux < 0) ux = 0;
    drawTextU(ux, uy, fontSel, zoom, color, s);
}

// Bind drawing target to the back buffer.
static void targetBackBuffer() {
    ER_TFT.Canvas_Image_Start_address(g_backFb);
    ER_TFT.Canvas_image_width(LCD_XSIZE_TFT);
    ER_TFT.Active_Window_XY(0, 0);
    ER_TFT.Active_Window_WH(LCD_XSIZE_TFT, LCD_YSIZE_TFT);
}

// Flip: show the back buffer, swap roles. Next draws land on the now-hidden frame.
static void flipBuffers() {
    ER_TFT.Main_Image_Start_Address(g_backFb);
    unsigned long tmp = g_frontFb;
    g_frontFb = g_backFb;
    g_backFb  = tmp;
    targetBackBuffer();
}

// --- Network / time -------------------------------------------------------

static void showMessage(const char* msg) {
    targetBackBuffer();
    fillScreenU(COL_BG);
    drawCenteredU(msg, USR_H / 2 - 16, 1, 1, COL_AMBER);
    flipBuffers();
}

void connectWiFi() {
    showMessage("Conectando WiFi...");
    Serial.print("Conectando ao WiFi");

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println(" conectado!");
    showMessage("WiFi conectado!");
    delay(1000);
}

void syncTime() {
    showMessage("Sincronizando hora...");
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);

    struct tm timeinfo;
    while (!getLocalTime(&timeinfo)) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("Hora sincronizada!");
}

bool fetchLocation() {
    HTTPClient http;
    http.begin("http://ip-api.com/json/?fields=status,city,lat,lon");
    int code = http.GET();
    if (code != 200) { http.end(); return false; }
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, http.getString());
    http.end();
    if (err || strcmp(doc["status"] | "", "success") != 0) return false;
    strlcpy(g_city, doc["city"] | "---", sizeof(g_city));
    g_lat = doc["lat"] | 0.0f;
    g_lon = doc["lon"] | 0.0f;
    Serial.printf("Localizacao: %s (%.4f, %.4f)\n", g_city, g_lat, g_lon);
    return true;
}

bool fetchWeather() {
    char url[160];
    snprintf(url, sizeof(url),
        "http://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f&current=temperature_2m",
        g_lat, g_lon);
    HTTPClient http;
    http.begin(url);
    int code = http.GET();
    if (code != 200) { http.end(); return false; }
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, http.getString());
    http.end();
    if (err) return false;
    g_weatherTemp = doc["current"]["temperature_2m"] | NAN;
    Serial.printf("Clima %s: %.1f C\n", g_city, g_weatherTemp);
    return true;
}

void updateWeather() {
    if (WiFi.status() != WL_CONNECTED) return;
    if (fetchWeather()) g_lastWeatherFetch = millis();
}

// --- Screens (user landscape 960x320) --------------------------------------

void displayDateTime() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) { showMessage("Erro ao ler hora"); return; }

    char dateStr[16], timeStr[16];
    strftime(dateStr, sizeof(dateStr), "%d/%m/%Y", &timeinfo);
    strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);

    fillScreenU(COL_BG);
    drawCenteredU("ASTRA",  30,  2, 2, COL_AMBER);   // 16x32 x2 = 32x64 glyphs
    fillRectU(100, 120, USR_W - 200, 3, COL_AMBER);  // separator
    drawCenteredU(timeStr, 150, 2, 2, COL_AMBER);    // HH:MM:SS big
    drawCenteredU(dateStr, 240, 1, 2, COL_AMBER);    // DD/MM/YYYY
}

void displayCpuTemp() {
    float tempC = (temprature_sens_read() - 32) / 1.8f;

    fillScreenU(COL_BG);
    drawCenteredU("CPU",  40,  2, 2, COL_AMBER);
    fillRectU(100, 130, USR_W - 200, 3, COL_AMBER);

    char tempStr[16];
    snprintf(tempStr, sizeof(tempStr), "%.1f C", tempC);
    drawCenteredU(tempStr, 160, 2, 3, COL_AMBER);    // 48x96 big
}

void displayWeather() {
    fillScreenU(COL_BG);
    drawCenteredU(g_city, 40, 1, 2, COL_AMBER);
    fillRectU(100, 120, USR_W - 200, 3, COL_AMBER);

    char tempStr[16];
    if (isnan(g_weatherTemp)) snprintf(tempStr, sizeof(tempStr), "--.- C");
    else                      snprintf(tempStr, sizeof(tempStr), "%.1f C", g_weatherTemp);
    drawCenteredU(tempStr, 150, 2, 3, COL_AMBER);
}

void displayDutyCycleDemo() {
    const uint32_t period = 2500;
    uint32_t t = millis() % period;
    float phase = (float)t / period;
    float duty = (phase < 0.5f) ? (phase * 2.0f) : (2.0f - phase * 2.0f);
    duty *= 95.0f;

    fillScreenU(COL_BG);
    drawCenteredU("DUTY BICOS", 20, 1, 2, COL_AMBER);

    char pctStr[8];
    snprintf(pctStr, sizeof(pctStr), "%d%%", (int)duty);
    uint16_t pctColor = (duty > 75.0f) ? COL_WARN : COL_AMBER;
    drawCenteredU(pctStr, 70, 2, 3, pctColor);        // 48x96 giant

    // Horizontal bar (landscape)
    const int barY = 210;
    const int barH = 70;
    const int barX = 60;
    const int barW = USR_W - 2 * barX;
    fillRectU(barX,            barY,            barW,    4,    COL_AMBER);
    fillRectU(barX,            barY + barH - 4, barW,    4,    COL_AMBER);
    fillRectU(barX,            barY,            4,       barH, COL_AMBER);
    fillRectU(barX + barW - 4, barY,            4,       barH, COL_AMBER);

    int fillW = (int)((barW - 12) * (duty / 100.0f));
    uint16_t fillColor = (duty > 75.0f) ? COL_WARN : COL_AMBER;
    fillRectU(barX + 6, barY + 6, fillW, barH - 12, fillColor);

    drawTextU(barX,                30 + 170, 0, 1, COL_AMBER, "0");
    drawTextU(barX + barW - 24,    30 + 170, 0, 1, COL_AMBER, "100");
}

// --- Arduino entry points --------------------------------------------------

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n[P-OBC] boot");

    // LT7680 bring-up (order from BuyDisplay demo)
    ER_TFT.Parallel_Init();
    ER_TFT.HW_Reset();
    ER_TFT.System_Check_Temp();
    delay(100);
    while (ER_TFT.LCD_StatusRead() & 0x02) { /* wait TASK_BUSY */ }
    ER_TFT.initial();
    ST7701S_Initial();
    ER_TFT.Display_ON();

    // Main window: shows front buffer.
    ER_TFT.Select_Main_Window_16bpp();
    ER_TFT.Main_Image_Start_Address(g_frontFb);
    ER_TFT.Main_Image_Width(LCD_XSIZE_TFT);
    ER_TFT.Main_Window_Start_XY(0, 0);

    // Rotated text for landscape mount.
    // Font_90_degree = "CCW 90° + horizontal flip" (per datasheet). The H-flip
    // makes glyphs read like "AMBULANCIA" in a rearview mirror. VDIR=1
    // (vertical scan bottom-to-top, reg 0x12 bit 3) converts the transpose
    // into a clean CW 90° rotation. The coord helpers compensate for the
    // resulting vertical flip by inverting uy.
    ER_TFT.Font_90_degree();
    // Match the HSCAN_L_to_R / VSCAN_T_to_B pattern exactly (no extra CmdWrite).
    ER_TFT.LCD_CmdWrite(0x12);
    uint8_t reg12 = ER_TFT.LCD_DataRead();
    reg12 |= cSetb3;                    // VDIR = 1 only
    ER_TFT.LCD_DataWrite(reg12);

    // Start drawing on back buffer.
    targetBackBuffer();
    fillScreenU(COL_BG);
    flipBuffers();
    fillScreenU(COL_BG);  // clear the other frame too

    connectWiFi();
    syncTime();

    showMessage("Buscando localizacao...");
    fetchLocation();
    showMessage("Buscando clima...");
    fetchWeather();
    g_lastWeatherFetch = millis();
}

void loop() {
    static uint8_t screen = 0;
    static uint32_t lastSwitch = 0;

    targetBackBuffer();
    switch (screen) {
        case 0: displayDateTime();       break;
        case 1: displayCpuTemp();        break;
        case 2: displayWeather();        break;
        case 3: displayDutyCycleDemo();  break;
    }
    flipBuffers();

    if (millis() - lastSwitch >= 5000) {
        screen = (screen + 1) % 4;
        lastSwitch = millis();
    }
    if (millis() - g_lastWeatherFetch >= 10UL * 60UL * 1000UL) updateWeather();

    delay(100);
}
