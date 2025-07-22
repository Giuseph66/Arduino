# Coleção de Projetos Arduino e ESP32

Este repositório reúne diversos experimentos e protótipos desenvolvidos com placas Arduino, ESP32 e uma variedade de sensores/atuadores. Cada pasta contém um sketch independente em C/C++ (extensão `.ino`) e, em alguns casos, scripts auxiliares.

## Estrutura do repositório

| Projeto | Caminho do sketch principal | Linhas | Descrição resumida |
|---------|----------------------------|--------|--------------------|
| Microfone (root) | `microfone.ino` | ~853 | Grava e transmite áudio utilizando ESP32, WebSockets e display OLED. |
| Microfone (pasta) | `microfone/microfone.ino` | ~916 | Versão modular do projeto de detecção/transmissão de áudio. |
| Giroscópio | `giroscopico/giroscopico.ino` | ~40 | Leitura básica de giroscópio/IMU. |
| Whatts | `whatts/whatts.ino` | ~691 | Controle/automação (detalhes no código). |
| ESP-Cam | `Esp_cam/Esp_cam.ino` | ~165 | Captura de imagem com câmera ESP32-CAM; inclui script Python para tratamento dos dados. |
| Doom | `Doom/Doom.ino` | ~378 | Experimento gráfico/visual (renderiza frames estilo Doom em display). |
| Sensor IR V2 | `sensor_distancia_infravermelho_V2/sensor_distancia_infravermelho_V2.ino` | ~30 | Medição de distância com sensor infravermelho (versão 2). |
| Sensor IR | `sensor_distancia_infravermelho/sensor_distancia_infravermelho.ino` | ~23 | Medição de distância com sensor infravermelho (versão inicial). |
| Sensor de Som | `sensor_som/sensor_som.ino` | ~30 | Detecção de picos de áudio/som ambiente. |
| Bracudo-Controlinho | `bracudo-controlinho/bracudo-controlinho.ino` | ~217 | Movimento de braço robótico “Bracudo” via servo-motores. |
| Display | `display/display.ino` | ~710 | Exemplos de uso de displays gráficos. |
| Comunicação Interface | `com_interface/com_interface.ino` | ~789 | Interface serial/RF entre dispositivos. |
| ESP Relé | `esp_rele/esp_rele.ino` | ~58 | Controle remoto de relé utilizando ESP32/ESP8266. |
| Ultrassônico | `ultrasonico/ultrasonico.ino` | ~138 | Medição de distância com sensor ultrassônico HC-SR04. |
| GPS | `gps/gps.ino` | ~67 | Leitura de coordenadas via módulo GPS. |

> As contagens de linha são aproximadas e foram geradas automaticamente.

## Como compilar

1. Abra o **Arduino IDE** ou **PlatformIO**.
2. Selecione a placa correta (por ex. “ESP32 Dev Module”).
3. Abra o arquivo `.ino` correspondente ao projeto desejado.
4. Instale as dependências listadas no topo do sketch (via *Biblioteca > Gerenciar bibliotecas*).
5. Configure os pinos e parâmetros de hardware conforme comentários no código.
6. Faça o upload para a placa.

## Licença

Este repositório é distribuído sob a licença MIT. Consulte o arquivo `LICENSE` (caso exista) para mais detalhes.
