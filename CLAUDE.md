# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

POBC is an ESP32 firmware project using the Arduino framework, managed with PlatformIO. IDE: CLion.

## Developer Context

O programador tem experiência em programação mas é iniciante em sistemas embarcados. Explicações sobre conceitos específicos de embarcados (GPIO, I2C, interrupções, timers, watchdog, etc.) são bem-vindas.

## Hardware

- **MCU:** ESP32 WROOM-32, 38 pinos (ESP32-DevKitC ou similar)
- **Display:** Módulo OLED 0.96" I2C, controlador SSD1306 (128x64 pixels)
  - Comunicação via I2C (pinos padrão: SDA = GPIO21, SCL = GPIO22)

## Build & Upload Commands

```bash
# Build
pio run

# Upload para o ESP32
pio run -t upload

# Monitor serial (115200 baud padrão)
pio device monitor

# Build + Upload + Monitor
pio run -t upload && pio device monitor

# Rodar testes
pio test

# Limpar build
pio run -t clean
```

## Project Structure

- `src/` — Código-fonte principal (main.cpp com setup/loop do Arduino)
- `include/` — Headers do projeto
- `lib/` — Bibliotecas locais do projeto (cada subdiretório é uma lib separada)
- `test/` — Testes unitários (PlatformIO test runner)
- `platformio.ini` — Configuração do PlatformIO (plataforma, board, dependências)

## Architecture

Segue o modelo Arduino padrão: `setup()` para inicialização e `loop()` para execução contínua. Bibliotecas customizadas devem ser colocadas em `lib/` como subdiretórios independentes. Dependências externas são declaradas via `lib_deps` no `platformio.ini`.
