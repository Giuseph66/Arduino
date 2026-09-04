# Comparação de Jammers nRF24L01 + ESP32

> Análise feita em 04/09/2026

---

## Projetos analisados

| Projeto | Arquivo local | Stars | Módulos |
|---|---|---|---|
| **Seu código** (`rf-24.ino` / `rf-24-c2.ino`) | `../rf-24.ino`, `../rf-24-c2/` | — | 1 ou 2 |
| **smoochiee** (original arquivado) | `smoochiee_original/FOR_DUAL_PINS.ino` | 1.1k | 2 |
| **SpiritBox** (rubberpirate) | `spiritbox/spiritbox.ino` | 9 | 2 + OLED |
| **W0rthlessS0ul/nRF24_jammer** | — | 880 | 1-5 |
| **ESP32-BlueJammer** (EmenstaNougat) | — | 7.9k | 2 (código fechado) |
| **ESPnRF24-Jammer** (chickendrop89) | — | 23 | 2 + OLED |

---

## Comparação técnica detalhada

### 1. Configuração base do rádio

| Parâmetro | Seu código | smoochiee | SpiritBox |
|---|---|---|---|
| `setPALevel` | `RF24_PA_MAX` | `RF24_PA_MAX` | `RF24_PA_MAX` |
| `setDataRate` | `RF24_2MBPS` | `RF24_2MBPS` | `RF24_2MBPS` |
| `setCRCLength` | `RF24_CRC_DISABLED` | `RF24_CRC_DISABLED` | `RF24_CRC_DISABLED` |
| `setAutoAck` | `false` | `false` | `false` |
| `setRetries` | `0, 0` | `0, 0` | `0, 0` |
| `setPayloadSize` | **5** (customizado) | padrão | padrão |
| `setAddressWidth` | **3** (customizado) | padrão | padrão |
| SPI speed | 4 MHz | **16 MHz** | **16 MHz** |
| `startConstCarrier` | ✅ sim | ✅ sim | ✅ sim |

---

### 2. Modo de varredura (loop principal)

| Projeto | Modo ativo | Canal inicial | Faixa | Delay |
|---|---|---|---|---|
| `rf-24.ino` | Sweep sequencial `for` (ch 14-79) | 45 | 14-79 | nenhum |
| `rf-24-c2.ino` | Sweep duplo (pares + ímpares) | 14/15 | 14-79 | nenhum |
| smoochiee | Aleatório OU hopping (chave física) | 45 | 0-79 | `delayMicroseconds(random(60))` |
| SpiritBox | Aleatório OU hopping (chave física) | 45 | 0-79 | `delayMicroseconds(random(60))` |

---

### 3. Diferenças críticas encontradas

#### A) SPI Speed: 4 MHz vs 16 MHz ⚠️ RELEVANTE

Seu código usa `RF24 radio(16, 15, 4000000)` — 4 MHz.
Os projetos de referência usam `16000000` — 16 MHz.

Isso afeta diretamente a velocidade com que o ESP32 troca o canal no rádio.
Com 4 MHz, cada chamada `setChannel()` leva ~4x mais tempo que com 16 MHz.
Em sweeps rápidos sem delay, isso reduz a densidade de cobertura do espectro.

**Ação: trocar para `16000000`**

#### B) Faixa de canais: 14-79 vs 0-79 ⚠️ RELEVANTE

Seu código começa em 14. Os outros começam em 0 ou 2.
Bluetooth usa canais 0-79 (2402-2480 MHz), então você está deixando de cobrir
os canais 0-13 (2400-2414 MHz), que incluem os canais de advertising BLE
37 (2402 MHz) e 38 (2426 MHz — este último parcialmente coberto).

BLE advertising channels: 37 = ch 0, 38 = ch 12, 39 = ch 39
Seu sweep começa em 14 → **está pulando os canais BLE 37 e 38!**

**Ação: começar em 0 ou 2**

#### C) Modo aleatório vs sweep sequencial

Os projetos mais usados (smoochiee, SpiritBox) implementam dois modos:
- **Aleatório**: `random(80)` com micro-delays — mais eficaz contra FHSS
- **Hopping sequencial**: passo fixo com bounce — mais previsível

Seu código usa apenas sweep sequencial com `for`. Não tem modo aleatório.
Para Bluetooth clássico (que usa FHSS com 79 canais), o modo aleatório é
geralmente mais eficaz pois cobre canais de forma menos previsível.

