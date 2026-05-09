# 🔍 Troubleshooting - Problemas de PIN no Computador

## **Problema:** Computador pede PIN "003654" e não conecta

### **🔧 Soluções para Testar:**

#### **1. Configuração Atual do Código:**
```cpp
// PIN removido completamente
// SerialBT.begin(BT_NAME); // Sem segundo parâmetro
```

#### **2. Soluções no Computador:**

**Windows:**
1. **Abra Configurações Bluetooth**
2. **Remova o dispositivo "Relogio"** se já estiver pareado
3. **Adicione novamente** o dispositivo
4. **Se pedir PIN, tente:**
   - `0000` (quatro zeros)
   - `1234` (PIN padrão)
   - `000000` (seis zeros)
   - Deixe em branco e pressione OK

**Mac:**
1. **Abra Preferências do Sistema > Bluetooth**
2. **Remova o dispositivo** se já estiver pareado
3. **Adicione novamente**
4. **Se pedir PIN, tente as mesmas opções acima**

**Linux:**
1. **Use bluetoothctl:**
   ```bash
   bluetoothctl
   remove XX:XX:XX:XX:XX:XX  # remove dispositivo
   scan on
   pair XX:XX:XX:XX:XX:XX    # pare novamente
   ```

#### **3. Aplicativos Alternativos:**

**Windows:**
- **PuTTY** - Mais confiável para conexões BT
- **Tera Term** - Interface mais simples
- **nRF Connect for Desktop** - Para testes

**Mac:**
- **Serial** - App nativo do Mac
- **CoolTerm** - Interface gráfica
- **nRF Connect for Desktop**

#### **4. Configurações Avançadas:**

**Se ainda não funcionar, tente no código:**
```cpp
// Opção 1: Forçar PIN específico
SerialBT.setPin("0000", 4);
SerialBT.begin(BT_NAME);

// Opção 2: Usar nome diferente
const char* BT_NAME = "ESP32_Relogio";

// Opção 3: Configuração completa
SerialBT.setPin("0000", 4);
SerialBT.begin(BT_NAME, true);
```

#### **5. Verificações no ESP32:**

1. **Serial Monitor deve mostrar:**
   ```
   === ESP32 Bluetooth Serial ===
   Dispositivo: Relogio
   Comandos: PING | LED 1/0 | STATUS | MEMORY | ECHO <texto>
   ```

2. **OLED deve mostrar:**
   - "BT SERIAL SERVER"
   - "Aguardando conexao..."

#### **6. Teste com Celular Primeiro:**

1. **Teste no celular** para confirmar que funciona
2. **Use app "Serial Bluetooth Terminal"**
3. **Se funcionar no celular**, o problema é específico do computador

#### **7. Soluções Específicas por Sistema:**

**Windows 10/11:**
- **Desabilite e reabilite** o Bluetooth
- **Atualize drivers** Bluetooth
- **Use modo de compatibilidade** no app

**Mac:**
- **Reset SMC** (System Management Controller)
- **Reset NVRAM**
- **Reinicie o Mac**

**Linux:**
- **Reinicie o serviço Bluetooth:**
  ```bash
  sudo systemctl restart bluetooth
  ```

#### **8. PINs Comuns para Testar:**

- `0000` (mais comum)
- `1234` (padrão)
- `000000` (seis zeros)
- `1111` (quatro uns)
- `8888` (quatro oitos)

#### **9. Se Nada Funcionar:**

1. **Teste em outro computador**
2. **Use adaptador Bluetooth USB** externo
3. **Considere usar WiFi** como alternativa
4. **Use cabo USB** para testes

---

## **📞 Dicas Finais:**

- **O PIN "003654"** é gerado automaticamente pelo ESP32
- **Tente conectar várias vezes** com PINs diferentes
- **Reinicie o ESP32** entre tentativas
- **Use aplicativos diferentes** para testar

---

**🎯 Dica:** O celular geralmente funciona melhor que o computador para conexões Bluetooth Serial! 