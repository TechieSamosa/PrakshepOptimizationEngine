/**
 * @file wgs84_gravity.hpp
 * @brief WGS84 J2 Gravity Model with geodetic coordinate support.
 *
 * This replaces the simplified gravity model from the early prototyping phase.
 * The RK4 flight computer must now query this class at every integration tick
 * to compute the gravitational acceleration vector.
 *
 * === WGS84 GRAVITY MODEL ===
 *
 * The model computes gravitational acceleration as a function of the vehicle's
 * ECI position, accounting for:
 *
 *   1. Point-mass gravity (central body term: -μ/r²)
 *   2. J2 oblateness perturbation (equatorial bulge of the geoid)
 *   3. Centrifugal force correction (for ECEF-relative computations)
 *
 * The J2 term is the dominant perturbation for LEO dynamics and causes:
 *   - Apsidal precession (rotation of the line of apsides)
 *   - Nodal regression (precession of the orbital plane)
 *   - Variation of gravitational acceleration with latitude
 *
 * === GEODETIC ↔ ECI CONVERSION ===
 *
 * The model provides bidirectional conversion between:
 *   - Geodetic coordinates (latitude φ, longitude λ, altitude h)
 *   - Earth-Centred Inertial (ECI) Cartesian coordinates (x, y, z)
 *
 * The geodetic-to-ECI conversion uses the WGS84 reference ellipsoid:
 *   - Semi-major axis (equatorial radius): a = 6,378,137.0 m
 *   - Flattening: f = 1/298.257223563
 *   - Eccentricity²: e² = 2f - f²
 *
 * === LAUNCH SITE: SDSC-SHAR Second Launch Pad (SLP) ===
 *
 * The Second Launch Pad at Satish Dhawan Space Centre is the primary
 * PSLV and LVM3 launch facility. Its geodetic coordinates are hardcoded
 * as the default initialisation state for the ECI conversion.
 *
 *   Latitude:  13.7199° N
 *   Longitude: 80.2304° E
 *   Altitude:  ~5 m ASL
 *
 * @author  Prakshep GNC Team
 * @version 3.0.0
 */

#pragma once

#include "prakshep/types.hpp"

namespace prakshep {
namespace physics {

// ===========================================================================
//  Geodetic coordinate structure
// ===========================================================================

struct GeodeticCoord {
    double latitude;   ///< Geodetic latitude (radians)
    double longitude;  ///< Geodetic longitude (radians)
    double altitude;   ///< Altitude above WGS84 ellipsoid (metres)
};

// ===========================================================================
//  WGS84 Gravity Model
// ===========================================================================

class WGS84_Gravity {
public:
    // =======================================================================
    //  WGS84 Ellipsoid Constants
    // =======================================================================

    /// WGS84 semi-major axis (equatorial radius, m)
    static constexpr double A_EQUATORIAL = 6378137.0;

    /// WGS84 flattening
    static constexpr double F = 1.0 / 298.257223563;

    /// WGS84 semi-minor axis (polar radius, m)
    static constexpr double B_POLAR = A_EQUATORIAL * (1.0 - F);

    /// First eccentricity squared: e² = 2f - f²
    static constexpr double E_SQ = 2.0 * F - F * F;

    /// Second eccentricity squared: e'² = (a²-b²)/b²
    static constexpr double EP_SQ = (A_EQUATORIAL * A_EQUATORIAL - B_POLAR * B_POLAR)
                                   / (B_POLAR * B_POLAR);

    // =======================================================================
    //  SDSC-SHAR Second Launch Pad (SLP) Coordinates
    // =======================================================================

    /// SLP geodetic latitude (degrees)
    static constexpr double SLP_LATITUDE_DEG  = 13.7199;

    /// SLP geodetic longitude (degrees)
    static constexpr double SLP_LONGITUDE_DEG = 80.2304;

    /// SLP altitude above WGS84 ellipsoid (metres)
    static constexpr double SLP_ALTITUDE_M    = 5.0;

    // =======================================================================
    //  Core API
    // =======================================================================

    /**
     * @brief Compute J2-perturbed gravitational acceleration in ECI frame.
     *
     * This is the primary function called by the RK4 integrator at every tick.
     *
     * @param position_eci  Vehicle position in ECI frame (m from Earth centre).
     * @return Gravitational acceleration vector in ECI (m/s²).
     */
    static Vec3 compute_gravity(const Vec3& position_eci);

    /**
     * @brief Compute gravitational acceleration with centrifugal correction.
     *
     * Returns the effective gravity (gravitational + centrifugal) for use
     * in body-frame or ECEF-relative computations.
     *
     * @param position_eci  Vehicle position in ECI frame (m).
     * @return Effective gravitational acceleration in ECI (m/s²).
     */
    static Vec3 compute_effective_gravity(const Vec3& position_eci);

    // =======================================================================
    //  Coordinate Conversions
    // =======================================================================

    /**
     * @brief Convert geodetic coordinates to ECI position.
     *
     * Uses the WGS84 ellipsoid to compute the ECI position at a given
     * geodetic latitude, longitude, altitude, and Greenwich Mean Sidereal
     * Time (GMST) angle.
     *
     * @param coord  Geodetic coordinates (lat/lon in radians, alt in metres).
     * @param gmst   Greenwich Mean Sidereal Time angle (radians).
     * @return ECI position vector (m from Earth centre).
     */
    static Vec3 geodetic_to_eci(const GeodeticCoord& coord, double gmst);

    /**
     * @brief Convert ECI position to geodetic coordinates.
     *
     * Uses Bowring's iterative method for high-precision inversion of the
     * WGS84 ellipsoid mapping.
     *
     * @param position_eci  Position in ECI frame (m).
     * @param gmst          Greenwich Mean Sidereal Time angle (radians).
     * @return Geodetic coordinates (lat/lon radians, alt metres).
     */
    static GeodeticCoord eci_to_geodetic(const Vec3& position_eci, double gmst);

    /**
     * @brief Compute the radius of curvature in the prime vertical (N).
     *
     * N(φ) = a / sqrt(1 - e²·sin²(φ))
     *
     * @param latitude  Geodetic latitude (radians).
     * @return Radius of curvature N (metres).
     */
    static double radius_of_curvature(double latitude);

    /**
     * @brief Compute the initial ECI state for SLP at a given GMST.
     *
     * Returns position and velocity of the launch pad in ECI,
     * including rotational velocity from Earth's spin.
     *
     * @param gmst  Greenwich Mean Sidereal Time angle (radians).
     * @return std::pair<Vec3, Vec3>: (position_eci, velocity_eci).
     */
    static std::pair<Vec3, Vec3> compute_slp_initial_state(double gmst);

    /**
     * @brief Compute GMST from Julian Date.
     *
     * @param jd  Julian Date (days).
     * @return GMST angle (radians, 0 to 2π).
     */
    static double julian_date_to_gmst(double jd);

    /**
     * @brief Convert calendar date/time to Julian Date.
     *
     * @param year   Calendar year.
     * @param month  Month (1-12).
     * @param day    Day of month.
     * @param hour   Hour (0-23).
     * @param minute Minute (0-59).
     * @param second Second (0-59, can be fractional).
     * @return Julian Date (days).
     */
    static double calendar_to_julian_date(int year, int month, int day,
                                          int hour, int minute, double second);
};

} // namespace physics
} // namespace prakshep
