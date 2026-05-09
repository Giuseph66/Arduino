# 🚀 Coleção de Projetos Arduino & ESP32

Bem-vindo ao repositório central de projetos de hardware. Este repositório contém uma vasta coleção de experimentos, desde sensores simples até sistemas complexos com servidores Node.js integrados.

## 🛠️ Categorias de Projetos

Os projetos estão divididos por arquitetura para facilitar a localização e compilação:

---

### 🤖 Arduino (Standard AVR / Generic)
Projetos focados em microcontroladores padrão (Uno, Nano, Mega) e sensores básicos.

| Projeto | O que é? | Para que serve? | Como usar? |
|:---:|---|---|---|
| [**BMO**](./ARDUINO/bmo) | Animação de Rosto | Cria uma interface visual estilo "BMO" (Adventure Time). | Use um display OLED e carregue o sketch `bmo.ino`. |
| [**Bracudo**](./ARDUINO/bracudo-controlinho) | Controle de Braço Robótico | Controla servos para movimentação de um braço robótico. | Conecte os servos nos pinos PWM definidos no código. |
| [**GPS**](./ARDUINO/gps) | Leitor de Coordenadas | Obtém latitude/longitude em tempo real via satélite. | Requer módulo GPS (ex: NEO-6M). Use em ambiente aberto. |
| [**EMG Arduino**](./ARDUINO/EMG) | Sensor Muscular | Lê sinais elétricos dos músculos para controle. | Requer sensor EMG. Ideal para próteses ou biofeedback. |
| [**RF Transmissor**](./ARDUINO/transmissoooo_433) | Transmissor 433MHz | Envia sinais sem fio via rádio frequência. | Use em conjunto com o código de receptor. |
| [**RF Receptor**](./ARDUINO/receptor_433) | Receptor 433MHz | Recebe sinais sem fio via rádio frequência. | Sintonize na mesma frequência do transmissor. |
| [**Ultrassônico**](./ARDUINO/ultrasonico) | Medidor de Distância | Mede distâncias usando ondas sonoras. | Use o sensor HC-SR04. O código retorna a distância em cm. |
| [**IR Distância V1**](./ARDUINO/sensor_distancia_infravermelho) | Sensor de Proximidade | Detecta obstáculos próximos usando luz IR (Versão 1). | Ideal para detecção de presença simples. |
| [**IR Distância V2**](./ARDUINO/sensor_distancia_infravermelho_V2) | Sensor de Proximidade | Medição aprimorada de distância com infravermelho. | Versão otimizada para maior precisão. |
| [**Som**](./ARDUINO/sensor_som) | Detector de Ruído | Detecta palmas ou picos de som no ambiente. | Ajuste o potenciômetro do módulo sensor para sensibilidade. |

---

### 🌐 ESP32 / ESP8266 (WiFi & Bluetooth)
Projetos avançados que utilizam conectividade, telas de alta resolução e integração com servidores.

