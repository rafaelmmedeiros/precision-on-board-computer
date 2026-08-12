# Arquitetura — padrões e módulos

Extraído de `CLAUDE.md` §5.1–5.2. As **regras** curtas ficam no CLAUDE.md; aqui
está o mapa completo e o detalhe de cada padrão. O GpsModule tem doc próprio:
[docs/gps-module.md](gps-module.md).

## Mapa de módulos

```
main.cpp
  ├─ powerTick()            ← state machine de ignição, primeira coisa do loop
  ├─ applyBacklightPolicy() ← nível PWM por power state + farol
  ├─ switch (powerCurrent()):
  │     ACTIVE              → telemetryTick + gpsTick + UI normal
  │     POST_TRIP_SUMMARY   → render TripSummaryScreen, sem telemetryTick
  │     GRACE               → telemetryPause + gpsPause + blank + light sleep
  │     DEEP_SLEEP_PENDING  → blank + esp_deep_sleep_start()
  ├─ pollDebugUnlock()      ← combo R+S revela/oculta telas de debug
  ├─ buttonsPoll() → ev     ← input do usuário (só ACTIVE/POST_TRIP_SUMMARY)
  └─ SCREENS[g_screen].draw()   ← anel dimensionado por ringSize()

PowerState ──reads──> GPIO 13 (ignição, ext0 wake) ──> Features (timeouts)
   │
   └─ chama tripLogStashInProgress / tripLogFinishCurrentTrip

Telemetry  ──reads──>  Features (flags + BENCH_TIME_SCALE)
   ▲
   │ getters (read-only)
   │
   ├─ ConsumptionScreen
   ├─ AutonomyScreen
   ├─ SystemScreen
   ├─ HistoryScreen ──reads──> TripLog (NVS persistence + stash slot)
   └─ TripSummaryScreen (só durante POST_TRIP_SUMMARY)

GpsModule ──reads/writes──> Serial1 (GPIO 39 RX / 15 TX)
   │   NMEA via TinyGPSPlus + handshake UBX auto-validado (gps-module.md)
   ├─ GpsScreen      (getters read-only)
   └─ GpsDebugScreen (getters read-only, atrás de POBC_DEBUG_MODE)
      Nenhum outro módulo consome GPS: Telemetry segue independente.

Buttons → main → ResetScreen (modal contextual)
                    │
                    └─ chama ResetSet.opts[i].fn (vem de cada Screen)

Layout.h  ──shared──>  Consumption, Autonomy, History, Gps, GpsDebug
Tft + BitmapFont + Fonts → todos os Screens
LT7680 (lib/) → Tft
```

## Telemetry — single source of truth

`Telemetry` é dona de **todo** o estado físico do veículo. Telas são **render puro**, nunca escrevem em estado.

- `telemetryTick()` chamado **uma vez por iteração** do loop principal, **antes** de qualquer render. Avança regime, integradores de viagem, tank drain, histórico, voltagem, temperaturas — tudo.
- Telas leem via getters (`telemetryKmL()`, `telemetryTankL()`, `telemetryVoltage()`, etc.) e nunca conhecem detalhes da simulação ou do sensor real por trás.
- **Novos sensores ou simulações default vão pra Telemetry.** Telas continuam intocadas.
- Resets centralizados: `telemetryResetTrip()` zera km e litros atomicamente. Telas delegam via suas `ResetSet`s.

Quando um sensor real for integrado, troca-se o branch `if constexpr (USE_REAL_*)` em Telemetry — telas não precisam saber.

## Padrão Screen

Cada tela expõe duas funções:

```cpp
void displayX();           // render puro — lê telemetry, desenha
ResetSet xResets();        // 0..2 opções de reset contextuais
```

`ResetSet` (em `include/Screens.h`) descreve o que aparece no modal de reset quando o usuário faz hold-R nessa tela. Ex.: ConsumptionScreen expõe `[R] Encerrar viagem`, HistoryScreen expõe `[R] Apagar histórico`.

