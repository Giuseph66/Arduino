# ESP32 Image Display System

Sistema completo para enviar imagens do computador para ESP32 e exibi-las em uma tela redonda 240x240.

## 🚀 Funcionalidades

- **Servidor Node.js** com interface web moderna
- **Upload de imagens** via drag & drop
- **Conversão automática** para formato RGB565 (240x240)
- **Comunicação WiFi** entre servidor e ESP32
- **Exibição automática** de imagens no display
- **Galeria de imagens** convertidas
- **Interface responsiva** e intuitiva

## 📋 Pré-requisitos

### Hardware
- ESP32 (qualquer modelo)
- Display GC9A01 240x240 (redondo)
- Conexões SPI para o display

### Software
- Node.js (versão 14 ou superior)
- Arduino IDE
- Bibliotecas Arduino:
  - `TFT_eSPI`
  - `WiFi`
  - `HTTPClient`
  - `ArduinoJson`

## 🛠️ Instalação

### 1. Configurar o Servidor Node.js

```bash
# Navegue para a pasta do servidor
cd server

# Instale as dependências
npm install

# Inicie o servidor
npm start
```

O servidor estará disponível em: `http://localhost:3000`

### 2. Configurar o ESP32

1. **Instale as bibliotecas necessárias** no Arduino IDE:
   - TFT_eSPI
   - ArduinoJson

2. **Configure o WiFi** no arquivo `esp32_image_receiver.h`:
   ```cpp
   const char* ssid = "SEU_WIFI_SSID";
   const char* password = "SUA_SENHA_WIFI";
   const char* serverURL = "http://192.168.1.100:3000"; // IP do seu servidor
   ```

3. **Configure o display** no arquivo `User_Setup.h`:
   ```cpp
   #define TFT_DC    16
   #define TFT_RST   4
   #define TFT_CS    5
   #define TFT_MOSI  23
   #define TFT_SCLK  18
   ```

4. **Carregue o código** no ESP32

## 🎯 Como Usar

### 1. Acesse a Interface Web
Abra seu navegador e vá para `http://localhost:3000`

### 2. Faça Upload de uma Imagem
- Arraste uma imagem para a área de upload
- Ou clique em "Selecionar Imagem"
- A imagem será automaticamente convertida para 240x240

### 3. Visualize o Resultado
- Veja o preview da imagem convertida
- Baixe o arquivo `.h` gerado
- A imagem aparecerá na galeria

### 4. No ESP32
- O ESP32 se conecta automaticamente ao WiFi
- Baixa imagens aleatórias do servidor a cada 10 segundos
- Exibe as imagens no display redondo

## 📁 Estrutura do Projeto

```
oled_240x240/
├── server/                 # Servidor Node.js
│   ├── package.json       # Dependências
│   ├── server.js          # Servidor principal
│   └── public/            # Interface web
│       ├── index.html     # Página principal
│       ├── style.css      # Estilos
│       └── script.js      # JavaScript
├── oled_240x240.ino       # Código principal ESP32
├── User_Setup.h           # Configuração do display
├── esp32_image_receiver.h # Sistema de recebimento
└── README.md              # Este arquivo
```

## 🔧 Configurações Avançadas

### Alterar Porta do Servidor
No arquivo `server.js`, altere:
```javascript
const PORT = 3000; // Mude para a porta desejada
```

### Alterar Intervalo de Troca de Imagens
No arquivo `oled_240x240.ino`, altere:
```cpp
if (millis() - lastImageChange >= 10000) { // 10 segundos
```

### Personalizar Cores do Display
No arquivo `esp32_image_receiver.h`, você pode alterar as cores de fundo e texto.

## 🐛 Solução de Problemas

### ESP32 não conecta ao WiFi
- Verifique se o SSID e senha estão corretos
- Certifique-se de que o ESP32 está próximo ao roteador
- Verifique se o WiFi suporta 2.4GHz (ESP32 não suporta 5GHz)

### Servidor não inicia
- Verifique se o Node.js está instalado
- Execute `npm install` na pasta do servidor
- Verifique se a porta 3000 está livre

### Imagens não aparecem no display
- Verifique a conexão SPI do display
- Confirme as configurações no `User_Setup.h`
- Verifique se o ESP32 está conectado ao servidor

### Erro de memória
- Imagens muito grandes podem causar problemas
- O sistema converte automaticamente para 240x240
- Se persistir, reinicie o ESP32

## 📊 Especificações Técnicas

- **Resolução**: 240x240 pixels
- **Formato**: RGB565 (16-bit)
- **Tamanho máximo**: ~115KB por imagem
- **Protocolo**: HTTP REST API
- **Frequência de atualização**: 10 segundos
- **Conexão**: WiFi 2.4GHz

## 🤝 Contribuição

Sinta-se à vontade para contribuir com melhorias:
1. Faça um fork do projeto
2. Crie uma branch para sua feature
3. Commit suas mudanças
4. Abra um Pull Request

## 📄 Licença

Este projeto está sob a licença MIT. Veja o arquivo LICENSE para mais detalhes.

## 🙏 Agradecimentos

- Comunidade Arduino
- Desenvolvedores das bibliotecas TFT_eSPI e ArduinoJson
- Comunidade ESP32

---

**Desenvolvido com ❤️ para a comunidade maker**
