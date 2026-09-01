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
| `esp32_gate/esp32_gate.ino` | ESP32 clássico | GATE fixo/relay |
| `esp32s3_probe/esp32s3_probe.ino` | ESP32-S3 | PROBE móvel/leaf |

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

## Descoberta, relay e protocolo

Inicialmente, cada placa adiciona apenas broadcast `FF:FF:FF:FF:FF:FF`.

```text
BASE  -- BASE_BEACON broadcast --> GATE
GATE  -- HELLO broadcast -------> BASE
GATE  -- GATE_BEACON broadcast -> PROBE
PROBE -- HELLO broadcast -------> GATE
```

BASE cria peer unicast somente para GATE. PROBE cria peer somente para GATE.
BASE ignora tráfego direto de PROBE; PROBE ignora `BASE_BEACON`. Assim o teste
força dois saltos mesmo se BASE ainda for audível. `HELLO` repete cada 3 s.

Link real usa confirmação de aplicação:

```text
PROBE -- PING(route_seq) --> GATE -- RELAY_PING --> BASE
BASE  -- RELAY_PONG ------> GATE -- PONG(route_seq) --> PROBE

GATE  -- HEARTBEAT(seq) -> BASE -- ACK(reply_to=seq) --> GATE
```

GATE faz store-and-forward de aplicação, não repetição RF transparente.
`ONLINE` PROBE depende de PONG vindo pela rota BASE→GATE→PROBE; LED não liga
só por beacon.

Para distinguir a falha de cada salto, GATE responde cada `PING` de PROBE com
`GATE_BEACON` unicast (`reply_to=PING.sequence`) e informa `baseOnline` no
bit 1 de `flags`. Essa resposta confirma PROBE↔GATE, mesmo quando o relay até
BASE falha.

Pacote binário fixo, 32 bytes:

```text
magic(2) version(1) type(1) sender_id(1) origin_id(1) flags(1) hop_count(1)
sequence(4) timestamp_ms(4) reply_to_sequence(4) origin_sequence(4)
metric_ms(4) relay_rssi(1) reserved(3)
```

Tipos: `HELLO`, `BASE_BEACON`, `GATE_BEACON`, `PING`, `PONG`, `RELAY_PING`,
`RELAY_PONG`, `HEARTBEAT`, `ACK`. `origin_sequence` correlaciona PING PROBE
nos dois saltos. `relay_rssi` é medido por GATE ao receber PROBE.

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

No RGB integrado do ESP32-S3:

```text
VERDE    PONG válido via GATE há menos de 2000 ms: PROBE→GATE→BASE→GATE→PROBE
AMARELO  GATE respondeu ao PING, mas falta PONG completo: PROBE↔GATE ativo
VERMELHO GATE não respondeu há 2000 ms
```

Em LED simples, verde/amarelo viram aceso e vermelho vira apagado: não há como
mostrar três cores sem LED RGB.

## Core ESP32/RSSI

Compatibilidade explícita por versão ESP-IDF:

| ESP-IDF | callback RX | RSSI |
|---|---|---|
| 5.1+ | `const esp_now_recv_info_t *info` | `info->rx_ctrl->rssi` |
| anterior | callback MAC legado | `RSSI=N/A` |

Callback TX muda separadamente em ESP-IDF 5.5+: usa `esp_now_send_info_t *`;
anterior usa MAC. Callbacks apenas copiam pacote/contador para fila FreeRTOS.
Validação, peers, Serial e respostas ocorrem em `loop()`.

Este PC possui Arduino-ESP32 3.2.0, baseado em ESP-IDF 5.4: RSSI RX fica
disponível. Boot imprime ESP-IDF detectado e `rx_rssi=AVAILABLE` ou `N/A`.
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

Os três precisam usar o protocolo 2; versões antigas não conversam com este
relay. Usar 115200. Durante ensaio, manter somente BASE conectada ao computador.

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
parser. Ao abrir a porta, o logger descarta bytes pendentes do monitor serial
anterior; terminal recebe timestamp host e `raw.txt` não é modificado.

BASE emite linhas estruturadas:

```text
RX|ms=15234|node=PROBE|hop=END_TO_END|via=GATE|type=RELAY_PING|seq=182|rssi=-63|base_rssi=-58|rtt=7
STAT|ms=16000|node=PROBE|hop=END_TO_END|via=GATE|online=YES|rx=192|lost=2|dup=0|ooo=0|pdr=98.97|rssi=-64|last_seen=231|rtt=7|rtt_avg=8.2
STAT|ms=16000|node=GATE|hop=GATE_BASE|online=YES|rx=205|lost=0|dup=0|ooo=0|pdr=100.00|rssi=-58|last_seen=34|rtt=4|rtt_avg=4.3
EVENT|ms=19000|node=PROBE|state=OFFLINE
```

`summary.txt`: RX/loss, PDR, RSSI mínimo/máximo/médio, RTT PROBE, p95 e maior
outage. RSSI fica dBm; software nunca inventa metros.

## Procedimento

### Teste 1 — bancada

Três placas próximas. Esperar BASE detectar GATE e PROBE `via=GATE`, ambos
ONLINE, RSSI e sequências coerentes. LED PROBE deve ON sólido.

### Teste 2 — casa

BASE escritório; levar PROBE por cômodos. Observar RSSI, RTT, PDR e LED.

### Teste 3 — portão

BASE escritório, GATE portão. Levar PROBE até portão, depois seguir linha
aproximadamente reta. Verde = cadeia inteira; amarelo = somente PROBE↔GATE;
vermelho = GATE sem resposta. No log BASE, `rssi` é PROBE→GATE e `base_rssi`
é GATE→BASE. Voltar alguns metros e confirmar recuperação automática.

### Teste 4 — limite

Cruzar limite várias vezes. Comparar anotações físicas com timestamps, RSSI,
PDR, RTT e eventos ONLINE/OFFLINE para fading, interferência e sombras.

## Limites conhecidos

- Sem GPS/ranging: medir distância fisicamente.
- Descoberta baseline é aberta/não criptografada. `ensurePeer()` é ponto futuro
  para peers encrypted sem alterar struct.
- Antena, orientação, paredes e interferência Wi-Fi afetam muito resultado.
- `QUEUE_DROPS` BASE indica saturação callback->loop; anotar ensaio separado.
- GATE não amplia o sinal de um pacote no ar: recebe, processa e transmite um
  novo pacote. A taxa útil e latência da rota sofrem dois saltos.
