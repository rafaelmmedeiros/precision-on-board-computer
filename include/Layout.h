#pragma once

// Shared layout constants for the screen views. Keeps every screen's
// title row, vertical divider, footer divider, and footer text baseline
// on the same coordinates so the UI looks coherent as new screens land.
//
// Coordinate system: user-space (Tft.h) with USR_W=960 (long axis) and
// USR_H=320 (short axis), origin top-left.
//
// SystemScreen uses its own 50/50 split (DIV_X=478) so it does not pull
// from the LEFT_W column here — only the title/footer rails apply.

namespace pobc {

// --- Title and footer rails (used by every screen) ------------------------

constexpr int TOP_LABEL_Y = 8;     // baseline Y for top-row labels
constexpr int FOOTER_DIV  = 252;   // Y of the horizontal footer divider line
constexpr int FOOTER_Y    = 260;   // baseline Y for footer text

// --- Two-pane split (Consumption, Autonomy) -------------------------------
//
// LEFT block: hero column on the left, RIGHT_X..USR_W is the data column.
// The vertical divider runs from DIV_Y_TOP down to FOOTER_DIV.

constexpr int LEFT_W    = 340;
constexpr int DIV_X     = LEFT_W;
constexpr int DIV_W     = 4;
constexpr int RIGHT_X   = DIV_X + DIV_W;   // = 344
constexpr int DIV_Y_TOP = 16;              // top inset for the vertical divider

} // namespace pobc
