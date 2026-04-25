# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

P-OBC (Precision On-Board Computer) — computador de bordo de precisão para Chevrolet Astra 2011 2.0 8V Flex, ECU Bosch Motronic ME7.9.9. ESP32 firmware usando Arduino framework sobre ESP-IDF, gerenciado com PlatformIO. IDE: CLion.

## Developer Context

O programador tem experiência em programação mas é iniciante em sistemas embarcados. Explicações sobre conceitos específicos de embarcados (GPIO, I2C, interrupções, timers, watchdog, etc.) são bem-vindas.

Desenvolvedor não irá perder oportunidade de fazer melhor que BMW.

## Hardware

- **MCU:** ESP32 WROOM-32, 38 pinos
- **Display dev:** OLED 0.96" I2C SSD1306 (128x64) — protótipo atual
- **Display dev 2:** TFT ILI9341 2.8" SPI — desenvolvimento
- **Display final:** ER-TFT4.58-1 320×960 IPS via LT7680 controller (SPI) + breakout FPC 20 pinos
- **RTC:** DS3231 com CR2032
- **Temperatura:** DS18B20 × 2 (interna e externa)
- **GPS:** NEO-8M com antena externa
- **SD Card:** Módulo SPI
- **Alimentação:** Regulador DC-DC buck automotivo (ex: LM2596 ou MP1584) com saída dupla:
  - 5V para ESP32 (via VIN) e periféricos 5V
  - 3.3V dedicado para display final e módulos 3.3V (não usar o regulador interno do ESP32 para display)
  - Entrada: 12-14.4V do carro + fusível 1A
  - GND comum entre todos os componentes
- **Buzzer:** Piezo passivo

## Mapa de Pinos ESP32

| Função | GPIO | Notas |
|--------|------|-------|
| I2C SDA (display/RTC) | 21 | Padrão Wire |
| I2C SCL (display/RTC) | 22 | Padrão Wire |
| Injeção (bicos) | 34 | Input-only, interrupt, divisor 5V→3.3V |
| VSS (velocidade) | 36 (VP) | Input-only, interrupt |
| Voltímetro carro | 32 | ADC1, divisor 14.4V→3.3V |
| Buzzer | a definir | PWM |
| DS18B20 (temp) | a definir | OneWire |
| SD Card (SPI) | a definir | VSPI padrão |
| GPS (Serial) | a definir | UART2 |
| Botão R (navegação) | a definir | Pull-up interno |
| Botão S (reset viagem) | a definir | Pull-up interno |
| Dimmer backlight | a definir | PWM |

## Sinais do Carro

- Pino 27 ECU → fio PT/MR → sinal de injeção (pulse width em µs)
- VSS painel → velocidade via k-factor calibrado por GPS
- Divisor resistivo 12-14.4V → ADC ESP32 (voltímetro)

## Fases do Projeto

**Fase 1 — Bancada (zero carro):**
Relógio/data via DS3231 sincronizado por NTP WiFi invisível. Temperaturas DS18B20. Voltímetro. Navegação entre telas por botão R. Buzzer feedback. Dimmer PWM backlight.

**Fase 2 — Sinais do carro:**
Interrupt handler pino 27 captura pulse width µs. Consumo instantâneo L/h e km/L. Duty cycle injetores %. Velocidade VSS com k-factor calibrado por GPS. Acumuladores viagem (km, litros, tempo). Reset viagem botão S.

**Fase 3 — GPS:**
Altitude, inclinação longitudinal/lateral, heading, velocidade independente, sincronização hora atômica substituindo NTP.

**Fase 4 — Features adicionais:**
Log CSV no SD card. Upload WiFi ao chegar em casa. Sensor etanol Continental. Sensor pressão combustível MHPS-10.

## UI Design

- Fundo preto. Texto amber #FCA017
- Cor do valor principal varia com consumo: verde (econômico) → amber (normal) → laranja (alto) → vermelho (duty >75%)
- Pixel shift 1px a cada 5 min (anti image-sticking)
- Breathing de brilho suave a cada 4 min
- Boot: logo Chevrolet + boas-vindas
- Desligar: resumo de viagem por 5 segundos
- Dimmer reage ao farol ligado

## Regras de Código

- C++17
- `volatile` obrigatório em variáveis compartilhadas entre interrupt e loop
- Sem `new`/`delete`/`std::vector` em runtime (alocação estática)
- `-Werror` — zero warnings
- Sem calibração subjetiva — dado físico direto
- Credenciais WiFi isoladas em `include/config.h` (gitignored)

## Build & Upload Commands

```bash
pio run                              # Build
pio run -t upload                    # Upload para o ESP32
pio device monitor                   # Monitor serial (115200 baud)
pio run -t upload && pio device monitor  # Build + Upload + Monitor
pio test                             # Rodar testes
pio run -t clean                     # Limpar build
```

## Decisão pendente: rotação do display final

Estado atual do firmware: `Font_90_degree` + `VDIR=1` (reg 0x12 bit 3). Texto sai **legível mas ponta-cabeça** do ponto de vista do motorista com mount datasheet (FPC na direita).

