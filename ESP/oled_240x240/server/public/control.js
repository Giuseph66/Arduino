let selectedImage = null;
let esp32IP = null;
let esp32Status = null;

// Inicialização
document.addEventListener('DOMContentLoaded', function() {
    loadImages();
    loadServerInfo();
    refreshESP32Status();
    
    // Atualiza status a cada 5 segundos
    setInterval(refreshESP32Status, 5000);
});

// Carrega informações do servidor
async function loadServerInfo() {
    try {
        const response = await fetch('/local-ip');
        const serverInfo = await response.json();
        
        document.getElementById('serverIP').textContent = serverInfo.ip;
        document.getElementById('serverURL').textContent = serverInfo.serverURL;
        
        console.log('Informações do servidor:', serverInfo);
        
    } catch (error) {
        console.error('Erro ao carregar informações do servidor:', error);
    }
}

// Carrega lista de imagens
async function loadImages() {
    try {
        const response = await fetch('/images');
        const images = await response.json();
        
        const imageGrid = document.getElementById('imageGrid');
        
        if (images.length === 0) {
            imageGrid.innerHTML = '<div class="loading">Nenhuma imagem disponível</div>';
            return;
        }
        
        imageGrid.innerHTML = images.map(image => `
            <div class="image-card" onclick="selectImage('${image.filename}')" data-filename="${image.filename}">
                <img src="${image.preview || '/placeholder.jpg'}" alt="${image.filename}">
                <div class="image-info">
                    <h4>${image.filename}</h4>
                    <p>Criado em ${new Date(image.createdAt).toLocaleDateString('pt-BR')}</p>
                </div>
            </div>
        `).join('');
        
    } catch (error) {
        console.error('Erro ao carregar imagens:', error);
        document.getElementById('imageGrid').innerHTML = '<div class="loading">Erro ao carregar imagens</div>';
    }
}

// Seleciona uma imagem
function selectImage(filename) {
    // Remove seleção anterior
    document.querySelectorAll('.image-card').forEach(card => {
        card.classList.remove('selected');
    });
    
    // Seleciona nova imagem
    const card = document.querySelector(`[data-filename="${filename}"]`);
    if (card) {
        card.classList.add('selected');
        selectedImage = filename;
        
        // Habilita botão de envio
        const sendButton = document.getElementById('sendButton');
        sendButton.disabled = false;
        sendButton.textContent = `📡 Enviar "${filename}"`;
    }
}

// Atualiza status do ESP32
async function refreshESP32Status() {
    try {
        // Primeiro, encontra o IP do ESP32
        esp32IP = await findESP32();
        
        if (esp32IP) {
            // Obtém status do ESP32
            const response = await fetch(`http://${esp32IP}/status`);
            if (response.ok) {
                esp32Status = await response.json();
                updateESP32Display(true);
            } else {
                updateESP32Display(false);
            }
        } else {
            updateESP32Display(false);
        }
        
    } catch (error) {
        console.error('Erro ao atualizar status:', error);
        updateESP32Display(false);
    }
}

// Encontra o IP do ESP32
async function findESP32() {
    const commonIPs = [
        '192.168.0.25',  // IP do ESP32 (onde ele está rodando)
        '192.168.1.100',
        '192.168.1.101',
        '192.168.0.100',
        '192.168.0.101'
    ];
    
    console.log('Procurando ESP32...');
    
    // Testa cada IP
    for (const ip of commonIPs) {
        try {
            console.log(`Testando IP: ${ip}`);
            const response = await fetch(`http://${ip}/test`, {
                method: 'GET',
                timeout: 3000
            });
            
            console.log(`Resposta de ${ip}:`, response.status);
            
            if (response.ok) {
                const result = await response.json();
                console.log(`Resultado de ${ip}:`, result);
                if (result.connected) {
                    console.log(`ESP32 encontrado em: ${ip}`);
                    return ip;
                }
            }
        } catch (error) {
            console.log(`Erro ao testar ${ip}:`, error.message);
            continue;
        }
    }
    
    console.log('ESP32 não encontrado');
    return null;
}

// Atualiza display do ESP32
function updateESP32Display(connected) {
    const statusIndicator = document.getElementById('statusIndicator');
    const statusText = document.getElementById('statusText');
    const esp32Info = document.getElementById('esp32Info');
    
    if (connected && esp32Status) {
        statusIndicator.classList.add('connected');
        statusText.textContent = 'Conectado';
        
        // Atualiza informações
        document.getElementById('esp32IP').textContent = esp32IP;
        document.getElementById('esp32Uptime').textContent = formatUptime(esp32Status.uptime);
        document.getElementById('currentImage').textContent = esp32Status.image_loaded ? 'Carregada' : 'Nenhuma';
        document.getElementById('wifiStatus').textContent = esp32Status.wifi === 'connected' ? 'Conectado' : 'Desconectado';
        
        esp32Info.style.display = 'block';
    } else {
        statusIndicator.classList.remove('connected');
        statusText.textContent = 'Desconectado';
        esp32Info.style.display = 'none';
    }
}

