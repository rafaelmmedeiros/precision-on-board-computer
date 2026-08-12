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

Destino de extração: `docs/<topico>.md`, indexado a partir daqui com **uma linha de resumo + link**. A **§8 (Engineering references)** é o índice central de tudo que já saiu.

Já extraído (o padrão a seguir está nesses arquivos): mapa de pinos, padrões de arquitetura, GpsModule e LT7680 — ver §8. O formato do índice é uma linha por doc:

```markdown
- [LT7680 — capacidades e limites da GPU](docs/lt7680-gpu.md) — primitivas GPU disponíveis, modos de cor, fontes
```

**Regra da extração:** o que fica no CLAUDE.md é a **regra curta** — o que uma sessão precisa saber pra não errar sem abrir mais nada. O que vai pro `docs/` é a **explicação**: tabelas longas, transições passo a passo, justificativas. Se uma seção extraída perdeu uma regra load-bearing no caminho, ela volta pra cá em uma linha.

Arquivos extraídos mantêm o mesmo padrão (markdown, headers nomeados, refs ao código quando útil, sem floreio) e abrem declarando de qual seção do CLAUDE.md saíram.

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
- **`-Wall -Wextra`** — zero warnings novos tolerados. `-Werror` **não** está
  ligado hoje (há dois warnings herdados: um `-Wformat-truncation` em
  `HistoryScreen.cpp:60` e um `-Wimplicit-fallthrough` dentro do TinyGPSPlus).
  Ligar `-Werror` é uma decisão em aberto — depende de limpar ou suprimir esses
  dois.
- **`build_unflags = -std=gnu++11`** é obrigatório. O framework Arduino anexa o
  próprio `-std=gnu++11` depois do nosso `-std=gnu++17`, e o último vence: sem o
  unflag, todo `if constexpr` do projeto compila como extensão avisada do GCC em
  vez de C++17 de verdade.
- **Sem `new`/`delete`/`std::vector` em runtime** — alocação estática
- **`volatile` obrigatório** em variáveis compartilhadas entre ISR e loop
- **Sem calibração subjetiva** — toda escala vem de medida física direta
- **Credenciais WiFi** isoladas em `include/config.h` (gitignored)

### 3.3 Build & upload commands

```bash
pio run                                  # Build (baixa lib_deps: TinyGPSPlus)
pio run -t upload                        # Upload pro ESP32
pio device monitor                       # Monitor serial (115200 baud)
pio run -t upload && pio device monitor  # Build + upload + monitor
pio test                                 # Roda testes unitários (test/ vazio hoje)
pio run -t clean                         # Limpa build
```

---

## 4. Hardware

### 4.1 Componentes ativos

- **MCU:** ESP32 WROOM-32 (38 pinos)
- **Display:** ER-TFT4.58-1 320×960 IPS via LT7680 (SPI) + breakout FPC 20 pinos
- **RTC:** DS3231 com bateria CR2032
- **Temperatura:** DS18B20 × 2 (interna + externa, OneWire)
- **GPS:** NEO-8M com antena externa. **Alimentação em trilho 3.3V comutado pela
  ignição** (pendência de hardware — MOSFET/load switch, ver §7.2): o módulo puxa
  ~25-30 mA continuamente, o que destruiria as metas de ~1 mA em light sleep e
  ~10 µA em deep sleep se ficasse no 3.3V permanente. A bateria de backup do
  próprio módulo preserva efemérides → hot start em ~1-5 s. Enquanto o switch não
  existe, a bancada alimenta direto no 3.3V permanente; o firmware não muda
  (sem energia, o link fica em `ABSENT`).
- **Alimentação:** Buck DC-DC automotivo (LM2596 / MP1584), saída dupla 5V (ESP32) + 3.3V (display e periféricos). **Entrada 12-14.4V vem da bateria direta com fusível 1A** (NÃO mais do 12V chaveado), porque o ESP32 fica energizado 100% do tempo, igual TID/MID factory. Estado de ignição é lido por sinal digital separado em GPIO 13, não pela presença/ausência de tensão de alimentação. Quiescent target: ~10 µA em deep sleep, ~1 mA em light sleep, ~80 mA em ACTIVE.
- **Buzzer:** piezo passivo (não montado ainda)

### 4.2 Mapa de pinos ESP32

**Ground truth completo:** [Mapa de pinos ESP32](docs/pinout.md) — tabelas de
conectados / reservados / sem alocação / livres, e as notas de design (reuso dos
pinos do painel ST7701S, ignição RTC-capable, expansão via I²C).

