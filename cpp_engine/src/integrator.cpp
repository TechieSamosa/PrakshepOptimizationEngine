/**
 * @file integrator.cpp
 * @brief 4th-order Runge-Kutta (RK4) integrator for 6-DOF rocket dynamics.
 *
 * Propagates the full 14-dimensional state vector:
 *   - Position (3)       : ECI metres
 *   - Velocity (3)       : ECI m/s
 *   - Attitude (4)       : Body-to-ECI quaternion (w, x, y, z)
 *   - Angular velocity (3): Body-frame rad/s
 *   - Mass (1)           : Total vehicle mass in kg
 *
 * The derivative function computes all applied forces and moments:
 *   - Gravitational acceleration (J2-perturbed, from gravity module)
 *   - Propulsive thrust (altitude-dependent Isp, gimbaled, from propulsion module)
 *   - Aerodynamic drag (Mach-dependent Cd, from aerodynamics module)
 *
 * Rotational dynamics are simplified: angular velocity is currently held
 * constant (no torque model). The quaternion is kinematically propagated
 * via q̇ = ½ q ⊗ ω and renormalized after each RK4 step to prevent
 * numerical drift from unity.
 *
 * Classical RK4 provides O(h⁴) local truncation error, which is more than
 * adequate for the ~16 ms timesteps used in real-time simulation.
 *
 * @author Prakshep GNC Team
 * @date 2026
 */

#include "prakshep/integrator.hpp"
#include "prakshep/constants.hpp"
#include "prakshep/atmosphere.hpp"
#include "prakshep/gravity.hpp"
#include "prakshep/aerodynamics.hpp"
#include "prakshep/propulsion.hpp"

#include <cmath>
#include <algorithm>

