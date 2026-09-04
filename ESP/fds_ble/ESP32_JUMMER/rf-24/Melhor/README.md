# Melhor.ino — Jammer 2.4 GHz
### ESP32 + 2× nRF24L01+ PA/LNA

> Projeto de pesquisa e aprendizado sobre interferência RF.
> O uso de jammers é ilegal em muitos países — utilize apenas em ambientes controlados e isolados para fins educacionais.

---

## O que esse projeto faz

Usa dois módulos nRF24L01+ PA/LNA conectados aos dois barramentos SPI independentes do ESP32 (HSPI e VSPI) para emitir portadora RF contínua e varrer todo o espectro 2.4 GHz entre os canais 0 e 79 (2400–2479 MHz).

Essa faixa cobre:
- **Bluetooth Clássico** — 79 canais, 2402–2480 MHz
- **BLE (Bluetooth Low Energy)** — 40 canais, incluindo advertising nos canais 37 (2402 MHz), 38 (2426 MHz) e 39 (2480 MHz)
- **Wi-Fi 2.4 GHz** — canais 1–13, dentro da mesma faixa
- **Drones e controles RC 2.4 GHz** — canais 0–125 (este código cobre 0–79)

---

## Hardware necessário

| Componente | Quantidade | Observação |
|---|---|---|
| ESP32 Dev Module (38 pinos) | 1 | Qualquer ESP32 com HSPI e VSPI disponíveis |
| nRF24L01+ **PA/LNA** | 2 | **Com amplificador e antena SMA** — não o módulo básico |
| Capacitor eletrolítico 100 µF | 2 | Um por módulo nRF24, tensão mínima 6V |
| Placa PCB de cobre (protótipo) | 1 | Para solda permanente |
| Fio 22–28 AWG | — | Para as conexões |
| Antenas 2.4 GHz (SMA) | 2 | Já incluídas no módulo PA/LNA |

---

## Mapa de pinos

### Radio 1 — HSPI

| nRF24L01+ | ESP32 GPIO |
|---|---|
| VCC | 3.3V |
| GND | GND |
| CE | **GPIO 16** |
| CSN | **GPIO 15** |
| SCK | **GPIO 14** |
| MOSI | **GPIO 13** |
| MISO | **GPIO 12** |
| IRQ | (não conectar) |

### Radio 2 — VSPI

| nRF24L01+ | ESP32 GPIO |
|---|---|
| VCC | 3.3V |
| GND | GND |
| CE | **GPIO 22** |
| CSN | **GPIO 21** |
| SCK | **GPIO 18** |
| MOSI | **GPIO 23** |
| MISO | **GPIO 19** |
| IRQ | (não conectar) |

---

## Montagem do capacitor — ESSENCIAL

O capacitor eletrolítico de 100 µF deve ser soldado **diretamente nos pinos VCC e GND de cada módulo nRF24**, o mais próximo possível do módulo.

```
nRF24 módulo
  VCC ──┬──────── 3.3V do ESP32
        │
       (+) capacitor 100 µF
        │
       (-) capacitor 100 µF
        │
  GND ──┴──────── GND do ESP32
```

**Por que é obrigatório:**
O nRF24L01+ em PA_MAX com amplificador pode puxar picos de corrente de 130–250 mA em microssegundos. O regulador 3.3V do ESP32 não consegue responder rápido o suficiente, causando quedas de tensão instantâneas que fazem o rádio resetar silenciosamente ou emitir com potência reduzida. O capacitor age como um reservatório de energia local que supre esses picos.

**100 µF é suficiente?**
Para uso em PCB com trilhas curtas, sim. Se tiver oscilações ou comportamento errático, pode adicionar em paralelo um capacitor cerâmico de 100 nF (0.1 µF) ao lado do eletrolítico. O eletrolítico resolve os picos lentos, o cerâmico resolve os picos de alta frequência.

**Polaridade:** o lado marcado com "−" (risco branco na lateral do capacitor) vai ao GND. O lado sem marcação (+) vai ao VCC.

---

## Dicas de montagem na PCB de cobre

1. **Plano de GND**: Se a placa tiver lado cobre sólido, use-o como plano de GND. Conecte todos os GNDs nesse plano. Reduz ruído e melhora estabilidade.

2. **Trilhas de VCC curtas**: A linha 3.3V → VCC do nRF24 deve ser curta e grossa. Trilha fina = resistência = queda de tensão sob carga.

3. **Capacitores próximos**: Solde o capacitor a menos de 5 mm dos pinos VCC/GND do módulo. Distância maior reduz a eficácia.

4. **Separe as antenhas**: Monte os dois módulos com as antenas apontando para direções diferentes (90°) para maximizar a cobertura espacial.

5. **ESP32 alimentação**: Use a alimentação USB ou uma fonte externa de 5V no pino VIN do ESP32. Não alimente pelo 3.3V diretamente de fora — o regulador interno lida melhor com os picos quando alimentado pelo lado de 5V.

