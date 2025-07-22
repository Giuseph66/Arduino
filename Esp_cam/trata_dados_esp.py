# API FastAPI assíncrona para reconhecimento facial

import io
from typing import List
import cv2
import time
import os
import numpy as np
from fastapi import FastAPI, File, UploadFile, HTTPException
from fastapi.responses import JSONResponse, HTMLResponse, StreamingResponse
import face_recognition
from PIL import Image
import sqlite3
import uvicorn
import asyncio
import requests

# Constantes
DB_PATH = "fotos.db"
TOLERANCE = 0.6  # tolerância do face_recognition.compare_faces

# URL padrão do stream do ESP32-CAM (pode ser alterada via variável de ambiente)
ESP_STREAM_DEFAULT = os.getenv("ESP_STREAM_URL", "http://192.168.0.53/stream")


app = FastAPI(title="API de Reconhecimento Facial")

# ================= Página Web =================

HTML_PAGE_TEMPLATE = """
<!DOCTYPE html>
<html lang=\"pt-br\">
<head>
    <meta charset=\"UTF-8\">
    <title>Monitor de Reconhecimento Facial ESP32-CAM</title>
    <style>
        body { font-family: Arial, sans-serif; background: #111; color: #eee; text-align: center; margin: 0; }
        header { background: #222; padding: 10px; }
        main { padding: 20px; }
        input[type=text] { width: 60%; padding: 8px; }
        button { padding: 8px 16px; }
        #faces { margin-top: 20px; display: flex; flex-wrap: wrap; justify-content: center; }
        .face-card { background:#222; border:1px solid #444; border-radius:8px; padding:10px; margin:5px; }
        .face-card h3 { margin:0 0 5px 0; color:#0f0; }
        .face-card p { margin:0; font-size:12px; }
        #status { margin-top: 10px; }
    </style>
</head>
<body>
    <header><h1>Monitor de Reconhecimento Facial (ESP32-CAM)</h1></header>
    <main>
        <input id=\"url\" type=\"text\" placeholder=\"{stream_url}\" />
        <button onclick=\"toggleMonitor()\">Iniciar</button>
        <div id=\"status\"></div>
        <img id=\"liveStream\" src=\"{stream_url}\" alt=\"Stream\" style=\"max-width:80%; border:1px solid #444; margin-top:15px;\" />
        <img id=\"snapshot\" src=\"\" alt=\"Snapshot\" style=\"max-width:80%; margin-top:15px; border:1px solid #444;\" />
        <div id=\"faces\"></div>
    </main>

    <script>
        let monitorando = false;
        let intervalId;

        async function fetchFaces() {
            const url = document.getElementById('url').value;
            if(!url) { document.getElementById('status').innerText = 'Informe a URL do stream.'; return; }
            try {
                const resp = await fetch(`/recognize_stream?stream_url=${encodeURIComponent(url)}&duracao=2`);
                if (!resp.ok) throw new Error('Erro: ' + resp.status);
                const data = await resp.json();
                renderFaces(data.faces);
                document.getElementById('status').innerText = `Última atualização: ${new Date().toLocaleTimeString()}`;
            } catch (e) {
                document.getElementById('status').innerText = e;
                console.error(e);
            }
        }

        function renderFaces(faces) {
            const cont = document.getElementById('faces');
            cont.innerHTML = '';
            if(faces.length === 0) {
                cont.innerHTML = '<p>Nenhum rosto identificado</p>';
                return;
            }
            faces.forEach(f => {
                const div = document.createElement('div');
                div.className = 'face-card';
                div.innerHTML = `<h3>${f.name}</h3><p>top: ${f.box.top}, right: ${f.box.right}<br>bottom: ${f.box.bottom}, left: ${f.box.left}</p>`;
                cont.appendChild(div);
            });
        }

        async function fetchSnapshot() {
            const url = document.getElementById('url').value;
            if(!url) return;
            // Adiciona timestamp para evitar cache
            document.getElementById('snapshot').src = `/snapshot?stream_url=${encodeURIComponent(url)}&t=${Date.now()}`;
        }

        function toggleMonitor() {
            if(!monitorando) {
                const u = document.getElementById('url').value;
                document.getElementById('liveStream').src = u;
                fetchFaces();
                intervalId = setInterval(fetchFaces, 4000); // a cada 4s
                monitorando = true;
                document.querySelector('button').innerText = 'Parar';
                fetchSnapshot();
            } else {
                clearInterval(intervalId);
                monitorando = false;
                document.querySelector('button').innerText = 'Iniciar';
            }
        }
    </script>
</body>
</html>
"""


