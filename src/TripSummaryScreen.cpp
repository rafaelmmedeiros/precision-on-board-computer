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

// Three equal columns: DISTANCIA | MEDIA | TEMPO. Vertical dividers are
// 4 px wide and the column widths are picked so the right-most column
// reaches the right edge cleanly.
static constexpr int DIV_W_LOCAL = 4;
static constexpr int COL_W       = (USR_W - 2 * DIV_W_LOCAL) / 3;
static constexpr int DIV1_X      = COL_W;
static constexpr int COL2_X      = DIV1_X + DIV_W_LOCAL;
static constexpr int DIV2_X      = COL2_X + COL_W;
static constexpr int COL3_X      = DIV2_X + DIV_W_LOCAL;

static constexpr int CONTENT_TOP    = 56;   // below the title
static constexpr int HERO_Y         = 80;   // numeric hero top edge
static constexpr int UNIT_LABEL_Y   = 168;  // unit label below hero

void displayTripSummary() {
    const float    tripKm  = telemetryTripKm();
    const float    tripL   = telemetryTripL();
    const uint32_t tripSec = telemetryTripSec();
    const float    tripAvg = (tripL > 0.0001f) ? (tripKm / tripL) : 0.0f;
    const int      tripH   = (int)(tripSec / 3600U);
    const int      tripM   = (int)((tripSec / 60U) % 60U);

    // Countdown — only used for the slim progress bar at the very bottom
    // edge. The big "VOLTE EM Ns" callout is gone: it confused the meaning
    // of the timer (it's how long until the screen turns off, not until
    // anything happens to the trip itself).
    const uint32_t total    = powerPostTripSummaryMs();
    const uint32_t elapsed  = powerStateElapsedMs();
    const uint32_t remainMs = (elapsed >= total) ? 0 : (total - elapsed);

    fillScreenU(COL_BG);

    // --- Title ------------------------------------------------------------
    drawCenteredU("VIAGEM ENCERRADA", TOP_LABEL_Y, 1, 2, COL_AMBER);

    // --- Vertical dividers (start below the title to avoid overlap) ------
    fillRectU(DIV1_X, CONTENT_TOP, DIV_W_LOCAL, FOOTER_DIV - CONTENT_TOP, COL_AMBER);
    fillRectU(DIV2_X, CONTENT_TOP, DIV_W_LOCAL, FOOTER_DIV - CONTENT_TOP, COL_AMBER);

    // The unit label below each hero is enough to identify the column —
    // a separate "DISTANCIA / MEDIA / TEMPO" header above would just
    // duplicate that information. Heroes sit higher in the column so the
    // unit doesn't crowd the footer divider.

    // --- Column 1: Distance ----------------------------------------------
    {
        char buf[12];
        snprintf(buf, sizeof(buf), "%d", (int)tripKm);
        drawBitmapTextCenteredIn(0, DIV1_X, HERO_Y, DSEG7_72, buf, COL_AMBER);
    }
    drawCenteredInU(0, DIV1_X, UNIT_LABEL_Y, 1, 2, COL_AMBER, "KM");

    // --- Column 2: Average km/L (with semantic color) --------------------
    {
        char buf[12];
        snprintf(buf, sizeof(buf), "%.1f", tripAvg);
        const uint16_t c = (tripAvg > 0.0f) ? consumptionColor(tripAvg) : COL_AMBER;
        drawBitmapTextCenteredIn(COL2_X, DIV2_X, HERO_Y, DSEG7_72, buf, c);
    }
    drawCenteredInU(COL2_X, DIV2_X, UNIT_LABEL_Y, 1, 2, COL_AMBER, "Km/l");

    // --- Column 3: Time ---------------------------------------------------
    {
        char buf[12];
        snprintf(buf, sizeof(buf), "%d:%02d", tripH, tripM);
        drawBitmapTextCenteredIn(COL3_X, USR_W, HERO_Y, DSEG7_72, buf, COL_AMBER);
    }
    drawCenteredInU(COL3_X, USR_W, UNIT_LABEL_Y, 1, 2, COL_AMBER, "h:mm");

    // --- Footer divider + grace-policy hint ------------------------------
    fillRectU(0, FOOTER_DIV, USR_W, 2, COL_AMBER);
    drawCenteredU("RETORNO EM 1H MANTEM ESTA VIAGEM",
                  FOOTER_Y, 0, 2, COL_AMBER);

    // --- Countdown progress bar at the bottom edge -----------------------
    // Same idiom as the hold-to-confirm bar — visual language stays
    // coherent. Indicates how much time is left before the display turns
    // off (i.e. before light sleep), not anything trip-related.
    const int barY = USR_H - 4;
    const int barW = (total > 0)
                     ? (int)((uint64_t)USR_W * remainMs / total)
                     : 0;
    if (barW > 0) fillRectU(0, barY, barW, 4, COL_AMBER);
}
