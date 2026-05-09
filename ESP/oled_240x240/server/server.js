const express = require('express');
const multer = require('multer');
const sharp = require('sharp');
const cors = require('cors');
const fs = require('fs');
const path = require('path');

const app = express();
const PORT = 3000;

// Middleware
app.use(cors());
app.use(express.json());
app.use(express.static('public'));

// Configuração do multer para upload
const storage = multer.diskStorage({
  destination: (req, file, cb) => {
    const uploadDir = 'uploads';
    if (!fs.existsSync(uploadDir)) {
      fs.mkdirSync(uploadDir);
    }
    cb(null, uploadDir);
  },
  filename: (req, file, cb) => {
    cb(null, Date.now() + '-' + file.originalname);
  }
});

const upload = multer({ 
  storage: storage,
  fileFilter: (req, file, cb) => {
    if (file.mimetype.startsWith('image/')) {
      cb(null, true);
    } else {
      cb(new Error('Apenas imagens são permitidas!'));
    }
  },
  limits: {
    fileSize: 10 * 1024 * 1024 // 10MB
  }
});

// Função para converter imagem para formato ESP32
async function convertImageForESP32(inputPath, outputPath) {
  try {
    // Redimensiona para 240x240 (tela redonda)
    const resized = await sharp(inputPath)
      .resize(240, 240, {
        fit: 'cover',
        position: 'center'
      })
      .raw()
      .toBuffer();

    // Converte para RGB565 (16-bit)
    const rgb565Data = [];
    for (let i = 0; i < resized.length; i += 3) {
      const r = resized[i];
      const g = resized[i + 1];
      const b = resized[i + 2];
      
      // Converte para RGB565
      const rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
      rgb565Data.push(rgb565);
    }

    // Salva como array C/C++
    const cArray = `// Imagem convertida para ESP32\n`;
    const cArrayData = `const uint16_t imageData[] = {\n`;
    const cArrayEnd = `};\nconst int imageSize = ${rgb565Data.length};\n`;

    let dataString = '';
    for (let i = 0; i < rgb565Data.length; i++) {
      dataString += `0x${rgb565Data[i].toString(16).padStart(4, '0')}`;
      if (i < rgb565Data.length - 1) {
        dataString += ', ';
        if ((i + 1) % 16 === 0) {
          dataString += '\n  ';
        }
      }
    }

    const finalData = cArray + cArrayData + '  ' + dataString + '\n' + cArrayEnd;
    
    fs.writeFileSync(outputPath, finalData);
    return true;
  } catch (error) {
    console.error('Erro na conversão:', error);
    return false;
  }
}

// Função para gerar preview da imagem
async function generatePreview(inputPath, outputPath) {
  try {
    await sharp(inputPath)
      .resize(200, 200, {
        fit: 'cover',
        position: 'center'
      })
      .jpeg({ quality: 80 })
      .toFile(outputPath);
    return true;
  } catch (error) {
    console.error('Erro ao gerar preview:', error);
    return false;
  }
}

// Rota para upload de imagem
app.post('/upload', upload.single('image'), async (req, res) => {
  try {
    if (!req.file) {
      return res.status(400).json({ error: 'Nenhuma imagem enviada' });
    }

    const inputPath = req.file.path;
    const baseName = path.parse(req.file.filename).name;
    const outputPath = path.join('converted', `${baseName}.h`);
    const previewPath = path.join('public', 'previews', `${baseName}.jpg`);

    // Cria diretórios se não existirem
    if (!fs.existsSync('converted')) {
      fs.mkdirSync('converted');
    }
    if (!fs.existsSync('public/previews')) {
      fs.mkdirSync('public/previews', { recursive: true });
    }

    // Converte a imagem
    const converted = await convertImageForESP32(inputPath, outputPath);
    const preview = await generatePreview(inputPath, previewPath);

    if (converted && preview) {
      res.json({
        success: true,
        message: 'Imagem convertida com sucesso!',
        filename: `${baseName}.h`,
        preview: `/previews/${baseName}.jpg`,
        downloadUrl: `/download/${baseName}.h`
      });
    } else {
      res.status(500).json({ error: 'Erro ao processar a imagem' });
    }

    // Remove o arquivo original
    fs.unlinkSync(inputPath);
  } catch (error) {
    console.error('Erro no upload:', error);
    res.status(500).json({ error: 'Erro interno do servidor' });
  }
});

// Rota para download do arquivo convertido
app.get('/download/:filename', (req, res) => {
  const filename = req.params.filename;
  const filePath = path.join('converted', filename);
  
  if (fs.existsSync(filePath)) {
    res.download(filePath);
  } else {
    res.status(404).json({ error: 'Arquivo não encontrado' });
  }
});

