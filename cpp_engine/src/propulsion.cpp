/**
 * @file propulsion.cpp
 * @brief Propulsion subsystem model for ISRO launch vehicles.
 *
 * Implements the propulsion force computation pipeline:
 *   1. Altitude-dependent specific impulse (Isp) interpolation between
 *      sea-level and vacuum values across the 0–20 km atmospheric transition.
 *   2. Thrust vector generation with gimbal deflection in body frame,
 *      then rotation into ECI via the vehicle attitude quaternion.
 *   3. Mass flow rate derivation from the rocket equation (F = ṁ·Isp·g₀).
 *
 * Solid motors are treated as non-throttleable (effective_throttle = 1.0).
 * The `count` field in StageConfig allows modeling clustered engines as a
 * single effective stage (e.g., 6× PSOM-XL strap-ons).
 *
 * Reference frame conventions:
 *   - Body frame: +Z = thrust axis (forward/up), +X = pitch axis, +Y = yaw axis
 *   - ECI frame:  Earth-Centered Inertial (J2000-like)
 *
 * @author Prakshep GNC Team
 * @date 2026
 */

#include "prakshep/propulsion.hpp"
#include "prakshep/constants.hpp"

#include <cmath>
#include <algorithm>

namespace prakshep {
namespace propulsion {

// ============================================================================
//  Isp Interpolation
// ============================================================================

/**
 * @brief Compute effective specific impulse at a given altitude.
 *
 * Below 20 km the atmosphere exerts significant back-pressure on the nozzle,
 * reducing effective Isp. We model this with a simple linear interpolation
 * between sea-level Isp and vacuum Isp over the 0–20 km altitude band.
 *
 * Above 20 km the ambient pressure is negligible (~5.5 kPa, <6% of sea-level)
 * so we use the vacuum Isp directly.
 *
 * Special case: if isp_sea_level is zero (upper-stage engine never designed
 * for atmospheric operation), we return isp_vacuum at all altitudes.
 *
 * @param stage  Stage configuration containing Isp values.
 * @param altitude  Geometric altitude in metres above the WGS84 ellipsoid.
 * @return Effective specific impulse in seconds.
 */
double interpolate_isp(const StageConfig& stage, double altitude) {
    // ------------------------------------------------------------------
    // Guard: upper-stage engines with no sea-level rating
    // ------------------------------------------------------------------
    if (stage.isp_sea_level <= 0.0) {
        return stage.isp_vacuum;
    }

    // ------------------------------------------------------------------
    // Atmospheric transition band: 0 – 20 km
    // ------------------------------------------------------------------
    constexpr double TRANSITION_ALTITUDE = 20000.0;  // metres

    if (altitude >= TRANSITION_ALTITUDE) {
        // Fully in vacuum – nozzle operates at design expansion ratio
        return stage.isp_vacuum;
    }

    if (altitude <= 0.0) {
        // At or below sea level – maximum back-pressure
        return stage.isp_sea_level;
    }

    // ------------------------------------------------------------------
    // Linear interpolation: fraction = altitude / 20 km
    // Isp = Isp_sl + fraction * (Isp_vac - Isp_sl)
    // ------------------------------------------------------------------
    const double fraction = altitude / TRANSITION_ALTITUDE;
    return stage.isp_sea_level + fraction * (stage.isp_vacuum - stage.isp_sea_level);
}

// ============================================================================
//  Thrust Vector Computation
// ============================================================================

/**
 * @brief Compute the propulsion state for a single stage at the current flight
 *        conditions.
 *
 * Algorithm:
 *   1. Check propellant availability; return depleted if remaining ≤ 0.
 *   2. Interpolate Isp for current altitude.
 *   3. Apply throttle (solid motors override to 1.0).
 *   4. Compute scalar thrust magnitude = thrust_vacuum × throttle × count.
 *   5. Build thrust direction in body frame (+Z nominal, then gimbal).
 *   6. Rotate gimbal'd body-frame vector into ECI via attitude quaternion.
 *   7. Scale by magnitude and compute mass flow rate.
 *
 * Gimbal model:
 *   - gimbal_pitch rotates the thrust vector about the body +X axis.
 *   - gimbal_yaw   rotates the thrust vector about the body +Y axis.
 *   Small-angle approximation is NOT used; full trigonometric rotation is
 *   applied so large gimbal angles (up to ±5°) are handled correctly.
 *
 * @param stage              Stage configuration (thrust, Isp, counts, etc.).
 * @param altitude           Current geometric altitude [m].
 * @param throttle           Commanded throttle fraction [0, 1].
 * @param gimbal_pitch       Gimbal deflection about body X-axis [rad].
 * @param gimbal_yaw         Gimbal deflection about body Y-axis [rad].
 * @param attitude           Body-to-ECI attitude quaternion.
 * @param remaining_propellant  Remaining propellant mass [kg].
 * @return PropulsionState containing ECI thrust vector, mass flow rate, and
 *         depletion flag.
 */
PropulsionState compute(const StageConfig& stage,
                        double altitude,
                        double throttle,
                        double gimbal_pitch,
                        double gimbal_yaw,
                        const Quaternion& attitude,
                        double remaining_propellant) {
    // ------------------------------------------------------------------
    // 1. Propellant depletion check
    // ------------------------------------------------------------------
    if (remaining_propellant <= 0.0) {
        PropulsionState depleted;
        depleted.thrust_force = Vec3{0.0, 0.0, 0.0};
        depleted.mass_flow_rate = 0.0;
        depleted.stage_depleted = true;
        return depleted;
    }

    // ------------------------------------------------------------------
    // 2. Altitude-dependent specific impulse
    // ------------------------------------------------------------------
    const double isp = interpolate_isp(stage, altitude);

    // ------------------------------------------------------------------
    // 3. Effective throttle – solid motors cannot be throttled
    // ------------------------------------------------------------------
    const double effective_throttle = stage.is_solid ? 1.0 : std::clamp(throttle, 0.0, 1.0);

    // ------------------------------------------------------------------
    // 4. Scalar thrust magnitude
    //    We use thrust_vacuum here because Isp already encodes the altitude
    //    correction. Alternatively one could interpolate thrust directly, but
    //    the Isp-based mass-flow approach is more physically consistent for
    //    the trajectory integrator.
    // ------------------------------------------------------------------
    const double thrust_magnitude = stage.thrust_vacuum * effective_throttle
                                    * static_cast<double>(stage.count);

    // ------------------------------------------------------------------
    // 5. Mass flow rate from the rocket equation: F = ṁ · Isp · g₀
    //    ⟹ ṁ = F / (Isp · g₀)
    // ------------------------------------------------------------------
    const double mass_flow = thrust_magnitude / (isp * constants::G0);

    // ------------------------------------------------------------------
    // 6. Build thrust direction in body frame
    //
    //    Nominal thrust axis: +Z  →  (0, 0, 1)
    //
    //    Gimbal deflection is modelled as two sequential rotations:
    //      a) Rotate about body X-axis by gimbal_pitch
    //      b) Rotate about body Y-axis by gimbal_yaw
    //
    //    Rotation about X by α:
    //      [1,    0,      0   ]   [0]   [       0       ]
    //      [0,  cos α, -sin α] × [0] = [    -sin α     ]
    //      [0,  sin α,  cos α]   [1]   [     cos α     ]
    //
    //    Then rotate that result about Y by β:
    //      [ cos β, 0, sin β]   [ 0      ]   [ sin β · cos α ]
    //      [   0,   1,   0  ] × [-sin α  ] = [    -sin α     ]
    //      [-sin β, 0, cos β]   [ cos α  ]   [ cos β · cos α ]
    // ------------------------------------------------------------------
    const double sp = std::sin(gimbal_pitch);
    const double cp = std::cos(gimbal_pitch);
    const double sy = std::sin(gimbal_yaw);
    const double cy = std::cos(gimbal_yaw);

    Vec3 thrust_body;
    thrust_body.x =  sy * cp;       // lateral (yaw) component
    thrust_body.y = -sp;            // lateral (pitch) component
    thrust_body.z =  cy * cp;       // axial component

    // ------------------------------------------------------------------
    // 7. Rotate body-frame thrust direction into ECI frame
    //    The attitude quaternion represents body-to-ECI rotation.
    // ------------------------------------------------------------------
    const Vec3 thrust_eci_dir = attitude.rotate(thrust_body);

    // ------------------------------------------------------------------
    // 8. Assemble output
    // ------------------------------------------------------------------
    PropulsionState result;
    result.thrust_force   = thrust_eci_dir * thrust_magnitude;
    result.mass_flow_rate = mass_flow;
    result.stage_depleted = false;

    return result;
}

}  // namespace propulsion
}  // namespace prakshep
