/**
 * @file environment.cpp
 * @brief Implementation of the High-Altitude Atmospheric Integrator.
 *
 * @author  Prakshep GNC Team
 * @version 2.0.0
 */

#include "prakshep/physics/environment.hpp"
#include <cmath>
#include <algorithm>

namespace prakshep {
namespace physics {

double Environment::compute_blended_density(double altitude_m, double base_density) {
    // -----------------------------------------------------------------------
    // The continuum mechanics aerodynamic drag model (using the standard US 1976
    // atmosphere) becomes invalid in the free-molecular flow regime at extreme
    // altitudes. To prevent RK4 instability from near-zero, fluctuating density
    // values, we smoothly taper the density to exactly 0.0 using a sigmoid-like
    // blending function across the Kármán line.
    // -----------------------------------------------------------------------

    double transition_start = KARMAN_LINE_M - (BLEND_WIDTH_M / 2.0);
    double transition_end   = KARMAN_LINE_M + (BLEND_WIDTH_M / 2.0);

    // Below the transition zone, use the base atmospheric model strictly.
    if (altitude_m <= transition_start) {
        return base_density;
    }

    // Above the transition zone, hard-clamp to 0.0 (absolute vacuum for drag purposes).
    // The vehicle is now purely governed by orbital mechanics (gravity).
    if (altitude_m >= transition_end) {
        return 0.0;
    }

    // Inside the transition zone (e.g. 90km - 110km):
    // Use a cosine interpolation (smoothstep) for a continuous C1 derivative.
    double fraction = (altitude_m - transition_start) / BLEND_WIDTH_M;
    double blend_factor = 0.5 * (1.0 + std::cos(fraction * M_PI));

    return base_density * blend_factor;
}

} // namespace physics
} // namespace prakshep
