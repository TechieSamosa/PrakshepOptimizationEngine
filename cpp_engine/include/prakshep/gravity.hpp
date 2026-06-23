#pragma once

/**
 * @file gravity.hpp
 * @brief J2-perturbed gravitational acceleration model interface.
 *
 * Provides the gravitational acceleration vector at any ECI position,
 * accounting for the dominant oblateness (J2) term of the Earth's
 * geopotential.
 *
 * @see gravity.cpp for the full implementation and physics derivation.
 *
 * @author  Prakshep GNC Team
 * @version 1.0.0
 */

#include "types.hpp"

namespace prakshep {
namespace gravity {

/**
 * @brief Compute gravitational acceleration at a given ECI position.
 *
 * Uses a point-mass + J2 oblate-Earth gravity model.  The J2 term
 * accounts for the equatorial bulge and is the single most important
 * perturbation for low-Earth orbit dynamics.
 *
 * @param position_eci  Position vector in the ECI frame (m from Earth centre).
 * @return Gravitational acceleration vector in ECI (m/s²).
 */
Vec3 compute(const Vec3& position_eci);

} // namespace gravity
} // namespace prakshep
