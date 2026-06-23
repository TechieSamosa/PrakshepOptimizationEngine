from fastapi import APIRouter
from api_server.services.sim_manager import sim_manager
from pydantic import BaseModel

router = APIRouter()

class InitRequest(BaseModel):
    rocket_type: int = 0 # 0=PSLV_XL, 1=GSLV_MK2, 2=LVM3
    payload_mass: float = 0.0

@router.post("/api/simulation/init")
async def init_sim(req: InitRequest):
    import prakshep_core
    rtype = prakshep_core.RocketType(req.rocket_type)
    sim_manager.init_sim(rtype, req.payload_mass)
    return {"status": "initialized"}

@router.post("/api/simulation/start")
async def start_sim():
    sim_manager.start()
    return {"status": "started"}

@router.post("/api/simulation/stop")
async def stop_sim():
    sim_manager.stop()
    return {"status": "stopped"}

@router.post("/api/simulation/reset")
async def reset_sim():
    sim_manager.reset()
    return {"status": "reset"}
