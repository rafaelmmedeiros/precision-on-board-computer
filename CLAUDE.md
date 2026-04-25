# CLAUDE.md

Orientação para Claude Code (claude.ai/code) ao trabalhar neste repositório.

---

## ⚠ Regra de manutenção — leia antes de qualquer outra coisa

Este documento é o ground truth do projeto pra qualquer sessão de Claude. **Drift entre este arquivo e o código é proibido.** Toda mudança que afete arquitetura, hardware, convenções, build ou decisões **deve atualizar a seção correspondente no mesmo commit**.

### Gatilhos de manutenção

Qualquer um destes implica revisar seções deste arquivo:

| Mudança no código/projeto | Seções a revisar |
|---|---|
| Novo módulo, módulo removido, refactor que altera deps | §5 (Arquitetura), §9 (Project Structure) |
| Novo/removido componente de hardware, repinagem | §4 |
| Mudança de flags, comandos ou dependências de build | §3.3 |
| Nova convenção de UI (gesto, cor, layout, fonte) | §5, §6 |
| Fase fechada / redirecionada | §7 |
| Decisão pendente resolvida | mover de §7.2 pra §4/§5/§6 conforme natureza; se virar referência, §8 |

### Checklist anti-drift (ao fim de toda sessão que tocou código)

1. Novo ou removido arquivo? → §9
2. Padrão arquitetural mudou? → §5
3. Convenção de UI mudou? → §6
4. Hardware mexeu? → §4
5. Decisão pendente foi fechada? → promover de §7.2 pro lar real

Se uma sessão termina com drift, **conserte antes do commit**. Não deixar pra depois.

### Quando dividir o arquivo

Este documento deve permanecer navegável. Soft caps:

- **Arquivo inteiro > 400 linhas** → considerar extração
- **Qualquer seção > 80 linhas** → forte candidata a extração

Destino de extração: `docs/<topico>.md`, indexado a partir daqui com **uma linha de resumo + link**. A seção 8 (Engineering references) é a primeira candidata natural quando crescer.

Formato do índice quando uma seção for extraída:

```markdown
## 8. Engineering references
- [LT7680 GPU capabilities](docs/lt7680-gpu.md) — primitivas GPU disponíveis, modos de cor, fontes
- [Display rotation problem](docs/display-rotation.md) — por que texto sai flipado, três caminhos de resolução
```

Arquivos extraídos mantêm o mesmo padrão (markdown, headers numerados ou nomeados, refs ao código quando útil, sem floreio).

---

## 1. Project Overview

**P-OBC** (Precision On-Board Computer) — computador de bordo de precisão para Chevrolet Astra 2011 2.0 8V Flex, ECU Bosch Motronic ME7.9.9.

- Plataforma: ESP32 WROOM-32, Arduino framework sobre ESP-IDF
- Toolchain: PlatformIO
- IDE: CLion
- Linguagem: C++17

## 2. Developer Context

Senior .NET dev (~15 anos de programação, ~5 profissionais), iniciante em sistemas embarcados. Experiência prévia com game dev — modelo mental de game loop, fixed timestep, frame budget se aplica direto ao loop do Arduino.

**Stance do projeto: "beat the OEMs, don't match them"** — o P-OBC é peça de afirmação, não clone. Quando uma decisão for "match factory" vs "exceed factory", ir além é o default. Pesquisar comportamento OEM serve pra estabelecer **piso**, não teto.

> "Desenvolvedor não irá perder oportunidade de fazer melhor que BMW."

Explicar conceitos de embarcados quando relevante (GPIO, I²C, SPI, PWM, interrupts, timers, watchdog, alocação estática, FreeRTOS) — paralelos com .NET e game dev são bem-vindos.

---

## 3. Working agreements

### 3.1 Metodologia iterativa

Projeto **não tem plano mestre fechado**. Avança com o material disponível no momento (peças que chegaram, ideias novas, restrições descobertas). As "Fases" da seção 7 são **catálogo de features**, não cronograma — pula-se entre tópicos conforme conveniência.

Implicações:
- Não cobrar sequenciamento de fases; sugerir o que faz sentido **agora**.
- Decisões que dependem de hardware ausente ficam em aberto.
- Protótipos descartáveis são bem-vindos pra validar feeling/UX antes de comprometer com layout final.

### 3.2 Regras de código

