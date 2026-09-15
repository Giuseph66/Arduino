#!/usr/bin/env python3
"""Bridge serial -> WebSocket para usar o painel em navegadores sem Web Serial (Firefox).

Uso: python3 bridge.py [/dev/ttyACM0] [porta_ws=8881]
No painel: ws://127.0.0.1:8881 -> "Conectar WebSocket".
Requer: pip install pyserial websockets
"""
import asyncio, sys, serial, websockets

dev = sys.argv[1] if len(sys.argv) > 1 else '/dev/ttyACM0'
wsport = int(sys.argv[2]) if len(sys.argv) > 2 else 8881

# DTR/RTS baixos ANTES de abrir: no circuito de auto-reset do ESP32,
# DTR=1/RTS=0 segura IO0 em LOW e um reset cai no bootloader (sem saída).
s = serial.Serial()
s.port, s.baudrate, s.timeout = dev, 115200, 0.1
s.dtr = False
s.rts = False
s.open()
clients = set()

async def handler(ws):
    clients.add(ws)
    try:
        async for msg in ws:          # comandos do painel ('z', 'c') -> ESP
            s.write(msg.encode())
    finally:
        clients.discard(ws)

async def pump():
    buf = b''
    while True:
        buf += await asyncio.to_thread(s.read, 256)
        while b'\n' in buf:
            line, buf = buf.split(b'\n', 1)
            if not clients:
                continue
            for c in list(clients):
                try:
                    await c.send(line.decode(errors='replace'))
                except Exception:
                    pass

async def main():
    print(f'serial {dev} -> ws://127.0.0.1:{wsport}')
    async with websockets.serve(handler, '127.0.0.1', wsport):
        await pump()

asyncio.run(main())
