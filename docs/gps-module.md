# GpsModule — link state machine e configuração UBX auto-validada

Referência do módulo de GPS do P-OBC. Indexado a partir de `CLAUDE.md` §5.3.
Arquivos: `include/GpsModule.h`, `src/GpsModule.cpp`, `include/GpsScreen.h`,
`src/GpsScreen.cpp`.


`include/GpsModule.h` + `src/GpsModule.cpp`. Funções livres com prefixo `gps`,
sem namespace (espelha `Telemetry`, o análogo direto). Parser: **TinyGPSPlus
1.1.0** — zero alocação dinâmica, e aceita o talker `$GN` do NEO-8M em modo
GPS+GLONASS.

**Por que a configuração não roda no `setup()`.** O GPS vive num trilho comutado
pela ignição (§4.1), então pode estar sem energia quando o `setup()` corre.
`gpsInit()` só abre a `Serial1`; todo o handshake vive em `gpsTick()`, fatiado
entre frames — cada tick faz no máximo uma ação e retorna, pra caber no orçamento
de 100 ms do loop.

```
ABSENT ──bytes aparecem──> PROBING ──baud travado──> VERIFYING
                                                        │
                                    tudo bate ──────────┼──> READY
                                                        │
                                    algo difere ──> CONFIGURING ──> (re-VERIFYING)

READY ──sem sentença válida por > 5 s──> ABSENT
```

**"Já configurei?" é lido do receptor, não de um flag.** Um flag na NVS
descreveria o que o *ESP32 fez*, não o que o *módulo tem* — troca-se o GPS e o
flag continuaria `true`, deixando o módulo novo com config de fábrica pra sempre.
Em vez disso, `VERIFYING` faz **polls UBX** (mensagem `CFG-*` com payload de
tamanho zero devolve a config corrente) e compara com o desejado. O invariante
verificado é o hardware real, então o mecanismo é idempotente e auto-curativo por
construção.

A aresta **READY → ABSENT** é o que fecha a história da troca de módulo: perder
energia, ser desconectado ou ser substituído derruba o link, e a volta passa
obrigatoriamente por `VERIFYING`. Todo ciclo de ignição já exercita esse caminho.
Nenhuma checagem de identidade (`MON-VER`/`SEC-UNIQID`) é necessária — importa a
config estar certa, não quem é o módulo.

| Frame | Ajuste | Poll de verificação |
|---|---|---|
| `CFG-NAV5` | `dynModel=4` (**Automotive**) via `mask=0x0001` | len 0 → checa `dynModel` |
| `CFG-MSG` | GGA e RMC rate 1; GLL/GSA/GSV/VTG rate **0** | len 2 → checa a rate do port UART1 (índice 1) |
| `CFG-RATE` | `measRate=200 ms` (**5 Hz**), timeRef GPS | len 0 → checa `measRate` |
| `CFG-PRT` | UART1 8N1 **38400** baud, proto UBX+NMEA | len 1 → checa `baudRate` |
| `CFG-CFG` | save (`saveMask=0x061F`, `deviceMask=0x17`) | — (efeito aparece como os 4 acima já corretos) |

Só os frames que falham o poll são reenviados. `CFG-PRT` vai **por último**
porque muda a taxa debaixo dos próprios pés; se a revalidação a 38400 falhar 3
vezes, o módulo desiste e aceita 9600 pelo resto da sessão em vez de reescrever a
cada boot. O probe testa 38400 **e** 9600, então salvar o baud não tranca ninguém
do lado de fora.

`dynModel=Automotive` não é cosmético: ele restringe as premissas de velocidade e
aceleração **vertical** do filtro de navegação, o que reduz o vagar da altitude —
exatamente o ruído que atrapalha a inclinação abaixo.

**Inclinação (grade).** Altitude GPS tem ruído vertical de ±10-15 m, então
`Δalt/Δdist` cru é lixo. O caminho: EMA na altitude (τ ≈ 8 s, `alpha` derivado do
intervalo real entre amostras), âncora re-posicionada a cada **200 m** de
`distanceBetween()`, e gates explícitos — fix válido, `hdop <= 2.0`,
`speed >= 20 km/h`. Fora dos gates o getter devolve `NAN` e a tela mostra `--`,
nunca um número que o sensor não sustenta (§3.2: sem calibração subjetiva).
Clamp de sanidade em ±25%.

`gpsPause()` (chamado nas duas bordas do light sleep) drena o RX, invalida a
âncora de grade — a posição pulou durante o sono — e força o link a `ABSENT`.

**Diagnóstico:** `gpsConfigRuns()` é o número a observar na bancada. Deve chegar
a 1 na primeira partida com um módulo de fábrica e **ficar em 1**. Se subir a
cada boot, ou o `CFG-CFG` não pegou ou algum poll está sendo comparado errado.
`gpsAckCount()`/`gpsNakCount()` contam os ACK-ACK/ACK-NAK do UBX.
