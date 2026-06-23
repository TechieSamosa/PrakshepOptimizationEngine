/**
 * @file gravity.cpp
 * @brief Implementation of the J2 oblate-Earth gravitational acceleration model.
 *
 * == Physics Background ==
 *
 * The Earth is not a perfect sphere — it bulges at the equator due to its
 * rotation.  The gravitational potential of an oblate body can be expanded
 * in spherical harmonics.  Retaining only the dominant zonal harmonic J2:
 *
 *   U(r, φ) = −μ/r · [ 1 − J2·(R_E/r)²·P2(sin φ) ]
 *
 * where:
 *   μ     = GM = 3.986004418×10¹⁴ m³/s²
 *   r     = geocentric distance
 *   φ     = geocentric latitude
 *   R_E   = equatorial radius (6378137 m, WGS84)
 *   J2    = 1.08263×10⁻³
 *   P2(x) = (3x² − 1)/2   (second Legendre polynomial)
 *
 * Taking the gradient of U in Cartesian (ECI) coordinates gives the
 * acceleration components directly, without needing to convert to
 * spherical coordinates:
 *
 *   Given r = (x, y, z) in ECI, r_mag = |r|, z_r = z / r_mag:
 *
 *   j2_factor = 1.5 · J2 · (R_E / r_mag)²
 *
 *   a_x = −(μ/r²)·(x/r)·[ 1 − j2_factor·(5·z_r² − 1) ]
 *   a_y = −(μ/r²)·(y/r)·[ 1 − j2_factor·(5·z_r² − 1) ]
 *   a_z = −(μ/r²)·(z/r)·[ 1 − j2_factor·(5·z_r² − 3) ]
 *
 * Note the asymmetry in the z-component: the (5z_r² − 3) vs (5z_r² − 1)
 * arises because the J2 perturbation preferentially pulls mass toward the
 * equatorial plane.
 *
 * == Effect on trajectories ==
 *
 * For a launch trajectory lasting ~20 minutes the J2 effect is small but
 * measurable — typically a few m/s of Δv.  It matters more for orbit
 * propagation (nodal regression, apsidal precession) but we include it
 * here for fidelity, especially for the final orbit-insertion burn.
 *
 * @author  Prakshep GNC Team
 * @version 1.0.0
 */

#include "prakshep/gravity.hpp"
#include "prakshep/constants.hpp"

#include <cmath>

namespace prakshep {
namespace gravity {

Vec3 compute(const Vec3& position_eci) {
    // ---------------------------------------------------------------
    // Step 1: Compute the geocentric distance |r|.
    //
    // For numerical safety we check that r_mag > 0 (position at
    // Earth's centre would be unphysical but let's not divide by
    // zero if the integrator produces a degenerate state).
    // ---------------------------------------------------------------
    const double r_sq  = position_eci.norm_squared();
    const double r_mag = std::sqrt(r_sq);

    if (r_mag < 1.0) {
        // Degenerate — return zero to avoid division by zero
        return {0.0, 0.0, 0.0};
    }

    // ---------------------------------------------------------------
    // Step 2: Precompute common factors.
    //
    //   mu_r2 = μ / r²     — base gravitational acceleration magnitude
    //   z_r   = z / r       — sine of geocentric latitude
    //   j2_factor = 1.5 · J2 · (R_E / r)²  — J2 perturbation scaling
    // ---------------------------------------------------------------
    const double mu_r2 = constants::GM / r_sq;
    const double z_r   = position_eci.z / r_mag;

    const double re_over_r = constants::R_EARTH / r_mag;
    const double j2_factor = 1.5 * constants::J2 * re_over_r * re_over_r;

    // ---------------------------------------------------------------
    // Step 3: Compute the x, y, z acceleration components.
    //
    // The expressions differ in the z-component because the partial
    // derivative of P2(sin φ) with respect to z introduces an extra
    // term:
    //
    //   ∂P2/∂x  and  ∂P2/∂y  contribute  (5z_r² − 1)
    //   ∂P2/∂z                contributes  (5z_r² − 3)
    //
    // This is the key mathematical signature of the J2 perturbation.
    // ---------------------------------------------------------------
    const double z_r_sq = z_r * z_r;

    //  Common multiplier for x and y components
    const double xy_factor = 1.0 - j2_factor * (5.0 * z_r_sq - 1.0);

    //  Z-component has a different factor
    const double z_factor  = 1.0 - j2_factor * (5.0 * z_r_sq - 3.0);

    // ---------------------------------------------------------------
    // Step 4: Assemble the acceleration vector.
    //
    // The negative sign gives attraction toward Earth's centre.
    // The (x/r), (y/r), (z/r) terms provide the unit vector direction.
    // ---------------------------------------------------------------
    const double a_x = -mu_r2 * (position_eci.x / r_mag) * xy_factor;
    const double a_y = -mu_r2 * (position_eci.y / r_mag) * xy_factor;
    const double a_z = -mu_r2 * (position_eci.z / r_mag) * z_factor;

    return {a_x, a_y, a_z};
}

} // namespace gravity
} // namespace prakshep
