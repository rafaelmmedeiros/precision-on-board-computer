#pragma once

#include "Screens.h"

// Range / autonomy screen: estimated km until empty as a confidence
// interval (left hero), tank fill gauge (right), MED/MIN/MAX km in the
// footer. The "± X km" and worst/best-case range numbers are the angle
// against OEM screens that lie with single-precision figures.
//
// Phase 1 (bench): tank level is simulated. Phase 2 reads the boia on
// GPIO 33 (already in pinmap). Average and stddev come from the same
// km/L history that ConsumptionScreen already maintains.
void displayAutonomy();

ResetSet autonomyResets();