- **C++17**
- **`-Werror`** — zero warnings tolerados
- **Sem `new`/`delete`/`std::vector` em runtime** — alocação estática
- **`volatile` obrigatório** em variáveis compartilhadas entre ISR e loop
- **Sem calibração subjetiva** — toda escala vem de medida física direta
- **Credenciais WiFi** isoladas em `include/config.h` (gitignored)

### 3.3 Build & upload commands

```bash
pio run                                  # Build
pio run -t upload                        # Upload pro ESP32
pio device monitor                       # Monitor serial (115200 baud)
pio run -t upload && pio device monitor  # Build + upload + monitor
pio test                                 # Roda testes unitários
pio run -t clean                         # Limpa build
```

---

## 4. Hardware

### 4.1 Componentes ativos

- **MCU:** ESP32 WROOM-32 (38 pinos)
- **Display:** ER-TFT4.58-1 320×960 IPS via LT7680 (SPI) + breakout FPC 20 pinos
- **RTC:** DS3231 com bateria CR2032
- **Temperatura:** DS18B20 × 2 (interna + externa, OneWire)
- **GPS:** NEO-8M com antena externa
- **Alimentação:** Buck DC-DC automotivo (LM2596 / MP1584), saída dupla 5V (ESP32) + 3.3V (display e periféricos). **Entrada 12-14.4V vem da bateria direta com fusível 1A** (NÃO mais do 12V chaveado), porque o ESP32 fica energizado 100% do tempo, igual TID/MID factory. Estado de ignição é lido por sinal digital separado em GPIO 13, não pela presença/ausência de tensão de alimentação. Quiescent target: ~10 µA em deep sleep, ~1 mA em light sleep, ~80 mA em ACTIVE.
- **Buzzer:** piezo passivo (não montado ainda)

### 4.2 Mapa de pinos ESP32

A ER-TFT4.58-1 + LT7680 consome **9 GPIOs** entre o bus VSPI principal, a bring-up SW-SPI do painel ST7701S e o BL_CONTROL. Mais 5 pinos pra ignição/farol/botões. Total: **14 pinos físicos conectados hoje**.

#### Conectados (hardware na bancada)

| Função | GPIO | Notas |
|--------|------|-------|
| Display SCK (VSPI, runtime) | 18 | LT7680 hardware SPI clock |
| Display MOSI (VSPI, runtime) | 23 | LT7680 hardware SPI data in |
| Display MISO (VSPI, runtime) | 19 | LT7680 hardware SPI data out |
| Display CS (LT7680) | 5 | VSPI chip-select runtime |
| Display RESET (LT7680) | 16 | Pulso só no `tftInit()`, depois idle |
| Painel ST7701S — SW-SPI CLK | 14 | Bring-up do painel só no `tftInit()`, depois idle |
| Painel ST7701S — SW-SPI CS | 17 | Bring-up do painel só no `tftInit()`, depois idle |
| Painel ST7701S — SW-SPI DI | 4 | Bring-up do painel só no `tftInit()`, depois idle |
| Dimmer backlight (BL_CONTROL) | 2 | LEDC PWM 5kHz 8-bit. API `tftBacklight(0..255)`. Polaridade active-low na placa atual (`BACKLIGHT_ACTIVE_LOW=true`). |
| Sinal de ignição | 13 | Digital active-low. `INPUT_PULLUP` interno + chave/opto pra GND. RTC-capable → `ext0` wake source. Bench: chave protoboard. Produção: PC817. |
| Farol aceso | 35 | Input-only, **sem pull interno** — exige resistor externo 10k pra 3.3V. Active-low. Lido por `telemetryHeadlightOn()`. |
| Botão R | 25 | Pull-up interno, chave pra GND |
| Botão S | 26 | Pull-up interno, chave pra GND |

#### Reservados (pinos comprometidos, hardware ainda não montado)

| Função | GPIO | Notas |
|--------|------|-------|
| I²C SDA (DS3231) | 21 | Padrão Wire — DS3231 ainda não soldado |
| I²C SCL (DS3231) | 22 | Padrão Wire |
| Voltímetro | 32 | ADC1, divisor 14.4V→3.3V. Mede bateria/alternador. |
| Boia combustível | 33 | ADC1 obrigatório (ADC2 inutilizável com WiFi) |
| Injeção (bicos) ECU | 34 | Input-only, interrupt, divisor 5V→3.3V |
| VSS (velocidade) | 36 (VP) | Input-only, interrupt, divisor |