| Projeto | O que é? | Para que serve? | Como usar? |
|:---:|---|---|---|
| [**Café IoT**](./ESP/cafe) | Automação de Cafeteira | Controla o preparo de café via WiFi com servidor Node.js. | Inicie o servidor em `server/` e carregue o sketch no ESP. |
| [**OLED 240x240**](./ESP/oled_240x240) | Visualizador de Imagens | Exibe imagens enviadas via browser para a tela do ESP32. | Acompanha um servidor Node.js para upload e conversão. |
| [**Doom ESP32**](./ESP/Doom) | Engine Gráfica | Uma implementação da engine de Doom para microcontroladores. | Requer display SPI compatível. Renderização em tempo real. |
| [**Relógio Smart**](./ESP/relogio) | Interface de Relógio | Interface completa com ícones, data, hora e notificações. | Utiliza a biblioteca TFT_eSPI para gráficos fluidos. |
| [**Biometria**](./ESP/Biometria) | Trava Biométrica | Gerenciamento de acesso via leitura de digital. | Requer sensor de impressão digital (ex: AS608). |
| [**WhatsApp**](./ESP/whatts) | Notificador/Interface | Integração para exibição de mensagens ou notificações. | Configuração de bot/API necessária no código. |
| [**Microfone**](./ESP/microfone) | Streamer de Áudio | Transmite áudio do microfone para o browser via WebSockets. | Abre um mini-servidor para ouvir o áudio remotamente. |
| [**ESP Cam**](./ESP/Esp_cam) | Câmera WiFi | Captura fotos e faz stream de vídeo via browser. | Requer módulo ESP32-CAM. Inclui scripts Python. |
| [**Giroscópio 3D**](./ESP/Giro_3D) | Visualização Espacial | Mostra a inclinação do dispositivo em um modelo 3D. | Requer sensor MPU6050 ou similar. |
| [**Tela Touch**](./ESP/tela_touch) | Interface Tátil | Exemplos de botões e menus interativos em telas touch. | Calibração de touch incluída no código inicial. |
| [**Relé WiFi**](./ESP/esp_rele) | Controle de Carga | Liga/Desliga lâmpadas ou aparelhos via WiFi. | Use para automação residencial simples via browser. |
| [**EMG ESP**](./ESP/EMG) | Eletromiografia WiFi | Sensor muscular com transmissão de dados sem fio. | Versão para ESP32 do sensor de atividade muscular. |
| [**Receptor IR**](./ESP/Receptor_infra) | Captura de Comandos | Decodifica sinais de controles remotos infravermelho. | Use para aprender códigos de controles de TV/AC. |
| [**Acelerômetro**](./ESP/acelerometro_giros) | Dados de Movimento | Leitura bruta de eixos X, Y, Z e rotação. | Ideal para projetos de estabilização ou detecção de queda. |
| [**Bluetooth**](./ESP/blutu) | Controle via BT | Comunicação serial via Bluetooth Classic ou BLE. | Controle o ESP32 usando aplicativos de terminal BT. |
| [**Automação IR**](./ESP/controla_infra_rele) | IR + Relé | Central que recebe IR e aciona relés físicos. | Ponte entre controles remotos comuns e automação real. |
| [**Display Lab**](./ESP/display) | Biblioteca Visual | Diversos exemplos de desenhos e fontes em telas TFT. | Útil para testar novas telas e layouts. |
| [**Envia/Recebe**](./ESP/envia_recebe) | Comunicação Geral | Template para troca de dados entre dispositivos. | Base para projetos de telemetria ou chat local. |
| [**Giroscópio**](./ESP/giroscopico) | Sensor de Ângulo | Medição precisa de inclinação e guinada. | Versão focada apenas no sensor de giroscópio. |
| [**Infravermelho**](./ESP/infra_vermelho) | Emissor de Sinal | Emula controles remotos para controlar TVs e outros. | Envie comandos IR gravados ou predefinidos. |
| [**Leitor B/C**](./ESP/leitor_b_c) | Decodificador | Interface para leitura de códigos de barras ou QR. | Use para sistemas de inventário ou controle de acesso. |
| [**Matriz 4x4**](./ESP/matrix_4x4) | Teclado Numérico | Interface de entrada para senhas e comandos. | Conecte um teclado de membrana 4x4 nos pinos indicados. |

---

## 🚀 Como Compilar e Usar

1.  **Ambiente**: Recomendamos o **Arduino IDE** (com suporte a ESP32 instalado) ou **VS Code + PlatformIO**.
2.  **Bibliotecas**: Verifique os `#include` no topo de cada arquivo `.ino`. As principais utilizadas são:
    *   `TFT_eSPI` (Para displays)
    *   `WiFi.h` (Para ESP32)
    *   `Adafruit_Sensor` (Para sensores diversos)
3.  **Configuração de Placa**:
    *   Para a pasta `ARDUINO/`: Selecione "Arduino Uno" ou "Nano".
    *   Para a pasta `ESP/`: Selecione "ESP32 Dev Module" ou "Generic ESP8266 Module".
4.  **Servidores Node.js**: Em pastas como `cafe` ou `oled_240x240`, entre na subpasta do servidor e execute:
    ```bash
    npm install
    node server.js
    ```

## 📜 Licença
Este repositório é de uso **restrito e privado**. O uso de qualquer conteúdo aqui presente requer a autorização expressa do proprietário. Consulte o arquivo [LICENSE](./LICENSE) para mais detalhes sobre os termos de uso e como entrar em contato.
