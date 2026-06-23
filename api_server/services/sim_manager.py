import threading
import prakshep_core

class SimulationManager:
    def __init__(self):
        self._lock = threading.Lock()
        self.sim = None
        self.is_running = False

    def init_sim(self, rocket_type: prakshep_core.RocketType = prakshep_core.RocketType.PSLV_XL, payload_mass: float = 0.0):
        with self._lock:
            self.sim = prakshep_core.Simulation(rocket_type, payload_mass)
            self.is_running = False

    def start(self):
        if self.sim is None:
            self.init_sim()
        self.is_running = True

    def stop(self):
        self.is_running = False

    def reset(self):
        with self._lock:
            if self.sim:
                self.sim.reset()
            self.is_running = False

    def tick(self, dt: float) -> dict | None:
        if not self.is_running or self.sim is None:
            return None
        with self._lock:
            if self.sim.is_complete:
                self.is_running = False
            frame = self.sim.tick(dt)
            return {
                "raw": {
                    "time": frame.time,
                    "position_eci": [frame.position_eci.x, frame.position_eci.y, frame.position_eci.z],
                    "velocity_eci": [frame.velocity_eci.x, frame.velocity_eci.y, frame.velocity_eci.z],
                    "mass": frame.mass,
                    "thrust": frame.thrust,
                    "propellant_remaining": frame.propellant_remaining,
                    "current_stage": frame.current_stage,
                },
                "analysed": {
                    "altitude": frame.altitude,
                    "velocity_magnitude": frame.velocity_magnitude,
                    "acceleration_magnitude": frame.acceleration_magnitude,
                    "downrange_distance": frame.downrange_distance,
                    "mach_number": frame.mach_number,
                    "dynamic_pressure": frame.dynamic_pressure,
                    "latitude": frame.latitude,
                    "longitude": frame.longitude,
                    "geodetic_altitude": frame.geodetic_altitude,
                    "pitch": frame.pitch,
                    "yaw": frame.yaw,
                    "roll": frame.roll,
                    "structural_integrity": frame.structural_integrity,
                    "crash_probability": frame.crash_probability,
                },
                "events": {
                    "max_q_reached": frame.max_q_reached,
                    "max_q_value": frame.max_q_value,
                    "event": frame.event,
                }
            }

sim_manager = SimulationManager()
