let currentFile = null;
let currentResult = null;

// Inicialização
document.addEventListener('DOMContentLoaded', function() {
    setupDragAndDrop();
    loadGallery();
});

// Configuração do drag and drop
function setupDragAndDrop() {
    const uploadArea = document.getElementById('uploadArea');
    const fileInput = document.getElementById('fileInput');

    // Drag and drop events
    uploadArea.addEventListener('dragover', (e) => {
        e.preventDefault();
        uploadArea.classList.add('dragover');
    });

    uploadArea.addEventListener('dragleave', () => {
        uploadArea.classList.remove('dragover');
    });

    uploadArea.addEventListener('drop', (e) => {
        e.preventDefault();
        uploadArea.classList.remove('dragover');
        
        const files = e.dataTransfer.files;
        if (files.length > 0) {
            handleFileSelect(files[0]);
        }
    });

    // File input change
    fileInput.addEventListener('change', (e) => {
        if (e.target.files.length > 0) {
            handleFileSelect(e.target.files[0]);
        }
    });
}

// Manipula a seleção de arquivo
function handleFileSelect(file) {
    if (!file.type.startsWith('image/')) {
        alert('Por favor, selecione apenas arquivos de imagem!');
        return;
    }

    currentFile = file;
    showPreview(file);
}

// Mostra preview da imagem
function showPreview(file) {
    const reader = new FileReader();
    reader.onload = function(e) {
        const previewSection = document.getElementById('previewSection');
        const previewImg = document.getElementById('previewImg');
        const previewInfo = document.getElementById('previewInfo');

        previewImg.src = e.target.result;
        previewInfo.textContent = `${file.name} (${(file.size / 1024).toFixed(1)} KB)`;
        
        previewSection.style.display = 'block';
        previewSection.classList.add('fade-in');
    };
    reader.readAsDataURL(file);
}

// Limpa o preview
function clearPreview() {
    document.getElementById('previewSection').style.display = 'none';
    document.getElementById('resultSection').style.display = 'none';
    document.getElementById('progressSection').style.display = 'none';
    currentFile = null;
    currentResult = null;
}

// Converte a imagem
async function convertImage() {
    if (!currentFile) {
        alert('Por favor, selecione uma imagem primeiro!');
        return;
    }

    const formData = new FormData();
    formData.append('image', currentFile);

    // Mostra progress bar
    showProgress();

    try {
        const response = await fetch('/upload', {
            method: 'POST',
            body: formData
        });

        const result = await response.json();

        if (result.success) {
            currentResult = result;
            showResult(result);
        } else {
            throw new Error(result.error || 'Erro na conversão');
        }
    } catch (error) {
        console.error('Erro:', error);
        alert('Erro ao converter a imagem: ' + error.message);
    } finally {
        hideProgress();
    }
}

// Mostra barra de progresso
function showProgress() {
    const progressSection = document.getElementById('progressSection');
    const progressFill = document.getElementById('progressFill');
    const progressText = document.getElementById('progressText');

    progressSection.style.display = 'block';
    progressFill.style.width = '0%';
    progressText.textContent = 'Convertendo imagem...';

    // Simula progresso
    let progress = 0;
    const interval = setInterval(() => {
        progress += Math.random() * 20;
        if (progress > 90) progress = 90;
        
        progressFill.style.width = progress + '%';
        
        if (progress > 50) {
            progressText.textContent = 'Processando pixels...';
        }
        
        if (progress > 80) {
            progressText.textContent = 'Finalizando conversão...';
        }
    }, 200);

    // Para a simulação após 3 segundos
    setTimeout(() => {
        clearInterval(interval);
        progressFill.style.width = '100%';
        progressText.textContent = 'Conversão concluída!';
    }, 3000);
}

// Esconde barra de progresso
function hideProgress() {
    setTimeout(() => {
        document.getElementById('progressSection').style.display = 'none';
    }, 1000);
}

