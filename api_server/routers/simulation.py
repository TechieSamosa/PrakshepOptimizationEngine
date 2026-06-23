from fastapi import APIRouter
from api_server.services.sim_manager import sim_manager
from pydantic import BaseModel

from typing import Optional

router = APIRouter()

class InitRequest(BaseModel):
    rocket_type: int = 0 # 0=PSLV_XL, 1=GSLV_MK2, 2=LVM3
    payload_mass: float = 0.0

class MissionConfig(BaseModel):
    vehicle: str
    launchPad: str
    payloadMass: float
    targetOrbit: str
    weatherProfile: str
    targetAltitude: Optional[float] = None
    targetInclination: Optional[float] = None
    relandingLocation: Optional[str] = None

@router.post("/api/simulation/init")
async def init_sim(req: InitRequest):
    import prakshep_core
    rtype = prakshep_core.RocketType(req.rocket_type)
    sim_manager.init_sim(rtype, req.payload_mass)
    return {"status": "initialized"}

@router.post("/api/simulation/start")
async def start_sim(config: Optional[MissionConfig] = None):
    import prakshep_core
    if config:
        v_map = {
            "PSLV-XL": 0,
            "GSLV-MK2": 1,
            "LVM3": 2,
            "FALCON-9": 2, # Map Falcon 9 to Heavy
            "AGNIBAAN": 0, 
            "SSLV": 0,
            "HSLV": 1,
            "NGLV": 2,
            "VIKRAM": 0
        }
        rtype_val = v_map.get(config.vehicle.upper(), 0)
        rtype = prakshep_core.RocketType(rtype_val)
        sim_manager.init_sim(rtype, config.payloadMass)
    else:
        sim_manager.init_sim()
        
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
