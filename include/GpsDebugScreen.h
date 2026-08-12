#pragma once

#include "Screens.h"

// GPS debug screen — first of the debug family, revealed by the unlock gesture
// (both buttons ~1.2 s) when POBC_DEBUG_MODE is on.
//
// Exists because the UBX configuration handshake happens in the car, on the
// ignition turn, with no serial monitor attached. It answers three questions
// that were previously only visible over USB:
//   1. Did the config apply? (want vs got, per setting, read from the receiver)
//   2. Is the link healthy? (baud, byte counters, checksum failures, ACK/NAK)
//   3. Why is the grade showing "--"? (which gate is blocking)
void displayGpsDebug();

ResetSet gpsDebugResets();
