#include "TripSummaryScreen.h"
#include "Layout.h"
#include "Telemetry.h"
#include "PowerState.h"
#include "Features.h"
#include "Tft.h"
#include "BitmapFont.h"
#include "Fonts.h"

#include <Arduino.h>
#include <stdio.h>

using namespace pobc;

void displayTripSummary() {
    const float    tripKm  = telemetryTripKm();
    const float    tripL   = telemetryTripL();
    const uint32_t tripSec = telemetryTripSec();
    const float    tripAvg = (tripL > 0.0001f) ? (tripKm / tripL) : 0.0f;
    const int      tripH   = (int)(tripSec / 3600U);
    const int      tripM   = (int)((tripSec / 60U) % 60U);

    // Countdown: how many whole seconds remain in the post-trip window.
    const uint32_t total    = powerPostTripSummaryMs();
    const uint32_t elapsed  = powerStateElapsedMs();
    const uint32_t remainMs = (elapsed >= total) ? 0 : (total - elapsed);
    const int      remainSec = (int)((remainMs + 999UL) / 1000UL);

    fillScreenU(COL_BG);

    // Top label
    drawCenteredU("VIAGEM ENCERRADA", TOP_LABEL_Y, 1, 2, COL_AMBER);

    // --- Trip totals: km + km/l hero pair across the upper half -----------
    char buf[24];

    // KM hero on the left half
    snprintf(buf, sizeof(buf), "%d", (int)tripKm);
    {
        const int   w = measureBitmapText(DSEG7_120, buf);
        const int   x = (USR_W / 2 - w) / 2;
        drawBitmapText(x, 56, DSEG7_120, buf, COL_AMBER);
    }
    drawCenteredInU(0, USR_W / 2, 188, 1, 2, COL_AMBER, "KM");

    // km/L hero on the right half
    snprintf(buf, sizeof(buf), "%.1f", tripAvg);
    {
        const int   w = measureBitmapText(DSEG7_120, buf);
        const int   x = USR_W / 2 + (USR_W / 2 - w) / 2;
        drawBitmapText(x, 56, DSEG7_120, buf, COL_AMBER);
    }
    drawCenteredInU(USR_W / 2, USR_W, 188, 1, 2, COL_AMBER, "Km/l");

    // Vertical divider between the two heroes
    fillRectU(USR_W / 2 - 1, DIV_Y_TOP, 2, 188 - DIV_Y_TOP, COL_AMBER);

    // Horizontal divider above the footer block
    fillRectU(0, FOOTER_DIV, USR_W, 2, COL_AMBER);

    // --- Footer: duration on the left, countdown on the right ------------
    snprintf(buf, sizeof(buf), "DURACAO %d:%02d", tripH, tripM);
    drawTextU(20, FOOTER_Y, 1, 2, COL_AMBER, buf);

    snprintf(buf, sizeof(buf), "VOLTE EM %ds", remainSec);
    {
        const int textW = (int)strlen(buf) * fontCellShort(1, 2);
        drawTextU(USR_W - textW - 20, FOOTER_Y, 1, 2, COL_AMBER, buf);
    }

    // Countdown progress bar across the very bottom — same idiom the
    // hold-to-confirm UI uses, so the visual language stays coherent.
    const int barY = USR_H - 4;
    const int barW = (total > 0) ? (int)((uint64_t)USR_W * remainMs / total) : 0;
    if (barW > 0) fillRectU(0, barY, barW, 4, COL_AMBER);
}
