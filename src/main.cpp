#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h>
#include <time.h>
#include "config.h"

extern "C" {
    uint8_t temprature_sens_read();
}

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

#define NTP_SERVER "pool.ntp.org"
#define GMT_OFFSET_SEC (-3 * 3600) // Brasília UTC-3
#define DAYLIGHT_OFFSET_SEC 0

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

static char g_city[32] = "---";
static float g_weatherTemp = NAN;
static float g_lat = 0.0f;
static float g_lon = 0.0f;
static uint32_t g_lastWeatherFetch = 0;

void showMessage(const char* msg) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 28);
    display.println(msg);
    display.display();
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

void displayDateTime() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        showMessage("Erro ao ler hora");
        return;
    }

    char dateStr[16];
    char timeStr[16];
    strftime(dateStr, sizeof(dateStr), "%d/%m/%Y", &timeinfo);
    strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);

    display.clearDisplay();

    // "ASTRA" no topo
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds("ASTRA", 0, 0, &x1, &y1, &w, &h);
    display.setCursor((SCREEN_WIDTH - w) / 2, 2);
    display.print("ASTRA");

    // Linha separadora
    display.drawLine(0, 22, SCREEN_WIDTH, 22, SSD1306_WHITE);

    // Data centralizada
    display.setTextSize(1);
    display.getTextBounds(dateStr, 0, 0, &x1, &y1, &w, &h);
    display.setCursor((SCREEN_WIDTH - w) / 2, 30);
    display.print(dateStr);

    // Hora centralizada e maior
    display.setTextSize(2);
    display.getTextBounds(timeStr, 0, 0, &x1, &y1, &w, &h);
    display.setCursor((SCREEN_WIDTH - w) / 2, 44);
    display.print(timeStr);

    display.display();
}

bool fetchLocation() {
    HTTPClient http;
    http.begin("http://ip-api.com/json/?fields=status,city,lat,lon");
    int code = http.GET();
    if (code != 200) {
        Serial.printf("ip-api falhou: %d\n", code);
        http.end();
        return false;
    }
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, http.getString());
    http.end();
    if (err || strcmp(doc["status"] | "", "success") != 0) {
        Serial.println("ip-api parse falhou");
        return false;
    }
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
    if (code != 200) {
        Serial.printf("open-meteo falhou: %d\n", code);
        http.end();
        return false;
    }
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, http.getString());
    http.end();
    if (err) {
        Serial.println("open-meteo parse falhou");
        return false;
    }
    g_weatherTemp = doc["current"]["temperature_2m"] | NAN;
    Serial.printf("Clima %s: %.1f C\n", g_city, g_weatherTemp);
    return true;
}

void updateWeather() {
    if (WiFi.status() != WL_CONNECTED) return;
    if (fetchWeather()) {
        g_lastWeatherFetch = millis();
    }
}

void displayWeather() {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);

    display.setTextSize(1);
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(g_city, 0, 0, &x1, &y1, &w, &h);
    display.setCursor((SCREEN_WIDTH - w) / 2, 4);
    display.print(g_city);

    display.drawLine(0, 22, SCREEN_WIDTH, 22, SSD1306_WHITE);

    char tempStr[16];
    if (isnan(g_weatherTemp)) {
        snprintf(tempStr, sizeof(tempStr), "--.- C");
    } else {
        snprintf(tempStr, sizeof(tempStr), "%.1f C", g_weatherTemp);
    }

    display.setTextSize(3);
    display.getTextBounds(tempStr, 0, 0, &x1, &y1, &w, &h);
    display.setCursor((SCREEN_WIDTH - w) / 2, 34);
    display.print(tempStr);

    display.display();
}

void displayDutyCycleDemo() {
    // Demo: onda triangular 0-95% com período de 2.5s
    const uint32_t period = 2500;
    uint32_t t = millis() % period;
    float phase = (float)t / period;
    float duty = (phase < 0.5f) ? (phase * 2.0f) : (2.0f - phase * 2.0f);
    duty *= 95.0f;

    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);

    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("DUTY BICOS");

    char pctStr[8];
    snprintf(pctStr, sizeof(pctStr), "%2d%%", (int)duty);
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(pctStr, 0, 0, &x1, &y1, &w, &h);
    display.setCursor(SCREEN_WIDTH - w, 0);
    display.print(pctStr);

    const int16_t barX = 4;
    const int16_t barY = 24;
    const int16_t barW = SCREEN_WIDTH - 8;
    const int16_t barH = 20;
    display.drawRect(barX, barY, barW, barH, SSD1306_WHITE);
    int16_t fillW = (int16_t)((barW - 4) * (duty / 100.0f));
    display.fillRect(barX + 2, barY + 2, fillW, barH - 4, SSD1306_WHITE);

    display.setCursor(barX, barY + barH + 6);
    display.print("0");
    display.setCursor(barX + barW - 18, barY + barH + 6);
    display.print("100");

    display.display();
}

void displayCpuTemp() {
    float tempC = (temprature_sens_read() - 32) / 1.8f;

    display.clearDisplay();

    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds("CPU", 0, 0, &x1, &y1, &w, &h);
    display.setCursor((SCREEN_WIDTH - w) / 2, 2);
    display.print("CPU");

    display.drawLine(0, 22, SCREEN_WIDTH, 22, SSD1306_WHITE);

    char tempStr[16];
    snprintf(tempStr, sizeof(tempStr), "%.1f C", tempC);

    display.setTextSize(3);
    display.getTextBounds(tempStr, 0, 0, &x1, &y1, &w, &h);
    display.setCursor((SCREEN_WIDTH - w) / 2, 34);
    display.print(tempStr);

    display.display();
}

void setup() {
    Serial.begin(115200);

    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println("Falha ao inicializar o display SSD1306");
        while (true);
    }

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
        case 0: displayDateTime(); break;
        case 1: displayCpuTemp(); break;
        case 2: displayWeather(); break;
        case 3: displayDutyCycleDemo(); break;
    }

    if (millis() - lastSwitch >= 5000) {
        screen = (screen + 1) % 4;
        lastSwitch = millis();
    }

    if (millis() - g_lastWeatherFetch >= 10UL * 60UL * 1000UL) {
        updateWeather();
    }

    delay(100);
}
