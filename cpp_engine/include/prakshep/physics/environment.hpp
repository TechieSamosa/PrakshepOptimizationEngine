/**
 * @file environment.hpp
 * @brief High-Altitude Atmospheric Integrator.
 *
 * This module extends standard atmospheric models (like the US Standard 1976)
 * to smoothly handle the transition into the free-molecular flow regime across
 * the Kármán line (~100km).
 *
 * It provides a blended atmospheric density function that effectively zeroes out
 * aerodynamic forces at high altitude, handing full authority to rigid-body
 * orbital mechanics.
 *
 * @author  Prakshep GNC Team
 * @version 2.0.0
 */

#pragma once

#include "prakshep/types.hpp"

namespace prakshep {
namespace physics {

class Environment {
public:
    /// The Kármán line, conventionally 100km
    static constexpr double KARMAN_LINE_M = 100000.0;
    
    /// The blending zone width (m) over which density tapers to exactly zero.
    /// E.g. from 90km to 110km.
    static constexpr double BLEND_WIDTH_M = 20000.0;

    /**
     * @brief Compute the blended atmospheric density.
     *
     * @param altitude_m     Current altitude above sea level (m)
     * @param base_density   Density calculated by the standard atmospheric model (kg/m^3)
     * @return Blended density that smoothly approaches zero (kg/m^3)
     */
    static double compute_blended_density(double altitude_m, double base_density);
};

} // namespace physics
} // namespace prakshep