// Formata tempo de uptime
function formatUptime(seconds) {
    const hours = Math.floor(seconds / 3600);
    const minutes = Math.floor((seconds % 3600) / 60);
    const secs = seconds % 60;
    
    if (hours > 0) {
        return `${hours}h ${minutes}m ${secs}s`;
    } else if (minutes > 0) {
        return `${minutes}m ${secs}s`;
    } else {
        return `${secs}s`;
    }
}

// Envia imagem selecionada
async function sendSelectedImage() {
    if (!selectedImage) {
        showNotification('❌ Nenhuma imagem selecionada', 'error');
        return;
    }
    
    if (!esp32IP) {
        showNotification('❌ ESP32 não encontrado', 'error');
        return;
    }
    
    try {
        console.log('Iniciando envio de imagem...');
        console.log('Imagem selecionada:', selectedImage);
        console.log('IP do ESP32:', esp32IP);
        
        showNotification('📡 Enviando imagem para ESP32...', 'info');
        
        // Envia via servidor Node.js
        const response = await fetch('/esp32/send-image', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                filename: selectedImage,
                esp32IP: esp32IP
            })
        });
        
        console.log('Resposta do servidor:', response.status);
        
        const result = await response.json();
        console.log('Resultado:', result);
        
        if (result.success) {
            showNotification(`✅ Imagem "${selectedImage}" enviada com sucesso!`, 'success');
            
            // Atualiza status após envio
            setTimeout(refreshESP32Status, 1000);
        } else {
            throw new Error(result.error || 'Erro desconhecido');
        }
        
    } catch (error) {
        console.error('Erro ao enviar imagem:', error);
        showNotification('❌ Erro ao enviar imagem: ' + error.message, 'error');
    }
}

// Função para mostrar notificações
function showNotification(message, type = 'info') {
    // Remove notificação anterior
    const existingNotification = document.querySelector('.notification');
    if (existingNotification) {
        existingNotification.remove();
    }
    
    // Cria nova notificação
    const notification = document.createElement('div');
    notification.className = `notification notification-${type}`;
    notification.textContent = message;
    
    // Estilos da notificação
    notification.style.cssText = `
        position: fixed;
        top: 20px;
        right: 20px;
        padding: 15px 20px;
        border-radius: 8px;
        color: white;
        font-weight: 500;
        z-index: 1000;
        animation: slideIn 0.3s ease;
        max-width: 300px;
        word-wrap: break-word;
    `;
    
    // Cores baseadas no tipo
    switch (type) {
        case 'success':
            notification.style.background = 'linear-gradient(135deg, #48bb78, #38a169)';
            break;
        case 'error':
            notification.style.background = 'linear-gradient(135deg, #f56565, #e53e3e)';
            break;
        case 'info':
            notification.style.background = 'linear-gradient(135deg, #4299e1, #3182ce)';
            break;
        default:
            notification.style.background = 'linear-gradient(135deg, #667eea, #764ba2)';
    }
    
    document.body.appendChild(notification);
    
    // Remove após 3 segundos
    setTimeout(() => {
        if (notification.parentNode) {
            notification.style.animation = 'slideOut 0.3s ease';
            setTimeout(() => {
                if (notification.parentNode) {
                    notification.remove();
                }
            }, 300);
        }
    }, 3000);
}

// Função para forçar atualização no ESP32
async function forceESP32Update() {
    if (!esp32IP) {
        showNotification('❌ ESP32 não encontrado', 'error');
        return;
    }
    
    try {
        showNotification('🔄 Forçando atualização no ESP32...', 'info');
        
        const response = await fetch(`http://${esp32IP}/update`, {
            method: 'POST'
        });
        
        if (response.ok) {
            showNotification('✅ ESP32 atualizado com sucesso!', 'success');
            setTimeout(refreshESP32Status, 1000);
        } else {
            throw new Error('ESP32 não respondeu');
        }
        
    } catch (error) {
        console.error('Erro ao forçar atualização:', error);
        showNotification('❌ Erro ao forçar atualização: ' + error.message, 'error');
    }
}

// Função para testar envio direto
async function testDirectSend() {
    if (!esp32IP) {
        showNotification('❌ ESP32 não encontrado', 'error');
        return;
    }
    
    try {
        console.log('Testando envio direto para ESP32...');
        
        // Dados de teste simples
        const testData = {
            filename: "test.jpg",
            data: [0xFFFF, 0x0000, 0xFFFF, 0x0000], // 4 pixels de teste
            size: 4,
            format: 'RGB565',
            title: "Teste",
            date: new Date().toLocaleDateString('pt-BR')
        };
        
        const response = await fetch(`http://${esp32IP}/receive-image`, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify(testData)
        });
        
        console.log('Resposta do ESP32:', response.status);
        
        if (response.ok) {
            const result = await response.json();
            console.log('Resultado:', result);
            showNotification('✅ Teste direto funcionou!', 'success');
        } else {
            throw new Error(`HTTP ${response.status}`);
        }
        
    } catch (error) {
        console.error('Erro no teste direto:', error);
        showNotification('❌ Erro no teste direto: ' + error.message, 'error');
    }
}
