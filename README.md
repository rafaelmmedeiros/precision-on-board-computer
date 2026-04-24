# P-OBC

Computador de bordo de precisão para Chevrolet Astra 2011 2.0 8V Flex.
ESP32 + Arduino framework sobre ESP-IDF, gerenciado com PlatformIO.

## Build & Upload

```bash
pio run                              # Build
pio run -t upload                    # Upload por USB
pio device monitor                   # Monitor serial (115200 baud)
pio run -t upload && pio device monitor
pio test                             # Testes
pio run -t clean                     # Limpar build
```

## Update wireless (OTA)

A bancada fica longe do PC, então o fluxo normal é flashar por WiFi.

### Primeiro flash (uma vez, por USB)

```bash
pio run -t upload
pio device monitor
```

No serial aparece:

```
IP: 192.168.x.x
OTA pronto em pobc.local
```

Anota o IP — útil se o mDNS não funcionar.

### Flash wireless (daqui em diante)

```bash
pio run -e esp32dev_ota -t upload
```

O environment `esp32dev_ota` está configurado em `platformio.ini` com
`upload_port = pobc.local`. Leva ~15 s em WiFi 2.4 GHz.

### Se `pobc.local` não resolver

Opção 1: instalar o Bonjour Print Services da Apple (dá suporte mDNS no Windows).

Opção 2: trocar em `platformio.ini` o hostname pelo IP direto:

```ini
[env:esp32dev_ota]
upload_port = 192.168.x.x
```

Reserva o IP no admin do roteador (DHCP estático pelo MAC do ESP) pra
não ter que atualizar toda vez.

### Notas

- OTA só funciona se o ESP estiver ligado e na mesma rede WiFi que o PC.
- Cada OTA reinicia o ESP. Na Fase 2+ (carro instalado), colocar guard
  de velocidade/ignição antes de aceitar update.
- Sem senha por enquanto — rede local, projeto pessoal. Adicionar
  `ArduinoOTA.setPassword(...)` se o firmware for rodar em rede compartilhada.

## Credenciais WiFi

Ficam em `include/config.h` (gitignored):

```cpp
#define WIFI_SSID     "SuaRede"
#define WIFI_PASSWORD "SuaSenha"
```

## Estrutura

- `src/` — Código-fonte principal
- `include/` — Headers (`config.h` com credenciais, gitignored)
- `lib/` — Bibliotecas locais
- `test/` — Testes (PlatformIO test runner)
- `platformio.ini` — Configuração PlatformIO

Mais detalhes sobre hardware, pinagem, fases do projeto e decisões de
design: ver `CLAUDE.md`.
