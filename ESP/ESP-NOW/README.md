# ESP-NOW range test

Laboratório ESP-NOW com três sketches Arduino independentes. Sem roteador,
Internet, MQTT, Bluetooth, criptografia, Long Range ou ajuste de potência TX.

## AVISO: antena ESP32U

> Se BASE for ESP32-WROOM-32U/32UE com conector U.FL/IPEX e sem antena externa,
> talvez não exista antena PCB utilizável. Alcance ficará severamente prejudicado
> e não representa alcance real ESP-NOW. Teste funcional de bancada ainda pode
> funcionar. Primeiro diagnóstico: instalar antena 2.4 GHz correta ou trocar
> BASE temporariamente por ESP32 com antena PCB.

## Arquitetura

```text
                    ESCRITÓRIO
                     ESP32U
                      BASE
                       |
           +-----------+-----------+
           |        ESP-NOW        |
           v                       v
        ESP32                   ESP32-S3
         GATE                    PROBE
        portão                   móvel

BASE USB -> serial_logger.py -> terminal + logs
```

| Sketch | Placa atual | Papel |
|---|---|---|
| `esp32u_base/esp32u_base.ino` | ESP32U | BASE/concentrador |
| `esp32_gate/esp32_gate.ino` | ESP32 clássico | GATE fixo |
| `esp32s3_probe/esp32s3_probe.ino` | ESP32-S3 | PROBE móvel |

Papel vem de `DEVICE_ID`, não modelo físico. Troca futura de placas não exige
redesenhar protocolo.

## Configuração

Todo sketch define no início:

```cpp
ESPNOW_CHANNEL   // 6 em todos
DEVICE_ID        // BASE=1, GATE=2, PROBE=3
SEND_INTERVAL_MS // BASE beacon=1000, GATE=1000, PROBE=500
LINK_TIMEOUT_MS  // BASE/GATE=3000, PROBE=2000
SERIAL_BAUD      // 115200
```

Todos devem permanecer canal 6. Wi-Fi usa STA, `WIFI_PS_NONE` quando API aceita
para reduzir variação de latência. Potência TX permanece padrão legal/core.

## Descoberta e protocolo

Inicialmente, cada placa adiciona apenas broadcast `FF:FF:FF:FF:FF:FF`.

```text
BASE  -- BASE_BEACON broadcast --> GATE / PROBE
GATE  -- HELLO broadcast -------> BASE
PROBE -- HELLO broadcast -------> BASE
BASE  -- BASE_BEACON unicast ---> emissor HELLO
```

BASE identifica MAC + `DEVICE_ID`, cria peer unicast automaticamente. GATE e
PROBE fazem mesmo ao receber beacon. `HELLO` repete cada 3 s, permitindo
redescoberta após reboot. Nenhum MAC precisa ser editado.

Link real usa confirmação de aplicação:

```text
PROBE -- PING(seq) ------> BASE -- PONG(reply_to=seq) --> PROBE
GATE  -- HEARTBEAT(seq) -> BASE -- ACK(reply_to=seq) --> GATE
```

`esp_now_send()` e callback TX significam somente fila/rádio; `ONLINE` depende
de PONG/ACK correspondente. PROBE LED não liga só por beacon.

Pacote binário fixo, 24 bytes:

```text
magic(2) version(1) type(1) sender_id(1) flags(1) reserved(2)
sequence(4) timestamp_ms(4) reply_to_sequence(4) metric_ms(4)
```

Tipos: `HELLO`, `BASE_BEACON`, `PING`, `PONG`, `HEARTBEAT`, `ACK`. `sequence`
é local por transmissor. Recebedor trata wrap `uint32_t`, estima gaps, conta
duplicata imediatamente anterior e fora de ordem.

PROBE/GATE reportam RTT recém-medido uma vez no próximo PING/HEARTBEAT. BASE
registra esse valor; assim logger USB da BASE produz RTT de PROBE sem USB móvel.

## LED PROBE ESP32-S3

Ordem de tentativa:

1. `RGB_BUILTIN` + `rgbLedWrite()` nativo Arduino-ESP32: verde sólido.
2. `LED_BUILTIN` digital.
3. `PROBE_LED_PIN`, inicialmente `-1`.

Se LED não responder, alterar topo de `esp32s3_probe/esp32s3_probe.ino`:

```cpp
#define PROBE_LED_PIN 48          // exemplo; usar GPIO real
#define PROBE_LED_ACTIVE_HIGH 1   // usar 0 se LED ativo baixo
```

Durante descoberta LED pisca curto. Depois:

```text
PONG válido há menos de 2000 ms -> ON sólido
sem PONG válido por 2000 ms       -> OFF
```