Resumo pra decisão rápida: **15 pinos conectados** hoje — 9 do display+backlight,
4 de ignição/farol/botões, 2 do GPS (Serial1 em 39/15). Reservados por comentário
mas ainda sem código: 21/22 (I²C), 32 (voltímetro), 33 (boia), 34 (injeção),
36 (VSS). **Só sobram 27 (buzzer), 12 e 0** — os dois últimos são strapping e
devem ser evitados. Ou seja: periférico novo daqui pra frente entra por
**I²C (21/22)** ou por **reuso pós-`tftInit()` de 4/14/17**.

### 4.3 Sinais do carro

- **Pino 27 ECU** → fio PT/MR → sinal de injeção (pulse width em µs) → GPIO 34
- **VSS painel** → velocidade via k-factor calibrado por GPS → GPIO 36
- **Divisor resistivo 12-14.4V** → ADC ESP32 → GPIO 32 (voltímetro)
- **12V chaveado pela chave de ignição** → opto PC817 (LED) → coletor opto → GPIO 13 (active-low, sinal digital de ignição)

---

## 5. Arquitetura

### 5.1 Mapa de módulos

**Diagrama completo:** [Arquitetura — padrões e módulos](docs/architecture.md) —
o mapa ASCII inteiro, com o corpo de cada branch do `loop()` e todas as setas.

O que precisa estar na cabeça sem abrir o doc é a **direção das dependências**:

```
PowerState  →  Telemetry, TripLog          state machine manda no resto
Telemetry   →  Screens                     getters, sentido único
GpsModule   →  GpsScreen, GpsDebugScreen   idem, e isolado da Telemetry
Buttons     →  main → ResetScreen          roteamento de input
Layout.h, Tft, BitmapFont, Fonts → Screens
LT7680 (lib/) → Tft
```

**As setas nunca invertem.** Tela não escreve em Telemetry; Telemetry não conhece
tela nem GPS; GpsModule não conhece Telemetry (§7.2). Quem quebra isso está
começando a criar o acoplamento que o padrão existe pra evitar.

Ordem no `loop()`: `powerTick()` → `applyBacklightPolicy()` → (só em ACTIVE)
`telemetryTick()` → `gpsTick()` → input → render → `flipBuffers()` → `tftTick()`.

### 5.2 Padrões — Telemetry, Screen, input, layout, persistência, power

**Detalhe completo:** [Arquitetura — padrões e módulos](docs/architecture.md) —
cada padrão explicado, mais a state machine do PowerState transição por transição.

As regras que valem em toda sessão, sem precisar abrir o doc:

- **Telemetry é dona de todo o estado físico do veículo.** Telas são render puro e nunca escrevem. `telemetryTick()` roda exatamente **uma vez** por iteração, **antes** de qualquer render. Sensor ou simulação nova entra na Telemetry, não na tela.
- **Cada tela expõe `displayX()` + `xResets()`** e não guarda estado entre frames além de UI ephemera (cursor, animação). Modelo fica na Telemetry ou no TripLog.
- **`main.cpp` roteia o input:** se `resetScreenActive()` → `resetScreenTick(ev)`; senão `handleNavEvent(ev)`. Tap troca tela, Hold R abre o modal de resets, Hold S volta pra home.
- **`Features.h` usa `if constexpr`** — branches mortos somem em compile time, custo zero de runtime. `ANY_SIMULATED` conta **só** os cinco sensores integráveis (INJECTOR, VSS, VOLTAGE, FUEL_LEVEL, TEMP_SENSORS); GPS, IGNITION e FAROL **não** entram. Quando os cinco viram `true`, `BENCH_TIME_SCALE` colapsa pra 1.0.
- **Toda tela nova puxa os rails de `Layout.h`.** Ajuste global = uma constante num lugar.
- **NVS via Preferences** é a persistência do projeto. SD card e exportação estão explicitamente fora de escopo.
- **`powerTick()` é a primeira coisa do loop**, antes de qualquer leitura de telemetria. Fora de ACTIVE o `telemetryTick()` **não** é chamado — motor desligado não avança integradores.

### 5.3 GpsModule — GPS NEO-8M

Extraído por tamanho (regra de manutenção):
[GpsModule — link state machine e config UBX](docs/gps-module.md) — máquina de
estados do link, polls UBX que validam a config no próprio receptor (em vez de um
flag na NVS), pacote automotivo `dynModel=4`, e derivação honesta de inclinação a
partir da altitude.