#### Sem pino atribuído (a alocar quando hardware chegar)

| Função | Status | Caminhos |
|---|---|---|
| Buzzer | Sem alocação | GPIO 27 era a intenção original — ainda livre se nada brigar até lá. |
| DS18B20 ×2 (interno + externo, OneWire) | Sem alocação | Opção mais natural: **reaproveitar GPIO 4 depois do `tftInit()`**, já que a SW-SPI do painel ST7701S só pulsa esse pino uma vez no boot e depois deixa idle. Custa um `pinMode()` posterior. Outra opção: expansor I²C ou um pino estritamente novo. |
| GPS NEO-8M (UART2) | Sem alocação | A pinagem default UART2 (16/17) **não está mais disponível** — GPIO 16 virou RESET do LT7680 e 17 é CS do painel ST7701S. Caminhos: (a) usar `Serial1` em pinos arbitrários, ex. RX em 39 (input-only, ADC1) e TX em 15 (strapping, mas seguro depois do boot); (b) reaproveitar 14/17 pós-init com `Serial2.begin(baud, cfg, 14, 17)`. |

#### Truly livres no ESP32

| GPIO | Notas |
|---|---|
| 27 | Reservado pra buzzer (intenção original mantida) |
| 39 (VN) | Input-only, ADC1, sem pull. Bom pra GPS RX ou ADC adicional. |
| 15 | Strapping (HIGH default no boot, LOW silencia boot messages). Usável após boot. |
| 12 | Strapping perigoso (define voltagem do flash interno) — **evitar** salvo último recurso. |
| 0 | Strapping (boot mode) — **evitar**. |

**Notas de design:**

- **Ignição em pino digital dedicado:** GPIO 13 é RTC-capable e funciona como `ext0` wake source — o ESP32 dorme em deep sleep e acorda na transição da chave indo pra LOW. NÃO é strapping pin. Antes da arquitetura always-on a ignição era inferida pela voltagem no GPIO 32; hoje GPIO 32 é só voltímetro.
- **Pinos do painel ST7701S são reutilizáveis:** GPIO 4, 14, 17 só são pulsados uma vez em `ST7701S_Initial()` (chamado dentro de `tftInit()`). Após isso ficam idle e podem ser reconfigurados via `pinMode()` pra outra função (OneWire, UART, etc.). Documentar bem quem reaproveita o quê pra evitar conflito de ordem de init.
- **Display RESET (16) também só é pulsado no boot,** mas é mais arriscado reaproveitar — se algum reset transiente for emitido depois (raro), bagunça o periférico que estiver lá.
- **Expansão futura via I²C:** o bus 21/22 escala — qualquer periférico I²C novo entra sem custo de pino, só conferindo conflito de endereço. Útil quando aparecer um sensor I²C novo, expansor de GPIO ou ADC externo.

### 4.3 Sinais do carro

- **Pino 27 ECU** → fio PT/MR → sinal de injeção (pulse width em µs) → GPIO 34
- **VSS painel** → velocidade via k-factor calibrado por GPS → GPIO 36
- **Divisor resistivo 12-14.4V** → ADC ESP32 → GPIO 32 (voltímetro)
- **12V chaveado pela chave de ignição** → opto PC817 (LED) → coletor opto → GPIO 13 (active-low, sinal digital de ignição)

---

## 5. Arquitetura

### 5.1 Mapa de módulos

```
main.cpp
  ├─ powerTick()            ← state machine de ignição, primeira coisa do loop
  ├─ switch (powerCurrent()):
  │     ACTIVE              → telemetryTick + UI normal
  │     POST_TRIP_SUMMARY   → render TripSummaryScreen, sem telemetryTick
  │     GRACE               → blank + esp_light_sleep_start()
  │     DEEP_SLEEP_PENDING  → blank + esp_deep_sleep_start()
  ├─ buttonsPoll() → ev     ← input do usuário (só ACTIVE/POST_TRIP_SUMMARY)
  └─ SCREENS[g_screen].draw()

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

Buttons → main → ResetScreen (modal contextual)
                    │
                    └─ chama ResetSet.opts[i].fn (vem de cada Screen)

Layout.h  ──shared──>  Consumption, Autonomy, History
Tft + BitmapFont + Fonts → todos os Screens
LT7680 (lib/) → Tft
```

### 5.2 Padrão Telemetry — single source of truth

