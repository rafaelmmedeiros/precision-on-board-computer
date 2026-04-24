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

// Panel is 320x960 portrait (LCD_XSIZE_TFT x LCD_YSIZE_TFT from LCD.h)
static constexpr int SCR_W = LCD_XSIZE_TFT;   // 320
static constexpr int SCR_H = LCD_YSIZE_TFT;   // 960

// P-OBC UI palette (RGB565)
static constexpr uint16_t COL_BG     = Black;
static constexpr uint16_t COL_AMBER  = 0xFD02;   // approx #FCA017
static constexpr uint16_t COL_WARN   = Red;

static char     g_city[32] = "---";
static float    g_weatherTemp = NAN;
static float    g_lat = 0.0f;
static float    g_lon = 0.0f;
static uint32_t g_lastWeatherFetch = 0;

// --- TFT helpers -----------------------------------------------------------

// Char cell width in pixels for the built-in CGROM fonts after Xn zoom.
// fontSel: 0=8x16, 1=12x24, 2=16x32  ; zoom: 1..4
static int cellW(uint8_t fontSel, uint8_t zoom) {
    int base = (fontSel == 0) ? 8 : (fontSel == 1) ? 12 : 16;
    return base * zoom;
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

static void drawCentered(const char* s, int y, uint8_t fontSel, uint8_t zoom, uint16_t color) {
    int w = (int)strlen(s) * cellW(fontSel, zoom);
    int x = (SCR_W - w) / 2;
    if (x < 0) x = 0;
    ER_TFT.Foreground_color_65k(color);
    ER_TFT.Background_color_65k(COL_BG);
    ER_TFT.CGROM_Select_Internal_CGROM();
    selectFont(fontSel, zoom);
    ER_TFT.Goto_Text_XY(x, y);
    ER_TFT.Show_String((char*)s);
}

static void fillScreen(uint16_t color) {
    ER_TFT.DrawSquare_Fill(0, 0, SCR_W, SCR_H, color);
}

static void selectMainCanvas() {
    ER_TFT.Select_Main_Window_16bpp();
    ER_TFT.Main_Image_Start_Address(layer1_start_addr);
    ER_TFT.Main_Image_Width(SCR_W);
    ER_TFT.Main_Window_Start_XY(0, 0);
    ER_TFT.Canvas_Image_Start_address(layer1_start_addr);
    ER_TFT.Canvas_image_width(SCR_W);
    ER_TFT.Active_Window_XY(0, 0);
    ER_TFT.Active_Window_WH(SCR_W, SCR_H);
}

// --- Network / time -------------------------------------------------------

void showMessage(const char* msg) {
    fillScreen(COL_BG);
    drawCentered(msg, SCR_H / 2 - 32, 2, 1, COL_AMBER);
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

// --- Screens ---------------------------------------------------------------

void displayDateTime() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) { showMessage("Erro ao ler hora"); return; }

    char dateStr[16], timeStr[16];
    strftime(dateStr, sizeof(dateStr), "%d/%m/%Y", &timeinfo);
    strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);

    fillScreen(COL_BG);
    drawCentered("ASTRA", 40, 2, 2, COL_AMBER);               // 32x64 chars
    ER_TFT.Foreground_color_65k(COL_AMBER);
    ER_TFT.DrawSquare_Fill(30, 180, SCR_W - 30, 184, COL_AMBER); // thin separator
    drawCentered(timeStr, 260, 2, 2, COL_AMBER);              // HH:MM:SS big
    drawCentered(dateStr, 400, 1, 2, COL_AMBER);              // DD/MM/YYYY
}

void displayCpuTemp() {
    float tempC = (temprature_sens_read() - 32) / 1.8f;

    fillScreen(COL_BG);
    drawCentered("CPU", 60, 2, 2, COL_AMBER);
    ER_TFT.DrawSquare_Fill(30, 200, SCR_W - 30, 204, COL_AMBER);

    char tempStr[16];
    snprintf(tempStr, sizeof(tempStr), "%.1f C", tempC);
    drawCentered(tempStr, 300, 2, 2, COL_AMBER);
}

