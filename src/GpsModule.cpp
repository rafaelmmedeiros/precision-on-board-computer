#include "GpsModule.h"
#include "Features.h"

#include <TinyGPS++.h>
#include <math.h>
#include <string.h>

using namespace pobc;

// === Wiring ================================================================

static constexpr int8_t  PIN_GPS_RX = 39;   // GPS TX -> ESP32 (input-only pin)
static constexpr int8_t  PIN_GPS_TX = 15;   // ESP32 -> GPS RX

// Probe order matters: the configured rate is tried first so a warm module
// locks on immediately. A factory-fresh (or reverted) module answers on 9600.
static constexpr uint32_t BAUD_TARGET = 38400;
static constexpr uint32_t BAUD_STOCK  =  9600;
static const uint32_t     PROBE_BAUDS[] = { BAUD_TARGET, BAUD_STOCK };

// RX buffer must be generous: the loop paces at 100 ms and a 5 Hz navigation
// cycle can bunch up if a frame runs long. The 256-byte default is too small.
static constexpr size_t RX_BUFFER = 1024;

// === Timings ===============================================================

static constexpr uint32_t PROBE_WINDOW_MS   = 1200;  // per baud candidate
static constexpr uint32_t ABSENT_RETRY_MS   = 3000;  // re-probe cadence
static constexpr uint32_t POLL_TIMEOUT_MS   =  400;
static constexpr uint32_t STARVE_TIMEOUT_MS = 5000;  // READY -> ABSENT
static constexpr uint8_t  POLL_RETRIES      =    2;
static constexpr uint8_t  MAX_VERIFY_PASSES =    2;  // one repair attempt
static constexpr uint8_t  MAX_PRT_FAILS     =    3;  // then accept 9600

// === Grade derivation ======================================================

static constexpr float GRADE_BASELINE_M = 200.0f;  // distance between samples
static constexpr float GRADE_TAU_SEC    =   8.0f;  // altitude EMA time constant
static constexpr float GRADE_MAX_PCT    =  25.0f;  // beyond this it is noise
static constexpr float GRADE_MIN_KMH    =  20.0f;  // speed gate
static constexpr float GRADE_MAX_HDOP   =   2.0f;  // fix-quality gate

// === UBX protocol ==========================================================

static constexpr uint8_t UBX_SYNC1 = 0xB5;
static constexpr uint8_t UBX_SYNC2 = 0x62;

static constexpr uint8_t CLS_NAV_ACK = 0x05;
static constexpr uint8_t ID_ACK_NAK  = 0x00;
static constexpr uint8_t ID_ACK_ACK  = 0x01;

static constexpr uint8_t CLS_CFG  = 0x06;
static constexpr uint8_t ID_PRT   = 0x00;
static constexpr uint8_t ID_MSG   = 0x01;
static constexpr uint8_t ID_RATE  = 0x08;
static constexpr uint8_t ID_CFG   = 0x09;
static constexpr uint8_t ID_NAV5  = 0x24;

static constexpr uint8_t NMEA_CLASS = 0xF0;

// Desired NMEA sentence rates. TinyGPSPlus only consumes GGA and RMC, so
// everything else is switched off — GSV in particular is the fattest sentence
// of the set and would burn UART bandwidth for nothing.
struct NmeaRate { uint8_t id; uint8_t rate; };
static constexpr NmeaRate NMEA_RATES[] = {
    { 0x00, 1 },   // GGA — position, altitude, satellites, HDOP
    { 0x01, 0 },   // GLL
    { 0x02, 0 },   // GSA
    { 0x03, 0 },   // GSV
    { 0x04, 1 },   // RMC — speed, course, date/time, validity
    { 0x05, 0 },   // VTG
};
static constexpr uint8_t NMEA_RATE_N = sizeof(NMEA_RATES) / sizeof(NMEA_RATES[0]);

