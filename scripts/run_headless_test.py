import asyncio
import subprocess
import time
import requests
import json
import websockets
import os

async def run_test():
    print("Starting FastAPI backend...")
    # Add MinGW to path for Windows DLLs
    env = os.environ.copy()
    env["PATH"] = "C:\\mingw64\\bin;" + env.get("PATH", "")
    
    server_process = subprocess.Popen(
        ["uvicorn", "api_server.main:app", "--host", "127.0.0.1", "--port", "8000"],
        env=env
    )
    
    # Wait for server to start
    for _ in range(10):
        try:
            r = requests.get("http://127.0.0.1:8000/")
            if r.status_code == 200:
                print("Server is up!")
                break
        except requests.ConnectionError:
            time.sleep(1)
    else:
        print("Failed to start server.")
        server_process.terminate()
        return

    try:
        # Initialize simulation
        print("Initializing simulation...")
        res = requests.post("http://127.0.0.1:8000/api/simulation/init", json={
            "rocket_type": 0,
            "payload_mass": 1750.0
        })
        print("Init response:", res.json())

        # Start simulation
        print("Starting simulation...")
        res = requests.post("http://127.0.0.1:8000/api/simulation/start")
        print("Start response:", res.json())

        telemetry_data = []
        
        # Connect to websocket
        print("Connecting to websocket...", flush=True)
        uri = "ws://127.0.0.1:8000/ws/telemetry"
        async with websockets.connect(uri) as websocket:
            print("Connected! Listening to telemetry...", flush=True)
            
            last_print = 0
            while True:
                message = await websocket.recv()
                data = json.loads(message)
                telemetry_data.append(data)
                
                t = data.get("time", 0.0)
                
                if t - last_print >= 10.0:
                    print(f"Flight Time: {t:.1f}s | Altitude: {data.get('altitude', 0)/1000:.2f}km | Q: {data.get('dynamic_pressure', 0)/1000:.2f}kPa", flush=True)
                    last_print = t
                    
                if t >= 180.0:
                    print("Reached 3 minutes of flight data.", flush=True)
                    break
                if data.get("event") == "ABORT":
                    print(f"Simulation aborted at T+{t}s", flush=True)
                    break
                if data.get("is_complete"):
                    print("Simulation marked complete.", flush=True)
                    break

        print(f"Captured {len(telemetry_data)} frames.", flush=True)
        
        # Save data
        os.makedirs("data", exist_ok=True)
        with open("data/telemetry_dump.json", "w") as f:
            json.dump(telemetry_data, f)
        print("Data saved to data/telemetry_dump.json")

    finally:
        print("Shutting down server...")
        server_process.terminate()
        server_process.wait()
        print("Test complete.")

if __name__ == "__main__":
    asyncio.run(run_test())
