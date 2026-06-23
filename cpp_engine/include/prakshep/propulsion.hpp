#pragma once

/**
 * @file propulsion.hpp
 * @brief Propulsion model interface for thrust, mass flow, and TVC.
 *
 * Computes the thrust force vector in ECI given the current stage
 * configuration, throttle setting, gimbal angles, and vehicle attitude.
 * Handles altitude-dependent Isp interpolation between sea-level and
 * vacuum values.
 *
 * @author  Prakshep GNC Team
 * @version 1.0.0
 */

#include "types.hpp"

namespace prakshep {
namespace propulsion {

/**
 * @struct PropulsionState
 * @brief  Output of the propulsion model for a single time step.
 */
struct PropulsionState {
    Vec3   thrust_force;     ///< Thrust force vector in ECI frame (N)
    double mass_flow_rate;   ///< Propellant mass flow rate (kg/s, positive = consuming)
    bool   stage_depleted;   ///< True if the stage has exhausted its propellant
};

/**
 * @brief Compute propulsion state for the current time step.
 *
 * Calculates thrust force in ECI considering:
 *   - Altitude-dependent Isp interpolation (sea-level → vacuum)
 *   - Throttle setting (0–1)
 *   - TVC gimbal deflection (pitch & yaw)
 *   - Vehicle attitude quaternion (body → ECI transformation)
 *   - Remaining propellant (stage depletion check)
 *
 * Thrust direction convention:
 *   In the body frame, nominal thrust is along +X (longitudinal axis).
 *   Gimbal angles deflect the thrust:
 *     - gimbal_pitch rotates about body Y
 *     - gimbal_yaw   rotates about body Z
 *
 * The body-frame thrust is then rotated to ECI via the attitude quaternion.
 *
 * @param stage                Stage configuration parameters.
 * @param altitude             Current geometric altitude (m) for Isp interpolation.
 * @param throttle             Throttle command [0, 1].
 * @param gimbal_pitch         TVC pitch deflection (radians).
 * @param gimbal_yaw           TVC yaw deflection (radians).
 * @param attitude             Body-to-ECI rotation quaternion.
 * @param remaining_propellant Remaining propellant mass (kg).
 * @return PropulsionState with thrust vector, mass flow, and depletion flag.
 */
PropulsionState compute(
    const StageConfig& stage,
    double altitude,
    double throttle,
    double gimbal_pitch,
    double gimbal_yaw,
    const Quaternion& attitude,
    double remaining_propellant
);

/**
 * @brief Interpolate specific impulse between sea-level and vacuum values.
 *
 * Uses a simple altitude-based linear interpolation:
 *   - At 0 m:       Isp = Isp_sea_level
 *   - At 80000 m:   Isp = Isp_vacuum
 *   - In between:   linear blend
 *   - Below 0 m:    Isp_sea_level
 *   - Above 80 km:  Isp_vacuum
 *
 * @param stage    Stage configuration with Isp values.
 * @param altitude Current geometric altitude (m).
 * @return Interpolated specific impulse (s).
 */
double interpolate_isp(const StageConfig& stage, double altitude);

} // namespace propulsion
} // namespace prakshep