// Rota para listar imagens convertidas
app.get('/images', (req, res) => {
  try {
    const convertedDir = 'converted';
    const previewsDir = 'public/previews';
    
    if (!fs.existsSync(convertedDir)) {
      return res.json([]);
    }

    const files = fs.readdirSync(convertedDir)
      .filter(file => file.endsWith('.h'))
      .map(file => {
        const baseName = path.parse(file).name;
        return {
          filename: file,
          preview: fs.existsSync(path.join(previewsDir, `${baseName}.jpg`)) 
            ? `/previews/${baseName}.jpg` 
            : null,
          downloadUrl: `/download/${file}`,
          createdAt: fs.statSync(path.join(convertedDir, file)).mtime
        };
      })
      .sort((a, b) => b.createdAt - a.createdAt);

    res.json(files);
  } catch (error) {
    console.error('Erro ao listar imagens:', error);
    res.status(500).json({ error: 'Erro interno do servidor' });
  }
});

// Rota para deletar imagem
app.delete('/images/:filename', (req, res) => {
  try {
    const filename = req.params.filename;
    const baseName = path.parse(filename).name;
    
    const convertedPath = path.join('converted', filename);
    const previewPath = path.join('public', 'previews', `${baseName}.jpg`);
    
    if (fs.existsSync(convertedPath)) {
      fs.unlinkSync(convertedPath);
    }
    
    if (fs.existsSync(previewPath)) {
      fs.unlinkSync(previewPath);
    }
    
    res.json({ success: true, message: 'Imagem deletada com sucesso' });
  } catch (error) {
    console.error('Erro ao deletar imagem:', error);
    res.status(500).json({ error: 'Erro interno do servidor' });
  }
});

// Rota para testar conexão com ESP32
app.get('/esp32/test', (req, res) => {
  // Simula teste de conexão (você pode implementar ping real aqui)
  res.json({
    connected: true,
    message: 'ESP32 conectado com sucesso',
    timestamp: new Date().toISOString()
  });
});

// Rota para enviar imagem específica para ESP32
app.post('/esp32/send-image', async (req, res) => {
  try {
    const { filename, esp32IP } = req.body;
    
    if (!filename) {
      return res.status(400).json({ error: 'Nome do arquivo é obrigatório' });
    }
    
    const filePath = path.join('converted', filename);
    
    if (!fs.existsSync(filePath)) {
      return res.status(404).json({ error: 'Arquivo não encontrado' });
    }
    
    // Lê o arquivo .h
    const fileContent = fs.readFileSync(filePath, 'utf8');
    
    // Extrai os dados da imagem
    const dataMatch = fileContent.match(/const uint16_t imageData\[\] = \{([^}]+)\}/);
    if (!dataMatch) {
      return res.status(400).json({ error: 'Formato de arquivo inválido' });
    }
    
    const dataString = dataMatch[1].trim();
    const values = dataString.split(',').map(v => {
      const cleanValue = v.trim().replace('0x', '');
      return parseInt(cleanValue, 16);
    });
    
    // Prepara dados para envio
    const imageData = {
      filename: filename,
      data: values,
      size: values.length,
      format: 'RGB565',
      title: path.parse(filename).name,
      date: new Date().toLocaleDateString('pt-BR')
    };
    
    // Se IP do ESP32 foi fornecido, envia diretamente
    if (esp32IP) {
      try {
        console.log(`Enviando imagem ${filename} para ESP32 ${esp32IP}...`);
        console.log(`Tamanho dos dados: ${imageData.data.length} pixels`);
        
        const axios = require('axios');
        const response = await axios.post(`http://${esp32IP}/receive-image`, imageData, {
          headers: {
            'Content-Type': 'application/json'
          },
          timeout: 15000
        });
        
        console.log(`Resposta do ESP32:`, response.status, response.data);
        console.log(`Imagem ${filename} enviada para ESP32 ${esp32IP} com sucesso!`);
        
        res.json({
          success: true,
          message: `Imagem ${filename} enviada para ESP32 com sucesso`,
          filename: filename,
          esp32IP: esp32IP,
          esp32Response: response.data,
          timestamp: new Date().toISOString()
        });
        
      } catch (esp32Error) {
        console.error('Erro ao enviar para ESP32:', esp32Error.message);
        console.error('Detalhes do erro:', esp32Error.response?.data || esp32Error.message);
        res.status(500).json({ 
          error: 'Erro ao enviar para ESP32', 
          details: esp32Error.message,
          esp32Response: esp32Error.response?.data
        });
      }
    } else {
      // Retorna os dados para o frontend enviar
      res.json({
        success: true,
        message: 'Dados da imagem preparados para envio',
        filename: filename,
        imageData: imageData,
        timestamp: new Date().toISOString()
      });
    }
    
  } catch (error) {
    console.error('Erro ao enviar para ESP32:', error);
    res.status(500).json({ error: 'Erro interno do servidor' });
  }
});

