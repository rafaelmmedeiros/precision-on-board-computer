#pragma once

#include "Screens.h"

// GPS screen: altitude hero on the left, grade / speed / heading on the right,
// fix quality in the footer. Reads GpsModule only — no vehicle telemetry.
void displayGps();

ResetSet gpsResets();