---

## Módulo nRF24L01+ PA/LNA — especificações

| Parâmetro | Valor |
|---|---|
| Frequência de operação | 2400–2525 MHz |
| Potência de saída (PA_MAX) | **+20 dBm** (100 mW) com amplificador |
| Sensibilidade de recepção | −94 dBm |
| Consumo em TX (PA_MAX) | ~130–250 mA (com amplificador) |
| Consumo em Standby | ~22 µA |
| Alcance típico ao ar livre | 100–1000 metros (depende das antenas) |
| Interface | SPI, até 10 MHz |
| Tensão de alimentação | 3.3V (não tolera 5V!) |

O módulo básico sem PA/LNA tem saída de apenas **0 dBm** (1 mW) e alcance de ~10 metros. O PA/LNA eleva para 100 mW — 100 vezes mais potência — o que faz diferença enorme na área de interferência.

---

## O que foi melhorado em relação ao código original

| Parâmetro | Código original (`rf-24.ino`) | `Melhor.ino` |
|---|---|---|
| SPI speed | 4 MHz | **16 MHz** (4× mais rápido) |
| Canal mínimo | 14 | **0** (cobre BLE adv. ch 37 e 38) |
| Canal máximo | 79 | 79 |
| Modo sweep | só sequencial | sequencial + aleatório + hopping |
| Rádios | 1 | **2** (cobertura dupla) |
| Desligar ESP32 Wi-Fi/BT | parcial | **completo** (`disconnect` também) |
| Comentários | inglês | **português** |

---

## Como os 3 modos funcionam

### Modo 0 — Sweep Sequencial
Os dois rádios percorrem os canais 0–79 em paralelo, cada um cobrindo metade:
- Radio1: canais pares (0, 2, 4 ... 78)
- Radio2: canais ímpares (1, 3, 5 ... 79)

Resultado: varredura completa do espectro a cada ciclo. Mais eficaz contra dispositivos que ficam num canal fixo (Wi-Fi, alto-falantes BT fixos).

### Modo 1 — Aleatório
Cada rádio salta para um canal completamente aleatório com um micro-delay aleatório entre 10–80 µs. Mais eficaz contra dispositivos com FHSS (Frequency Hopping Spread Spectrum), pois o padrão de interferência é imprevisível e não pode ser "esquivado" facilmente.

### Modo 2 — Hopping Assimétrico
- Radio1: passo +2 com bounce nos extremos
- Radio2: passo +4 com bounce nos extremos

Os dois rádios se movem em velocidades diferentes, garantindo que sempre estejam em regiões distintas do espectro e criando padrões de interferência complexos.

---

## Resultado esperado

Com dois módulos nRF24L01+ PA/LNA a 100 mW cada:

| Alvo | Resultado esperado |
|---|---|
| Bluetooth clássico (fone, caixa de som) | Interferência eficaz em raio de 10–30 m |
| BLE (teclado, mouse, IoT) | Interferência eficaz em raio de 5–20 m |
| Wi-Fi 2.4 GHz | Degradação notável em raio de 5–15 m |
| Drones RC 2.4 GHz | Interferência em canais cobertos (0–79) |

O alcance varia conforme:
- Potência do alvo (BT 5.3 é mais robusto que BT 4.0)
- Obstáculos entre o jammer e o alvo (paredes, metal)
- Qualidade das antenas

---

## Diagnóstico — o rádio não inicia?

O código imprime no Serial (115200 baud) os detalhes de inicialização.
Se aparecer `FALHOU ao iniciar`, verifique:

1. **Capacitor** — está conectado corretamente? Polaridade correta?
2. **GND comum** — todos os GNDs (ESP32 + os dois módulos) estão conectados entre si?
3. **3.3V** — mediu a tensão no VCC do módulo com multímetro? Deve ser entre 3.0V e 3.6V.
4. **Pinos SPI** — confira o mapa de pinos acima. Um pino trocado é a causa mais comum de `radio.begin()` falhar.
5. **Módulo queimado** — se o chip foi alimentado acidentalmente com 5V, pode ter queimado. O nRF24 não tolera 5V.

---

## Dependências (biblioteca)

- **RF24** — versão local em `../../libraries/RF24/`
  A biblioteca local foi modificada nos valores padrão de `payloadSize` (5 bytes) e `addressWidth` (3 bytes), necessários para o `startConstCarrier` funcionar corretamente em chips nRF24L01+ variante P.

---

## Referências consultadas

- https://github.com/smoochiee/Ble-jammer (lógica original)
- https://github.com/rubberpirate/SpiritBox (dual SPI + OLED)
- https://github.com/W0rthlessS0ul/nRF24_jammer (multi-módulo)
- https://github.com/EmenstaNougat/ESP32-BlueJammer (7.9k stars, código fechado)
- Datasheet nRF24L01+ rev2 (em `../../libraries/RF24/datasheets/`)
