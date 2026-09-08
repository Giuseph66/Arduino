#!/usr/bin/env python3
# =============================================================
#  serial_log.py — Captura o Serial Monitor do ESP32 e salva
#  em arquivo .txt com timestamp em cada linha.
#
#  USO:
#    python3 serial_log.py
#    python3 serial_log.py --port /dev/ttyACM0 --baud 115200
#    python3 serial_log.py --port /dev/ttyUSB0
#
#  O arquivo é salvo em serial_log_YYYYMMDD_HHMMSS.txt
#  Ctrl+C para parar.
# =============================================================

import serial
import serial.tools.list_ports
import argparse
import sys
from datetime import datetime

# --- argumentos de linha de comando ---
parser = argparse.ArgumentParser(description="Captura Serial do ESP32 para arquivo txt")
parser.add_argument("--port",  default=None,   help="Porta serial (ex: /dev/ttyACM0)")
parser.add_argument("--baud",  default=115200,  type=int, help="Baud rate (padrão: 115200)")
parser.add_argument("--out",   default=None,   help="Nome do arquivo de saída (opcional)")
args = parser.parse_args()

# --- auto-detecta porta se não informada ---
porta = args.port
if porta is None:
    portas = list(serial.tools.list_ports.comports())
    candidatas = [p for p in portas if "ttyACM" in p.device or "ttyUSB" in p.device]
    if not candidatas:
        print("ERRO: Nenhuma porta serial encontrada.")
        print("Portas disponíveis:")
        for p in portas:
            print(f"  {p.device} — {p.description}")
        sys.exit(1)
    porta = candidatas[0].device
    print(f"Porta detectada automaticamente: {porta}")

# --- nome do arquivo de saída ---
timestamp_inicio = datetime.now().strftime("%Y%m%d_%H%M%S")
arquivo = args.out if args.out else f"serial_log_{timestamp_inicio}.txt"

print(f"Porta  : {porta}")
print(f"Baud   : {args.baud}")
print(f"Arquivo: {arquivo}")
print(f"Iniciando captura... (Ctrl+C para parar)\n")
print("-" * 60)

try:
    with serial.Serial(porta, args.baud, timeout=1) as ser, \
         open(arquivo, "w", encoding="utf-8") as f:

        # cabeçalho no arquivo
        f.write(f"# Serial log — ESP32 Jammer\n")
        f.write(f"# Porta : {porta}\n")
        f.write(f"# Baud  : {args.baud}\n")
        f.write(f"# Início: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
        f.write(f"# {'=' * 54}\n\n")
        f.flush()

        while True:
            try:
                linha = ser.readline()
                if not linha:
                    continue

                # decodifica (ignora bytes inválidos)
                linha_str = linha.decode("utf-8", errors="replace").rstrip("\r\n")

                # timestamp de cada linha
                ts = datetime.now().strftime("%H:%M:%S.%f")[:-3]  # HH:MM:SS.mmm
                linha_formatada = f"{ts} -> {linha_str}"

                # exibe no terminal
                print(linha_formatada)

                # salva no arquivo (flush imediato — não perde dados se travar)
                f.write(linha_formatada + "\n")
                f.flush()

            except UnicodeDecodeError:
                pass  # ignora lixo na linha

except serial.SerialException as e:
    print(f"\nERRO ao abrir porta {porta}: {e}")
    print("Verifique se o Arduino IDE / Serial Monitor está fechado.")
    sys.exit(1)

except KeyboardInterrupt:
    print(f"\n\nCaptura encerrada.")
    print(f"Arquivo salvo: {arquivo}")
    sys.exit(0)