// Mostra resultado
function showResult(result) {
    const resultSection = document.getElementById('resultSection');
    const resultImg = document.getElementById('resultImg');
    const downloadBtn = document.getElementById('downloadBtn');
    const sendBtn = document.getElementById('sendBtn');

    resultImg.src = result.preview;
    downloadBtn.onclick = () => downloadFile(result.downloadUrl);
    sendBtn.onclick = () => sendToESP32(result.filename);

    resultSection.style.display = 'block';
    resultSection.classList.add('fade-in');

    // Recarrega a galeria
    loadGallery();
}

// Download do arquivo
function downloadFile(url) {
    if (url) {
        window.open(url, '_blank');
    } else if (currentResult) {
        window.open(currentResult.downloadUrl, '_blank');
    }
}

// Envia para ESP32 (placeholder)
function sendToESP32(filename) {
    // Aqui você implementaria a comunicação com o ESP32
    alert(`Funcionalidade de envio para ESP32 será implementada!\nArquivo: ${filename}`);
    
    // Exemplo de como seria:
    // fetch('/send-to-esp32', {
    //     method: 'POST',
    //     headers: { 'Content-Type': 'application/json' },
    //     body: JSON.stringify({ filename: filename })
    // });
}

// Carrega a galeria
async function loadGallery() {
    try {
        const response = await fetch('/images');
        const images = await response.json();

        const gallery = document.getElementById('gallery');
        
        if (images.length === 0) {
            gallery.innerHTML = '<div class="loading">Nenhuma imagem convertida ainda</div>';
            return;
        }

        gallery.innerHTML = images.map(image => `
            <div class="gallery-item fade-in">
                <img src="${image.preview || '/placeholder.jpg'}" alt="${image.filename}">
                <h4>${image.filename}</h4>
                <p>Criado em ${new Date(image.createdAt).toLocaleDateString('pt-BR')}</p>
                <div class="gallery-actions">
                    <button class="btn-download" onclick="downloadFile('${image.downloadUrl}')">
                        📥 Baixar
                    </button>
                    <button class="btn-delete" onclick="deleteImage('${image.filename}')">
                        🗑️ Deletar
                    </button>
                </div>
            </div>
        `).join('');
    } catch (error) {
        console.error('Erro ao carregar galeria:', error);
        document.getElementById('gallery').innerHTML = '<div class="loading">Erro ao carregar imagens</div>';
    }
}

// Deleta imagem
async function deleteImage(filename) {
    if (!confirm('Tem certeza que deseja deletar esta imagem?')) {
        return;
    }

    try {
        const response = await fetch(`/images/${filename}`, {
            method: 'DELETE'
        });

        const result = await response.json();

        if (result.success) {
            loadGallery(); // Recarrega a galeria
        } else {
            throw new Error(result.error);
        }
    } catch (error) {
        console.error('Erro ao deletar:', error);
        alert('Erro ao deletar a imagem: ' + error.message);
    }
}

// Função para testar conexão com ESP32
async function testESP32Connection() {
  try {
    const response = await fetch('/esp32/test');
    const result = await response.json();
    
    if (result.connected) {
      showNotification('✅ ESP32 conectado com sucesso!', 'success');
      return true;
    } else {
      showNotification('❌ ESP32 não conectado', 'error');
      return false;
    }
  } catch (error) {
    console.error('Erro ao testar conexão ESP32:', error);
    showNotification('❌ Erro ao conectar com ESP32', 'error');
    return false;
  }
}

