# Mapa de pinos ESP32 — P-OBC

Extraído de `CLAUDE.md` §4.2 pela regra de manutenção. Este arquivo é o ground
truth da pinagem: qualquer repinagem, novo periférico ou pino liberado atualiza
aqui **no mesmo commit**.


A ER-TFT4.58-1 + LT7680 consome **9 GPIOs** entre o bus VSPI principal, a bring-up SW-SPI do painel ST7701S e o BL_CONTROL. Mais 4 pinos pra ignição/farol/botões e 2 pro GPS (Serial1). Total: **15 pinos físicos conectados hoje** — a contagem anterior de "14" estava errada, a tabela abaixo sempre teve 13 linhas.

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
| Dimmer backlight (BL_CONTROL) | 2 | LEDC PWM 5kHz 8-bit. API `tftBacklight(0..255)`. Placa atual é active-high (`BACKLIGHT_ACTIVE_LOW=false`). Hoje o PWM só chega ao LED onboard — alcança o BL_CONTROL do LT7680 após cortar o hard-tie de 3.3V. |
| Sinal de ignição | 13 | Digital active-low. `INPUT_PULLUP` interno + chave/opto pra GND. RTC-capable → `ext0` wake source. Bench: chave protoboard. Produção: PC817. |
| Farol aceso | 35 | Input-only, **sem pull interno** — exige resistor externo 10k pra 3.3V. Active-low. Lido por `telemetryHeadlightOn()`. |
| Botão R | 25 | Pull-up interno, chave pra GND. No carro: botão da alavanca do limpador (contatos a medir, instalacao-carro.md §1.3) |
| Botão S | 26 | Pull-up interno, chave pra GND. Idem |
| GPS RX (Serial1) | 39 (VN) | GPS TX → ESP32. Input-only, sem pull interno — o GPS aciona a linha push-pull em 3.3V, então não precisa resistor. |
| GPS TX (Serial1) | 15 | ESP32 → GPS RX, usado pra config UBX (§5.3). Strapping MTDO: no reset é *entrada* com pull-up HIGH (log de boot habilitado). O RX do GPS é entrada de alta impedância e não puxa a linha pra baixo → seguro. **Não** pôr pull-down nessa linha. No carro, **1k em série**: com o GPS no trilho comutado e sem energia, o TX em HIGH alimentaria o módulo pelo diodo de proteção. |

#### Reservados (pinos comprometidos, hardware ainda não montado)

| Função | GPIO | Notas |
|--------|------|-------|
| I²C SDA (DS3231) | 21 | Padrão Wire — DS3231 ainda não soldado |
| I²C SCL (DS3231) | 22 | Padrão Wire |
| Voltímetro | 32 | ADC1, divisor 1M/180k (16V→2.44V). Mede bateria/alternador. |
| Boia combustível | 33 | ADC1 obrigatório (ADC2 inutilizável com WiFi). Divisor 1M/R2, R2 conforme a tensão medida no fio. |
| Injeção (bicos) ECU | 34 | Input-only, interrupt. Buffer BC547 de alta impedância — provavelmente a saída de consumo da ECU, mas se for fio cru de injetor tem 12 V com picos de ~70 V; divisor direto não serve. GPIO HIGH = bico aberto (no caso do injetor cru; polaridade da saída de consumo a confirmar). |
| VSS (velocidade) | 36 (VP) | Input-only, interrupt. Mesmo buffer BC547 da injeção. **Errata GPIO-3.11:** glitches de ~80 ns quando o ADC liga (ver instalacao-carro.md §4.2). |

Circuitos de interface completos: [Instalação no carro](instalacao-carro.md) §3.

#### Sem pino atribuído (a alocar quando hardware chegar)

| Função | Status | Caminhos |
|---|---|---|
| HOLD da auto-retenção | Contingência (`CLAUDE.md` §7.2) | GPIO 27. Precisa ser pino nativo — num expansor I²C a saída fica travada se o ESP32 travar e o fail-safe some (instalacao-carro.md §3.5). |
| DS18B20 ×2 (interno + externo, OneWire) | Sem alocação | Opção mais natural: **reaproveitar GPIO 4 depois do `tftInit()`**, já que a SW-SPI do painel ST7701S só pulsa esse pino uma vez no boot e depois deixa idle. Custa um `pinMode()` posterior. Outra opção: expansor I²C ou um pino estritamente novo. |

#### Truly livres no ESP32

| GPIO | Notas |
|---|---|
| 27 | Livre (buzzer saiu do escopo). Candidato ao HOLD da auto-retenção, se ela for adotada. |
| 12 | Strapping perigoso (define voltagem do flash interno) — **evitar** salvo último recurso. |
| 0 | Strapping (boot mode) — **evitar**. |

O GPS fechou os dois últimos pinos confortáveis (39 e 15). Qualquer periférico
novo daqui pra frente entra por **I²C (21/22)**, por reuso pós-`tftInit()` dos
pinos do painel (4/14/17), ou paga o preço de 12/0.

**Reuso de 14/17 pra UART foi avaliado e descartado.** Eletricamente daria, mas
`ST7701S_Initial()` deixa os três pinos como saídas push-pull em HIGH sem
teardown; usar 14 como RX de UART faria o CLK do ST7701S oscilar enquanto o CS
(17) também alterna, o que pode empurrar lixo pro registrador de config do
painel. Como 39/15 estavam livres, não valia o risco.

**Notas de design:**

- **Ignição em pino digital dedicado:** GPIO 13 é RTC-capable e funciona como `ext0` wake source — o ESP32 dorme em deep sleep e acorda na transição da chave indo pra LOW. NÃO é strapping pin. Antes da arquitetura always-on a ignição era inferida pela voltagem no GPIO 32; hoje GPIO 32 é só voltímetro.
- **Pinos do painel ST7701S são reutilizáveis:** GPIO 4, 14, 17 só são pulsados uma vez em `ST7701S_Initial()` (chamado dentro de `tftInit()`). Após isso ficam idle e podem ser reconfigurados via `pinMode()` pra outra função (OneWire, UART, etc.). Documentar bem quem reaproveita o quê pra evitar conflito de ordem de init.
- **Display RESET (16) também só é pulsado no boot,** mas é mais arriscado reaproveitar — se algum reset transiente for emitido depois (raro), bagunça o periférico que estiver lá.
- **Expansão futura via I²C:** o bus 21/22 escala — qualquer periférico I²C novo entra sem custo de pino, só conferindo conflito de endereço. Útil quando aparecer um sensor I²C novo, expansor de GPIO ou ADC externo.