O hardware limita as opções. Combinações testadas:
- `Font_90_degree` sozinho → texto espelhado (efeito "AMBULÂNCIA")
- `Font_90_degree + VDIR=1` → legível, ponta-cabeça (estado atual)
- `Font_90_degree + HDIR=1` → **chuva de pixel** (corrompe framebuffer SDRAM)
- `Font_90_degree + VDIR=1 + HDIR=1` → chuva de pixel
- Flip 180° em software (coord flip + string reverse) sobre Font_90+VDIR → ponta-cabeça **e** espelhado

O `Font_90_degree` do LT7680 é "CCW 90° + H-flip" embutido (conforme datasheet). `VDIR` é o único scan flip compatível com o framebuffer. Isso deixa uma rotação fixa de 180° off em relação ao que se quer com mount "FPC direita = bottom".

**Caminhos de decisão para o futuro:**

1. **Aceitar ponta-cabeça e inverter mount físico** — girar o display 180° no gabinete (FPC passa a sair pela esquerda). Sem código novo.
2. **Fonte bitmap em software** — abandonar CGROM do LT7680. Tabela ASCII 5×7 ou 8×8 (~760 bytes) rasterizada com `DrawSquare_Fill` em posições rotacionadas pela CPU. Controle total de orientação. Custo: ~20 ms para string de 10 chars via SPI, aceitável para UI 10 Hz.
3. **Mount portrait 320×960** — abandonar landscape, usar display vertical (tipo sidebar automotivo). `Font_0_degree` default, sem flips, layout natural. Requer redesenho da UI.

## Limitações e possibilidades de layout no LT7680

**O que a GPU do LT7680 faz nativamente (via ER_TFT):**
- `DrawSquare` / `DrawSquare_Fill` — retângulos (outline e preenchido)
- `DrawCircle_Fill`, `Start_Circle_or_Ellipse` — círculos e elipses
- `Start_Line`, `Line_Start_XY`/`Line_End_XY` — linhas arbitrárias
- `Start_Triangle` / `Start_Triangle_Fill` — triângulos
- `Start_Circle_Square` — retângulos com cantos arredondados
- `DrawPixel` — pixel individual (lento via SPI, só para casos pontuais)
- BTE (Block Transfer Engine) — copiar/mover regiões da SDRAM com ROP codes e alpha blending; útil para sprites e composição
- Múltiplas layers (framebuffers) — pelo menos 11 layers de 614400 bytes cada na SDRAM ~8 MB

**Modos de cor:**
- 16bpp RGB565 (65K cores) — atual
- 24bpp RGB888 (16M cores) — se precisarmos de gradientes suaves, custa 2x memória
- 8bpp — indexado, não aplicável pra UI em cor

**Texto via CGROM interna:**
- Fontes 8×16, 12×24, 16×32 ASCII (Latin-1 / ISO-8859-1/2/3/4)
- Zoom 1x a 4x (largura e altura independentes) → efetivo de 8×16 até 64×128 por char
- Rotação 0° ou 90° (com H-flip embutido no 90°, problema conhecido)
- Background transparente ou colorido

**Pode-se adicionar:**
- **Chip de flash SPI externo** (ex: W25Q64, ligado nos pinos SFI do LT7680) — fontes TrueType pré-rasterizadas de tamanho arbitrário, sprites grandes. Tem slot de footprint na breakout.
- **Bitmap font em software** (alternativa 2 acima) — sem hardware extra.

**Limitações reais:**
- Sem rotação nativa de bitmaps/imagens (só texto 0°/90°)
- Sem antialiasing nativo (bordas de retângulos e círculos são "aliased")
- Sem gradientes nativos — simulável com BTE alpha blending ou muitos rects pequenos
- SPI a 8 MHz no driver atual — ~3 µs por registrador, limita throughput de operações granulares (mas fill/BTE são feitos pela GPU, sem overhead de pixel)

**O que dá pra fazer para o P-OBC (Fase 1-4):**
- **Gauges de barra** (consumo, duty cycle, voltagem) — retângulos preenchidos em gradiente de 3 cores via segmentação manual, trivial
- **Números grandes** (velocidade, km/L) — fontes 16×32 com zoom 4x dá glyphs de 64×128, legível a metros
- **Indicadores de alerta** — retângulos/círculos piscando via troca de layer
- **Mapa de altitude/inclinação** (Fase 3 GPS) — linhas finas em uma layer, overlay numérico em outra
- **Animações suaves** (boot logo, desligar) — pré-renderizar frames em flash externo e DMA → SDRAM → display via BTE
- **Dimming noturno** — reduzir brilho do backlight via PWM (já previsto no hardware)
- **Pixel shift anti burn-in** — trivial, mover origem do canvas alguns px a cada N minutos

**O que NÃO dá sem esforço extra:**
- Fontes custom proporcionais (precisa de flash externo OU bitmap font em software)
- Texto em ângulos arbitrários (45°, 30°, etc.)
- Antialiasing de verdade (workaround: supersample + BTE)
- Vídeo ou animações complexas em tempo real

## Project Structure

- `src/` — Código-fonte principal (main.cpp com setup/loop do Arduino)
- `include/` — Headers do projeto (`config.h` com credenciais WiFi, gitignored)
- `lib/` — Bibliotecas locais do projeto (cada subdiretório é uma lib separada)
- `test/` — Testes unitários (PlatformIO test runner)
- `platformio.ini` — Configuração do PlatformIO (plataforma, board, dependências)
