/**
 * @file flight_computer.cpp
 * @brief On-board flight computer (GNC) for ISRO launch vehicle simulation.
 *
 * Implements the flight guidance logic that converts mission requirements
 * into real-time TVC (Thrust Vector Control) commands. The primary guidance
 * law is a pre-programmed gravity turn with proportional feedback steering.
 *
 * Pitch program (based on PSLV flight profile):
 *   T+0   to T+10s  : 90° (vertical ascent, clearing the launch tower)
 *   T+10  to T+15s  : 90° → 80° (pitch-over kick manoeuvre)
 *   T+15  to T+120s : 80° → 30° (gravity turn, progressively pitching over)
 *   T+120 to T+300s : 30° → 5° (shallow descent toward horizontal)
 *   T+300s+         : 0° (horizontal flight for orbit insertion)
 *
 * The pitch-over kick at T+10s initiates the gravity turn. In reality this
 * is a small impulsive attitude change (~2–3° over 3–5 seconds) that allows
 * gravity to naturally curve the trajectory, reducing structural loads and
 * gravity losses compared to a pure pitch-program turn.
 *
 * Steering law:
 *   A simple proportional controller computes gimbal commands to drive the
 *   vehicle's actual pitch toward the commanded pitch. Gain is tuned for
 *   stability with the typical vehicle inertia and TVC authority.
 *
 * TVC override:
 *   The RL agent (or manual control) can override the flight computer's
 *   gimbal and throttle commands for training and experimentation.
 *
 * @author Prakshep GNC Team
 * @date 2026
 */

#include "prakshep/flight_computer.hpp"
#include "prakshep/constants.hpp"

#include <cmath>
#include <algorithm>

