#pragma once

// Trip summary view shown during PowerState::POST_TRIP_SUMMARY.
//
// Renders one screenful: trip totals (duration, km, average km/L) plus a
// large countdown indicating how long the driver has to come back before
// the trip is closed. Outside POST_TRIP_SUMMARY this view is never drawn,
// so it is intentionally not part of the SCREENS[] navigation rotation.
//
// Pure render — reads telemetry getters and the elapsed-ms in the current
// power state. main.cpp routes button taps into PowerState::powerSkipPostTripSummary().

void displayTripSummary();
