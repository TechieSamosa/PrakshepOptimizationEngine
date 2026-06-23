import gymnasium as gym
from gymnasium import spaces
import numpy as np
import prakshep_core
from ml_environment.reward_functions import compute_reward

class PrakshepEnv(gym.Env):
    """Custom Environment that follows gym interface"""
    metadata = {'render_modes': ['human']}

    def __init__(self, rocket_type=0, payload_mass=1750.0):
        super(PrakshepEnv, self).__init__()
        
        self.rocket_type = rocket_type
        self.payload_mass = payload_mass
        
        # Instantiate the core C++ simulation
        rt = prakshep_core.RocketType(self.rocket_type)
        self.sim = prakshep_core.Simulation(rt, self.payload_mass)
        
        # Action space: [throttle, gimbal_pitch, gimbal_yaw]
        # Normalized between -1 and 1
        self.action_space = spaces.Box(low=-1.0, high=1.0, shape=(3,), dtype=np.float32)
        
        # Observation space
        # [altitude, velocity_mag, pitch, yaw, roll, dynamic_pressure, mass, current_stage, angular_vel_mag]
        # Normalized for better NN convergence
        self.observation_space = spaces.Box(low=-np.inf, high=np.inf, shape=(9,), dtype=np.float32)
        
        self.previous_telemetry = None
        self.max_steps = 10000 # Max episodes length
        self.current_step = 0

    def step(self, action):
        # Action from NN: [-1, 1]
        throttle = (action[0] + 1.0) / 2.0 # Map to [0, 1]
        # Assuming max gimbal is ~4 degrees (0.07 rad)
        gimbal_pitch = action[1] * 0.07 
        gimbal_yaw = action[2] * 0.07
        
        # Apply action
        self.sim.set_tvc_override(gimbal_pitch, gimbal_yaw, throttle)
        
        # Advance simulation (e.g. 10 ticks of 0.01s = 0.1s step)
        telemetry_frame = None
        for _ in range(6):
            telemetry_frame = self.sim.tick(1.0/60.0)
            
        self.current_step += 1
        
        telemetry = self._frame_to_dict(telemetry_frame)
        
        # Extract state
        obs = self._get_obs(telemetry)
        
        # Compute reward
        reward = compute_reward(telemetry, action, self.previous_telemetry)
        self.previous_telemetry = telemetry
        
        # Check termination
        terminated = self.sim.is_complete or telemetry["event"] == "ABORT" or telemetry.get("crash_probability", 0) > 0.8
        
        # Check success
        if telemetry["velocity_magnitude"] >= 7400.0 and telemetry["altitude"] >= 500000.0:
            terminated = True
            
        truncated = self.current_step >= self.max_steps
        
        info = telemetry
        
        return obs, reward, terminated, truncated, info

    def reset(self, seed=None, options=None):
        super().reset(seed=seed)
        self.sim.reset()
        self.current_step = 0
        
        telemetry_frame = self.sim.tick(0.0) # tick 0 to get initial state
        telemetry = self._frame_to_dict(telemetry_frame)
        self.previous_telemetry = telemetry
        
        obs = self._get_obs(telemetry)
        info = telemetry
        return obs, info

    def _frame_to_dict(self, frame):
        return {
            "time": frame.time,
            "altitude": frame.altitude,
            "velocity_magnitude": frame.velocity_magnitude,
            "acceleration_magnitude": getattr(frame, "acceleration_magnitude", 0.0),
            "downrange_distance": getattr(frame, "downrange_distance", 0.0),
            "mach_number": frame.mach_number,
            "dynamic_pressure": frame.dynamic_pressure,
            "max_q_reached": frame.max_q_reached,
            "max_q_value": frame.max_q_value,
            "latitude": frame.latitude,
            "longitude": frame.longitude,
            "geodetic_altitude": frame.geodetic_altitude,
            "pitch": frame.pitch,
            "yaw": frame.yaw,
            "roll": frame.roll,
            "current_stage": frame.current_stage,
            "thrust": frame.thrust,
            "mass": frame.mass,
            "propellant_remaining": frame.propellant_remaining,
            "structural_integrity": getattr(frame, "structural_integrity", 1.0),
            "crash_probability": getattr(frame, "crash_probability", 0.0),
            "event": frame.event
        }

    def _get_obs(self, telemetry):
        # Normalize variables for observation
        alt = telemetry["altitude"] / 600000.0
        vel = telemetry["velocity_magnitude"] / 8000.0
        pitch = telemetry["pitch"] / 180.0
        yaw = telemetry["yaw"] / 180.0
        roll = telemetry["roll"] / 180.0
        q_dyn = telemetry["dynamic_pressure"] / 50000.0
        mass = telemetry["mass"] / 400000.0
        stage = telemetry["current_stage"] / 5.0
        
        # compute angular vel proxy from pitch/yaw diff
        ang_vel = 0.0
        if self.previous_telemetry:
            dp = telemetry["pitch"] - self.previous_telemetry["pitch"]
            dy = telemetry["yaw"] - self.previous_telemetry["yaw"]
            dr = telemetry["roll"] - self.previous_telemetry["roll"]
            dt = 0.1 # approx time passed
            ang_vel = (math.sqrt(dp**2 + dy**2 + dr**2) / dt) / 10.0 # Normalized roughly
            
        return np.array([alt, vel, pitch, yaw, roll, q_dyn, mass, stage, ang_vel], dtype=np.float32)

    def render(self):
        pass

    def close(self):
        pass

import math
