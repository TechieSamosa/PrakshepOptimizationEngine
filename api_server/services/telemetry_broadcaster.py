import asyncio
import json
from api_server.services.sim_manager import sim_manager
from api_server.config import settings
from typing import Set
from fastapi import WebSocket

class TelemetryBroadcaster:
    def __init__(self):
        self.active_connections: Set[WebSocket] = set()
        self.is_broadcasting = False
        self._task = None

    async def connect(self, websocket: WebSocket):
        await websocket.accept()
        self.active_connections.add(websocket)

    def disconnect(self, websocket: WebSocket):
        self.active_connections.remove(websocket)

    async def start_broadcasting(self):
        if self.is_broadcasting:
            return
        self.is_broadcasting = True
        self._task = asyncio.create_task(self._broadcast_loop())

    async def stop_broadcasting(self):
        self.is_broadcasting = False
        if self._task:
            await self._task
            self._task = None

    async def _broadcast_loop(self):
        dt = 1.0 / settings.TICK_RATE_HZ
        sleep_time = dt
        while self.is_broadcasting:
            frame = sim_manager.tick(dt)
            if frame and self.active_connections:
                message = json.dumps(frame)
                stale_connections = set()
                for connection in self.active_connections:
                    try:
                        await connection.send_text(message)
                    except Exception:
                        stale_connections.add(connection)
                
                for stale in stale_connections:
                    self.active_connections.remove(stale)

            await asyncio.sleep(sleep_time)

broadcaster = TelemetryBroadcaster()