@app.get("/", response_class=HTMLResponse)
async def index():
    """Retorna página web para monitorar reconhecimento facial."""
    return HTML_PAGE_TEMPLATE.replace("{stream_url}", ESP_STREAM_DEFAULT)

# ================= Snapshot Endpoint =================

def _captura_snapshot(stream_url: str) -> bytes:
    """Captura um único frame JPEG do stream MJPEG."""
    cap = cv2.VideoCapture(stream_url)
    if not cap.isOpened():
        raise ValueError("Não foi possível abrir o stream: " + stream_url)
    ret, frame = cap.read()
    cap.release()
    if not ret:
        raise ValueError("Não foi possível capturar frame do stream")
    ok, buf = cv2.imencode('.jpg', frame)
    if not ok:
        raise ValueError("Falha ao codificar frame em JPEG")
    return buf.tobytes()


# ======= Utilitários para abrir stream de forma robusta =======


def _abrir_stream_opencv(url: str):
    """Tenta abrir o stream MJPEG usando diferentes back-ends do OpenCV."""
    backends = [cv2.CAP_FFMPEG, cv2.CAP_GSTREAMER, cv2.CAP_ANY]
    for be in backends:
        cap = cv2.VideoCapture(url, be)
        if cap.isOpened():
            return cap
    return None


def _ler_frame_requests(url: str, timeout: int = 5) -> bytes:
    """Obtém um frame JPEG lendo manualmente o multipart MJPEG via requests."""
    r = requests.get(url, stream=True, timeout=timeout)
    boundary = None
    for h in r.headers.get("Content-Type", "").split(";"):
        h = h.strip()
        if h.startswith("boundary="):
            boundary = h.split("=", 1)[1]
    if boundary is None:
        boundary = "frame"
    boundary = ("--" + boundary).encode()

    bytes_buf = b""
    for chunk in r.iter_content(chunk_size=1024):
        bytes_buf += chunk
        a = bytes_buf.find(boundary)
        if a != -1:
            start = bytes_buf.find(b"\xff\xd8", a)
            end = bytes_buf.find(b"\xff\xd9", start)
            if start != -1 and end != -1:
                jpeg = bytes_buf[start:end+2]
                return jpeg
    raise ValueError("Não foi possível obter frame via HTTP")


@app.get("/snapshot")
async def snapshot(stream_url: str):
    """Obtém uma imagem JPEG única do stream fornecido."""
    if not stream_url:
        raise HTTPException(status_code=400, detail="Parâmetro stream_url obrigatório.")
    try:
        loop = asyncio.get_running_loop()
        img_bytes = await loop.run_in_executor(None, _captura_snapshot, stream_url)
    except ValueError as ve:
        raise HTTPException(status_code=500, detail=str(ve))
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Erro ao capturar snapshot: {str(e)}")
    return StreamingResponse(io.BytesIO(img_bytes), media_type="image/jpeg")


def carregar_encodings_banco() -> List[tuple[str, np.ndarray]]:
    """Lê o banco SQLite e devolve lista de (nome, encoding numpy)."""
    conn = sqlite3.connect(DB_PATH)
    cursor = conn.cursor()
    cursor.execute("CREATE TABLE IF NOT EXISTS faces (id INTEGER PRIMARY KEY AUTOINCREMENT, nome TEXT, pontos BLOB)")
    conn.commit()
    cursor.execute("SELECT nome, pontos FROM faces")
    dados = []
    for nome, blob in cursor.fetchall():
        encoding = np.frombuffer(blob, dtype=np.float64)
        dados.append((nome, encoding))
    conn.close()
    return dados


# Carrega encodings na inicialização da aplicação
ENCODINGS_CAD: List[tuple[str, np.ndarray]] = []


@app.on_event("startup")
async def startup_event():
    global ENCODINGS_CAD
    ENCODINGS_CAD = carregar_encodings_banco()
    print(f"Encodings carregados: {len(ENCODINGS_CAD)} pessoas")