static constexpr uint16_t NAV_MEAS_RATE_MS = 200;   // 5 Hz
static constexpr uint8_t  DYN_MODEL_AUTO   = 4;     // CFG-NAV5 "Automotive"

// Verification steps, in order. NAV5/RATE/MSG are applied at the current baud;
// PRT goes last because it changes the rate out from under us.
enum : uint8_t {
    STEP_NAV5 = 0,
    STEP_RATE,
    STEP_MSG_FIRST,
    STEP_PRT = STEP_MSG_FIRST + NMEA_RATE_N,
    STEP_COUNT,
};

// === State =================================================================

static TinyGPSPlus g_gps;
static GpsLink     g_link      = GpsLink::ABSENT;
static uint32_t    g_stateMs   = 0;
static uint32_t    g_baud      = 0;
static uint8_t     g_probeIdx  = 0;

// Verify/configure walk
static uint8_t  g_step        = 0;
static uint16_t g_stepBad     = 0;   // bitmask of steps whose poll mismatched
static uint8_t  g_stepRetry   = 0;
static bool     g_pollPending = false;
static uint32_t g_pollMs      = 0;
static uint8_t  g_verifyPass  = 0;
static uint8_t  g_prtFails    = 0;

// UBX receive frame
static uint8_t  g_ubxState   = 0;
static uint8_t  g_ubxCls     = 0;
static uint8_t  g_ubxId      = 0;
static uint16_t g_ubxLen     = 0;
static uint16_t g_ubxIdx     = 0;
static uint8_t  g_ubxCkA     = 0;
static uint8_t  g_ubxCkB     = 0;
static uint8_t  g_ubxBuf[64];
static bool     g_respHave   = false;
static uint8_t  g_respCls    = 0;
static uint8_t  g_respId     = 0;
static uint16_t g_respLen    = 0;
static uint8_t  g_respBuf[64];

// Cached readings
static bool     g_locValid   = false;
static double   g_lat        = 0.0;
static double   g_lon        = 0.0;
static float    g_altM       = 0.0f;
static float    g_speedKmh   = 0.0f;
static float    g_courseDeg  = 0.0f;
static float    g_hdop       = 99.9f;
static uint8_t  g_sats       = 0;
static uint32_t g_lastLocMs  = 0;
static uint32_t g_lastNmeaMs = 0;

// Grade derivation
static float    g_altEma      = NAN;
static uint32_t g_altEmaMs    = 0;
static bool     g_anchorValid = false;
static double   g_anchorLat   = 0.0;
static double   g_anchorLon   = 0.0;
static float    g_anchorAlt   = 0.0f;
static float    g_gradePct    = NAN;

// Diagnostics
static uint32_t g_ackCount   = 0;
static uint32_t g_nakCount   = 0;
static uint32_t g_configRuns = 0;

// === UBX helpers ===========================================================

static void ubxSend(uint8_t cls, uint8_t id, const uint8_t* payload, uint16_t len) {
    uint8_t hdr[6] = { UBX_SYNC1, UBX_SYNC2, cls, id,
                       (uint8_t)(len & 0xFF), (uint8_t)(len >> 8) };
    uint8_t a = 0, b = 0;
    for (uint8_t i = 2; i < 6; ++i) { a = (uint8_t)(a + hdr[i]); b = (uint8_t)(b + a); }
    for (uint16_t i = 0; i < len; ++i) { a = (uint8_t)(a + payload[i]); b = (uint8_t)(b + a); }
    Serial1.write(hdr, 6);
    if (len && payload) Serial1.write(payload, len);
    Serial1.write(a);
    Serial1.write(b);
}

static void ubxAccum(uint8_t c) {
    g_ubxCkA = (uint8_t)(g_ubxCkA + c);
    g_ubxCkB = (uint8_t)(g_ubxCkB + g_ubxCkA);
}