namespace prakshep {
namespace integrator {

// ============================================================================
//  StateDerivative: represents the time derivative of the full state vector
// ============================================================================

/**
 * @brief Intermediate structure holding the time derivative of each state
 *        component, used by the RK4 algorithm to accumulate k-values.
 *
 * Operator overloads for scalar multiplication and addition allow concise
 * RK4 update expressions like: state + (k1 + 2*k2 + 2*k3 + k4) * (1/6)
 */
struct StateDerivative {
    Vec3       pos_dot;      ///< dPosition/dt = velocity [m/s]
    Vec3       vel_dot;      ///< dVelocity/dt = acceleration [m/s²]
    Quaternion att_dot;      ///< dAttitude/dt = ½ q ⊗ ω_body [1/s]
    Vec3       omega_dot;    ///< dAngularVelocity/dt = torque/inertia [rad/s²]
    double     mass_dot;     ///< dMass/dt = -mass_flow_rate [kg/s]
};

/**
 * @brief Scalar multiplication for StateDerivative (used in RK4 weighting).
 */
static StateDerivative operator*(const StateDerivative& d, double s) {
    StateDerivative result;
    result.pos_dot   = d.pos_dot * s;
    result.vel_dot   = d.vel_dot * s;
    // Quaternion scaling: scale each component individually
    result.att_dot.w = d.att_dot.w * s;
    result.att_dot.x = d.att_dot.x * s;
    result.att_dot.y = d.att_dot.y * s;
    result.att_dot.z = d.att_dot.z * s;
    result.omega_dot = d.omega_dot * s;
    result.mass_dot  = d.mass_dot * s;
    return result;
}

/**
 * @brief Scalar multiplication (commutative form).
 */
static StateDerivative operator*(double s, const StateDerivative& d) {
    return d * s;
}

/**
 * @brief Addition of two StateDerivatives (used in RK4 summation).
 */
static StateDerivative operator+(const StateDerivative& a, const StateDerivative& b) {
    StateDerivative result;
    result.pos_dot   = a.pos_dot + b.pos_dot;
    result.vel_dot   = a.vel_dot + b.vel_dot;
    result.att_dot.w = a.att_dot.w + b.att_dot.w;
    result.att_dot.x = a.att_dot.x + b.att_dot.x;
    result.att_dot.y = a.att_dot.y + b.att_dot.y;
    result.att_dot.z = a.att_dot.z + b.att_dot.z;
    result.omega_dot = a.omega_dot + b.omega_dot;
    result.mass_dot  = a.mass_dot + b.mass_dot;
    return result;
}

// ============================================================================
//  State advancement helper
// ============================================================================

/**
 * @brief Advance a StateVector by a StateDerivative scaled by a time step.
 *
 * Computes: new_state = old_state + derivative * dt
 *
 * The quaternion is updated by direct component addition (Euler integration
 * of the quaternion derivative). Renormalization is deferred to after the
 * full RK4 step to avoid repeated normalization overhead.
 *
 * @param state  Current state vector.
 * @param d      State derivative (already scaled by the desired factor).
 * @return Advanced state vector.
 */
static StateVector advance_state(const StateVector& state, const StateDerivative& d) {
    StateVector result;

    // Translational state
    result.position = state.position + d.pos_dot;
    result.velocity = state.velocity + d.vel_dot;

    // Attitude quaternion (component-wise Euler step)
    result.attitude.w = state.attitude.w + d.att_dot.w;
    result.attitude.x = state.attitude.x + d.att_dot.x;
    result.attitude.y = state.attitude.y + d.att_dot.y;
    result.attitude.z = state.attitude.z + d.att_dot.z;

    // Rotational state
    result.angular_velocity = state.angular_velocity + d.omega_dot;

    // Mass (clamp to prevent negative mass from numerical overshoot)
    result.mass = std::max(state.mass + d.mass_dot, 1.0);

    // Propagate time
    result.time = state.time;  // Time is managed by the caller

    return result;
}

// ============================================================================
//  Derivative computation (the "f" in dy/dt = f(y, t))
// ============================================================================

/**
 * @brief Compute the time derivative of the full state vector at a given
 *        instant, incorporating all applied forces.
 *
 * Force model:
 *   1. Gravity: J2-perturbed gravitational acceleration from gravity module.
 *   2. Propulsion: Gimbaled thrust from the active stage, altitude-dependent
 *      Isp, mass flow rate.
 *   3. Aerodynamics: Mach-dependent drag opposing the velocity vector.
 *
 * Rotational kinematics:
 *   Quaternion derivative: q̇ = ½ q ⊗ Ω
 *   where Ω = (0, ωx, ωy, ωz) is the angular velocity as a pure quaternion.
 *
 * Rotational dynamics are simplified: ω̇ = 0 (no torque model).
 * This is acceptable for trajectory-level simulation where attitude is
 * commanded by the flight computer rather than free-flying.
 *
 * @param state          Current state vector.
 * @param config         Vehicle configuration.
 * @param current_stage  Index of the currently active stage.
 * @param throttle       Commanded throttle [0, 1].
 * @param gimbal_pitch   Gimbal deflection about body X [rad].
 * @param gimbal_yaw     Gimbal deflection about body Y [rad].
 * @param dt             Time step size (for context, not used in derivative).
 * @return StateDerivative at the current state.
 */
static StateDerivative compute_derivative(
    const StateVector& state,
    const RocketConfig& config,
    int current_stage,
    double throttle,
    double gimbal_pitch,
    double gimbal_yaw)
{
    StateDerivative deriv;

    // ------------------------------------------------------------------
    // 1. Geometric altitude (approximate spherical Earth)
    //    More accurate than subtracting R_EARTH from |pos| for oblate Earth,
    //    but sufficient for atmosphere/propulsion lookups.
    // ------------------------------------------------------------------
    const double position_magnitude = state.position.norm();
    const double altitude = position_magnitude - constants::R_EARTH;

    // ------------------------------------------------------------------
    // 2. Atmosphere state at current altitude
    // ------------------------------------------------------------------
    const AtmosphereState atm = atmosphere::compute(
        std::max(altitude, 0.0)  // Clamp to prevent negative altitude lookup
    );

    // ------------------------------------------------------------------
    // 3. Gravitational acceleration (J2-perturbed)
    // ------------------------------------------------------------------
    const Vec3 gravity_accel = gravity::compute(state.position);

    // ------------------------------------------------------------------
    // 4. Propulsion forces
    //    Compute thrust and mass flow rate from the current stage.
    //    If the stage index exceeds available stages, no thrust is produced.
    // ------------------------------------------------------------------
    Vec3   thrust_accel{0.0, 0.0, 0.0};
    double mass_flow = 0.0;

    if (current_stage >= 0 &&
        current_stage < static_cast<int>(config.stages.size())) {
        const StageConfig& stage = config.stages[static_cast<size_t>(current_stage)];

        // Remaining propellant is not tracked here; pass a large number
        // to ensure the propulsion module doesn't flag depletion during
        // intermediate RK4 evaluations. Depletion is handled by the
        // Simulation class between full time steps.
        const propulsion::PropulsionState prop = propulsion::compute(
            stage,
            std::max(altitude, 0.0),
            throttle,
            gimbal_pitch,
            gimbal_yaw,
            state.attitude,
            stage.propellant_mass  // Use full load for derivative evaluation
        );

        // Convert thrust force to acceleration: a = F/m
        if (state.mass > 0.0) {
            thrust_accel = prop.thrust_force * (1.0 / state.mass);
        }
        mass_flow = prop.mass_flow_rate;
    }

    // ------------------------------------------------------------------
    // 5. Aerodynamic drag acceleration
    //    Only significant below ~100 km (Kármán line). The aerodynamics
    //    module returns the drag force vector; divide by mass for accel.
    // ------------------------------------------------------------------
    Vec3 drag_accel{0.0, 0.0, 0.0};
    Vec3 grid_fin_accel{0.0, 0.0, 0.0};
    Vec3 aero_torque{0.0, 0.0, 0.0};
    
    if (altitude < 100000.0 && altitude >= 0.0) {
        const Vec3 drag_force = aerodynamics::compute_drag(state, config, atm);
        if (state.mass > 0.0) {
            drag_accel = drag_force * (1.0 / state.mass);
        }
        
        // ------------------------------------------------------------------
        // Grid Fin Aerodynamics & 6-DOF Steering Moments
        // ------------------------------------------------------------------
        if (config.grid_fins.deployed && throttle == 0.0) { // Typically used during coast/descent
            // Map TVC inputs to grid fin pitch/yaw deflections
            double fin_pitch = gimbal_pitch;
            double fin_yaw = gimbal_yaw;
            double angle_of_attack = std::max(std::abs(fin_pitch), std::abs(fin_yaw));
            
            // Limit to max mechanical deflection
            angle_of_attack = std::min(angle_of_attack, config.grid_fins.max_angle);
            
            const Vec3 grid_fin_force = aerodynamics::compute_grid_fin_force(state, atm, config.grid_fins, angle_of_attack);
            
            if (state.mass > 0.0) {
                grid_fin_accel = grid_fin_force * (1.0 / state.mass);
                
                // Assume grid fins are located at the top of the stage (approx 0.4 * length from CoM)
                double lever_arm = config.total_height * 0.4;
                
                // Lift force creates pitching/yawing moment
                // Simplified moment calculation based on body-frame axes
                // F_lift approx orthogonal to velocity. 
                // We map fin pitch -> body X torque, fin yaw -> body Y torque
                double q = aerodynamics::compute_dynamic_pressure(atm.density, state.velocity.norm());
                double lift_coef = 2.0 * M_PI * angle_of_attack;
                if (state.velocity.norm() / atm.speed_of_sound > 1.0) lift_coef *= 0.5; // very rough proxy
                
                double moment_mag = q * lift_coef * config.grid_fins.area * lever_arm;
                
                // Construct body-frame torque vector
                Vec3 body_torque;
                body_torque.x = (fin_pitch > 0 ? 1 : -1) * moment_mag * std::abs(fin_pitch)/(angle_of_attack + 1e-6);
                body_torque.y = (fin_yaw > 0 ? 1 : -1) * moment_mag * std::abs(fin_yaw)/(angle_of_attack + 1e-6);
                body_torque.z = 0.0;
                
                aero_torque = body_torque;
            }
        }
    }

    // ------------------------------------------------------------------
    // 6. Total translational acceleration
    // ------------------------------------------------------------------
    const Vec3 total_accel = gravity_accel + thrust_accel + drag_accel + grid_fin_accel;

    // ------------------------------------------------------------------
    // 7. Position derivative = velocity
    // ------------------------------------------------------------------
    deriv.pos_dot = state.velocity;

    // ------------------------------------------------------------------
    // 8. Velocity derivative = total acceleration
    // ------------------------------------------------------------------
    deriv.vel_dot = total_accel;

    // ------------------------------------------------------------------
    // 9. Quaternion derivative: q̇ = ½ q ⊗ Ω
    //
    //    where Ω is the angular velocity expressed as a pure quaternion:
    //    Ω = (0, ωx, ωy, ωz)
    //
    //    Quaternion multiplication q ⊗ Ω:
    //    If q = (qw, qx, qy, qz) and Ω = (0, wx, wy, wz), then:
    //    q ⊗ Ω = (
    //      -qx*wx - qy*wy - qz*wz,
    //       qw*wx + qy*wz - qz*wy,
    //       qw*wy + qz*wx - qx*wz,
    //       qw*wz + qx*wy - qy*wx
    //    )
    //    Then multiply by 0.5.
    // ------------------------------------------------------------------
    const double wx = state.angular_velocity.x;
    const double wy = state.angular_velocity.y;
    const double wz = state.angular_velocity.z;

    const double qw = state.attitude.w;
    const double qx = state.attitude.x;
    const double qy = state.attitude.y;
    const double qz = state.attitude.z;

    deriv.att_dot.w = 0.5 * (-qx * wx - qy * wy - qz * wz);
    deriv.att_dot.x = 0.5 * ( qw * wx + qy * wz - qz * wy);
    deriv.att_dot.y = 0.5 * ( qw * wy + qz * wx - qx * wz);
    deriv.att_dot.z = 0.5 * ( qw * wz + qx * wy - qy * wx);

    // ------------------------------------------------------------------
    // 10. Angular velocity derivative = I⁻¹ (τ - ω × Iω)
    //     Implement true rigid body dynamics for VTVL grid fin steering.
    // ------------------------------------------------------------------
    // Approximate inertia tensor for a uniform cylinder:
    // Ixx = Iyy = 1/12 * m * L^2 + 1/4 * m * R^2
    // Izz = 1/2 * m * R^2
    double r = std::sqrt(config.reference_area / M_PI);
    double h = config.total_height;
    double I_xx = (1.0 / 12.0) * state.mass * h * h + (1.0 / 4.0) * state.mass * r * r;
    double I_yy = I_xx;
    double I_zz = 0.5 * state.mass * r * r;

    // Euler's equations of rigid body motion:
    // τ_x = I_xx * ω_dot_x + (I_zz - I_yy) * ω_y * ω_z
    // τ_y = I_yy * ω_dot_y + (I_xx - I_zz) * ω_z * ω_x
    // τ_z = I_zz * ω_dot_z + (I_yy - I_xx) * ω_x * ω_y
    
    // Add active aero steering torques
    Vec3 total_torque = aero_torque;

    if (I_xx > 0 && I_yy > 0 && I_zz > 0) {
        deriv.omega_dot.x = (total_torque.x - (I_zz - I_yy) * wy * wz) / I_xx;
        deriv.omega_dot.y = (total_torque.y - (I_xx - I_zz) * wz * wx) / I_yy;
        deriv.omega_dot.z = (total_torque.z - (I_yy - I_xx) * wx * wy) / I_zz;
    } else {
        deriv.omega_dot = Vec3{0.0, 0.0, 0.0};
    }

    // ------------------------------------------------------------------
    // 11. Mass derivative = negative mass flow rate
    // ------------------------------------------------------------------
    deriv.mass_dot = -mass_flow;

    return deriv;
}

// ============================================================================
//  RK4 Integration Step
// ============================================================================

/**
 * @brief Perform a single classical 4th-order Runge-Kutta integration step.
 *
 * The RK4 method evaluates the derivative function at four points within
 * the time step to achieve O(h⁴) accuracy:
 *
 *   k₁ = h · f(tₙ, yₙ)
 *   k₂ = h · f(tₙ + h/2, yₙ + k₁/2)
 *   k₃ = h · f(tₙ + h/2, yₙ + k₂/2)
 *   k₄ = h · f(tₙ + h, yₙ + k₃)
 *
 *   yₙ₊₁ = yₙ + (k₁ + 2k₂ + 2k₃ + k₄) / 6
 *
 * After the integration step, the attitude quaternion is renormalized to
 * prevent drift from the unit quaternion manifold due to accumulated
 * floating-point errors.
 *
 * @param state          Current state vector at time tₙ.
 * @param config         Vehicle configuration.
 * @param current_stage  Index of the currently burning stage.
 * @param throttle       Commanded throttle fraction [0, 1].
 * @param gimbal_pitch   Gimbal deflection about body X [rad].
 * @param gimbal_yaw     Gimbal deflection about body Y [rad].
 * @param dt             Integration time step [s].
 * @return New state vector at time tₙ₊₁ = tₙ + dt.
 */
StateVector rk4_step(const StateVector& state,
                     const RocketConfig& config,
                     int current_stage,
                     double throttle,
                     double gimbal_pitch,
                     double gimbal_yaw,
                     double dt) {
    // ==================================================================
    // k₁ = dt · f(state)
    // ==================================================================
    const StateDerivative f1 = compute_derivative(
        state, config, current_stage, throttle, gimbal_pitch, gimbal_yaw);
    const StateDerivative k1 = f1 * dt;

    // ==================================================================
    // k₂ = dt · f(state + k₁/2)
    //
    // Evaluate the derivative at the midpoint using k₁.
    // ==================================================================
    const StateVector s2 = advance_state(state, k1 * 0.5);
    const StateDerivative f2 = compute_derivative(
        s2, config, current_stage, throttle, gimbal_pitch, gimbal_yaw);
    const StateDerivative k2 = f2 * dt;

    // ==================================================================
    // k₃ = dt · f(state + k₂/2)
    //
    // Re-evaluate at the midpoint using the improved k₂ slope.
    // ==================================================================
    const StateVector s3 = advance_state(state, k2 * 0.5);
    const StateDerivative f3 = compute_derivative(
        s3, config, current_stage, throttle, gimbal_pitch, gimbal_yaw);
    const StateDerivative k3 = f3 * dt;

    // ==================================================================
    // k₄ = dt · f(state + k₃)
    //
    // Evaluate at the endpoint using the full k₃ step.
    // ==================================================================
    const StateVector s4 = advance_state(state, k3);
    const StateDerivative f4 = compute_derivative(
        s4, config, current_stage, throttle, gimbal_pitch, gimbal_yaw);
    const StateDerivative k4 = f4 * dt;

    // ==================================================================
    // Combine: weighted average of the four slopes
    //   Δy = (k₁ + 2k₂ + 2k₃ + k₄) / 6
    // ==================================================================
    const StateDerivative weighted_sum = (k1 + 2.0 * k2 + 2.0 * k3 + k4) * (1.0 / 6.0);
    StateVector new_state = advance_state(state, weighted_sum);

    // ==================================================================
    // Post-step: advance time
    // ==================================================================
    new_state.time = state.time + dt;

    // ==================================================================
    // Post-step: quaternion renormalization
    //
    // The attitude quaternion must remain on the unit sphere (|q| = 1).
    // Numerical integration introduces small perturbations away from unity.
    // Renormalizing after each step bounds the drift to O(h⁴) per step.
    //
    // This is far more numerically stable than using a Lagrange multiplier
    // constraint and effectively free computationally.
    // ==================================================================
    new_state.attitude = new_state.attitude.normalized();

    // ==================================================================
    // Post-step: clamp mass to prevent negative values
    // ==================================================================
    if (new_state.mass < 1.0) {
        new_state.mass = 1.0;  // Minimum 1 kg to avoid division by zero
    }

    return new_state;
}

}  // namespace integrator
}  // namespace prakshep