namespace prakshep {

// ============================================================================
//  Constructor
// ============================================================================

/**
 * @brief Construct a FlightComputer bound to a specific vehicle configuration.
 *
 * The configuration is stored by pointer (non-owning). The caller must
 * ensure the RocketConfig outlives the FlightComputer.
 *
 * @param config  Reference to the vehicle configuration.
 */
FlightComputer::FlightComputer(const RocketConfig& config)
    : config_(&config)
    , tvc_override_active_(false)
    , override_gimbal_pitch_(0.0)
    , override_gimbal_yaw_(0.0)
    , override_throttle_(1.0)
    , is_pad_3_(false)
{
    // Pad 3 direct azimuth check is determined via coordinates in Simulation but we can flag it here if needed later
}

// ============================================================================
//  TVC Override Interface
// ============================================================================

/**
 * @brief Activate TVC override mode with specified gimbal and throttle commands.
 *
 * When active, the compute() method bypasses the gravity turn guidance
 * and returns the override values directly. This allows the RL agent or
 * human operator to take direct control of the vehicle.
 *
 * @param gimbal_pitch  Gimbal deflection about body X-axis [rad].
 * @param gimbal_yaw    Gimbal deflection about body Y-axis [rad].
 * @param throttle      Throttle fraction [0, 1].
 */
void FlightComputer::set_tvc_override(double gimbal_pitch, double gimbal_yaw, double throttle) {
    tvc_override_active_  = true;
    override_gimbal_pitch_ = gimbal_pitch;
    override_gimbal_yaw_   = gimbal_yaw;
    override_throttle_     = std::clamp(throttle, 0.0, 1.0);
}

/**
 * @brief Deactivate TVC override, returning to autonomous gravity turn guidance.
 */
void FlightComputer::clear_tvc_override() {
    tvc_override_active_ = false;
}

// ============================================================================
//  Pitch Program
// ============================================================================

/**
 * @brief Compute the commanded pitch angle from the pre-programmed gravity
 *        turn profile.
 *
 * The pitch angle is measured from horizontal (0° = horizontal, 90° = vertical).
 * This convention matches standard aerospace practice where "pitch angle"
 * or "flight path angle" is the angle above the local horizontal.
 *
 * The profile is a piecewise-linear function of time, designed to:
 *   1. Clear the launch tower vertically (0–10 s)
 *   2. Initiate the gravity turn with a gentle kick (10–15 s)
 *   3. Follow an approximately optimal gravity turn (15–120 s)
 *   4. Approach horizontal for orbit insertion (120–300 s)
 *   5. Hold horizontal for circularisation (300+ s)
 *
 * @param time      Mission elapsed time [s].
 * @param altitude  Current geometric altitude [m] (not used currently, reserved
 *                  for altitude-triggered guidance extensions).
 * @return Commanded pitch angle [rad] above horizontal.
 */
double FlightComputer::compute_pitch_angle(double time, [[maybe_unused]] double altitude) const {
    // ------------------------------------------------------------------
    // Pitch profile definition (angles in degrees, converted to rad at return)
    // ------------------------------------------------------------------
    double pitch_deg;

    if (time <= 10.0) {
        // ---------------------------------------------------------------
        // Phase 1: Vertical ascent (0–10 s)
        //
        // The vehicle rises straight up to clear the launch tower
        // (Umbilical Tower height ~80 m at SDSC-SHAR) and establish
        // sufficient altitude for the pitch-over manoeuvre.
        // ---------------------------------------------------------------
        pitch_deg = 90.0;
    }
    else if (time <= 15.0) {
        // ---------------------------------------------------------------
        // Phase 2: Pitch-over kick (10–15 s)
        //
        // A programmed pitch rate of ~2°/s tilts the vehicle from 90°
        // to 80°. This small perturbation from vertical is enough to
        // initiate the gravity turn – gravity will naturally curve the
        // trajectory from this point.
        //
        // In reality, the pitch kick is triggered by an inertial
        // measurement unit (IMU) detecting sufficient vertical velocity
        // (typically 50–80 m/s).
        // ---------------------------------------------------------------
        const double fraction = (time - 10.0) / 5.0;
        pitch_deg = 90.0 - fraction * 10.0;  // 90° → 80°
    }
    else if (time <= 120.0) {
        // ---------------------------------------------------------------
        // Phase 3: Gravity turn (15–120 s)
        //
        // The vehicle follows an approximately zero-angle-of-attack
        // trajectory, pitching over from 80° to 30° as gravity
        // progressively bends the flight path toward horizontal.
        //
        // The linear ramp is an approximation; real gravity turns
        // follow a nonlinear path determined by the balance of thrust,
        // gravity, and aerodynamic forces.
        //
        // Max-Q occurs during this phase (typically T+60–80 s) when
        // the product of atmospheric density and velocity squared peaks.
        // ---------------------------------------------------------------
        const double fraction = (time - 15.0) / 105.0;
        pitch_deg = 80.0 - fraction * 50.0;  // 80° → 30°
    }
    else if (time <= 300.0) {
        // ---------------------------------------------------------------
        // Phase 4: Pitch-over toward horizontal (120–300 s)
        //
        // Upper-stage burns continue pitching the vehicle toward
        // horizontal. The rate is slower (30° to 5° over 180 s ≈ 0.14°/s)
        // as the vehicle is now well above the dense atmosphere.
        //
        // Fairing jettison typically occurs at T+200–250 s when
        // aerodynamic heating is no longer a concern (altitude > 100 km).
        // ---------------------------------------------------------------
        const double fraction = (time - 120.0) / 180.0;
        pitch_deg = 30.0 - fraction * 25.0;  // 30° → 5°
    }
    else {
        // ---------------------------------------------------------------
        // Phase 5: Horizontal flight (300+ s)
        //
        // The vehicle is essentially horizontal, burning to achieve
        // orbital velocity. Final orbit insertion and circularisation
        // burns occur in this phase.
        // ---------------------------------------------------------------
        pitch_deg = 0.0;
    }

    // Convert to radians for the guidance law
    return pitch_deg * constants::DEG_TO_RAD;
}

// ============================================================================
//  Main Guidance Computation
// ============================================================================

/**
 * @brief Compute the flight command (gimbal + throttle) for the current state.
 *
 * Algorithm:
 *   1. If TVC override is active, return the override values immediately.
 *   2. Determine the commanded pitch from the gravity turn program.
 *   3. Extract the current pitch from the vehicle's attitude quaternion
 *      by converting to Euler angles.
 *   4. Compute the pitch error (desired - actual).
 *   5. Apply proportional control to derive gimbal pitch command.
 *   6. Yaw gimbal is held at zero (simple 2D trajectory for now).
 *   7. Throttle is set to full (1.0) – throttling is reserved for RL control.
 *
 * Proportional gain:
 *   Kp = 0.5 rad/rad (i.e., for every 1 rad of pitch error, command 0.5 rad
 *   of gimbal deflection). This is conservative – typical launch vehicles
 *   use gains of 0.3–1.0 depending on the vehicle's rotational inertia
 *   and TVC authority. The gain is clamped by the stage's gimbal_range.
 *
 * @param state          Current state vector.
 * @param current_stage  Index of the currently burning stage.
 * @return FlightCommand with throttle and gimbal deflections.
 */
FlightCommand FlightComputer::compute(const StateVector& state, int current_stage) {
    FlightCommand cmd;
    cmd.stage_separate = false;
    cmd.abort          = false;

    // ------------------------------------------------------------------
    // Coast Phase Check (Upper stages prior to apogee insertion)
    // ------------------------------------------------------------------
    const double position_magnitude = state.position.norm();
    const double altitude = position_magnitude - constants::R_EARTH;
    
    // For specific vehicles, if we are waiting for apogee, set thrust to 0
    bool is_coasting = false;
    if (altitude > 130000.0 && state.velocity.norm() < 7200.0 && state.velocity.dot(state.position) > 0) {
        // We are ascending above 130km but not orbital yet.
        // A true mission planner would define the exact coast windows.
        // For simulation, if we reach 150km and want 500km apogee, we coast.
        // is_coasting = true; (Simulated later via explicit mission timelines)
    }

    // ------------------------------------------------------------------
    // Hoverslam / Suicide Burn Equation (VTVL EDL Physics)
    // ------------------------------------------------------------------
    bool is_edl_active = false;
    if (state.velocity.dot(state.position) < 0 && altitude < 50000.0) {
        // Vehicle is descending
        if (config_->type == RocketType::NGLV || config_->type == RocketType::VIKRAM) {
            is_edl_active = true;
            // Simplified Hoverslam Ignition Time Equation
            // V_f^2 = V_i^2 + 2 * a_net * d
            // a_net = (T_max / m) - g + (0.5 * rho * v^2 * Cd * A / m)
            double current_mass = state.mass;
            double max_thrust = 0.0;
            if (current_stage < (int)config_->stages.size()) {
                max_thrust = config_->stages[current_stage].thrust_sea_level;
            }
            if (max_thrust > 0) {
                double g = 9.81;
                double a_thrust = max_thrust / current_mass;
                double a_net = a_thrust - g; // Ignoring aero drag for conservative estimate
                
                // Distance to zero velocity
                double v_z = std::abs(state.velocity.dot(state.position.normalized())); // Vertical velocity magnitude
                double required_burn_distance = (v_z * v_z) / (2.0 * a_net);
                
                if (altitude <= required_burn_distance * 1.1) { // 10% safety margin for ignition
                    cmd.throttle = 1.0;
                } else {
                    cmd.throttle = 0.0; // Free-fall
                }
            } else {
                cmd.throttle = 0.0;
            }
            
            // Grid fin steering towards target would go here
            // We expose grid fin angles to RL agent via TVC override
        }
    }

    // ------------------------------------------------------------------
    // 1. TVC override – bypass all guidance logic
    // ------------------------------------------------------------------
    if (tvc_override_active_) {
        cmd.throttle     = override_throttle_;
        cmd.gimbal_pitch = override_gimbal_pitch_;
        cmd.gimbal_yaw   = override_gimbal_yaw_;
        return cmd;
    }

    // ------------------------------------------------------------------
    // 2. Commanded pitch from the gravity turn program
    // ------------------------------------------------------------------

    const double desired_pitch_rad = compute_pitch_angle(state.time, altitude);

    // ------------------------------------------------------------------
    // 3. Extract current pitch from attitude quaternion
    //
    //    Convert the body-to-ECI quaternion to Euler angles (ZYX convention).
    //    We need the pitch angle, which is the rotation about the Y-axis.
    //
    //    For a quaternion q = (w, x, y, z), the pitch (θ) is:
    //    θ = asin(2(wz + xy))... wait, actually let's use the standard
    //    aerospace ZYX Euler extraction:
    //
    //    The vehicle's thrust axis is body +Z. In ECI, the thrust direction
    //    is attitude.rotate({0,0,1}). The pitch above local horizontal is
    //    the angle between this vector and the local horizontal plane.
    //
    //    Local horizontal at position r: perpendicular to r (radial direction).
    //    Pitch = asin(dot(thrust_dir, r_hat))
    // ------------------------------------------------------------------
    const Vec3 thrust_dir_eci = state.attitude.rotate(Vec3{0.0, 0.0, 1.0});
    const Vec3 r_hat = state.position.normalized();

    // Pitch angle: angle between thrust direction and local horizontal
    // sin(pitch) = dot(thrust_dir, radial_up)
    const double sin_pitch = thrust_dir_eci.dot(r_hat);
    const double current_pitch_rad = std::asin(std::clamp(sin_pitch, -1.0, 1.0));

    // ------------------------------------------------------------------
    // 4. Pitch error (proportional control)
    // ------------------------------------------------------------------
    const double pitch_error = desired_pitch_rad - current_pitch_rad;

    // ------------------------------------------------------------------
    // 5. Proportional gimbal command
    //
    //    Kp = 0.5: moderate gain providing smooth convergence.
    //    Gimbal command is clamped to the stage's physical gimbal range.
    //
    //    Sign convention: positive gimbal_pitch tilts the thrust vector
    //    to pitch the vehicle down (nose moves toward horizontal).
    //    A negative pitch error means we need to pitch down → positive gimbal.
    // ------------------------------------------------------------------
    constexpr double Kp = 0.5;
    double gimbal_cmd = -Kp * pitch_error;  // Negative because gimbal opposes error

    // Clamp to physical gimbal limits of the current stage
    double max_gimbal = 0.07;  // Default 4° if no stage info
    if (current_stage >= 0 &&
        current_stage < static_cast<int>(config_->stages.size())) {
        max_gimbal = config_->stages[static_cast<size_t>(current_stage)].gimbal_range;
    }
    gimbal_cmd = std::clamp(gimbal_cmd, -max_gimbal, max_gimbal);

    // ------------------------------------------------------------------
    // 6. Assemble flight command
    // ------------------------------------------------------------------
    if (!is_edl_active && !is_coasting) {
        cmd.throttle     = 1.0;          // Full throttle (default for unpowered guidance)
        cmd.gimbal_pitch = gimbal_cmd;   // Pitch axis gimbal [rad]
        cmd.gimbal_yaw   = 0.0;          // No yaw steering (2D trajectory)
    } else if (is_coasting) {
        cmd.throttle = 0.0;
        cmd.gimbal_pitch = 0.0;
        cmd.gimbal_yaw = 0.0;
    } else if (is_edl_active) {
        // Throttle set by Hoverslam logic above
        // Gimbal used to maintain retrograde attitude
        cmd.gimbal_pitch = gimbal_cmd * 0.5; // Reduced gain during descent
        cmd.gimbal_yaw = 0.0;
    }

    return cmd;
}

}  // namespace prakshep