// Função para enviar imagem para ESP32
async function sendImageToESP32(filename) {
  try {
    showNotification('📡 Enviando imagem para ESP32...', 'info');
    
    // Primeiro, obtém o IP do ESP32
    const esp32IP = await getESP32IP();
    if (!esp32IP) {
      throw new Error('ESP32 não encontrado na rede');
    }
    
    // Envia a imagem
    const response = await fetch('/esp32/send-image', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json'
      },
      body: JSON.stringify({ 
        filename: filename,
        esp32IP: esp32IP
      })
    });

    const result = await response.json();
    
    if (result.success) {
      showNotification(`✅ Imagem enviada para ESP32 (${esp32IP}) com sucesso!`, 'success');
    } else {
      throw new Error(result.error);
    }
  } catch (error) {
    console.error('Erro ao enviar para ESP32:', error);
    showNotification('❌ Erro ao enviar para ESP32: ' + error.message, 'error');
  }
}

// Função para descobrir o IP do ESP32
async function getESP32IP() {
  // Lista de IPs comuns para testar
  const commonIPs = [
    '192.168.0.25',  // IP configurado no código
    '192.168.1.100',
    '192.168.1.101',
    '192.168.0.100',
    '192.168.0.101'
  ];
  
  // Obtém o IP da rede local
  const localIP = await getLocalIP();
  if (localIP) {
    const baseIP = localIP.substring(0, localIP.lastIndexOf('.'));
    commonIPs.unshift(`${baseIP}.25`); // Adiciona o IP configurado no início
  }
  
  // Testa cada IP
  for (const ip of commonIPs) {
    try {
      const response = await fetch(`http://${ip}/test`, {
        method: 'GET',
        timeout: 2000
      });
      
      if (response.ok) {
        const result = await response.json();
        if (result.connected) {
          console.log(`ESP32 encontrado em: ${ip}`);
          return ip;
        }
      }
    } catch (error) {
      // IP não responde, continua testando
      continue;
    }
  }
  
  return null;
}

// Função para obter IP local
async function getLocalIP() {
  try {
    const response = await fetch('https://api.ipify.org?format=json');
    const data = await response.json();
    return data.ip;
  } catch (error) {
    console.error('Erro ao obter IP local:', error);
    return null;
  }
}

// Função para enviar imagem diretamente (fallback)
async function sendImageDirectly(filename, esp32IP) {
  try {
    // Obtém os dados da imagem
    const response = await fetch('/esp32/send-image', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json'
      },
      body: JSON.stringify({ filename: filename })
    });

    const result = await response.json();
    
    if (result.success && result.imageData) {
      // Envia diretamente para o ESP32
      const esp32Response = await fetch(`http://${esp32IP}/receive-image`, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json'
        },
        body: JSON.stringify(result.imageData)
      });
      
      if (esp32Response.ok) {
        showNotification(`✅ Imagem enviada diretamente para ESP32 (${esp32IP})!`, 'success');
        return true;
      } else {
        throw new Error('ESP32 não respondeu');
      }
    } else {
      throw new Error('Erro ao obter dados da imagem');
    }
  } catch (error) {
    console.error('Erro no envio direto:', error);
    return false;
  }
}

// Função para obter imagem aleatória
async function getRandomImage() {
  try {
    const response = await fetch('/esp32/random-image');
    const result = await response.json();
    
    if (response.ok) {
      return result;
    } else {
      throw new Error(result.error);
    }
  } catch (error) {
    console.error('Erro ao obter imagem aleatória:', error);
    return null;
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

// Função para verificar status do servidor
async function checkServerStatus() {
  try {
    const response = await fetch('/status');
    const status = await response.json();
    
    console.log('Status do servidor:', status);
    
    // Atualiza informações na interface
    const statusElement = document.getElementById('serverStatus');
    if (statusElement) {
      statusElement.innerHTML = `
        <div class="status-info">
          <span class="status-indicator ${status.status}"></span>
          <span>Servidor: ${status.status}</span>
          <span>Imagens: ${status.images.count}</span>
          <span>Uptime: ${Math.round(status.uptime)}s</span>
        </div>
      `;
    }
    
    return status;
  } catch (error) {
    console.error('Erro ao verificar status:', error);
    return null;
  }
}
