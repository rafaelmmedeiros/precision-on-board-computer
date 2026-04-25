#pragma once

// Fuel-economy screen: live km/L hero on the left, 12-bar history on the
// right (leftmost bar is the live instantaneous reading; the others are
// locked 5-min averages that shift right as time passes), trip summary
// footer across the bottom.
//
// Phase 1 (bench): all values are simulated. Phase 2 swaps simInstant()
// for a value derived from the injector pulse-width interrupt.
void displayConsumption();
