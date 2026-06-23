#pragma once

/**
 * @file constants.hpp
 * @brief Physical constants used throughout the Prakshep rocket simulation engine.
 *
 * All values are stored as `constexpr double` for compile-time evaluation and
 * zero-overhead access.  Units follow the SI system unless otherwise noted.
 *
 * Reference sources:
 *   - WGS84 ellipsoid parameters (NIMA TR8350.2, 2000)
 *   - US Standard Atmosphere 1976 (NOAA/NASA/USAF)
 *   - ISRO public mission documents for Sriharikota SDSC-SHAR
 *
 * @author  Prakshep GNC Team
 * @version 1.0.0
 */

#include <cmath>

namespace prakshep {
namespace constants {

// ============================================================================
//  Earth — gravitational & geometric parameters (WGS84)
// ============================================================================

/** @brief Standard gravitational parameter μ = G·M_Earth (m³/s²). */
constexpr double GM = 3.986004418e14;

/** @brief WGS84 equatorial radius (m). */
constexpr double R_EARTH = 6378137.0;

/** @brief WGS84 polar radius (m). */
constexpr double R_POLAR = 6356752.3142;

/**
 * @brief Second zonal harmonic of the geopotential (J2).
 *
 * J2 captures the dominant oblateness of the Earth and is the leading
 * perturbation term for LEO orbit propagation.  Value from EGM96.
 */
constexpr double J2 = 1.08263e-3;

/** @brief Earth's sidereal rotation rate (rad/s). */
constexpr double EARTH_OMEGA = 7.2921150e-5;

/** @brief WGS84 flattening factor f = (a − b) / a. */
constexpr double FLATTENING = 1.0 / 298.257223563;

/** @brief Standard gravitational acceleration at sea level (m/s²). */
constexpr double G0 = 9.80665;

// ============================================================================
//  Atmosphere — US Standard Atmosphere 1976 reference values
// ============================================================================

/** @brief Standard sea-level atmospheric pressure (Pa). */
constexpr double P0 = 101325.0;

/** @brief Standard sea-level air density (kg/m³). */
constexpr double RHO0 = 1.225;

/** @brief Standard sea-level temperature (K). */
constexpr double T0 = 288.15;

/** @brief Specific gas constant for dry air (J/(kg·K)). */
constexpr double R_AIR = 287.05287;

/** @brief Ratio of specific heats for air (cp/cv). */
constexpr double GAMMA_AIR = 1.4;

/**
 * @brief Upper geopotential altitude limit of the standard atmosphere model (m).
 *
 * Above this altitude the model transitions to a simple exponential decay.
 */
constexpr double ATMOSPHERE_UPPER_LIMIT = 84852.0;

/**
 * @brief Effective Earth radius for geopotential altitude conversion (m).
 *
 * Used in the formula H = R·z / (R + z) to convert geometric altitude z
 * to geopotential altitude H.  This value differs slightly from the WGS84
 * equatorial radius because it is tuned for the gravity-altitude model.
 */
constexpr double R_EARTH_GEOPOTENTIAL = 6356766.0;

// ============================================================================
//  Sriharikota SDSC-SHAR — launch site parameters
// ============================================================================

/** @brief Geodetic latitude of Sriharikota first launch pad (degrees). */
constexpr double LAUNCH_LATITUDE = 13.7340;

/** @brief Geodetic longitude of Sriharikota first launch pad (degrees). */
constexpr double LAUNCH_LONGITUDE = 80.2350;

/** @brief Launch pad altitude above mean sea level (m). */
constexpr double LAUNCH_ALTITUDE = 5.0;

/**
 * @brief Launch azimuth for sun-synchronous orbit missions (degrees).
 *
 * Measured clockwise from true north.  SSO launches from Sriharikota
 * typically fly south-southeast to reach retrograde polar orbits.
 */
constexpr double LAUNCH_AZIMUTH_POLAR = 140.0;

/**
 * @brief Launch azimuth for geostationary transfer orbit missions (degrees).
 *
 * Eastward launch to exploit Earth's rotational velocity.
 */
constexpr double LAUNCH_AZIMUTH_GTO = 102.0;

// ============================================================================
//  Unit conversions
// ============================================================================

/** @brief Multiply by this to convert degrees → radians. */
constexpr double DEG_TO_RAD = M_PI / 180.0;

/** @brief Multiply by this to convert radians → degrees. */
constexpr double RAD_TO_DEG = 180.0 / M_PI;

} // namespace constants
} // namespace prakshep
