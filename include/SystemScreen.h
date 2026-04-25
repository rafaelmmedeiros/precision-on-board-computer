#pragma once

#include "Screens.h"

// Main dashboard screen: date/time on the left, voltage + temperatures on the
// right, separated by a vertical divider. Every value except the clock is
// simulated so the UI exercises all color thresholds during bench demo.
void displaySystem();

ResetSet systemResets();