O essencial pra não errar ao mexer: `gpsInit()` **não** configura nada (o GPS pode
estar sem energia no boot, §4.1) — o handshake vive em `gpsTick()`, fatiado entre
frames. `gpsTick()` roda só em ACTIVE, logo após `telemetryTick()`; `gpsPause()`
nas duas bordas do light sleep. Nenhum outro módulo consome GPS:
`telemetrySpeedKmh()` segue independente por decisão explícita (§7.2).

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
- Hero + dados (Consumption, Autonomy, Gps): split em `LEFT_W=340`, divisor vertical, painel direito até `USR_W`. Na GpsScreen o painel direito empilha 3 linhas (72+72+64 px) dentro da faixa `DIV_Y_TOP`..`FOOTER_DIV` de 236 px — daí os gaps apertados de 8 px. O hero de altitude troca DSEG7_120 por DSEG7_72 quando 4 dígitos (serra, >999 m) não caberiam na coluna.
- Full-width (History): footer rails apenas
- 50/50 (System): exceção legada — divisor centrado em x=478

### 6.3 Gestos de input

| Contexto | Tap R | Tap S | Hold R (3s) | Hold S |
|----------|-------|-------|-------------|--------|
| Tela normal | Tela anterior | Próxima tela | Abre modal de Resets | (1.5s) Volta pra Consumption (home) |
| Modal Resets | Cancela | Cancela | Confirma reset[0] | (3s) Confirma reset[1] |

**Feedback visual:** barrinha de 3 px no rodapé cresce conforme o hold avança (âmbar pra R = destrutivo, verde pra S = nav). Some se soltar antes do threshold. Dentro do modal, cada opção tem sua própria barra abaixo do label.

**Gesto extra — R+S juntos por 1.2s:** revela/oculta as telas de debug (§6.7). Detectado direto de `buttonHoldMs()` em `DEBUG_UNLOCK_MS`, **abaixo** do threshold de 1.5s do Hold-S, pra que o combo seja reconhecido antes do "volta pra home" disparar. Depois do combo o input fica surdo por `DEBUG_COMBO_DEAF_MS` (2.5s) pra engolir os holds que a mesma pressão física ainda vai emitir — o Hold-R chega em 3.0s, então uma janela menor que 1.8s deixaria o modal de reset abrir sozinho.

### 6.7 Telas de debug

Família de telas atrás de `POBC_DEBUG_MODE` (`Features.h`). Existem porque diagnóstico que só sai no monitor serial é inútil no carro — que é exatamente onde os subsistemas rodam.

- **Ficam sempre no fim do array `SCREENS[]`**, depois de `PROD_SCREEN_COUNT`. `ringSize()` decide o tamanho do anel: sem unlock, o anel é só produção e a navegação normal não muda em nada.
- **Unlock não persiste.** Reboot volta travado — default seguro num carro.
- **O flag é macro + constexpr.** O `#if` no inicializador do `SCREENS[]` é o único jeito de as telas realmente saírem do binário (medido: 2.1 KB); um `constexpr` sozinho não elimina, porque o array referenciaria as funções de desenho. O `constexpr DEBUG_MODE` é o que o resto do código consulta, então a fonte de verdade continua uma só.
- **Convenção de conteúdo:** densidade acima de beleza. CGROM 12×24 zoom 1 (12 px/char, 80 colunas), sem DSEG7, verde/vermelho como semáforo por item.
- **Mostrar observado vs desejado, não só OK/BAD.** `GPS DEBUG` exibe `DYN 4/4`, `BAUD 38400/9600` — o valor que o hardware reportou ao lado do que foi pedido. É o que separa ferramenta de diagnóstico de semáforo.

Hoje: **GPS DEBUG** (config UBX verificada no receptor, saúde do link, gates da inclinação, lat/lon). Candidatas naturais quando os sensores chegarem: sensores/ADC e power/sleep.

### 6.4 Frame pacing e double buffer

- `main.cpp` paceia o loop a `FRAME_PERIOD_MS = 100` (10 fps uniforme em todas as telas)
- `flipBuffers()` espera 20 ms após `Main_Image_Start_Address` antes de liberar o canvas — necessário pra evitar tearing quando o desenho dura mais de uma frame de painel (~16 ms a 60Hz). Sem essa espera, telas pesadas (Consumption) piscam.

### 6.5 Anti-burn-in

`tftTick()` desloca a origem do canvas em ciclo de 4 cantos a cada 5 minutos. Pixel shift de 2 px previne image-sticking sem afetar legibilidade.

