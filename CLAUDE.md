# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

P-OBC (Precision On-Board Computer) — computador de bordo de precisão para Chevrolet Astra 2011 2.0 8V Flex, ECU Bosch Motronic ME7.9.9. ESP32 firmware usando Arduino framework sobre ESP-IDF, gerenciado com PlatformIO. IDE: CLion.

## Developer Context

O programador tem experiência em programação mas é iniciante em sistemas embarcados. Explicações sobre conceitos específicos de embarcados (GPIO, I2C, interrupções, timers, watchdog, etc.) são bem-vindas.

Desenvolvedor não irá perder oportunidade de fazer melhor que BMW.

## Metodologia de Trabalho

Projeto **não tem plano mestre fechado**. Avança de forma iterativa, com o material que estiver à mão no momento (peças que chegaram, ideias novas, restrições descobertas). As "Fases" abaixo são roteiro geral, não cronograma rígido — pula-se de tópico conforme conveniência (ex.: enquanto sensores não chegam, foca em UI simulada e firmware de input).

Implicações práticas:
- Não cobrar sequenciamento das fases; sugerir o que faz sentido **agora** dado o que está disponível.
- Decisões que dependem de hardware ausente ficam em aberto — registrar como "pendente" em vez de forçar.
- Protótipos descartáveis são bem-vindos para validar feeling/UX antes de comprometer com layout final.

## Hardware

- **MCU:** ESP32 WROOM-32, 38 pinos
- **Display dev:** OLED 0.96" I2C SSD1306 (128x64) — protótipo atual
- **Display dev 2:** TFT ILI9341 2.8" SPI — desenvolvimento
- **Display final:** ER-TFT4.58-1 320×960 IPS via LT7680 controller (SPI) + breakout FPC 20 pinos
- **RTC:** DS3231 com CR2032
- **Temperatura:** DS18B20 × 2 (interna e externa)
- **GPS:** NEO-8M com antena externa
- **Alimentação:** Regulador DC-DC buck automotivo (ex: LM2596 ou MP1584) com saída dupla:
  - 5V para ESP32 (via VIN) e periféricos 5V
  - 3.3V dedicado para display final e módulos 3.3V (não usar o regulador interno do ESP32 para display)
  - Entrada: 12-14.4V do carro + fusível 1A
  - GND comum entre todos os componentes
- **Buzzer:** Piezo passivo

## Mapa de Pinos ESP32

Atribuição tentativa cobrindo Fase 1-4. Não é final, mas é o caminho de trabalho. 19 pinos atribuídos, sobra folga (GPIOs 13, 14, 39 input-only livres). Sem necessidade de expansor I²C ou shift register para o escopo atual.

| Função | GPIO | Notas |
|--------|------|-------|
| I²C SDA (display, RTC) | 21 | Padrão Wire |
| I²C SCL (display, RTC) | 22 | Padrão Wire |
| Voltímetro + chave ligada | 32 | ADC1, divisor 14.4V→3.3V. Threshold >8V = ignição ligada (combo: economiza pino) |
| Injeção (bicos) | 34 | Input-only, interrupt, divisor 5V→3.3V |
| VSS (velocidade) | 36 (VP) | Input-only, interrupt, divisor |
| Boia combustível | 33 | ADC1 obrigatório (ADC2 inutilizável com WiFi ligado) |
| GPS RX | 16 | UART2 |
| GPS TX | 17 | UART2 |
| Farol aceso | 35 | Input-only. Usar opto-acoplador (PC817) em vez de divisor — isolamento galvânico contra rede suja do carro |
| Botão R (navegação) | 25 | Pull-up interno |
| Botão S (reset viagem) | 26 | Pull-up interno |
| Buzzer | 27 | PWM (LEDC) |
| DS18B20 ×2 (interna + externa) | 4 | OneWire, ambos sensores no mesmo bus, endereçados por ROM ID 64-bit. Anotar qual ROM ID é interno/externo no firmware |
| Dimmer backlight | 2 | PWM (LED onboard, OK para PWM) |
| Display CS | 5 | VSPI |
| SPI MOSI | 23 | VSPI display |
| SPI MISO | 19 | VSPI display |
| SPI SCK | 18 | VSPI display |

**Notas de design:**

- **Voltímetro como sinal de ignição:** mesmo pino lê tensão do sistema E detecta chave ligada via threshold. Bônus: detecta cranking (queda durante partida) e alternador morto (<13V com motor ligado). Mesma abordagem que OBCs OEM usam.
- **DS18B20 num bus só:** dois sensores compartilham GPIO 4. Identificar ROM ID de cada um uma vez no firmware e fixar.
- **Expansão futura se faltar pino:** PCF8574 / MCP23017 (expansor I²C, +8/+16 pinos no barramento existente) ou ADS1115 (ADC 16-bit I²C, melhor que o ADC nativo do ESP32 — vale a pena para sensor de pressão MHPS-10 da Fase 4 só pela qualidade).

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
Sensor etanol Continental. Sensor pressão combustível MHPS-10.

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
