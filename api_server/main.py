import os
if os.name == 'nt' and os.path.exists("C:/mingw64/bin"):
    os.add_dll_directory("C:/mingw64/bin")

from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from api_server.config import settings
from api_server.routers import telemetry, simulation
from api_server.services.telemetry_broadcaster import broadcaster

app = FastAPI(title="Prakshep API Engine")

app.add_middleware(
    CORSMiddleware,
    allow_origins=settings.CORS_ORIGINS,
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

app.include_router(telemetry.router)
app.include_router(simulation.router)

@app.on_event("startup")
async def startup_event():
    # Start the broadcaster when the server starts
    await broadcaster.start_broadcasting()

@app.on_event("shutdown")
async def shutdown_event():
    await broadcaster.stop_broadcasting()

@app.get("/")
def read_root():
    return {"status": "Prakshep API Engine running", "version": "1.0.0"}
