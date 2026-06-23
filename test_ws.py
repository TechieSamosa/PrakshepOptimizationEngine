import asyncio
import websockets
import json
import urllib.request
import urllib.error

def post(url, data=None):
    req = urllib.request.Request(url, method="POST")
    if data:
        req.add_header('Content-Type', 'application/json')
        data = json.dumps(data).encode('utf-8')
    else:
        req.add_header('Content-Length', '0')
    with urllib.request.urlopen(req, data=data) as response:
        return json.loads(response.read().decode())

async def test_api():
    print("Init:", post("http://127.0.0.1:8000/api/simulation/init", {"rocket_type": 0, "payload_mass": 1750.0}))
    print("Start:", post("http://127.0.0.1:8000/api/simulation/start"))

    print("Connecting to websocket...")
    async with websockets.connect("ws://127.0.0.1:8000/ws/telemetry") as ws:
        count = 0
        while count < 30:
            data = await ws.recv()
            frame = json.loads(data)
            if count % 10 == 0:
                print(f"Time: {frame['time']:.2f}s | Alt: {frame['altitude']:.2f}m | Vel: {frame['velocity_magnitude']:.2f}m/s | Mass: {frame['mass']:.2f}kg")
            count += 1
            
    print("Stop:", post("http://127.0.0.1:8000/api/simulation/stop"))

if __name__ == "__main__":
    asyncio.run(test_api())