## Core ESP32/RSSI

Compatibilidade explícita por versão ESP-IDF:

| ESP-IDF | callback RX | RSSI |
|---|---|---|
| 5.1+ | `const esp_now_recv_info_t *info` | `info->rx_ctrl->rssi` |
| anterior | callback MAC legado | `RSSI=N/A` |

Callback TX muda separadamente em ESP-IDF 5.5+: usa `esp_now_send_info_t *`;
anterior usa MAC. Callbacks apenas copiam pacote/contador para fila FreeRTOS.
Validação, peers, Serial e respostas ocorrem em `loop()`.

Este PC não possui Arduino ESP32 Core nem `arduino-cli`; compilação local não
foi possível. Boot imprime ESP-IDF detectado e `rx_rssi=AVAILABLE` ou `N/A`.
Se Core incomum falhar, guarde versão exata + erro antes de alterar callbacks.

## Gravação

Instale **ESP32 by Espressif Systems** no Arduino IDE. Referências genéricas:

```text
ESP32 clássico BASE/GATE: ESP32 Dev Module
ESP32-S3 PROBE:           ESP32S3 Dev Module
```

Selecione modelo exato se souber fabricante. Ordem recomendada:

1. Abrir/gravar `esp32u_base/esp32u_base.ino` no ESP32U.
2. Abrir/gravar `esp32_gate/esp32_gate.ino` no GATE.
3. Abrir/gravar `esp32s3_probe/esp32s3_probe.ino` no PROBE.

Usar 115200. Durante ensaio, manter somente BASE conectada ao computador.

## Logger serial

```bash
python3 -m pip install pyserial
ls /dev/ttyUSB*
ls /dev/ttyACM*
dmesg | tail
python3 tools/serial_logger.py --port /dev/ttyUSB0 --baud 115200
```

Sem `--port`, lista portas e permite escolha:

```bash
python3 tools/serial_logger.py
```

Não abrir Serial Monitor Arduino enquanto logger possui porta. Ctrl+C encerra e
gera resumo:

```text
logs/
└── YYYY-MM-DD_HH-MM-SS/
    ├── raw.txt      # bytes Serial exatos; sem timestamp adicionado
    ├── probe.txt    # apenas linhas node=PROBE
    ├── gate.txt     # apenas linhas node=GATE
    └── summary.txt  # criado Ctrl+C
```

Linhas desconhecidas ficam em `raw.txt`, aparecem terminal, nunca derrubam
parser. Terminal recebe timestamp host; `raw.txt` não é modificado.

BASE emite linhas estruturadas:

```text
RX|ms=15234|node=PROBE|mac=AA:BB:CC:DD:EE:FF|type=PING|seq=182|rssi=-63|rtt=7
TX|ms=15235|node=PROBE|mac=AA:BB:CC:DD:EE:FF|type=PONG|seq=91|reply_to=182|status=QUEUED
STAT|ms=16000|node=PROBE|mac=AA:BB:CC:DD:EE:FF|online=YES|rx=192|lost=2|dup=0|ooo=0|pdr=98.97|rssi=-64|last_seen=231|rtt=7|rtt_avg=8.2
EVENT|ms=19000|node=PROBE|state=OFFLINE
```

`summary.txt`: RX/loss, PDR, RSSI mínimo/máximo/médio, RTT PROBE, p95 e maior
outage. RSSI fica dBm; software nunca inventa metros.

## Procedimento

### Teste 1 — bancada

Três placas próximas. Esperar BASE detectar GATE + PROBE, ambos ONLINE, RSSI e
sequências coerentes. LED PROBE deve ON sólido.

### Teste 2 — casa

BASE escritório; levar PROBE por cômodos. Observar RSSI, RTT, PDR e LED.

### Teste 3 — portão

BASE escritório, GATE portão. Levar PROBE até portão, depois seguir linha
aproximadamente reta. LED ON = PING/PONG bidirecional recente; OFF = timeout.
Voltar alguns metros e confirmar recuperação automática.

### Teste 4 — limite

Cruzar limite várias vezes. Comparar anotações físicas com timestamps, RSSI,
PDR, RTT e eventos ONLINE/OFFLINE para fading, interferência e sombras.

## Limites conhecidos

- Sem GPS/ranging: medir distância fisicamente.
- Descoberta baseline é aberta/não criptografada. `ensurePeer()` é ponto futuro
  para peers encrypted sem alterar struct.
- Antena, orientação, paredes e interferência Wi-Fi afetam muito resultado.
- `QUEUE_DROPS` BASE indica saturação callback->loop; anotar ensaio separado.
