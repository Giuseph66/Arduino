# ESP32 Bluetooth Serial - Controle via Celular

## 📱 **Como Conectar e Usar**

### **1. Aplicativos Recomendados para Android/iOS:**

#### **Android:**
- **Serial Bluetooth Terminal** - GRATUITO
- **Bluetooth Terminal** - GRATUITO
- **nRF Connect** - GRATUITO (para testes)

#### **iOS:**
- **Serial Bluetooth Terminal** - GRATUITO
- **Bluetooth Terminal** - GRATUITO

#### **Windows/Mac:**
- **PuTTY** - GRATUITO
- **Tera Term** - GRATUITO
- **nRF Connect for Desktop** - GRATUITO

### **2. Passos para Conectar:**

1. **Compile e carregue o código no ESP32**
2. **Observe o PIN na tela do display** (será mostrado por 3 segundos)
3. **Abra o aplicativo Bluetooth Serial no celular/computador**
4. **Procure por "Relogio"**
5. **Conecte ao dispositivo usando o PIN: `0000`**
6. **Envie comandos terminados em Enter (\\n)**

### **3. Comandos Disponíveis:**

| Comando | Função | Resposta JSON |
|---------|--------|---------------|
| `PING` | Teste de conectividade | `{"pong":1}` |
| `LED 1` | Liga LED onboard | `{"ok":true,"led":1}` |
| `LED 0` | Desliga LED onboard | `{"ok":true,"led":0}` |
| `STATUS` | Status completo | `{"ok":true,"uptime_ms":X,"led":0/1,"free_memory":X}` |
| `MEMORY` | Memória livre | `{"memory":X}` |
| `ECHO texto` | Eco do texto | `{"echo":"texto"}` |

### **4. Comandos Locais (Keypad):**

| Tecla | Função |
|-------|--------|
| `5` | Reinicia o Bluetooth |
| `9` | Mostra PIN na tela |
| `A` | Envia `{"local_cmd":"A"}` |
| `B` | Envia `{"local_cmd":"B"}` |
| `C` | Envia status da memória |
| `D` | Alterna LED onboard |

### **5. Interface OLED:**

#### **Tela de Aguardando:**
- Mostra "BT SERIAL SERVER" e "Aguardando conexao..."

#### **Tela Conectado:**
- Status da conexão
- Tempo de conexão
- Contadores de comandos/respostas

#### **Tela de Comando:**
- Mostra comando recebido
- Status "Processando..."

#### **Tela de Resposta:**
- Mostra "JSON enviado"
- Contadores totais

### **6. Configurações:**

```
Nome do Dispositivo: Relogio
PIN: 0000
Baud Rate: 115200 (padrão)
```

### **7. Exemplo de Uso com Serial Bluetooth Terminal:**

1. **Abra o Serial Bluetooth Terminal**
2. **Clique em "Connect"**
3. **Procure "Relogio inteligente"**
4. **Digite o PIN: 1234**
5. **Conecte**
6. **Envie comandos:**
   ```
   PING
   LED 1
   STATUS
   ECHO Hello World
   ```

### **8. Vantagens do BluetoothSerial:**

✅ **Mais simples** que BLE  
✅ **Compatível** com mais aplicativos  
✅ **Comunicação bidirecional** em tempo real  
✅ **Ponteamento USB-BT** automático  
✅ **Menos problemas** de visibilidade  

### **9. Troubleshooting:**

#### **Não consegue conectar:**
- Verifique se o ESP32 está ligado
- Reinicie o ESP32
- Verifique se o Bluetooth está ativo no celular
- Tente o PIN: 1234

#### **Comandos não funcionam:**
- Verifique se está conectado
- Envie comandos terminados em Enter
- Verifique se o comando está correto (maiúsculas)

#### **Não recebe respostas:**
- Verifique se o aplicativo suporta recebimento
- Ative o modo de recebimento no app
- Verifique se o ESP32 está processando comandos (OLED)

### **10. Monitor Serial:**

O ESP32 envia informações úteis pelo Serial Monitor:
- Status de conexão/desconexão
- Comandos recebidos
- Respostas enviadas
- Teclas pressionadas no keypad

### **11. Ponteamento USB-BT:**

O código inclui ponteamento automático:
- **USB → BT**: O que você digitar no Serial Monitor vai para o celular
- **BT → USB**: O que chegar do celular aparece no Serial Monitor

---

## 🚀 **Pronto para usar!**

Agora você pode controlar seu ESP32 via Bluetooth Serial pelo celular! 🎯 