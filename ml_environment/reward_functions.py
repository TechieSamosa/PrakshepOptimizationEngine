import math

def compute_reward(state, action, previous_state):
    """
    Computes the highly sophisticated dense reward for the RL agent.
    
    state dictionary keys:
    - altitude, velocity_magnitude, mass, dynamic_pressure, mach_number
    - pitch, yaw, roll, angular_velocity_magnitude
    - max_q_reached, max_q_value, structural_integrity
    - event
    
    action: [throttle, gimbal_pitch, gimbal_yaw]
    """
    reward = 0.0
    
    alt = state["altitude"]
    vel = state["velocity_magnitude"]
    q_dyn = state["dynamic_pressure"]
    ang_vel = state.get("angular_velocity_magnitude", 0.0) # we can derive it or add it to telemetry later
    
    prev_alt = previous_state["altitude"] if previous_state else alt
    prev_vel = previous_state["velocity_magnitude"] if previous_state else vel
    
    # Target orbital parameters
    TARGET_VELOCITY = 7400.0 # m/s (LEO insertion velocity)
    TARGET_ALTITUDE = 600000.0 # 600km for SSO
    Q_MAX_LIMIT = 40000.0 # Pa (Typical limit for launch vehicles)
    
    # 1. Incremental Velocity/Altitude Reward (Dense)
    if vel < TARGET_VELOCITY:
        delta_v = vel - prev_vel
        reward += delta_v * 0.1 # Reward acceleration
    
    delta_alt = alt - prev_alt
    if alt < TARGET_ALTITUDE:
        reward += delta_alt * 0.001 # Small reward for climbing
        
    # 2. Safety / Structural Stress Penalty (Max-Q)
    if q_dyn > Q_MAX_LIMIT:
        # Heavy penalty scaling with exceedance
        reward -= 0.05 * (q_dyn - Q_MAX_LIMIT)
        
    if state.get("structural_integrity", 1.0) < 1.0:
        reward -= 5.0 # Penalty for losing integrity
        
    if state["event"] == "ABORT" or state.get("crash_probability", 0.0) > 0.8:
        reward -= 500.0 # Huge penalty for failing
        
    # 3. Attitude & Gimbal penalties (prevent violent shaking)
    # Action is [throttle, gimbal_pitch, gimbal_yaw]
    throttle, gimbal_pitch, gimbal_yaw = action
    
    # Extreme gimbal angles penalty
    gimbal_magnitude_sq = gimbal_pitch**2 + gimbal_yaw**2
    if gimbal_magnitude_sq > 0.5**2: # Penalize if using >50% of gimbal range continuously
        reward -= 2.0 * gimbal_magnitude_sq
        
    # Penalize rapid angular velocity (shaking)
    if ang_vel > math.radians(10.0): # More than 10 deg/s
        reward -= 5.0 * ang_vel
        
    # 4. Massive Sparse Reward for Success
    # Assume insertion is reached if velocity > 7.4km/s and altitude > 500km
    if vel >= TARGET_VELOCITY and alt >= 500000.0:
        reward += 1000.0
        
    return reward