Telas **não** mantêm estado entre frames além de UI ephemera (cursor, animação). Tudo de modelo está em Telemetry ou TripLog.

## Input pipeline

`Buttons` (`include/Buttons.h`):
- Polling em GPIO 25 (R) e 26 (S), pull-up interno, debounce 25 ms
- Eventos emitidos: `BTN_EV_R_TAP`, `BTN_EV_S_TAP`, `BTN_EV_R_HOLD`, `BTN_EV_S_HOLD`
- Tap dispara no release (se hold não tiver disparado antes); hold dispara quando o threshold é cruzado
- Threshold por botão é configurável via `buttonsSetHoldMs()` — `main.cpp` muda contextualmente: 1.5s pra S fora do modal (nav rápido) vs 3s dentro (confirm destrutivo)

`main.cpp` decide o roteamento:
- Se `resetScreenActive()` → eventos vão pro modal (`resetScreenTick(ev)`)
- Caso contrário → `handleNavEvent(ev)` (Tap = troca tela, Hold R = abre modal de resets, Hold S = home)

`ResetScreen` (`include/ResetScreen.h`):
- Recebe `ResetSet` da tela origem
- Renderiza opções com barra de progresso por hold
- Tap = cancela; Hold confirma a opção respectiva
- Toast "X RESETADO" ao confirmar, depois fecha

## Features.h — flags de sensor, fast-forward e power timeouts

`include/Features.h`, namespace `pobc`:

```cpp
constexpr bool USE_REAL_INJECTOR     = false; // GPIO 34
constexpr bool USE_REAL_VSS          = false; // GPIO 36
constexpr bool USE_REAL_VOLTAGE      = false; // GPIO 32
constexpr bool USE_REAL_FUEL_LEVEL   = false; // GPIO 33 (boia)
constexpr bool USE_REAL_TEMP_SENSORS = false; // DS18B20 (pino a alocar — ver §4.2)
constexpr bool USE_REAL_GPS          = false; // Serial1 (RX 39 / TX 15)
constexpr bool USE_REAL_IGNITION     = false; // GPIO 13 (digital active-low)
constexpr bool USE_REAL_FAROL        = true;  // GPIO 35 (digital active-low, sem sim)
constexpr bool ANY_SIMULATED = /* OR dos 5 sims negados */; // gate do BENCH_TIME_SCALE
constexpr float BENCH_TIME_SCALE     = 60.0f; // 1 real sec = 1 sim min

constexpr uint8_t  BACKLIGHT_LEVEL_NIGHT =  20;   // bancada (conservador)
constexpr uint8_t  BACKLIGHT_LEVEL_DAY   =  85;   // bancada (conservador; produção → 255)
constexpr uint8_t  BACKLIGHT_LEVEL_OFF   =   0;
constexpr bool     BACKLIGHT_ACTIVE_LOW  = false; // placa atual: active-high

constexpr uint32_t POST_TRIP_SUMMARY_MS                = 60UL * 1000UL;        // 60 s
constexpr uint32_t GRACE_PERIOD_MS                     = 60UL * 60UL * 1000UL; // 1 h
constexpr uint32_t BENCH_POST_TRIP_SUMMARY_MS_OVERRIDE = 10UL * 1000UL;        // 10 s na bancada
constexpr uint32_t BENCH_GRACE_MS_OVERRIDE             = 30UL * 1000UL;        // 30 s na bancada
```

Telemetry usa `if constexpr` nessas flags. Branches mortos somem do binário em compile time — zero custo de runtime.

Quando os cinco flags de simulação (`INJECTOR`, `VSS`, `VOLTAGE`, `FUEL_LEVEL`, `TEMP_SENSORS`) forem todos true, `ANY_SIMULATED` vira false e `BENCH_TIME_SCALE` colapsa pra 1.0 (a sim para). `GPS`, `IGNITION` e `FAROL` **não** entram nessa conta — só os cinco sims integráveis. Hoje na bancada os cinco estão false, então o sim corre 60x acelerado pra animar o medidor de tanque em minutos em vez de horas. Os dois overrides `BENCH_POST_TRIP_SUMMARY_MS_OVERRIDE` e `BENCH_GRACE_MS_OVERRIDE` colapsam pros valores reais (`POST_TRIP_SUMMARY_MS` / `GRACE_PERIOD_MS`) quando `USE_REAL_IGNITION` é true — produção sempre usa 60s + 1h.