### 6.6 Backlight automático

`main.cpp::applyBacklightPolicy()` é chamado a cada frame, depois do `powerTick()`. Decide o nível PWM (0..255) com base no estado de power + farol:

| PowerState | Farol off | Farol on |
|---|---|---|
| `ACTIVE` | `BACKLIGHT_LEVEL_DAY` (85) | `BACKLIGHT_LEVEL_NIGHT` (20) |
| `POST_TRIP_SUMMARY` | `BACKLIGHT_LEVEL_DAY` | `BACKLIGHT_LEVEL_NIGHT` |
| `GRACE` | `BACKLIGHT_LEVEL_OFF` (0) | `BACKLIGHT_LEVEL_OFF` |
| `DEEP_SLEEP_PENDING` | `BACKLIGHT_LEVEL_OFF` | `BACKLIGHT_LEVEL_OFF` |

Sinal sai em GPIO 2 via LEDC (5 kHz, 8 bits). **Hoje o PWM só chega ao LED onboard** — pra dimerizar o backlight real do LT7680 falta cortar o hard-tie de 3.3V do BL_CONTROL e levar GPIO 2 até esse pino. A placa atual é active-high (`BACKLIGHT_ACTIVE_LOW=false`); `tftBacklight(0..255)` continua 0=escuro/255=claro e inverte internamente se a flag virar true. Os níveis estão calibrados pra bancada — não cegar o operador é prioridade. Em produção, sob luz solar do carro, faz sentido subir DAY pra 255 e revisitar NIGHT.

---

## 7. Roadmap

### 7.1 Fases — catálogo, não cronograma

> Ler em conjunto com a metodologia iterativa (3.1). Cada fase é um conjunto de features cujo bloqueio é hardware específico.

**Fase 1 — Bancada (sem carro):** RTC DS3231 + NTP fallback, temps DS18B20, voltímetro, dimmer PWM. UI completa rodando sobre simulação (estado atual).

**Fase 2 — Sinais do carro:** interrupt do GPIO 34 (injeção, pulse width µs), VSS no GPIO 36 com k-factor calibrado por GPS — a `VEL` da GpsScreen já é a régua pronta pra essa calibração (ver a divisão de papéis em §7.2). Sinal de ignição já é digital em GPIO 13 (não depende mais do voltímetro), e a detecção automática de fim de viagem (>1h em GRACE) já está implementada via PowerState (§5.8) — quando o opto for montado, basta `USE_REAL_IGNITION=true`.

**Fase 3 — GPS:** altitude, inclinação, heading e velocidade independente **entregues** (GpsModule + GpsScreen, §5.3), com config UBX automotiva persistida e auto-validada. Pendentes na fase: NTP→GPS pra hora atômica, e reverse geocoding offline pra cidade atual (§7.2).

### 7.2 Decisões em aberto

- **Trilho 3.3V comutado pela ignição pro GPS** — pendente (hardware). O NEO-8M
  puxa ~25-30 mA continuamente; no 3.3V permanente isso destrói as metas de ~1 mA
  em light sleep e ~10 µA em deep sleep da §4.1. Falta um MOSFET P-channel
  high-side / load switch acionado pela ignição (ou por um GPIO de enable
  dedicado, se valer poder ligar o GPS com a chave desligada). O firmware já
  assume esse mundo: o handshake UBX é lazy e o link cai pra `ABSENT` sem energia
  (§5.3). Bancada roda no 3.3V permanente até então.

- **VSS vs GPS como fonte de velocidade** — **resolvido**. Não são
  intercambiáveis: o GPS é mais *exato* (Doppler da portadora, ~±0.2-0.4 km/h,
  absoluto, sem calibração), o VSS é mais *preciso e responsivo* (centenas de Hz,
  resolve em baixa velocidade, funciona em túnel e cânion urbano, mas a exatidão
  depende do k-factor, que varia com desgaste/medida/pressão do pneu). A 5 Hz o
  GPS ainda atrasa em aceleração e frenagem forte, o que viraria erro acumulado
  nos integradores de viagem. Divisão de papéis:
  **VSS = fonte de runtime** (trip km, km/L, autonomia);
  **GPS = régua de calibração** do k-factor, mais fallback se o VSS faltar.
  Por isso `telemetrySpeedKmh()` **não** consome `gpsSpeedKmh()` — a velocidade
  GPS fica na GpsScreen como leitura independente, que é justamente o que serve
  pra comparar contra o VSS na Fase 2.

