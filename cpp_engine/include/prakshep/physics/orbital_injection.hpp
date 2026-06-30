/**
 * @file orbital_injection.hpp
 * @brief MECO-2 and Orbital Target Verification Logic.
 *
 * This module is evaluated continuously by the RK4 flight computer during
 * the PS4 burn. It converts the Cartesian ECI state vector (Position + Velocity)
 * into classical Keplerian orbital elements to check if the target apogee
 * and perigee have been achieved.
 *
 * @author  Prakshep GNC Team
 * @version 2.0.0
 */

#pragma once

#include "prakshep/types.hpp"

namespace prakshep {
namespace physics {

struct OrbitalElements {
    double semi_major_axis;   // a (meters)
    double eccentricity;      // e
    double inclination;       // i (radians)
    double apogee_altitude;   // meters above Earth surface
    double perigee_altitude;  // meters above Earth surface
};

class OrbitalInjection {
public:
    /**
     * @brief Compute the instantaneous Keplerian orbital elements from an ECI state vector.
     *
     * @param position_eci Current position vector in ECI frame (m)
     * @param velocity_eci Current velocity vector in ECI frame (m/s)
     * @return OrbitalElements containing apogee, perigee, etc.
     */
    static OrbitalElements compute_elements(const Vec3& position_eci, const Vec3& velocity_eci);

    /**
     * @brief Check if the current orbit meets the target criteria.
     *
     * @param current         The instantaneous orbital elements
     * @param target_apogee   Desired apogee altitude (m)
     * @param target_perigee  Desired perigee altitude (m)
     * @param tolerance       Acceptable error margin in altitude (m)
     * @return true if both apogee and perigee are within tolerance of targets.
     */
    static bool check_orbital_target(
        const OrbitalElements& current,
        double target_apogee,
        double target_perigee,
        double tolerance = 5000.0 // 5 km default tolerance
    );
};

} // namespace physics
} // namespace prakshep
