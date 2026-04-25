#pragma once

#include "Buttons.h"
#include "Screens.h"

// Contextual reset modal. Entered from any screen via hold-R-3s; lists 0..2
// reset options exposed by the origin screen, hold-R/S-3s confirms, taps
// cancel. Returns to the underlying screen automatically after confirmation
// or cancel.
void resetScreenEnter(const ResetSet& opts);
bool resetScreenActive();

// Drive the modal each frame. Pass the current button event (or BTN_EV_NONE)
// so the modal can react to taps/holds.
void resetScreenTick(ButtonEvent ev);

// Render the modal. Caller should still draw the underlying screen behind
// it on the same frame so transitions look smooth — but in practice the
// modal fills the screen, so drawing the underlying screen is optional.
void displayReset();
