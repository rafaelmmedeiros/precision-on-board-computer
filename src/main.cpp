#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <time.h>
#include "config.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

#define NTP_SERVER "pool.ntp.org"
#define GMT_OFFSET_SEC (-3 * 3600) // Brasília UTC-3
#define DAYLIGHT_OFFSET_SEC 0

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

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

void setup() {
    Serial.begin(115200);

    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println("Falha ao inicializar o display SSD1306");
        while (true);
    }

    connectWiFi();
    syncTime();

    // Desconecta WiFi após sincronizar (economiza energia)
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    Serial.println("WiFi desligado");
}

void loop() {
    displayDateTime();
    delay(1000);
}