void displayWeather() {
    fillScreen(COL_BG);
    drawCentered(g_city, 60, 1, 2, COL_AMBER);
    ER_TFT.DrawSquare_Fill(30, 180, SCR_W - 30, 184, COL_AMBER);

    char tempStr[16];
    if (isnan(g_weatherTemp)) snprintf(tempStr, sizeof(tempStr), "--.- C");
    else                      snprintf(tempStr, sizeof(tempStr), "%.1f C", g_weatherTemp);
    drawCentered(tempStr, 280, 2, 2, COL_AMBER);
}

void displayDutyCycleDemo() {
    const uint32_t period = 2500;
    uint32_t t = millis() % period;
    float phase = (float)t / period;
    float duty = (phase < 0.5f) ? (phase * 2.0f) : (2.0f - phase * 2.0f);
    duty *= 95.0f;

    fillScreen(COL_BG);
    drawCentered("DUTY BICOS", 60, 1, 2, COL_AMBER);

    char pctStr[8];
    snprintf(pctStr, sizeof(pctStr), "%d%%", (int)duty);
    uint16_t pctColor = (duty > 75.0f) ? COL_WARN : COL_AMBER;
    drawCentered(pctStr, 180, 2, 3, pctColor);              // 48x96 giant

    // Vertical bar (makes sense on a 320x960 portrait bar display)
    const int barX = 100;
    const int barW = SCR_W - 2 * barX;
    const int barY = 400;
    const int barH = 480;
    ER_TFT.Foreground_color_65k(COL_AMBER);
    // frame as 4 thin rectangles (no non-fill helper here)
    ER_TFT.DrawSquare_Fill(barX,            barY,            barX + barW,     barY + 4,        COL_AMBER);
    ER_TFT.DrawSquare_Fill(barX,            barY + barH - 4, barX + barW,     barY + barH,     COL_AMBER);
    ER_TFT.DrawSquare_Fill(barX,            barY,            barX + 4,        barY + barH,     COL_AMBER);
    ER_TFT.DrawSquare_Fill(barX + barW - 4, barY,            barX + barW,     barY + barH,     COL_AMBER);

    int fillH = (int)((barH - 12) * (duty / 100.0f));
    uint16_t fillColor = (duty > 75.0f) ? COL_WARN : COL_AMBER;
    // fill grows from the bottom up
    ER_TFT.DrawSquare_Fill(barX + 6, barY + barH - 6 - fillH,
                           barX + barW - 6, barY + barH - 6, fillColor);

    drawCentered("0",   barY + barH + 20, 0, 2, COL_AMBER);
    drawCentered("100", barY - 50,        0, 2, COL_AMBER);
}

// --- Arduino entry points --------------------------------------------------

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n[P-OBC] boot");

    // LT7680 bring-up (order from BuyDisplay demo)
    ER_TFT.Parallel_Init();          // SPI pins
    ER_TFT.HW_Reset();
    ER_TFT.System_Check_Temp();
    delay(100);
    while (ER_TFT.LCD_StatusRead() & 0x02) { /* wait TASK_BUSY */ }
    ER_TFT.initial();                // LT7680 PLL/SDRAM/TCON
    ST7701S_Initial();               // ST7701S panel config (SW-SPI on GPIO 17/14/4)
    ER_TFT.Display_ON();

    selectMainCanvas();
    fillScreen(COL_BG);

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

    switch (screen) {
        case 0: displayDateTime();       break;
        case 1: displayCpuTemp();        break;
        case 2: displayWeather();        break;
        case 3: displayDutyCycleDemo();  break;
    }

    if (millis() - lastSwitch >= 5000) {
        screen = (screen + 1) % 4;
        lastSwitch = millis();
    }
    if (millis() - g_lastWeatherFetch >= 10UL * 60UL * 1000UL) updateWeather();

    delay(100);
}
