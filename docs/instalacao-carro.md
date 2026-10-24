# Instalação no carro — lista de compras e circuitos de interface

Doc novo (não extraído), complementa `CLAUDE.md` §4 (Hardware). Cobre o que falta
comprar pra sair da bancada e ligar o P-OBC no Astra, e o circuito de interface de
cada sinal do carro. Pinagem continua sendo ground truth em
[pinout.md](pinout.md) — este doc só diz *como* cada fio chega até o pino.

---

## 1. Antes de comprar — o chicote do TID e o que medir

O P-OBC ocupa o lugar do TID original, então o chicote dele é o ponto de partida:
já traz alimentação, ignição, iluminação, o sensor de temperatura externa e,
provavelmente, o sinal de velocidade. **Só injeção e boia precisam de fio novo.**

### 1.1 Pinagem do conector do TID — ponto de partida, não confirmada no carro

Há dois layouts na família Opel/GM, e as cores dizem qual é o seu. Código de cores
Opel: sw preto · rt vermelho · br marrom · bl azul · ws branco · gr cinza ·
ge amarelo · gn verde.

**Layout Astra G / Corsa C (TID de 10 caracteres)** — o mais provável no Astra
brasileiro, que é derivado do Astra G. Fonte:
[Carluccio wiki](https://wiki.carluccio.de/index.php/Opel_TID).

| Pino | Função | Cor (Astra G) | Uso no P-OBC |
|---|---|---|---|
| 1 | Ignição (Kl. 15) | preto | Circuito A → GPIO 13; alimenta o 7805 do GPS |
| 2 | AA — rádio ligado (esconde a data) | branco-vermelho | — |
| 3 | +12 V permanente (Kl. 30) | vermelho | Alimentação (fusível 1 A do lado do P-OBC) |
| 4 | Iluminação noturna dimerizada | cinza-amarelo | Circuito A → GPIO 35 (farol) |
| 5 | Temperatura externa + | azul-branco | NTC original (§1.2) |
| 6 | Massa (Kl. 31) | marrom | GND |
| 7 | Temperatura externa − | azul | NTC original |
| 8 | Diagnóstico | marrom-branco | Não usar |
| 9 | SDV — velocidade (volume do rádio) | azul-vermelho | Circuito B → GPIO 36 (VSS), **se** pulsar |
| 10 | SCL (barramento do rádio) | marrom-vermelho | — |
| 11 | SDA | marrom-preto | — |
| 12 | MRQ | marrom-cinza | — |

**Layout antigo (Corsa B / Astra F, TID de 8 caracteres)**, pra comparação:
1 Kl.30 vermelho · 2 temp. · 3 massa marrom · 4 temp. − · 5 Kl.15 preto ·
6 Kl.58 (farol) cinza-verde · 7 Kl.58d cinza-preto · 8 AA · 9 SCL · 10 MRQ ·
11 SDA · 12 velocidade azul-vermelho. Fontes:
[thecorsa.co.uk](https://www.thecorsa.co.uk/forum/technical-help/tutorials/electrical-tutorials/368-mfd-tid-gid-pinout-and-schematic),
[astrasite.de](https://www.astrasite.de/mid-nachrusten/).

**Qual é o seu:** vermelho no pino 3 e preto no 1 → layout Astra G. Vermelho no 1
e preto no 5 → layout antigo. Confirmar pino a pino no multímetro de qualquer
jeito — os fóruns divergem na numeração, e nada garante que a GM do Brasil seguiu
o código de cores da Opel.

Dois cruzamentos que dão alguma confiança:

- **Azul-vermelho é velocidade nos dois layouts** (pino 9 no novo, 12 no antigo).
  Se pulsar, o VSS sai direto do chicote do TID, sem fio até o câmbio. No Astra F
  esse sinal é da ordem de 16000 pulsos/km (~900 Hz a 200 km/h); o k-factor do
  Astra brasileiro sai da calibração por GPS de qualquer forma.
- **O "fio PT/MR" do pino 27 da ECU** (`CLAUDE.md` §4.3) bate com o *sinal de
  consumo* (Verbrauchssignal) que a ECU Opel entrega ao MID — preto-marrom no
  código Opel ([astrasite.de](https://www.astrasite.de/mid-nachrusten/)). Se for
  isso, o pino 27 é uma saída da ECU feita pra computador de bordo, não o fio cru
  de um injetor: onda quadrada limpa, sem pico indutivo. O circuito B serve nos
  dois casos; o osciloscópio decide.

### 1.2 O que o chicote do TID muda na lista

- **Alimentação e ignição** vêm do conector do TID → os derivadores de fusível
  viram opcionais (só se o Kl.15 do TID cair na partida, §4.1).
- **Temperatura externa:** o carro já tem um NTC no para-choque (GM 9152245, 2
  fios) nos pinos 5/7. Reaproveitar elimina a sonda DS18B20 externa e o cabo até a
  frente do carro. O custo: o NTC é analógico e todos os pinos do ADC1 (32-39) já
  estão tomados → precisa do **ADS1115** (§2.6). Curva do NTC: medir a resistência
  em 2-3 temperaturas conhecidas (gelo fundente, ambiente contra termômetro de
  referência, água morna) e ajustar Steinhart-Hart. Medida física, não tabela de
  fórum.
- **VSS:** se o pino 9 pulsar, o cabo blindado até o câmbio sai da lista.
- **Conector:** pra não cortar o chicote original, arrumar um **TID usado ou
  queimado** e aproveitar o conector da placa dele. Vira um adaptador
  plug-and-play, reversível.
- **Dimmer de graça:** o pino 4 carrega o nível da rodinha de iluminação do painel.
  Num canal livre do ADS1115, o backlight do P-OBC segue o dimmer do carro — que
  é o que o TID original faz, e o mínimo pra não ofuscar à noite.

### 1.3 O que medir no carro

| O quê | Como | Decide |
|---|---|---|
| Layout do conector do TID | Cores + multímetro pino a pino | Tabela do §1.1 |
| Pino 1 (Kl.15) durante a partida | Multímetro DC enquanto dá a partida | Se cair → ignição de outra linha (§4.1) |
| Pino 9 (SDV) | Hz rodando devagar, ou com a roda motriz suspensa | VSS pelo TID ou fio até o câmbio |
| Pino 4 com o dimmer em várias posições | DC e Hz | Analógico ou PWM. Se for PWM, o farol no GPIO 35 precisa de filtro (RC maior ou janela no firmware) |
| Pinos 5/7 (NTC) | Resistência com o TID desconectado; continuidade do 7 com a massa | Referência do NTC |
| Rádio original sem o TID | Ligar com o TID desconectado | Os pull-ups do barramento ficam no TID ([Carluccio](https://wiki.carluccio.de/index.php/Opel_TID)); conferir se o rádio não estranha |
| Pino 27 da ECU | Osciloscópio, ou analisador lógico depois do circuito B | Saída de consumo dedicada ou injetor cru |
| Fio da boia (no conector do painel de instrumentos) | DC com a ignição ligada | R2 do divisor (§3.3). No Astra F a boia chega no pino 7 do velocímetro, azul-preto — pista, não confirmação |
| Fios no sensor de velocidade (só se o pino 9 não servir) | Contar | 3 = Hall, circuito B serve. 2 = indutivo, precisa de comparador (LM393) |
| Tamanho do fusível (só se precisar de derivador) | Olhar | Mini, padrão ou low-profile |

Referência achada pra boia do Astra 2.0 8V Flex 2004-2011 (peça DS 2317, aftermarket):
**~38 Ω cheio, ~283 Ω vazio** ([DS](https://www.ds.ind.br/en/products/fuel-level-sensor/2317)).
Não é especificação GM — confirmar no carro.

---

## 2. Lista de compras

Coluna **Tem?** é pra você marcar o que já está na bancada. Quantidades já incluem
sobra pra erro de solda.

### 2.1 Interface de sinais

| Tem? | Item | Qtd | Usado em | Link |
|---|---|---|---|---|
| [ ] | Optoacoplador **PC817** (DIP-4) | 5 | Ignição, farol (circuito A) |  |
| [ ] | Transistor **BC547** (TO-92) | 10 | Injeção, VSS (circuito B), buzzer |  |
| [ ] | Diodo **1N4148** | 20 | Proteção de reverso em todos os circuitos |  |
| [ ] | **Kit de resistores filme metálico 1%**, 1/4 W, sortido E24 (10 Ω–1 MΩ) | 1 kit | Tudo. Valores usados: 1k, 2k2, 10k, 22k, 100k, 180k, 1M |  |
| [ ] | Capacitor cerâmico **100 nF** | 20 | Filtro nos GPIOs, desacoplamento |  |
| [ ] | Capacitor cerâmico **330 nF** | 2 | Entrada do 7805 |  |
| [ ] | Capacitor eletrolítico **10 µF / 25 V** | 5 | Saída do 7805 |  |
| [ ] | Regulador **L7805CV** (TO-220) | 2 | Trilho comutado do GPS (§3.4) |  |

### 2.2 Sensores e periféricos

| Tem? | Item | Qtd | Notas | Link |
|---|---|---|---|---|
| [ ] | **DS18B20 sonda à prova d'água** (inox, cabo 1-3 m) | 1 | Temp. externa — **só se não reaproveitar o NTC original** (§1.2). Atrás do para-choque/grade, longe do radiador |  |
| [ ] | **DS18B20** (sonda ou TO-92) | 1 | Temp. interna |  |
| [ ] | **DS3231** módulo (ZS-042) + CR2032 | 1 | Ver pegadinha §4.3 |  |
| [ ] | **Buzzer passivo 5 V** (magnético, 12 mm) | 1 | GPIO 27 via BC547 |  |
| [ ] | **Botão pulsador NA**, painel, 12-16 mm, metálico | 2 | R e S |  |
| [ ] | Antena GPS ativa com base magnética, **cabo ≥ 3 m** | 1 | Só se a atual não alcança o ponto de instalação (painel/vidro traseiro) |  |
| [ ] | **ESP32 DevKit 38 pinos** (mesmo modelo) | 1 | Reserva — se o único queimar no carro, o projeto para |  |

### 2.3 Fonte

O projeto da fonte é seu (§4.1) — aqui é só a lista pra não esquecer nada no pedido.

| Tem? | Item | Qtd | Notas | Link |
|---|---|---|---|---|
| [ ] | Porta-fusível inline + fusível de lâmina **1 A** | 1 + 5 | No Kl.30 que vem do conector do TID, do lado do P-OBC |  |
| [ ] | **Derivador de fusível** (tamanho conforme §1.3) | 1 | **Opcional** — só se o Kl.15 do TID cair na partida |  |
| [ ] | Módulo buck **5 V** | 1 | ESP32 (VIN) |  |
| [ ] | Módulo buck **3,3 V** | 1 | Display |  |
| [ ] | Proteção de entrada (reverso + TVS + eletrolítico de entrada) | — | Conforme seu projeto |  |

Ponto concreto pra escolha do módulo buck: os LM2596 comuns gastam ~5-10 mA
parados (LED de power + chaveamento), e a DevKit mais alguns mA (AMS1117 + chip
USB). Com o carro parado isso dá ~0,2 Ah/dia — não mata a bateria em semanas, mas
está longe dos ~10 µA da §4.1. Desoldar o LED do módulo já ajuda.

### 2.4 Montagem e chicote

| Tem? | Item | Qtd | Notas | Link |
|---|---|---|---|---|
| [ ] | Placa perfurada ilhada (fenolite/fibra) 7×9 cm ou maior | 2 | Uma de reserva |  |
| [ ] | Barra de pinos fêmea 1×20 | 2 | ESP32 removível (soquete) |  |
| [ ] | Borne KF301 (parafuso) 2 e 3 vias | 10 | Entrada dos fios do carro |  |
| [ ] | **TID usado ou queimado** (ferro-velho, Mercado Livre) | 1 | Fonte do conector que encaixa no chicote original — adaptador sem cortar fio (§1.2) |  |
| [ ] | **Conector automotivo à prova d'água 8 vias** (macho+fêmea) | 1 | Chicote → P-OBC, pra poder tirar a unidade |  |
| [ ] | Conector 4 vias (botões) e 3 vias (sonda externa, se houver) | 1 cada | |  |
| [ ] | **Cabo blindado 2 vias** (sinal + GND) | 5 m | Injeção (e VSS, se o pino 9 do TID não servir) — malha no GND **só do lado do P-OBC** |  |
| [ ] | Cabo automotivo 0,5 mm² (ou 22 AWG silicone), 4+ cores | 5 m cada | Adaptador do TID até a placa, boia |  |
| [ ] | Terminal olhal | 5 | GND no chassi |  |
| [ ] | Termo-retrátil sortido | 1 kit | |  |
| [ ] | Caixa plástica (patola) pra eletrônica | 1 | |  |

### 2.5 Ferramentas (se ainda não tem)

| Tem? | Item | Por quê | Link |
|---|---|---|---|
| [ ] | Multímetro **com escala de Hz** | Medir VSS rodando devagar sem osciloscópio |  |
| [ ] | Analisador lógico USB 24 MHz 8 canais (clone Saleae) | Ver os pulsos de injeção/VSS **depois** do circuito, no lado 3,3 V. Barato e decisivo pra Fase 2 |  |
| [ ] | Osciloscópio de bolso (opcional) | Ver o sinal cru de 12 V antes de ligar |  |

### 2.6 ADC externo — necessário se reaproveitar o NTC original

| Tem? | Item | Qtd | Por quê | Link |
|---|---|---|---|---|
| [ ] | Módulo **ADS1115** (I²C, 16 bits, 4 canais) | 2 | Um de reserva. ADC do ESP32 é ruidoso e não-linear; o ADS1115 entra no barramento I²C (21/22) que já existe, libera os GPIOs 32/33 e elimina a errata do §4.2. Voltímetro preciso também é o que viabiliza compensar o tempo morto do injetor pela tensão — algo que computador de bordo de fábrica não faz |  |

Alocação natural dos 4 canais: **A0** voltímetro · **A1** boia · **A2** NTC externo
(§1.2) · **A3** nível do dimmer (pino 4 do TID, com RC se for PWM). Os divisores do
§3.3 continuam iguais, só mudam de destino. O ADS1115 aceita até VDD + 0,3 V na
entrada — alimentar em 3,3 V e manter o mesmo teto de ~2,4 V dos divisores.

---

## 3. Circuitos de interface

Regra geral: **pull-ups vão no 3V3 da placa do ESP32**, não no buck do display. GND
de tudo junto, um ponto só no chassi.

### 3.1 Circuito A — opto PC817 (ignição GPIO 13, farol GPIO 35)

Pra linhas de *alimentação* de 12 V (ignição, iluminação). Elas aguentam o mA do
LED sem perceber. Se o pino 4 do TID (§1.1) for PWM, a saída vai pulsar junto e o
100 nF não basta — subir pra 10 µF (τ = 100 ms) ou tratar no firmware como "LOW
em qualquer amostra dos últimos N ms = farol aceso".

Lado do carro:

```
 12 V (ignição ou lanterna) ──[2k2]──●──── PC817 pino 1 (ânodo do LED)
                                     │
                                  1N4148   (cátodo em cima,
                                     │      ânodo embaixo)
 GND do carro ───────────────────────●──── PC817 pino 2 (cátodo do LED)
```

Lado do ESP32:

```
 3V3 (ESP32) ──[10k]──●──── GPIO 13 (ignição) / GPIO 35 (farol)
                      │
                      ├──── PC817 pino 4 (coletor)
                      │
                   [100nF]
                      │
 GND ─────────────────●──── PC817 pino 3 (emissor)
```

- 2k2: ~6 mA no LED a 14,4 V, ~3,5 mA a 9 V na partida — ainda satura.
- Saída active-low (12 V presente → GPIO LOW), igual ao que `PowerState` e
  `telemetryHeadlightOn()` já esperam.
- O 10k externo é **obrigatório** no 35 (sem pull interno) e recomendado no 13
  (o pull interno é fraco pra ambiente de carro).

### 3.2 Circuito B — buffer BC547 alta impedância (injeção GPIO 34, VSS GPIO 36)

Pra *sinais*: não podem ser carregados. O pinout antigo dizia "divisor 5V→3.3V"
pra injeção — não serve pro pior caso. O pino 27 da ECU é provavelmente a saída de
consumo dedicada (§1.1), uma onda quadrada limpa; mas, se for o fio cru de um
injetor, ele fica em **12 V com o bico fechado**, cai pra ~0,5-1,5 V quando a ECU
abre e dá um pico indutivo de **~60-70 V** no fechamento. O VSS pode ser 5 V ou
12 V. Este circuito aguenta todos esses casos e puxa < 0,15 mA da linha.

```
 sinal ──[100k]──●──────●──── base BC547
                 │      │
               [22k]  1N4148  (cátodo em cima, na base;
                 │      │      ânodo embaixo, no GND)
 GND ────────────●──────●──── emissor BC547

 3V3 (ESP32) ──[10k]──●──── GPIO 34 (injeção) / GPIO 36 (VSS)
                      │
                      └──── coletor BC547
```

- Limiar ≈ 0,65 V × (122k / 22k) ≈ **3,6 V** na entrada: ignora o "baixo" sujo do
  driver da ECU, pega 5 V e 12 V.
- Pico de 70 V → 0,7 mA na base, inofensivo. O 1N4148 segura pico negativo.
- **Polaridade:** sinal alto → GPIO LOW. Se o pino 27 for injetor cru, isso dá
  **GPIO HIGH = bico aberto** (largura de pulso = tempo em HIGH). Se for a saída de
  consumo, a polaridade é a que o analisador lógico mostrar.
- Sem consumo com a chave desligada (linha em 0 V → transistor cortado).
- Pinagem do BC547 varia por fabricante — conferir C-B-E no datasheet do lote.

### 3.3 Circuito C — divisor ADC (voltímetro GPIO 32, boia GPIO 33)

```
 sinal ──[1M]──┬──────────┬──── GPIO 32 / GPIO 33
               │          │
             [R2]      [100nF]
               │          │
 GND ──────────┴──────────┘
```

- **Voltímetro:** R2 = **180k** → 16 V vira 2,44 V (topo da faixa linear do ADC com
  atenuação de 11 dB). Ligar **depois do fusível e antes do diodo de reverso**, pra
  não medir a queda do diodo. Consome ~12 µA permanente.
- **Boia:** R2 depende do que você mediu no §1. Alvo: tensão máxima da boia → ~2,0 V
  no pino. `R2 ≈ 1M × 2,0 / (Vmax − 2,0)`. Se Vmax ≤ 2,4 V, dispensa o divisor:
  só 100k em série + 100nF.
- O 1M em série já limita qualquer transiente a dezenas de µA — dispensa diodo de
  grampo (que vazaria e estragaria a leitura nessa impedância).
- Calibração contra multímetro (voltímetro) e contra litros na bomba (boia) —
  medida física, não ajuste a olho (§3.2 do CLAUDE.md).

### 3.4 Trilho comutado do GPS (pendência §7.2 do CLAUDE.md)

Caminho proposto, mais simples que o MOSFET high-side: um **7805 alimentado pelo
12 V da ignição**. Com a chave desligada não há tensão de entrada, então o consumo
é zero por construção. Linear em vez de buck = sem ruído de chaveamento perto do
GPS. Dissipa ~0,3 W a 14,4 V/30 mA, não precisa de dissipador.

```
 12 V ignição ──●──── 7805 pino 1 (IN)
                │
             [330nF]      7805 pino 2 (GND) ── GND
                │
 GND ───────────●

 7805 pino 3 (OUT) ──●──── VCC do GPS
                     │
              [100nF] + [10µF]
                     │
 GND ────────────────●──── GND do GPS

 GPIO 15 (TX) ──[1k]──── RX do GPS
 GPIO 39 (RX) ────────── TX do GPS
```

- Módulos tipo GY-NEO8M aceitam 3,3-5 V no VCC (regulador próprio). Se o seu só
  aceita 3,3 V, ponha um AMS1117-3.3 depois do 7805.
- O **1k no TX é obrigatório** nesse arranjo: `gpsPause()` não desliga a Serial1,
  então o GPIO 15 fica em HIGH com o GPS sem energia e alimentaria o módulo pelo
  diodo de proteção do pino RX. O resistor limita isso a ~0,3 mA.

### 3.5 Buzzer (GPIO 27)

```
 5 V ──●──────────────┐
       │              │
    1N4148         buzzer
 (cátodo no 5 V)   passivo
       │              │
       ●──────────────┴──── coletor BC547

 GPIO 27 ──[1k]──────────── base BC547
 GND ────────────────────── emissor BC547
```

---

## 4. Pegadinhas da instalação

### 4.1 Ignição: escolher uma linha que não cai na partida

Muitas linhas "15" cortam enquanto o motor de arranque gira (alívio de carga dos
acessórios). O debounce do `PowerState` é de 50 ms; uma partida de 1-2 s numa linha
dessas vira "chave desligada" → `POST_TRIP_SUMMARY` no meio da partida. Use uma
linha que fica viva durante a partida — primeiro candidato é o pino 1 do conector
do TID (testar, §1.3); se cair, a do painel de instrumentos. O GPS (§3.4) deve sair
da mesma linha.

### 4.2 GPIO 36 e 39 — errata do ESP32

Errata **GPIO-3.11**: quando o SAR ADC liga, GPIO 36 e 39 são puxados pra baixo por
~80 ns ([Espressif](https://docs.espressif.com/projects/esp-chip-errata/en/latest/esp32/03-errata-description/esp32/gpio-inputs-pulled-down.html)).
O P-OBC tem **VSS no 36 e GPS RX no 39**, e o voltímetro/boia vão usar o ADC. Na
prática: pulsos fantasma no VSS e bytes corrompidos no GPS (aparece como
`failed checksum` na GPS DEBUG) assim que o ADC entrar em uso.

Saídas, em ordem de preferência:
1. **ADS1115** (§2.6) — o ADC interno nunca liga, o problema não existe.
2. Firmware: `adc_power_acquire()` mantém o ADC sempre ligado (+~1 mA, só em
   ACTIVE) e os glitches somem; e ler o VSS pelo periférico PCNT com filtro de
   glitch em vez de interrupt cru.

Não muda a compra, mas vale saber antes de virar `USE_REAL_VOLTAGE=true`.

### 4.3 DS3231 ZS-042 com CR2032

O módulo ZS-042 tem um circuito de recarga feito pra LIR2032 (recarregável). Com
CR2032 (não recarregável) ele tenta carregar a pilha. Alimentado em 3,3 V o efeito
é pequeno, mas o certo é desoldar o resistor de 200 Ω (ou o diodo) ao lado do
suporte da bateria. Aproveite e desolde o LED de power (consumo em sleep).

### 4.4 GPIO 2 (backlight) é strapping

Ao cortar o hard-tie do BL_CONTROL e ligar o GPIO 2 nele, se o breakout tiver
pull-up nesse pino o upload por USB pode começar a falhar ("wrong boot mode") — o
ESP32 só entra em modo de gravação com o GPIO 2 em LOW. Se acontecer: gravar via
OTA (`pio run -e esp32dev_ota -t upload`), que já existe no `platformio.ini`.

### 4.5 Ajustar o buck antes de ligar qualquer coisa

Módulos buck ajustáveis saem de fábrica com tensão aleatória. Ajustar o trimpot no
multímetro **sem carga**, só depois ligar ESP32/display.

### 4.6 USB + 12 V ao mesmo tempo

Nem toda DevKit tem diodo entre o 5 V do USB e o VIN. Com a placa alimentada pelo
carro, não plugar o USB do notebook sem conferir o esquema da sua placa — de novo,
OTA resolve.

---

## 5. Prompt pro Claude (chat com busca web)

A pinagem do TID (§1.1) veio de fontes europeias do Astra G e do Corsa. O que falta
é confirmação específica do Astra brasileiro e a cotação. Colar no chat:

> Tenho um Chevrolet Astra 2011 2.0 8V Flex (ECU Bosch Motronic ME7.9.9,
> plataforma Astra G brasileira) e vou trocar o TID original (relógio/data/
> temperatura, peça A2C53141118 / 093353585) por um computador de bordo próprio,
> reaproveitando o chicote do TID. Pela fonte europeia (wiki.carluccio.de, "Opel
> TID"), o conector de 12 pinos do TID do Astra G é: 1 Kl.15 preto, 2 AA, 3 Kl.30
> vermelho, 4 iluminação dimerizada, 5/7 sensor de temperatura externa, 6 massa,
> 8 diagnóstico, 9 SDV (velocidade) azul-vermelho, 10-12 SCL/SDA/MRQ do rádio.
> Preciso, com fontes brasileiras se possível (diagrama elétrico GM, fóruns
> Clube do Astra, manuais de instalação de computador de bordo aftermarket):
> (1) confirmar essa pinagem no Astra brasileiro, especialmente se o pino 9 tem
> sinal de velocidade em pulsos;
> (2) confirmar se o pino 27 da ECU (fio preto-marrom) é a saída de sinal de
> consumo para computador de bordo ou o fio de um injetor;
> (3) o pino e a cor do fio da boia no conector do painel de instrumentos;
> (4) a curva ou valor nominal do sensor de temperatura externa GM 9152245;
> (5) se o Kl.15 do TID se mantém durante a partida.
> Cite as fontes. Depois, com a lista de compras abaixo, monte um carrinho com
> preços no Mercado Livre e AliExpress, separando o que compensa comprar aqui
> (rapidez) do que compensa importar.
>
> [colar a seção 2 deste documento]

---

## 6. Fontes

Consultadas em 2026-10-24. Nenhuma é documentação GM do Astra brasileiro — tudo
aqui é pista a confirmar no carro.

**Chicote do TID (§1.1)**
- [Carluccio wiki — Opel TID](https://wiki.carluccio.de/index.php/Opel_TID) — pinagem de 12 pinos do TID de 10 caracteres (Astra G / Corsa C), cores, protocolo do barramento SCL/SDA/MRQ (níveis, pull-ups no TID)
- [thecorsa.co.uk — MFD/TID/GID pinout and schematic](https://www.thecorsa.co.uk/forum/technical-help/tutorials/electrical-tutorials/368-mfd-tid-gid-pinout-and-schematic) — layout antigo (Corsa B, conector ZF 32-1), velocidade no pino 12
- [astrasite.de — MID nachrüsten](https://www.astrasite.de/mid-nachrusten/) — conector X19 do TID e X20 do MID no Astra F; sinal de consumo da ECU em preto-marrom; ~16000 pulsos/km no Astra F pós-facelift; boia no pino 7 do velocímetro
- [Clube do Vectra — Vectra B TID, MID, GID](https://www.clubedovectra.com.br/forum/viewtopic.php?t=4424) — funções de TID/MID/CID, MID precisa da boia e do sensor de temperatura
- [Gabisa — TID A2C53141118 / 093353585](https://gabisaonlineautopecas.commercesuite.com.br/painel/relogios-e-computadores/tid-relogio-digital-original-vectra-06-011-astra-99-011) — part number do TID do Astra 99-2011 / Vectra 06-11

**Sensores do carro (§1)**
- [Mercado Livre — sensor de temperatura TID/MID](https://produto.mercadolivre.com.br/MLB-1734217603-sensor-temperatura-tid-mid-agile-astra-vectra-zafira-omega-_JM) — sensor externo de 2 fios, para-choque dianteiro
- [Renotech — sensor de temperatura externa GM 9152245](https://www.renotech.com.br/sensor-temperatura-externa-gm-astra-vectra-zafira-omega/p) — part number do NTC
- [DS — sensor de nível 2317](https://www.ds.ind.br/en/products/fuel-level-sensor/2317) — boia do Astra 2.0 8V Flex 2004-2011, ~38 Ω cheio / ~283 Ω vazio (aftermarket)
- [DS — informativo do sensor de velocidade (VSS)](https://www.ds.ind.br/media/linhas/info/84/c4/df/25%20-%20Informativo%20Sensor%20de%20Velocidade%20(VSS)_web.pdf) — VSS Hall de 3 fios, alimentação 5 V ou 12 V conforme o veículo, teste em Hz

**ESP32 (§4.2)**
- [Espressif — errata GPIO-3.11](https://docs.espressif.com/projects/esp-chip-errata/en/latest/esp32/03-errata-description/esp32/gpio-inputs-pulled-down.html) — GPIO 36/39 puxados pra baixo por ~80 ns quando o SAR ADC liga
