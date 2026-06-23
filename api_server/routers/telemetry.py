from fastapi import APIRouter, WebSocket, WebSocketDisconnect
from api_server.services.telemetry_broadcaster import broadcaster

router = APIRouter()

@router.websocket("/ws/telemetry")
async def websocket_telemetry(websocket: WebSocket):
    await broadcaster.connect(websocket)
    try:
        while True:
            # Keep connection open and handle incoming messages if any (like TVC override commands)
            data = await websocket.receive_text()
            # In Phase 4, we will parse control commands here
    except WebSocketDisconnect:
        broadcaster.disconnect(websocket)