`Telemetry` é dona de **todo** o estado físico do veículo. Telas são **render puro**, nunca escrevem em estado.

- `telemetryTick()` chamado **uma vez por iteração** do loop principal, **antes** de qualquer render. Avança regime, integradores de viagem, tank drain, histórico, voltagem, temperaturas — tudo.
- Telas leem via getters (`telemetryKmL()`, `telemetryTankL()`, `telemetryVoltage()`, etc.) e nunca conhecem detalhes da simulação ou do sensor real por trás.
- **Novos sensores ou simulações default vão pra Telemetry.** Telas continuam intocadas.
- Resets centralizados: `telemetryResetTrip()` zera km e litros atomicamente. Telas delegam via suas `ResetSet`s.

Quando um sensor real for integrado, troca-se o branch `if constexpr (USE_REAL_*)` em Telemetry — telas não precisam saber.

### 5.3 Padrão Screen

Cada tela expõe duas funções:

```cpp
void displayX();           // render puro — lê telemetry, desenha
ResetSet xResets();        // 0..2 opções de reset contextuais
```

`ResetSet` (em `include/Screens.h`) descreve o que aparece no modal de reset quando o usuário faz hold-R nessa tela. Ex.: ConsumptionScreen expõe `[R] Encerrar viagem`, HistoryScreen expõe `[R] Apagar histórico`.

Telas **não** mantêm estado entre frames além de UI ephemera (cursor, animação). Tudo de modelo está em Telemetry ou TripLog.

### 5.4 Input pipeline

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

### 5.5 Features.h — flags de sensor + fast-forward + power timeouts

`include/Features.h`, namespace `pobc`:

```cpp
constexpr bool USE_REAL_INJECTOR     = false; // GPIO 34
constexpr bool USE_REAL_VSS          = false; // GPIO 36
constexpr bool USE_REAL_VOLTAGE      = false; // GPIO 32
constexpr bool USE_REAL_FUEL_LEVEL   = false; // GPIO 33 (boia)
constexpr bool USE_REAL_TEMP_SENSORS = false; // DS18B20 (pino a alocar — ver §4.2)
constexpr bool USE_REAL_GPS          = false; // UART2
constexpr bool USE_REAL_IGNITION     = false; // GPIO 13 (digital active-low)
constexpr bool USE_REAL_FAROL        = true;  // GPIO 35 (digital active-low, sem sim)
constexpr float BENCH_TIME_SCALE     = 60.0f; // 1 real sec = 1 sim min

constexpr uint8_t  BACKLIGHT_LEVEL_DAY   = 153;  // 60 % na bancada
constexpr uint8_t  BACKLIGHT_LEVEL_NIGHT =  76;  // 30 % na bancada
constexpr uint8_t  BACKLIGHT_LEVEL_OFF   =   0;
constexpr bool     BACKLIGHT_ACTIVE_LOW  = true;

constexpr uint32_t POST_TRIP_SUMMARY_MS                = 60UL * 1000UL;        // 60 s
constexpr uint32_t GRACE_PERIOD_MS                     = 60UL * 60UL * 1000UL; // 1 h
constexpr uint32_t BENCH_POST_TRIP_SUMMARY_MS_OVERRIDE = 10UL * 1000UL;        // 10 s na bancada
constexpr uint32_t BENCH_GRACE_MS_OVERRIDE             = 30UL * 1000UL;        // 30 s na bancada
```

Telemetry usa `if constexpr` nessas flags. Branches mortos somem do binário em compile time — zero custo de runtime.

Quando todas as flags forem true, `BENCH_TIME_SCALE` colapsa pra 1.0 e a sim para. Hoje na bancada todas false — o sim corre 60x acelerado pra animar o medidor de tanque em minutos em vez de horas. Os dois overrides `BENCH_POST_TRIP_SUMMARY_MS_OVERRIDE` e `BENCH_GRACE_MS_OVERRIDE` colapsam pros valores reais (`POST_TRIP_SUMMARY_MS` / `GRACE_PERIOD_MS`) quando `USE_REAL_IGNITION` é true — produção sempre usa 60s + 1h.

### 5.6 Layout.h — rails compartilhados

`include/Layout.h`, namespace `pobc`:

