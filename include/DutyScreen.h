#pragma once

// Injector duty-cycle demo: animated triangle-wave bar that hits 0..95 % on a
// 2.5 s period. Bar goes red above 75 %. Simulated for bench; in Phase 2 this
// will be driven by the real pulse-width interrupt handler.
void displayDuty();
