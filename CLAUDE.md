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
- **Alimentação:** Buck DC-DC automotivo (LM2596 / MP1584), saída dupla 5V (ESP32) + 3.3V (display e periféricos), entrada 12-14.4V do carro com fusível 1A
- **Buzzer:** piezo passivo (não montado ainda)

### 4.2 Mapa de pinos ESP32

19 pinos atribuídos. GPIOs 13, 14, 39 input-only sobram. Sem necessidade de expansor I²C ou shift register no escopo atual.

| Função | GPIO | Notas |
|--------|------|-------|
| I²C SDA (display, RTC) | 21 | Padrão Wire |
| I²C SCL (display, RTC) | 22 | Padrão Wire |
| Voltímetro + chave ligada | 32 | ADC1, divisor 14.4V→3.3V. Threshold >8V = ignição ligada |
| Injeção (bicos) | 34 | Input-only, interrupt, divisor 5V→3.3V |
| VSS (velocidade) | 36 (VP) | Input-only, interrupt, divisor |
| Boia combustível | 33 | ADC1 obrigatório (ADC2 inutilizável com WiFi) |
| GPS RX | 16 | UART2 |
| GPS TX | 17 | UART2 |
| Farol aceso | 35 | Input-only. PC817 opto-acoplador (não divisor) — isolamento galvânico |
| Botão R | 25 | Pull-up interno |
| Botão S | 26 | Pull-up interno |
| Buzzer | 27 | PWM (LEDC) |
| DS18B20 ×2 | 4 | OneWire — sensores endereçados por ROM ID 64-bit, anotar interno/externo |
| Dimmer backlight | 2 | PWM (LED onboard, ok pra PWM) |
| Display CS | 5 | VSPI |
| SPI MOSI | 23 | VSPI display |
| SPI MISO | 19 | VSPI display |
| SPI SCK | 18 | VSPI display |

**Notas de design:**

- **Voltímetro como sinal de ignição:** mesmo pino lê tensão e detecta ignição via threshold. Bônus: detecta cranking (queda durante partida) e alternador morto (<13V com motor ligado).
- **DS18B20 num bus só:** dois sensores compartilham GPIO 4. Identificar ROM ID de cada uma vez no firmware.
- **Expansão futura:** PCF8574/MCP23017 (expansor I²C) ou ADS1115 (ADC 16-bit, vale a pena pro MHPS-10 da Fase 4).

### 4.3 Sinais do carro

- **Pino 27 ECU** → fio PT/MR → sinal de injeção (pulse width em µs) → GPIO 34
- **VSS painel** → velocidade via k-factor calibrado por GPS → GPIO 36
- **Divisor resistivo 12-14.4V** → ADC ESP32 → GPIO 32 (voltímetro + ignição)

---

## 5. Arquitetura

### 5.1 Mapa de módulos

