/**
 * @file orbital_injection.cpp
 * @brief Implementation of MECO-2 and Orbital Target Verification Logic.
 *
 * @author  Prakshep GNC Team
 * @version 2.0.0
 */

#include "prakshep/physics/orbital_injection.hpp"
#include "prakshep/constants.hpp"
#include <cmath>
#include <algorithm>

namespace prakshep {
namespace physics {

OrbitalElements OrbitalInjection::compute_elements(const Vec3& position_eci, const Vec3& velocity_eci) {
    OrbitalElements elements = {0};

    // Magnitudes
    double r = position_eci.norm();
    double v = velocity_eci.norm();

    if (r == 0.0) return elements; // Prevent div by zero at origin

    // 1. Specific Orbital Energy (epsilon) = v^2/2 - GM/r
    double epsilon = (v * v) / 2.0 - (constants::GM / r);

    // 2. Semi-major axis (a) = -GM / (2 * epsilon)
    // If epsilon > 0, orbit is hyperbolic (escape). We clamp/handle as needed.
    if (epsilon >= 0.0) {
        // Escape trajectory
        elements.semi_major_axis = INFINITY;
        elements.eccentricity = 1.0;
        elements.apogee_altitude = INFINITY;
        elements.perigee_altitude = r - constants::R_EARTH; // Perigee is at least current altitude
        return elements;
    }

    elements.semi_major_axis = -constants::GM / (2.0 * epsilon);

    // 3. Specific Angular Momentum (h) = r x v
    Vec3 h_vec = position_eci.cross(velocity_eci);
    double h = h_vec.norm();

    // 4. Eccentricity vector (e) = ( (v^2 - GM/r)*r - (r.v)*v ) / GM
    // Alternatively, using specific energy and momentum: e = sqrt(1 + (2 * epsilon * h^2) / GM^2)
    double e_sq = 1.0 + (2.0 * epsilon * h * h) / (constants::GM * constants::GM);
    elements.eccentricity = std::sqrt(std::max(0.0, e_sq));

    // 5. Inclination (i) = acos(h_z / h)
    elements.inclination = std::acos(h_vec.z / h);

    // 6. Apogee (r_a) and Perigee (r_p) radius
    double r_a = elements.semi_major_axis * (1.0 + elements.eccentricity);
    double r_p = elements.semi_major_axis * (1.0 - elements.eccentricity);

    // 7. Convert to altitude above Earth's surface
    // Note: A highly accurate model would use the oblate Earth radius at the
    // argument of periapsis. For this trigger logic, we use standard equatorial radius.
    elements.apogee_altitude = r_a - constants::R_EARTH;
    elements.perigee_altitude = r_p - constants::R_EARTH;

    return elements;
}

bool OrbitalInjection::check_orbital_target(
    const OrbitalElements& current,
    double target_apogee,
    double target_perigee,
    double tolerance
) {
    // If we're on an escape trajectory, we missed LEO target
    if (std::isinf(current.semi_major_axis)) {
        return false;
    }

    double apo_error = std::abs(current.apogee_altitude - target_apogee);
    double peri_error = std::abs(current.perigee_altitude - target_perigee);

    return (apo_error <= tolerance) && (peri_error <= tolerance);
}

} // namespace physics
} // namespace prakshep