#### D) `setPayloadSize(5)` e `setAddressWidth(3)` — modificação na biblioteca

Seu código define payload de 5 bytes e endereço de 3 bytes (modificados no
RF24.cpp local). O smoochiee não faz isso — usa os padrões da biblioteca.
Essa modificação é relevante para `startConstCarrier` no chip nRF24L01+
(variante `isPVariant()`). O smoochiee menciona que o RF24.cpp precisa ser
editado na linha 1972 para o `startConstCarrier` funcionar corretamente.

A nota do seu código indica que essas edições foram feitas diretamente na
biblioteca local — isso é correto e necessário para módulos nRF24L01+.

#### E) `esp_wifi_disconnect()` — o smoochiee chama, o seu não

Detalhe menor: smoochiee/SpiritBox chamam `esp_wifi_disconnect()` além de
`esp_wifi_stop()`. Não costuma fazer diferença prática pois o WiFi já é parado.

---

## Por que pode não estar funcionando?

Possíveis causas, em ordem de probabilidade:

### 1. Alimentação insuficiente do nRF24L01 (causa mais comum!)
O nRF24L01+ em PA_MAX consome picos de ~150 mA. O regulador de 3.3V do ESP32
tipicamente fornece 200-300 mA no total. Com dois módulos, isso pode causar
instabilidade ou falha silenciosa no `radio.begin()`.

**Solução obrigatória**: colocar capacitor eletrolítico de 10-100 µF entre
VCC e GND de cada módulo nRF24. Isso é mencionado em TODOS os projetos de
referência como requisito. Sem o capacitor, o rádio pode iniciar mas ter
comportamento errático.

### 2. SPI a 4 MHz em vez de 16 MHz
Reduz drasticamente a velocidade de troca de canal. Teste com 16 MHz.

### 3. Faixa de canais incompleta (começa em 14)
Está pulando canais BLE 37 e 38. Mudar para começar em 0.

### 4. Módulo sem antena PA/LNA
Os projetos de referência e toda a comunidade recomendam fortemente o
módulo **nRF24L01+ PA/LNA** (com amplificador externo e antena SMA),
não o módulo básico sem amplificador. O módulo básico tem alcance de
~10 metros em campo aberto. O PA/LNA chega a 100+ metros.

Se o seu módulo é o básico (sem antena grande), o alcance é muito limitado
e a potência de interferência é bem menor mesmo em PA_MAX.

### 5. `startConstCarrier` pode não funcionar em módulos genéricos Si24R1
Módulos chineses baratos às vezes usam chip Si24R1 (clone) que não é
`isPVariant()`. Nesse caso, `startConstCarrier` não envia o payload em loop
e a portadora pode ser menos estável.

---

## Recomendações de melhoria para o seu código

```cpp
// 1. Aumentar SPI speed de 4MHz para 16MHz
RF24 radio(16, 15, 16000000);  // era 4000000

// 2. Começar sweep em 0, não em 14
for (int i = 0; i < 80; i++) {  // era: for (int i = 14; i < 79; i++)
    radio.setChannel(i);
}

// 3. Adicionar modo aleatório (mais eficaz contra FHSS/BT)
void randomMode() {
    radio.setChannel(random(80));
    radio1.setChannel(random(80));
    delayMicroseconds(random(60));
}
```

---

## Resumo: é o código ou o módulo?

**Provavelmente os dois contribuem**, mas:

- O código tem diferenças corrigíveis (SPI speed, faixa de canais)
- A causa mais provável de "não funcionar" é **falta de capacitor** ou
  uso de **módulo básico sem PA/LNA**
- Os projetos com mais estrelas (smoochiee 1.1k, EmenstaNougat 7.9k) confirmam
  funcionamento mas todos usam **nRF24L01+ PA/LNA** com capacitor

Se aplicar as correções acima e ainda não funcionar bem → módulo provavelmente
é o limitante (básico sem amplificador, ou clone Si24R1 com problemas).

---

## Links de referência

- smoochiee (original): https://github.com/smoochiee/Ble-jammer
- smoochiee (atualizado): https://github.com/smoochiee/Noisy-boy-esp32-Bluetooth-jammer
- SpiritBox: https://github.com/rubberpirate/SpiritBox
- W0rthlessS0ul: https://github.com/W0rthlessS0ul/nRF24_jammer
- ESP32-BlueJammer: https://github.com/EmenstaNougat/ESP32-BlueJammer
- ESPnRF24-Jammer: https://github.com/chickendrop89/ESPnRF24-Jammer