- **Desligar o display em GRACE / DEEP_SLEEP** — resolvido (lógica). `tftBacklight()` em `Tft.h` faz PWM em GPIO 2 (LEDC), `main.cpp::applyBacklightPolicy()` (§6.6) decide os níveis a cada frame. **Pendência de hardware:** o PWM hoje chega só ao LED onboard — pra dimerizar o backlight real do LT7680 falta cortar o hard-tie de 3.3V do BL_CONTROL e levar GPIO 2 até esse pino. A placa atual é active-high, então `BACKLIGHT_ACTIVE_LOW=false`; `tftBacklight()` inverte internamente se um breakout futuro for active-low — basta flipar a flag.
- **Crank guard (delay pós-ignição)** — pendente. OEMs atrasam o boot do TID/MID após girar a chave pra esperar a tensão estabilizar quando o alternador assume; durante a partida a bateria afunda pra ~9-10V e o regime de injeção/VSS fica fora do normal, então qualquer leitura nesse intervalo é lixo (voltímetro vermelho, km/L absurdo). Como o ESP32 é always-on, não é "boot delay" e sim um sub-estado de ACTIVE: ao detectar ignição LOW, segurar UI/telemetria num estado "aquecendo" até ver tensão estável >12.5V por 1-2s, com timeout fixo (~2s) como fallback enquanto o voltímetro não estiver montado. Encaixe natural: novo estado `CRANK_GUARD` no PowerState entre wake e ACTIVE, ou flag interna na Telemetry que congela getters nos últimos valores conhecidos. Decidir entre essas duas formas quando for implementar.
- **Cidade atual via GPS (reverse geocoding offline)** — pendente. GPS dá só lat/lon; nome de cidade exige tabela embarcada. Motivação: animar a viagem mostrando "cruzando Mossoró", "entrando em Natal" — nenhum TID/MID factory faz isso, encaixa direto no espírito "beat the OEMs". Caminhos: (a) lista IBGE de ~5570 municípios como (nome, lat_centro, lon_centro) + nearest-neighbor — cabe em poucas centenas de KB na flash interna, precisão suficiente pra cidade-resolução; (b) polígonos GeoJSON dos municípios + point-in-polygon — preciso na divisa, mas provavelmente exige flash SPI externo (W25Q64 já previsto em §8.1); (c) online via Nominatim/Google só em WiFi — descartado, carro não tem WiFi em movimento. Default natural é (a) com fallback "fora de área" quando o município mais próximo passar de um raio sane (ex. >50 km). Decidir formato da tabela (PROGMEM array, blob em SPIFFS/LittleFS) e onde a info aparece na UI (footer da SystemScreen? tela própria de viagem?) quando entrar na Fase 3.

---

## 8. Engineering references

Referências técnicas extraídas por tamanho (regra de manutenção). Não precisa ler
na entrada — consultar quando a decisão tocar nesses tópicos.

- [Mapa de pinos ESP32](docs/pinout.md) — tabelas de conectados / reservados / livres, e as notas de design de reuso de pino
- [Arquitetura — padrões e módulos](docs/architecture.md) — mapa de módulos completo, Telemetry, padrão Screen, input pipeline, Features.h, Layout.h, persistência NVS, PowerState
- [GpsModule — link state machine e config UBX](docs/gps-module.md) — máquina de estados do link GPS, verificação da config no próprio receptor, pacote UBX automotivo, derivação de inclinação
- [LT7680 — capacidades e limites da GPU](docs/lt7680-gpu.md) — primitivas GPU disponíveis, modos de cor, fontes, limitações reais

---

## 9. Project Structure

### Pastas

```
src/        — código-fonte (.cpp)
include/    — headers (.h)
lib/        — bibliotecas locais (cada subdir é uma lib)
docs/       — referências extraídas do CLAUDE.md (indexadas em §8)
test/       — testes unitários (PlatformIO test runner) — vazio hoje
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
| `GpsModule.{h,cpp}` | NEO-8M: Serial1, NMEA via TinyGPSPlus, link state machine, config UBX automotiva auto-validada, derivação de inclinação (§5.3) |
| `GpsScreen.{h,cpp}` | altitude + inclinação + velocidade/heading + qualidade do fix e estado do link |
| `GpsDebugScreen.{h,cpp}` | tela de debug (§6.7): config UBX observada vs desejada, saúde do link, gates da inclinação, lat/lon |
| `lib/LT7680/` | driver vendor do controller LT7680 |
| `include/config.h` | credenciais WiFi (gitignored) |