- `TOP_LABEL_Y`, `FOOTER_DIV`, `FOOTER_Y` — posições do título superior e do divisor/texto do footer (usados por todas as telas com esse padrão)
- `LEFT_W`, `DIV_X`, `DIV_W`, `RIGHT_X`, `DIV_Y_TOP` — split de duas colunas (Consumption, Autonomy)
- `SystemScreen` usa layout 50/50 próprio, não puxa daqui

Telas declaram `using namespace pobc;` e aplicam direto. Ajuste global = uma constante em Layout.h.

### 5.7 Persistência — NVS via Preferences

`TripLog` (`include/TripLog.h`):
- Buffer FIFO de `TRIP_LOG_MAX = 12` registros
- `TripRecord` = 28 bytes (validado por `static_assert`)
- Persistido como blob único na namespace `triplog` do NVS
- Escrita em `tripLogFinishCurrentTrip()` — gatilhos: encerramento manual via Hold-R em Consumption, OU automático após 1h de chave desligada (PowerState GRACE expirado)
- **Stash slot** (`KEY_STASH`): snapshot da viagem em curso, escrito por `tripLogStashInProgress()` na transição POST_TRIP_SUMMARY → GRACE. Brownout durante GRACE não perde a viagem — `tripLogConsumeStashOrFinalize()` no boot frio decide entre restaurar (se < `GRACE_PERIOD_MS` desde o stash, via `time(nullptr)`/DS3231) ou finalizar no log
- Recovery: blob corrompido ou parcial no boot → log começa zerado

NVS é a escolha pra estado persistente do projeto. SD card e exportação foram explicitamente removidos do escopo.

### 5.8 PowerState — state machine de ignição

`include/PowerState.h` + `src/PowerState.cpp`. Sempre ativo no `loop()` antes de qualquer outra coisa.

Estados: `ACTIVE`, `POST_TRIP_SUMMARY`, `GRACE`, `DEEP_SLEEP_PENDING`. Transições:

- **ACTIVE → POST_TRIP_SUMMARY:** GPIO 13 vai pra HIGH (ignição off). `telemetryTick()` para de ser chamado — engine off não deve avançar integradores.
- **POST_TRIP_SUMMARY → ACTIVE:** GPIO 13 volta pra LOW dentro de 60s. Viagem **continua intacta**, sem stash, sem reset.
- **POST_TRIP_SUMMARY → GRACE:** 60s expirados ou tap em qualquer botão. Stash da viagem em NVS, `telemetryPause()`, blank no display, `esp_light_sleep_start()`.
- **GRACE → ACTIVE:** wake do light sleep (ext0 GPIO 13 LOW ou timer 250ms periódico) e ignição estável LOW. Limpa stash, retoma a viagem.
- **GRACE → DEEP_SLEEP_PENDING:** 1h esgotada (`GRACE_PERIOD_MS` em produção, `BENCH_GRACE_MS_OVERRIDE` na bancada). Limpa stash, `tripLogFinishCurrentTrip()` (fica no histórico), próximo loop chama `esp_deep_sleep_start()` com `rtc_gpio_pullup_en` + ext0.
- **(boot) → ACTIVE / DEEP_SLEEP_PENDING:** `powerInit()` lê GPIO 13, decide. Se `ESP_RST_DEEPSLEEP`, pula WiFi/NTP no `setup()` pra wake instantâneo.

Debounce: 50 ms na leitura digital. Sleep deltas no `telemetryTick()` são clampados (>5s = descartado) como rede de segurança independente do `telemetryPause()` explícito.

---

## 6. UI conventions

### 6.1 Coordenadas, paleta, fontes