def processar_imagem(image_bytes: bytes):
    """Processa a imagem e retorna lista de dicts {name, box}."""
    # Carrega imagem via PIL -> NumPy (RGB)
    img = Image.open(io.BytesIO(image_bytes)).convert("RGB")
    frame = np.array(img)

    # Reduz para acelerar face_locations
    small_frame = cv2.resize(frame, (0, 0), fx=0.25, fy=0.25)
    rgb_small = small_frame  # já está em RGB

    # Localiza rostos e gera encodings na imagem reduzida
    boxes_small = face_recognition.face_locations(rgb_small)
    encodings = face_recognition.face_encodings(rgb_small, boxes_small)

    # Reescala caixas para o tamanho original
    boxes_original = [(top * 4, right * 4, bottom * 4, left * 4)
                      for top, right, bottom, left in boxes_small]

    faces_info = []
    for encoding, box in zip(encodings, boxes_original):
        nome_detectado = "Desconhecido"
        for nome_cad, enc_cad in ENCODINGS_CAD:
            matches = face_recognition.compare_faces([enc_cad], encoding, tolerance=TOLERANCE)
            if matches[0]:
                nome_detectado = nome_cad
                break
        faces_info.append({
            "name": nome_detectado,
            "box": {
                "top": box[0],
                "right": box[1],
                "bottom": box[2],
                "left": box[3]
            }
        })

    return faces_info


# ======== Processamento de Stream ========


def processar_stream(stream_url: str, duracao_segundos: int = 5, frame_skip: int = 5):
    """Lê um stream MJPEG por alguns segundos e retorna rostos detectados.

    stream_url: URL do endpoint /stream do ESP32-CAM.
    duracao_segundos: quanto tempo capturar (para não bloquear indefinidamente).
    frame_skip: processa 1 a cada *frame_skip* frames para reduzir carga.
    """
    cap = _abrir_stream_opencv(stream_url)
    if cap is None:
        raise ValueError("Não foi possível abrir o stream via OpenCV. Verifique URL ou backend de vídeo.")

    inicio = time.time()
    frames_lidos = 0
    faces_encontradas: List[dict] = []

    while time.time() - inicio < duracao_segundos:
        ret, frame = cap.read()
        if not ret:
            continue

        frames_lidos += 1
        if frames_lidos % frame_skip != 0:
            continue  # pula frames para acelerar

        # Converte frame BGR (OpenCV) -> bytes JPEG
        ret_jpeg, buf = cv2.imencode('.jpg', frame)
        if not ret_jpeg:
            continue

        faces = processar_imagem(buf.tobytes())
        faces_encontradas.extend(faces)

    cap.release()

    # Deduplicar resultados (mantém primeiro box de cada nome)
    vistos = {}
    unicos = []
    for face in faces_encontradas:
        nome = face["name"]
        if nome not in vistos:
            vistos[nome] = True
            unicos.append(face)

    return unicos


# ======== Endpoint de Stream ========


@app.get("/recognize_stream")
async def recognize_stream(stream_url: str, duracao: int = 5):
    """Recebe URL de stream MJPEG e retorna rostos detectados nos primeiros N segundos."""
    if not stream_url:
        raise HTTPException(status_code=400, detail="Parâmetro stream_url obrigatório.")

    try:
        loop = asyncio.get_running_loop()
        faces_info = await loop.run_in_executor(None, processar_stream, stream_url, duracao)
    except ValueError as ve:
        raise HTTPException(status_code=500, detail=str(ve))
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Erro ao processar stream: {str(e)}")

    return JSONResponse({"faces": faces_info})


@app.post("/recognize")
async def recognize(file: UploadFile = File(...)):
    """Recebe imagem e devolve reconhecimento facial + caixas."""
    if file.content_type not in {"image/jpeg", "image/png"}:
        raise HTTPException(status_code=415, detail="Formato de imagem não suportado. Use JPEG ou PNG.")

    image_bytes = await file.read()
    if not image_bytes:
        raise HTTPException(status_code=400, detail="Arquivo vazio.")

    try:
        loop = asyncio.get_running_loop()
        faces_info = await loop.run_in_executor(None, processar_imagem, image_bytes)
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Erro ao processar imagem: {str(e)}")

    return JSONResponse({"faces": faces_info})


if __name__ == "__main__":
    # Executa a API localmente: uvicorn trata_dados:app --reload
    uvicorn.run("trata_dados_esp:app", host="0.0.0.0", port=8888, reload=True)
