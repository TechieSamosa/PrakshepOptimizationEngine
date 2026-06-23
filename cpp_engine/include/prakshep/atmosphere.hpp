#pragma once

/**
 * @file atmosphere.hpp
 * @brief US Standard Atmosphere 1976 model interface.
 *
 * Provides atmospheric state (temperature, pressure, density, speed of sound)
 * as a function of geometric altitude above mean sea level.  Valid from sea
 * level to ~250 km; above ~85 km the model transitions to an exponential
 * density decay.
 *
 * @see atmosphere.cpp for the full 7-layer implementation.
 *
 * @author  Prakshep GNC Team
 * @version 1.0.0
 */

#include "types.hpp"

namespace prakshep {
namespace atmosphere {

/**
 * @brief Compute atmospheric state at a given geometric altitude.
 *
 * Implements the US Standard Atmosphere 1976 with 7 discrete layers.
 * Above the model ceiling (~84.852 km geopotential) an exponential
 * density decay is used with a 6500 m scale height.
 *
 * @param geometric_altitude_m  Altitude above mean sea level (m).
 *                              Negative values are clamped to 0.
 * @return AtmosphereState containing T (K), P (Pa), ρ (kg/m³), a (m/s).
 */
AtmosphereState compute(double geometric_altitude_m);

/**
 * @brief Convert geometric altitude to geopotential altitude.
 *
 * Uses the relation:
 *   H = R_geo · z / (R_geo + z)
 *
 * where R_geo is the effective Earth radius for geopotential conversion.
 *
 * @param z  Geometric altitude above MSL (m).
 * @return   Geopotential altitude (m).
 */
double geometric_to_geopotential(double z);

} // namespace atmosphere
} // namespace prakshep