```
main.cpp
  ├─ telemetryTick()        ← uma vez por loop, antes de qualquer render
  ├─ buttonsPoll() → ev     ← input do usuário
  ├─ resetScreenActive() ?  ← se modal ativa, encaminha pro modal
  └─ SCREENS[g_screen].draw()

Telemetry  ──reads──>  Features (flags)
   ▲
   │ getters (read-only)
   │
   ├─ ConsumptionScreen
   ├─ AutonomyScreen
   ├─ SystemScreen
   └─ HistoryScreen ──reads──> TripLog (NVS persistence)

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

### 5.5 Features.h — flags de sensor + fast-forward

`include/Features.h`, namespace `pobc`:

```cpp
constexpr bool USE_REAL_INJECTOR     = false; // GPIO 34
constexpr bool USE_REAL_VSS          = false; // GPIO 36
constexpr bool USE_REAL_VOLTAGE      = false; // GPIO 32
constexpr bool USE_REAL_FUEL_LEVEL   = false; // GPIO 33 (boia)
constexpr bool USE_REAL_TEMP_SENSORS = false; // GPIO 4 (DS18B20)
constexpr bool USE_REAL_GPS          = false; // UART2
constexpr float BENCH_TIME_SCALE     = 60.0f; // 1 real sec = 1 sim min
```

Telemetry usa `if constexpr` nessas flags. Branches mortos somem do binário em compile time — zero custo de runtime.

Quando todas as flags forem true, `BENCH_TIME_SCALE` colapsa pra 1.0 e a sim para. Hoje na bancada todas false — o sim corre 60x acelerado pra animar o medidor de tanque em minutos em vez de horas.

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
- Escrita só em `tripLogFinishCurrentTrip()` (manual encerramento, futuramente auto via ignição-off > 1h)
- Recovery: blob corrompido ou parcial no boot → log começa zerado

NVS é a escolha pra estado persistente do projeto. SD card e exportação foram explicitamente removidos do escopo.

---

## 6. UI conventions

### 6.1 Coordenadas, paleta, fontes

- **Sistema de coords (user-space, `Tft.h`):** `USR_W = 960` × `USR_H = 320`, origem top-left
- **Mount:** `Font_90_degree` + `VDIR=1` (ver seção 8.2 sobre rotação)
- **Paleta** (`Tft.h`):
  - `COL_BG` preto puro
  - `COL_AMBER` (#FCA017) — texto e UI base
  - `COL_GOOD` verde, `COL_HOT` vermelho, `COL_COLD` azul
- **Fontes:**
  - CGROM 8×16, 12×24, 16×32 com zoom 1-4x
  - Bitmap proprietário DSEG7 nos tamanhos 48 / 72 / 120 px (heroes numéricos)
- **Lógica de cor para consumo:** verde >14 km/L · amber 10-14 · vermelho <10. Mesma lógica replicada em barras de histórico.

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

---

## 7. Roadmap

### 7.1 Fases — catálogo, não cronograma

> Ler em conjunto com a metodologia iterativa (3.1). Cada fase é um conjunto de features cujo bloqueio é hardware específico.

**Fase 1 — Bancada (sem carro):** RTC DS3231 + NTP fallback, temps DS18B20, voltímetro, dimmer PWM. UI completa rodando sobre simulação (estado atual).

**Fase 2 — Sinais do carro:** interrupt do GPIO 34 (injeção, pulse width µs), VSS no GPIO 36 com k-factor calibrado por GPS, threshold de ignição no voltímetro. **Detecção automática de viagem** (ignição off > 1h = nova viagem) liga aqui.

**Fase 3 — GPS:** altitude, inclinação, heading, velocidade independente, NTP→GPS pra hora atômica.

**Fase 4 — Extras:** sensor de etanol (Continental), sensor de pressão de combustível (MHPS-10).

### 7.2 Decisões em aberto

- **Rotação do display final** — texto sai legível mas ponta-cabeça com o mount datasheet. Detalhes e opções na seção 8.2.

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

**Texto via CGROM interna:** 8×16, 12×24, 16×32 ASCII (Latin-1/ISO-8859-1/2/3/4), zoom 1-4x independente em W/H, rotação 0° ou 90° (com H-flip embutido no 90° — ver 8.2).

**Limitações reais:**
- Sem rotação nativa de bitmaps (só texto 0°/90°)
- Sem antialiasing (workaround: supersample + BTE)
- Sem gradientes nativos (BTE alpha blending ou muitos rects)
- SPI a 8 MHz — ~3 µs por registrador, limita ops granulares (mas fill/BTE rodam na GPU sem overhead de pixel)

**Disponível pra adicionar:**
- Chip de flash SPI externo (W25Q64) nos pinos SFI do LT7680 — fontes TrueType pré-rasterizadas, sprites grandes
- Bitmap font em software — sem hardware extra

### 8.2 Decisão pendente: rotação do display

Estado atual: `Font_90_degree` + `VDIR=1` (reg 0x12 bit 3). Texto sai **legível mas ponta-cabeça** do ponto de vista do motorista com mount datasheet (FPC à direita).

Combinações testadas:
- `Font_90_degree` sozinho → texto espelhado ("AMBULÂNCIA")
- `Font_90_degree + VDIR=1` → legível, ponta-cabeça (estado atual)
- `Font_90_degree + HDIR=1` → chuva de pixel (corrompe framebuffer SDRAM)
- `Font_90_degree + VDIR=1 + HDIR=1` → chuva de pixel
- Flip 180° em software (coord flip + string reverse) → ponta-cabeça **e** espelhado

`Font_90_degree` no LT7680 é "CCW 90° + H-flip" embutido (datasheet). `VDIR` é o único scan flip compatível com o framebuffer. Sobra rotação fixa de 180° off em relação ao mount "FPC direita = bottom".

**Caminhos:**

1. **Aceitar ponta-cabeça e inverter mount físico** — girar display 180° no gabinete (FPC sai pela esquerda). Sem código novo.
2. **Bitmap font em software** — abandonar CGROM. Tabela ASCII 5×7 ou 8×8 (~760 bytes) rasterizada com `DrawSquare_Fill` em posições rotacionadas pela CPU. Controle total. Custo: ~20 ms por string de 10 chars, aceitável a 10 Hz.
3. **Mount portrait 320×960** — abandonar landscape, sidebar automotivo vertical. `Font_0_degree` default, sem flips. Requer redesenho da UI.

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
| `main.cpp` | setup/loop, init, dispatch de tela, roteamento de input |
| `Telemetry.{h,cpp}` | dona do estado do veículo, sim de regime, integradores, histórico |
| `Features.h` | flags por sensor, BENCH_TIME_SCALE |
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
| `TripLog.{h,cpp}` | persistência NVS do log de viagens |
| `lib/LT7680/` | driver vendor do controller LT7680 |
| `include/config.h` | credenciais WiFi (gitignored) |
