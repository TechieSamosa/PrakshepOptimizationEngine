#pragma once

/**
 * @file integrator.hpp
 * @brief Classical 4th-order Runge-Kutta (RK4) integrator for 6-DOF state propagation.
 *
 * Provides a single-step RK4 integrator that advances the full StateVector
 * (position, velocity, attitude, angular velocity, mass) by one time step dt.
 * The equations of motion incorporate:
 *   - J2-perturbed gravity
 *   - Atmospheric drag (with co-rotation correction)
 *   - Thrust (with TVC gimbal)
 *   - Mass depletion
 *
 * RK4 offers 4th-order accuracy (global error ~ O(dt⁴)) with excellent
 * stability for the ODE systems encountered in launch trajectory simulation.
 * For a 60 Hz simulation tick (dt ≈ 16.67 ms) the truncation error is
 * negligible compared to modelling uncertainties.
 *
 * @author  Prakshep GNC Team
 * @version 1.0.0
 */

#include "types.hpp"

namespace prakshep {
namespace integrator {

/**
 * @brief Advance the state vector by one RK4 step.
 *
 * Evaluates the equations of motion at four intermediate points
 * (k1, k2, k3, k4) and blends them with the standard RK4 weights
 * (1/6, 1/3, 1/3, 1/6) to produce the state at t + dt.
 *
 * The equations of motion integrated are:
 *
 *   dr/dt = v                                  (kinematics)
 *   dv/dt = a_gravity + a_drag + a_thrust       (Newton's second law)
 *   dq/dt = ½ q ⊗ ω                            (attitude kinematics)
 *   dω/dt = 0 (simplified — no torque model)    (attitude dynamics)
 *   dm/dt = −ṁ_propellant                       (mass depletion)
 *
 * @param state         Current state vector at time t.
 * @param config        Vehicle configuration (stages, aero, etc.).
 * @param current_stage Index of the currently active stage.
 * @param throttle      Throttle command [0, 1].
 * @param gimbal_pitch  TVC pitch deflection (radians).
 * @param gimbal_yaw    TVC yaw deflection (radians).
 * @param dt            Time step size (seconds).
 * @return StateVector at time t + dt.
 */
StateVector rk4_step(
    const StateVector& state,
    const RocketConfig& config,
    int current_stage,
    double throttle,
    double gimbal_pitch,
    double gimbal_yaw,
    double dt
);

} // namespace integrator
} // namespace prakshep