// Feed one byte. UBX frames are extracted transparently; everything else goes
// to the NMEA parser. A lone 0xB5 that is not followed by 0x62 is handed back
// to TinyGPS so a truncated sync cannot eat a sentence.
static void feedByte(uint8_t c) {
    switch (g_ubxState) {
    case 0:
        if (c == UBX_SYNC1) { g_ubxState = 1; return; }
        g_gps.encode((char)c);
        return;

    case 1:
        if (c == UBX_SYNC2) { g_ubxState = 2; return; }
        g_ubxState = 0;
        g_gps.encode((char)UBX_SYNC1);
        g_gps.encode((char)c);
        return;

    case 2:                                  // class — first checksummed byte
        g_ubxCls = c;
        g_ubxCkA = c;
        g_ubxCkB = c;
        g_ubxState = 3;
        return;

    case 3:                                  // id
        g_ubxId = c;
        ubxAccum(c);
        g_ubxState = 4;
        return;

    case 4:                                  // length, low byte
        g_ubxLen = c;
        ubxAccum(c);
        g_ubxState = 5;
        return;

    case 5:                                  // length, high byte
        g_ubxLen = (uint16_t)(g_ubxLen | ((uint16_t)c << 8));
        ubxAccum(c);
        g_ubxIdx = 0;
        g_ubxState = g_ubxLen ? 6 : 7;
        return;

    case 6:                                  // payload (oversized frames clip)
        if (g_ubxIdx < sizeof(g_ubxBuf)) g_ubxBuf[g_ubxIdx] = c;
        ++g_ubxIdx;
        ubxAccum(c);
        if (g_ubxIdx >= g_ubxLen) g_ubxState = 7;
        return;

    case 7:                                  // CK_A
        g_ubxState = (c == g_ubxCkA) ? 8 : 0;
        return;

    case 8:                                  // CK_B — frame complete
        g_ubxState = 0;
        if (c != g_ubxCkB) return;
        if (g_ubxCls == CLS_NAV_ACK) {
            if (g_ubxId == ID_ACK_ACK) ++g_ackCount;
            if (g_ubxId == ID_ACK_NAK) ++g_nakCount;
            return;
        }
        if (g_ubxLen > sizeof(g_respBuf)) return;   // not a config reply we poll
        g_respCls = g_ubxCls;
        g_respId  = g_ubxId;
        g_respLen = g_ubxLen;
        memcpy(g_respBuf, g_ubxBuf, g_ubxLen);
        g_respHave = true;
        return;

    default:
        g_ubxState = 0;
        return;
    }
}

