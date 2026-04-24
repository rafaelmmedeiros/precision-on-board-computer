#include "DutyScreen.h"
#include "Tft.h"

#include <Arduino.h>
#include <stdio.h>

void displayDuty() {
    // Triangle wave 0 → 95 % → 0 with a 2.5 s period.
    const uint32_t period = 2500;
    uint32_t t = millis() % period;
    float phase = (float)t / period;
    float duty = (phase < 0.5f) ? (phase * 2.0f) : (2.0f - phase * 2.0f);
    duty *= 95.0f;

    fillScreenU(COL_BG);
    drawCenteredU("DUTY BICOS", 20, 1, 2, COL_AMBER);

    char pctStr[8];
    snprintf(pctStr, sizeof(pctStr), "%d%%", (int)duty);
    uint16_t pctColor = (duty > 75.0f) ? COL_HOT : COL_AMBER;
    drawCenteredU(pctStr, 70, 2, 3, pctColor);

    const int barY = 210;
    const int barH = 70;
    const int barX = 60;
    const int barW = USR_W - 2 * barX;
    fillRectU(barX,            barY,            barW, 4,    COL_AMBER);
    fillRectU(barX,            barY + barH - 4, barW, 4,    COL_AMBER);
    fillRectU(barX,            barY,            4,    barH, COL_AMBER);
    fillRectU(barX + barW - 4, barY,            4,    barH, COL_AMBER);

    int fillW = (int)((barW - 12) * (duty / 100.0f));
    uint16_t fillColor = (duty > 75.0f) ? COL_HOT : COL_AMBER;
    fillRectU(barX + 6, barY + 6, fillW, barH - 12, fillColor);

    drawTextU(barX,             barY + barH + 20, 0, 1, COL_AMBER, "0");
    drawTextU(barX + barW - 24, barY + barH + 20, 0, 1, COL_AMBER, "100");
}