- **Sistema de coords (user-space, `Tft.h`):** `USR_W = 960` × `USR_H = 320`, origem top-left
- **Mount:** `Font_90_degree` + `VDIR=1` (compensa H-flip embutido no modo 90°)
- **Paleta** (`Tft.h`):
  - `COL_BG` preto puro
  - `COL_AMBER` (#FCA017) — texto e UI base
  - `COL_GOOD` verde, `COL_HOT` vermelho, `COL_COLD` azul
- **Fontes:**
  - CGROM 8×16, 12×24, 16×32 com zoom 1-4x
  - Bitmap proprietário DSEG7 nos tamanhos 48 / 72 / 120 px (heroes numéricos)
- **Lógica de cor por grandeza física** (em `Tft.h`, single source of truth — todas as telas chamam o helper, ninguém redefine threshold):
  - `consumptionColor(kmL)` — verde >14 · amber 10-14 · vermelho <10
  - `tempColor(°C)` — azul <10 · amber 10-30 · vermelho >30
  - `voltageColor(V)` — vermelho <12 ou >14.8 · amber 12-13.5 (sem alternador) · verde 13.5-14.8 (carregando)

### 6.2 Layout

Use `Layout.h` em qualquer tela nova. Padrões existentes:
- Hero + dados (Consumption, Autonomy): split em `LEFT_W=340`, divisor vertical, painel direito até `USR_W`
- Full-width (History): footer rails apenas
- 50/50 (System): exceção legada — divisor centrado em x=478

### 6.3 Gestos de input

| Contexto | Tap R | Tap S | Hold R (3s) | Hold S |
|----------|-------|-------|-------------|--------|
| Tela normal | Tela anterior | Próxima tela | Abre modal de Resets | (1.5s) Volta pra Consumption (home) |
| Modal Resets | Cancela | Cancela | Confirma reset[0] | (3s) Confirma reset[1] |

**Feedback visual:** barrinha de 3 px no rodapé cresce conforme o hold avança (âmbar pra R = destrutivo, verde pra S = nav). Some se soltar antes do threshold. Dentro do modal, cada opção tem sua própria barra abaixo do label.

### 6.4 Frame pacing e double buffer

- `main.cpp` paceia o loop a `FRAME_PERIOD_MS = 100` (10 fps uniforme em todas as telas)
- `flipBuffers()` espera 20 ms após `Main_Image_Start_Address` antes de liberar o canvas — necessário pra evitar tearing quando o desenho dura mais de uma frame de painel (~16 ms a 60Hz). Sem essa espera, telas pesadas (Consumption) piscam.

### 6.5 Anti-burn-in

`tftTick()` desloca a origem do canvas em ciclo de 4 cantos a cada 5 minutos. Pixel shift de 2 px previne image-sticking sem afetar legibilidade.

### 6.6 Backlight automático

`main.cpp::applyBacklightPolicy()` é chamado a cada frame, depois do `powerTick()`. Decide o nível PWM (0..255) com base no estado de power + farol:

| PowerState | Farol off | Farol on |
|---|---|---|
| `ACTIVE` | `BACKLIGHT_LEVEL_DAY` (153 ≈ 60 %) | `BACKLIGHT_LEVEL_NIGHT` (76 ≈ 30 %) |
| `POST_TRIP_SUMMARY` | `BACKLIGHT_LEVEL_DAY` | `BACKLIGHT_LEVEL_NIGHT` |
| `GRACE` | `BACKLIGHT_LEVEL_OFF` (0) | `BACKLIGHT_LEVEL_OFF` |
| `DEEP_SLEEP_PENDING` | `BACKLIGHT_LEVEL_OFF` | `BACKLIGHT_LEVEL_OFF` |

Sinal sai em GPIO 2 via LEDC (5 kHz, 8 bits) e chega ao backlight do LT7680. Os níveis estão calibrados pra bancada — não cegar o operador é prioridade. Em produção, sob luz solar do carro, faz sentido subir DAY pra 255 e revisitar NIGHT.

---

## 7. Roadmap

### 7.1 Fases — catálogo, não cronograma

> Ler em conjunto com a metodologia iterativa (3.1). Cada fase é um conjunto de features cujo bloqueio é hardware específico.

**Fase 1 — Bancada (sem carro):** RTC DS3231 + NTP fallback, temps DS18B20, voltímetro, dimmer PWM. UI completa rodando sobre simulação (estado atual).

**Fase 2 — Sinais do carro:** interrupt do GPIO 34 (injeção, pulse width µs), VSS no GPIO 36 com k-factor calibrado por GPS. Sinal de ignição já é digital em GPIO 13 (não depende mais do voltímetro), e a detecção automática de fim de viagem (>1h em GRACE) já está implementada via PowerState (§5.8) — quando o opto for montado, basta `USE_REAL_IGNITION=true`.

**Fase 3 — GPS:** altitude, inclinação, heading, velocidade independente, NTP→GPS pra hora atômica.

### 7.2 Decisões em aberto

- **Desligar o display em GRACE / DEEP_SLEEP** — resolvido. `tftBacklight()` em `Tft.h` faz PWM em GPIO 2 (LEDC), `main.cpp::applyBacklightPolicy()` (§6.6) decide os níveis a cada frame. GPIO 2 já chega ao backlight da placa atual (não precisou refazer trilha). Polaridade invertida da placa é tratada por `BACKLIGHT_ACTIVE_LOW=true` em `Features.h` — se trocar de breakout, basta flipar a flag.

---

## 8. Engineering references

Seções de referência técnica. Não precisa ler na entrada — consultar quando a decisão tocar nesses tópicos.

### 8.1 LT7680 — capacidades e limites da GPU

**Nativo via ER_TFT (lib/LT7680):**
- `DrawSquare`/`DrawSquare_Fill` — retângulos (outline e preenchido)
- `DrawCircle_Fill`, `Start_Circle_or_Ellipse` — círculos e elipses
- `Start_Line` — linhas arbitrárias
- `Start_Triangle`/`_Fill` — triângulos
- `Start_Circle_Square` — retângulos com cantos arredondados
- `DrawPixel` — único, lento via SPI; usar só em casos pontuais
- BTE (Block Transfer Engine) — copiar/mover regiões da SDRAM com ROP codes e alpha blending; útil pra sprites e composição
- Múltiplas layers (≥11) de 614400 bytes na SDRAM ~8 MB

**Modos de cor:** 16bpp RGB565 (atual, 65K cores), 24bpp RGB888 (16M, 2× memória), 8bpp indexado.

**Texto via CGROM interna:** 8×16, 12×24, 16×32 ASCII (Latin-1/ISO-8859-1/2/3/4), zoom 1-4x independente em W/H, rotação 0° ou 90° (no modo 90° há H-flip embutido — compensado por `VDIR=1`, mount atual).

**Limitações reais:**
- Sem rotação nativa de bitmaps (só texto 0°/90°)
- Sem antialiasing (workaround: supersample + BTE)
- Sem gradientes nativos (BTE alpha blending ou muitos rects)
- SPI a 8 MHz — ~3 µs por registrador, limita ops granulares (mas fill/BTE rodam na GPU sem overhead de pixel)

**Disponível pra adicionar:**
- Chip de flash SPI externo (W25Q64) nos pinos SFI do LT7680 — fontes TrueType pré-rasterizadas, sprites grandes
- Bitmap font em software — sem hardware extra

---

## 9. Project Structure

### Pastas

```
src/        — código-fonte (.cpp)
include/    — headers (.h)
lib/        — bibliotecas locais (cada subdir é uma lib)
test/       — testes unitários (PlatformIO test runner)
platformio.ini  — config (board, deps, flags)
```

### Módulos do firmware (em `src/` + `include/`)

| Módulo | Papel |
|--------|-------|
| `main.cpp` | setup/loop, init, dispatch por PowerState, roteamento de input, entradas de light/deep sleep |
| `PowerState.{h,cpp}` | state machine de ignição (ACTIVE / POST_TRIP_SUMMARY / GRACE / DEEP_SLEEP_PENDING), debounce GPIO 13 |
| `Telemetry.{h,cpp}` | dona do estado do veículo, sim de regime, integradores, histórico, pause/restore pra sleep |
| `Features.h` | flags por sensor, BENCH_TIME_SCALE, timeouts da power state machine |
| `Buttons.{h,cpp}` | input R/S debounce, tap, hold, threshold por botão |
| `ResetScreen.{h,cpp}` | modal contextual de resets |
| `Screens.h` | tipos `ResetOption` / `ResetSet` |
| `Layout.h` | rails de coordenada compartilhados |
| `Tft.{h,cpp}` | wrapper LT7680, double buffer, fontes, helpers de geom |
| `BitmapFont.{h,cpp}` | renderização de glyphs DSEG7 |
| `Fonts.{h,cpp}` | tabelas DSEG7_48/72/120 |
| `BootScreen.{h,cpp}` | splash de boot |
| `SystemScreen` | clock + voltagem + temperaturas |
| `ConsumptionScreen` | km/L instantâneo + histórico 5 min + viagem atual |
| `AutonomyScreen` | range estimado (com ±confidence) + tanque |
| `HistoryScreen` | gráfico das últimas 12 viagens encerradas |
| `TripSummaryScreen.{h,cpp}` | resumo da viagem encerrada (visível só durante POST_TRIP_SUMMARY) |
| `TripLog.{h,cpp}` | persistência NVS do log de viagens + stash slot pra recuperação após brownout em GRACE |
| `lib/LT7680/` | driver vendor do controller LT7680 |
| `include/config.h` | credenciais WiFi (gitignored) |
