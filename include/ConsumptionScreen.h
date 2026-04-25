#pragma once

#include "Screens.h"

// Fuel-economy screen: live km/L hero on the left, 12-bar history on the
// right (leftmost bar is the live instantaneous reading; the others are
// locked 5-min averages that shift right as time passes), trip summary
// footer across the bottom.
//
// Pure view over Telemetry — no simulation, no integration here.
void displayConsumption();

ResetSet consumptionResets();
