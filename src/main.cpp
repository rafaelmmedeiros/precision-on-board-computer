#include <Arduino.h>
#include <WiFi.h>
#include <time.h>

#include "Tft.h"
#include "SystemScreen.h"
#include "DutyScreen.h"
#include "config.h"

#define NTP_SERVER          "pool.ntp.org"
#define GMT_OFFSET_SEC      (-3 * 3600)   // Brasília UTC-3
#define DAYLIGHT_OFFSET_SEC 0

static constexpr uint32_t SYSTEM_DWELL_MS = 3UL * 60UL * 1000UL;  // 3 min
static constexpr uint32_t DUTY_DWELL_MS   = 5UL * 1000UL;

// --- Boot messages ---------------------------------------------------------

static void showMessage(const char* msg) {
    targetBackBuffer();
    fillScreenU(COL_BG);
    drawCenteredU(msg, USR_H / 2 - 16, 1, 1, COL_AMBER);
    flipBuffers();
}

static void connectWiFi() {
    showMessage("Conectando WiFi...");
    Serial.print("Conectando ao WiFi");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println(" conectado!");
    showMessage("WiFi conectado!");
    delay(800);
}

static void syncTime() {
    showMessage("Sincronizando hora...");
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
    struct tm timeinfo;
    while (!getLocalTime(&timeinfo)) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("Hora sincronizada.");
}

// --- Arduino entry points --------------------------------------------------

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n[P-OBC] boot");

    tftInit();
    connectWiFi();
    syncTime();
}

void loop() {
    static uint8_t  screen = 0;
    static uint32_t lastSwitch = 0;
    static const uint32_t dwell[] = { SYSTEM_DWELL_MS, DUTY_DWELL_MS };

    targetBackBuffer();
    switch (screen) {
        case 0: displaySystem(); break;
        case 1: displayDuty();   break;
    }
    flipBuffers();

    if (millis() - lastSwitch >= dwell[screen]) {
        screen = (screen + 1) % 2;
        lastSwitch = millis();
    }

    tftTick();   // advance anti-image-sticking pixel shift
    delay(100);
}
