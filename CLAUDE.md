# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

P-OBC (Precision On-Board Computer) — computador de bordo de precisão para Chevrolet Astra 2011 2.0 8V Flex, ECU Bosch Motronic ME7.9.9. ESP32 firmware usando Arduino framework sobre ESP-IDF, gerenciado com PlatformIO. IDE: CLion.

## Developer Context

O programador tem experiência em programação mas é iniciante em sistemas embarcados. Explicações sobre conceitos específicos de embarcados (GPIO, I2C, interrupções, timers, watchdog, etc.) são bem-vindas.

## Hardware

- **MCU:** ESP32 WROOM-32, 38 pinos
- **Display dev:** OLED 0.96" I2C SSD1306 (128x64) — protótipo atual
- **Display dev 2:** TFT ILI9341 2.8" SPI — desenvolvimento
- **Display final:** ER-TFT4.58-1 320×960 IPS via LT7680 controller (SPI) + breakout FPC 20 pinos
- **RTC:** DS3231 com CR2032
- **Temperatura:** DS18B20 × 2 (interna e externa)
- **GPS:** NEO-8M com antena externa
- **SD Card:** Módulo SPI
- **Alimentação:** Regulador DC-DC automotivo + fusível 1A
- **Buzzer:** Piezo passivo

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

## Project Structure

- `src/` — Código-fonte principal (main.cpp com setup/loop do Arduino)
- `include/` — Headers do projeto (`config.h` com credenciais WiFi, gitignored)
- `lib/` — Bibliotecas locais do projeto (cada subdiretório é uma lib separada)
- `test/` — Testes unitários (PlatformIO test runner)
- `platformio.ini` — Configuração do PlatformIO (plataforma, board, dependências)