## Layout.h — rails compartilhados

`include/Layout.h`, namespace `pobc`:

- `TOP_LABEL_Y`, `FOOTER_DIV`, `FOOTER_Y` — posições do título superior e do divisor/texto do footer (usados por todas as telas com esse padrão)
- `LEFT_W`, `DIV_X`, `DIV_W`, `RIGHT_X`, `DIV_Y_TOP` — split de duas colunas (Consumption, Autonomy)
- `SystemScreen` usa layout 50/50 próprio, não puxa daqui

Telas declaram `using namespace pobc;` e aplicam direto. Ajuste global = uma constante em Layout.h.

## Persistência — NVS via Preferences

`TripLog` (`include/TripLog.h`):
- Buffer FIFO de `TRIP_LOG_MAX = 12` registros
- `TripRecord` = 28 bytes (validado por `static_assert`)
- Persistido como blob único na namespace `triplog` do NVS
- Escrita em `tripLogFinishCurrentTrip()` — gatilhos: encerramento manual via Hold-R em Consumption, OU automático após 1h de chave desligada (PowerState GRACE expirado)
- **Stash slot** (`KEY_STASH`): snapshot da viagem em curso, escrito por `tripLogStashInProgress()` na transição POST_TRIP_SUMMARY → GRACE. Brownout durante GRACE não perde a viagem — `tripLogConsumeStashOrFinalize()` no boot frio decide entre restaurar (se < `GRACE_PERIOD_MS` desde o stash, via `time(nullptr)`/DS3231) ou finalizar no log
- Recovery: blob corrompido ou parcial no boot → log começa zerado

NVS é a escolha pra estado persistente do projeto. SD card e exportação foram explicitamente removidos do escopo.

## PowerState — state machine de ignição

`include/PowerState.h` + `src/PowerState.cpp`. Sempre ativo no `loop()` antes de qualquer outra coisa.

Estados: `ACTIVE`, `POST_TRIP_SUMMARY`, `GRACE`, `DEEP_SLEEP_PENDING`. Transições:

- **ACTIVE → POST_TRIP_SUMMARY:** GPIO 13 vai pra HIGH (ignição off). `telemetryTick()` para de ser chamado — engine off não deve avançar integradores.
- **POST_TRIP_SUMMARY → ACTIVE:** GPIO 13 volta pra LOW dentro de 60s. Viagem **continua intacta**, sem stash, sem reset.
- **POST_TRIP_SUMMARY → GRACE:** 60s expirados ou tap em qualquer botão. Stash da viagem em NVS, `telemetryPause()`, blank no display, `esp_light_sleep_start()`.
- **GRACE → ACTIVE:** wake do light sleep (ext0 GPIO 13 LOW ou timer 250ms periódico) e ignição estável LOW. Limpa stash, retoma a viagem.
- **GRACE → DEEP_SLEEP_PENDING:** 1h esgotada (`GRACE_PERIOD_MS` em produção, `BENCH_GRACE_MS_OVERRIDE` na bancada). Limpa stash, `tripLogFinishCurrentTrip()` (fica no histórico), próximo loop chama `esp_deep_sleep_start()` com `rtc_gpio_pullup_en` + ext0.
- **(boot) → ACTIVE / DEEP_SLEEP_PENDING:** `powerInit()` lê GPIO 13, decide. Se `ESP_RST_DEEPSLEEP`, pula WiFi/NTP no `setup()` pra wake instantâneo.

Debounce: 50 ms na leitura digital. Sleep deltas no `telemetryTick()` são clampados (>5s = descartado) como rede de segurança independente do `telemetryPause()` explícito.
