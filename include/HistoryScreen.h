#pragma once

#include "Screens.h"

// Trip history screen: bar chart of last N closed trips (oldest left,
// newest right), each bar's height = avg km/L, color = same green/amber/red
// thresholds as Consumption. Best-of-window bar gets a highlight border.
// Footer summarizes melhor + ultima.
//
// Reset on this screen: "Apagar historico" — wipes the NVS log.
void displayHistory();

ResetSet historyResets();