// Rota para obter lista de imagens (formato otimizado para ESP32)
app.get('/esp32/images', (req, res) => {
  try {
    const convertedDir = 'converted';
    
    if (!fs.existsSync(convertedDir)) {
      return res.json([]);
    }

    const files = fs.readdirSync(convertedDir)
      .filter(file => file.endsWith('.h'))
      .map(file => {
        const baseName = path.parse(file).name;
        const stats = fs.statSync(path.join(convertedDir, file));
        return {
          filename: file,
          basename: baseName,
          size: stats.size,
          createdAt: stats.mtime.toISOString(),
          downloadUrl: `/download/${file}`
        };
      })
      .sort((a, b) => new Date(b.createdAt) - new Date(a.createdAt));

    res.json(files);
  } catch (error) {
    console.error('Erro ao listar imagens para ESP32:', error);
    res.status(500).json({ error: 'Erro interno do servidor' });
  }
});

// Rota para obter imagem aleatória (para ESP32)
app.get('/esp32/random-image', (req, res) => {
  try {
    const convertedDir = 'converted';
    
    if (!fs.existsSync(convertedDir)) {
      return res.status(404).json({ error: 'Nenhuma imagem disponível' });
    }

    const files = fs.readdirSync(convertedDir)
      .filter(file => file.endsWith('.h'));

    if (files.length === 0) {
      return res.status(404).json({ error: 'Nenhuma imagem disponível' });
    }

    // Seleciona imagem aleatória
    const randomIndex = Math.floor(Math.random() * files.length);
    const selectedFile = files[randomIndex];
    const baseName = path.parse(selectedFile).name;
    
    const stats = fs.statSync(path.join(convertedDir, selectedFile));
    
    res.json({
      filename: selectedFile,
      basename: baseName,
      size: stats.size,
      createdAt: stats.mtime.toISOString(),
      downloadUrl: `/download/${selectedFile}`
    });
    
  } catch (error) {
    console.error('Erro ao obter imagem aleatória:', error);
    res.status(500).json({ error: 'Erro interno do servidor' });
  }
});

// Rota para obter dados da imagem em formato JSON (para ESP32)
app.get('/esp32/image-data/:filename', (req, res) => {
  try {
    const filename = req.params.filename;
    const filePath = path.join('converted', filename);
    
    if (!fs.existsSync(filePath)) {
      return res.status(404).json({ error: 'Arquivo não encontrado' });
    }
    
    const fileContent = fs.readFileSync(filePath, 'utf8');
    
    // Extrai os dados da imagem do arquivo .h
    const dataMatch = fileContent.match(/const uint16_t imageData\[\] = \{([^}]+)\}/);
    if (!dataMatch) {
      return res.status(400).json({ error: 'Formato de arquivo inválido' });
    }
    
    const dataString = dataMatch[1].trim();
    const values = dataString.split(',').map(v => {
      const cleanValue = v.trim().replace('0x', '');
      return parseInt(cleanValue, 16);
    });
    
    res.json({
      filename: filename,
      data: values,
      size: values.length,
      format: 'RGB565'
    });
    
  } catch (error) {
    console.error('Erro ao obter dados da imagem:', error);
    res.status(500).json({ error: 'Erro interno do servidor' });
  }
});

// Rota para status do servidor
app.get('/status', (req, res) => {
  const convertedDir = 'converted';
  const uploadsDir = 'uploads';
  
  let imageCount = 0;
  let totalSize = 0;
  
  if (fs.existsSync(convertedDir)) {
    const files = fs.readdirSync(convertedDir).filter(f => f.endsWith('.h'));
    imageCount = files.length;
    files.forEach(file => {
      const stats = fs.statSync(path.join(convertedDir, file));
      totalSize += stats.size;
    });
  }
  
  res.json({
    status: 'online',
    uptime: process.uptime(),
    images: {
      count: imageCount,
      totalSize: totalSize,
      averageSize: imageCount > 0 ? Math.round(totalSize / imageCount) : 0
    },
    directories: {
      converted: fs.existsSync(convertedDir),
      uploads: fs.existsSync(uploadsDir)
    },
    timestamp: new Date().toISOString()
  });
});

// Rota para servir a página principal
app.get('/', (req, res) => {
  res.sendFile(path.join(__dirname, 'public', 'index.html'));
});

// Rota para servir a página de controle
app.get('/control', (req, res) => {
  res.sendFile(path.join(__dirname, 'public', 'control.html'));
});

// Rota para obter IP local
app.get('/local-ip', (req, res) => {
  const os = require('os');
  const interfaces = os.networkInterfaces();
  
  let localIP = 'localhost';
  for (const name of Object.keys(interfaces)) {
    for (const iface of interfaces[name]) {
      if (iface.family === 'IPv4' && !iface.internal) {
        localIP = iface.address;
        break;
      }
    }
    if (localIP !== 'localhost') break;
  }
  
  res.json({
    ip: localIP,
    serverURL: `http://${localIP}:3000`,
    message: `Atualize o ESP32 com: const char* serverURL = "http://${localIP}:3000";`
  });
});

app.listen(PORT, () => {
  console.log(`🚀 Servidor rodando na porta ${PORT}`);
  console.log(`📱 Acesse: http://localhost:${PORT}`);
  console.log(`🔗 API disponível em: http://localhost:${PORT}/api`);
  console.log(`📊 Status: http://localhost:${PORT}/status`);
});