static uint32_t le32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint16_t le16(const uint8_t* p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

// === Config frames =========================================================

static void pollStep(uint8_t step) {
    if (step == STEP_NAV5) { ubxSend(CLS_CFG, ID_NAV5, nullptr, 0); return; }
    if (step == STEP_RATE) { ubxSend(CLS_CFG, ID_RATE, nullptr, 0); return; }
    if (step == STEP_PRT)  { const uint8_t p[1] = { 1 };
                             ubxSend(CLS_CFG, ID_PRT, p, 1); return; }
    const uint8_t i = (uint8_t)(step - STEP_MSG_FIRST);
    const uint8_t p[2] = { NMEA_CLASS, NMEA_RATES[i].id };
    ubxSend(CLS_CFG, ID_MSG, p, 2);
}

// True when the polled response matches what we want.
static bool stepMatches(uint8_t step, uint8_t cls, uint8_t id,
                        const uint8_t* p, uint16_t len) {
    if (cls != CLS_CFG) return false;
    if (step == STEP_NAV5) return id == ID_NAV5 && len == 36 && p[2] == DYN_MODEL_AUTO;
    if (step == STEP_RATE) return id == ID_RATE && len == 6 && le16(p) == NAV_MEAS_RATE_MS;
    if (step == STEP_PRT)  return id == ID_PRT  && len == 20 && le32(p + 8) == BAUD_TARGET;
    const uint8_t i = (uint8_t)(step - STEP_MSG_FIRST);
    // CFG-MSG poll answers with the rate for all six ports; UART1 is index 1.
    return id == ID_MSG && len == 8 &&
           p[0] == NMEA_CLASS && p[1] == NMEA_RATES[i].id &&
           p[3] == NMEA_RATES[i].rate;
}

static void applyStep(uint8_t step) {
    if (step == STEP_NAV5) {
        uint8_t p[36] = { 0 };
        p[0] = 0x01;               // mask: apply dynModel only
        p[2] = DYN_MODEL_AUTO;
        ubxSend(CLS_CFG, ID_NAV5, p, sizeof(p));
        return;
    }
    if (step == STEP_RATE) {
        const uint8_t p[6] = { (uint8_t)(NAV_MEAS_RATE_MS & 0xFF),
                               (uint8_t)(NAV_MEAS_RATE_MS >> 8),
                               0x01, 0x00,    // navRate: every measurement
                               0x01, 0x00 };  // timeRef: GPS time
        ubxSend(CLS_CFG, ID_RATE, p, sizeof(p));
        return;
    }
    if (step == STEP_PRT) {
        uint8_t p[20] = { 0 };
        p[0]  = 1;                 // portID: UART1
        p[4]  = 0xD0; p[5] = 0x08; // mode: 8N1
        p[8]  = (uint8_t)(BAUD_TARGET & 0xFF);
        p[9]  = (uint8_t)((BAUD_TARGET >> 8) & 0xFF);
        p[10] = (uint8_t)((BAUD_TARGET >> 16) & 0xFF);
        p[11] = (uint8_t)((BAUD_TARGET >> 24) & 0xFF);
        p[12] = 0x03;              // inProtoMask: UBX + NMEA
        p[14] = 0x03;              // outProtoMask: UBX + NMEA
        ubxSend(CLS_CFG, ID_PRT, p, sizeof(p));
        return;
    }
    const uint8_t i = (uint8_t)(step - STEP_MSG_FIRST);
    const uint8_t p[3] = { NMEA_CLASS, NMEA_RATES[i].id, NMEA_RATES[i].rate };
    ubxSend(CLS_CFG, ID_MSG, p, sizeof(p));
}

// Persist to every available non-volatile store, so the module keeps its
// configuration across ignition cycles and the usual boot is verify-only.
static void saveConfig() {
    const uint8_t p[13] = { 0, 0, 0, 0,                // clearMask
                            0x1F, 0x06, 0x00, 0x00,    // saveMask
                            0, 0, 0, 0,                // loadMask
                            0x17 };                    // BBR | flash | EEPROM | SPI
    ubxSend(CLS_CFG, ID_CFG, p, sizeof(p));
}

// === Link plumbing =========================================================

static void openBaud(uint32_t baud) {
    if (g_baud == baud) return;
    g_baud = baud;
    if (Serial1) Serial1.updateBaudRate(baud);
}

static void enterState(GpsLink s) {
    g_link     = s;
    g_stateMs  = millis();
    g_respHave = false;
    g_ubxState = 0;
}

static void resetGradeAnchor() {
    g_altEma      = NAN;
    g_anchorValid = false;
    g_gradePct    = NAN;
}

// `fresh` marks a newly (re)established link, which resets the repair-attempt
// budget. Without that reset, a session that once exhausted MAX_VERIFY_PASSES
// could never repair a module plugged in later.
static void beginVerify(bool fresh) {
    g_step        = 0;
    g_stepBad     = 0;
    g_stepRetry   = 0;
    g_pollPending = false;
    if (fresh) g_verifyPass = 0;
    enterState(GpsLink::VERIFYING);
}

// Drain the UART. Returns true if a well-formed NMEA sentence landed, which is
// what "the module is talking to us at this baud" means.
static bool drain() {
    const uint32_t before = g_gps.passedChecksum();
    int budget = 512;   // bounded so a flood cannot stretch the frame
    while (Serial1.available() > 0 && budget-- > 0) {
        feedByte((uint8_t)Serial1.read());
    }
    if (g_gps.passedChecksum() != before) {
        g_lastNmeaMs = millis();
        return true;
    }
    return false;
}

// === Cache + grade =========================================================

static void refreshCache() {
    if (g_gps.location.isValid() && g_gps.location.isUpdated()) {
        g_lat       = g_gps.location.lat();
        g_lon       = g_gps.location.lng();
        g_locValid  = true;
        g_lastLocMs = millis();
    }
    if (g_gps.altitude.isUpdated())   g_altM      = (float)g_gps.altitude.meters();
    if (g_gps.speed.isUpdated())      g_speedKmh  = (float)g_gps.speed.kmph();
    if (g_gps.course.isUpdated())     g_courseDeg = (float)g_gps.course.deg();
    if (g_gps.hdop.isUpdated())       g_hdop      = (float)g_gps.hdop.hdop();
    if (g_gps.satellites.isUpdated()) g_sats      = (uint8_t)g_gps.satellites.value();
}

// Smooth altitude, then take grade over a distance baseline long enough that
// GPS vertical noise (±10-15 m) cannot dominate the result.
static void updateGrade() {
    if (!gpsHasFix()) { resetGradeAnchor(); return; }

    const uint32_t now = millis();
    if (isnan(g_altEma)) {
        g_altEma   = g_altM;
        g_altEmaMs = now;
    } else {
        const float dt = (now - g_altEmaMs) / 1000.0f;
        g_altEmaMs = now;
        float alpha = dt / GRADE_TAU_SEC;
        if (alpha > 1.0f) alpha = 1.0f;
        if (alpha > 0.0f) g_altEma += (g_altM - g_altEma) * alpha;
    }

    if (!g_anchorValid) {
        g_anchorLat   = g_lat;
        g_anchorLon   = g_lon;
        g_anchorAlt   = g_altEma;
        g_anchorValid = true;
        return;
    }

    const double d = TinyGPSPlus::distanceBetween(g_anchorLat, g_anchorLon,
                                                  g_lat, g_lon);
    if (d < GRADE_BASELINE_M) return;

    // Gates: without them the number would be noise dressed up as data.
    if (g_hdop <= GRADE_MAX_HDOP && g_speedKmh >= GRADE_MIN_KMH) {
        float g = (float)((g_altEma - g_anchorAlt) / d * 100.0);
        if (g >  GRADE_MAX_PCT) g =  GRADE_MAX_PCT;
        if (g < -GRADE_MAX_PCT) g = -GRADE_MAX_PCT;
        g_gradePct = g;
    } else {
        g_gradePct = NAN;
    }

    g_anchorLat = g_lat;
    g_anchorLon = g_lon;
    g_anchorAlt = g_altEma;
}

// === Link state machine ====================================================

static void tickAbsent() {
    if (drain()) {                       // module started talking at this baud
        beginVerify(true);
        return;
    }
    if (millis() - g_stateMs >= ABSENT_RETRY_MS) {
        g_probeIdx = 0;
        openBaud(PROBE_BAUDS[0]);
        enterState(GpsLink::PROBING);
    }
}

static void tickProbing() {
    if (drain()) {
        Serial.printf("[GPS] link em %lu baud\n", (unsigned long)g_baud);
        beginVerify(true);
        return;
    }
    if (millis() - g_stateMs < PROBE_WINDOW_MS) return;

    ++g_probeIdx;
    if (g_probeIdx >= (sizeof(PROBE_BAUDS) / sizeof(PROBE_BAUDS[0]))) {
        enterState(GpsLink::ABSENT);
        return;
    }
    openBaud(PROBE_BAUDS[g_probeIdx]);
    g_stateMs = millis();
}

// Walk the steps one poll at a time. Each tick either sends a poll, consumes a
// response, or expires a timeout — never waits in a loop.
static void tickVerifying() {
    drain();

    if (g_step >= STEP_COUNT) {
        if (g_stepBad == 0) {
            Serial.printf("[GPS] config verificada em %lu baud, %lu run(s)\n",
                          (unsigned long)g_baud, (unsigned long)g_configRuns);
            enterState(GpsLink::READY);
        } else if (g_verifyPass >= MAX_VERIFY_PASSES) {
            // Give up repairing this pass. If the module also went quiet (the
            // usual cause: a CFG-PRT that moved it to a rate it cannot hold),
            // fall back to ABSENT so the probe re-hunts the baud instead of
            // sitting in READY talking to nobody.
            if (millis() - g_lastNmeaMs >= STARVE_TIMEOUT_MS) {
                Serial.println("[GPS] config falhou e o link calou — re-probe");
                enterState(GpsLink::ABSENT);
            } else {
                Serial.printf("[GPS] config divergente (mask 0x%04X) apos %u "
                              "passes — seguindo degradado\n",
                              (unsigned)g_stepBad, (unsigned)g_verifyPass);
                enterState(GpsLink::READY);
            }
        } else {
            ++g_verifyPass;
            enterState(GpsLink::CONFIGURING);
        }
        return;
    }

    if (!g_pollPending) {
        // A failed PRT is not worth chasing forever: give up on 38400 and let
        // the session run at the stock rate rather than rewriting every boot.
        if (g_step == STEP_PRT && g_prtFails >= MAX_PRT_FAILS) {
            ++g_step;
            return;
        }
        pollStep(g_step);
        g_pollPending = true;
        g_pollMs      = millis();
        g_respHave    = false;
        return;
    }

    if (g_respHave) {
        if (!stepMatches(g_step, g_respCls, g_respId, g_respBuf, g_respLen)) {
            g_stepBad = (uint16_t)(g_stepBad | (1u << g_step));
        }
        g_respHave    = false;
        g_pollPending = false;
        g_stepRetry   = 0;
        ++g_step;
        return;
    }

    if (millis() - g_pollMs >= POLL_TIMEOUT_MS) {
        if (g_stepRetry < POLL_RETRIES) {
            ++g_stepRetry;
            g_pollPending = false;      // resend on the next tick
            return;
        }
        // No answer at all — treat as mismatched so it gets applied blind.
        g_stepBad     = (uint16_t)(g_stepBad | (1u << g_step));
        g_stepRetry   = 0;
        g_pollPending = false;
        ++g_step;
    }
}

static void tickConfiguring() {
    drain();

    Serial.printf("[GPS] aplicando config (mask 0x%04X)\n", (unsigned)g_stepBad);

    // NAV5 / RATE / MSG first, at the rate we can currently talk.
    for (uint8_t s = 0; s < STEP_PRT; ++s) {
        if (g_stepBad & (1u << s)) applyStep(s);
    }

    // PRT last: it moves the baud out from under us.
    const bool doPrt = (g_stepBad & (1u << STEP_PRT)) && g_prtFails < MAX_PRT_FAILS;
    if (doPrt) {
        applyStep(STEP_PRT);
        Serial1.flush();          // let the frame leave before switching rate
        delay(20);                // receiver needs a moment to reconfigure
        openBaud(BAUD_TARGET);
        ++g_prtFails;             // cleared below once 38400 actually verifies
    }

    saveConfig();
    ++g_configRuns;
    beginVerify(false);
}

static void tickReady() {
    drain();
    if (millis() - g_lastNmeaMs >= STARVE_TIMEOUT_MS) {
        // Power cut, unplugged, or swapped for another module. Dropping to
        // ABSENT forces a fresh verify on its return — this is the whole
        // module-swap story.
        Serial.println("[GPS] link perdido — revalidara na volta");
        resetGradeAnchor();
        g_locValid = false;
        enterState(GpsLink::ABSENT);
        return;
    }
    if (g_prtFails && g_baud == BAUD_TARGET) g_prtFails = 0;
}

// === Simulation ============================================================

static void tickSim() {
    const float t = millis() / 1000.0f;
    g_locValid  = true;
    g_lastLocMs = millis();
    g_sats      = 8;
    g_hdop      = 1.2f;
    g_altM      = 80.0f + 30.0f * sinf(t * 2.0f * (float)M_PI / 90.0f);
    g_speedKmh  = 80.0f;
    g_courseDeg = fmodf(t * 4.0f, 360.0f);
    g_gradePct  = 6.0f * cosf(t * 2.0f * (float)M_PI / 90.0f);
    g_link      = GpsLink::READY;
}

// === Public API ============================================================

void gpsInit() {
    if constexpr (USE_REAL_GPS) {
        Serial1.setRxBufferSize(RX_BUFFER);
        g_baud = PROBE_BAUDS[0];
        Serial1.begin(g_baud, SERIAL_8N1, PIN_GPS_RX, PIN_GPS_TX);
        g_lastNmeaMs = millis();
        enterState(GpsLink::PROBING);
        Serial.printf("[GPS] Serial1 RX=%d TX=%d, probe %lu baud\n",
                      (int)PIN_GPS_RX, (int)PIN_GPS_TX, (unsigned long)g_baud);
    } else {
        g_link = GpsLink::READY;
    }
}

void gpsTick() {
    if constexpr (!USE_REAL_GPS) {
        tickSim();
        return;
    }

    switch (g_link) {
        case GpsLink::ABSENT:      tickAbsent();      break;
        case GpsLink::PROBING:     tickProbing();     break;
        case GpsLink::VERIFYING:   tickVerifying();   break;
        case GpsLink::CONFIGURING: tickConfiguring(); break;
        case GpsLink::READY:       tickReady();       break;
    }

    refreshCache();
    updateGrade();
}

void gpsPause() {
    if constexpr (USE_REAL_GPS) {
        while (Serial1.available() > 0) Serial1.read();
        g_ubxState = 0;
        g_respHave = false;
        g_locValid = false;
        // Force a re-verify on wake: the module was unpowered across the sleep
        // and may not even be the same one anymore.
        enterState(GpsLink::ABSENT);
    }
    resetGradeAnchor();
}

bool gpsHasFix() {
    if constexpr (!USE_REAL_GPS) return true;
    return g_locValid && (millis() - g_lastLocMs) < 5000;
}

uint8_t  gpsSatellites() { return g_sats; }
float    gpsHdop()       { return g_hdop; }
float    gpsAltitudeM()  { return isnan(g_altEma) ? g_altM : g_altEma; }
float    gpsSpeedKmh()   { return g_speedKmh; }
float    gpsCourseDeg()  { return g_courseDeg; }
float    gpsGradePct()   { return g_gradePct; }
uint32_t gpsFixAgeMs()   { return g_locValid ? (millis() - g_lastLocMs) : 0xFFFFFFFFul; }

const char* gpsCardinal() {
    if (!gpsHasFix()) return "--";
    return TinyGPSPlus::cardinal(g_courseDeg);
}

GpsLink gpsLink() { return g_link; }

const char* gpsLinkLabel() {
    switch (g_link) {
        case GpsLink::ABSENT:      return "AUSENTE";
        case GpsLink::PROBING:     return "PROBE";
        case GpsLink::VERIFYING:   return "CFG?";
        case GpsLink::CONFIGURING: return "CFG...";
        case GpsLink::READY:       return "OK";
    }
    return "?";
}

uint32_t gpsCharsProcessed() { return g_gps.charsProcessed(); }
uint32_t gpsFailedChecksum() { return g_gps.failedChecksum(); }
uint32_t gpsAckCount()       { return g_ackCount; }
uint32_t gpsNakCount()       { return g_nakCount; }
uint32_t gpsConfigRuns()     { return g_configRuns; }
