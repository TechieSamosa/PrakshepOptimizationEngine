/**
 * @file wgs84_gravity.cpp
 * @brief Implementation of the WGS84 J2 Gravity Model.
 *
 * === J2 PERTURBATION PHYSICS ===
 *
 * The Earth is not a perfect sphere. Its equatorial radius is ~21 km larger
 * than its polar radius due to rotational flattening. This oblateness is
 * captured by the J2 coefficient of the spherical harmonic expansion of the
 * geopotential.
 *
 * For a vehicle at ECI position r = (x, y, z):
 *
 *   r_mag = |r|
 *   mu_r2 = GM / r_mag²
 *   z_r   = z / r_mag
 *   j2_f  = 1.5 * J2 * (R_eq / r_mag)²
 *
 *   a_x = -mu_r2 * (x/r_mag) * [1 - j2_f * (5·z_r² - 1)]
 *   a_y = -mu_r2 * (y/r_mag) * [1 - j2_f * (5·z_r² - 1)]
 *   a_z = -mu_r2 * (z/r_mag) * [1 - j2_f * (5·z_r² - 3)]
 *
 * Note the asymmetry in the z-component: (5·z_r² - 3) vs (5·z_r² - 1).
 * This is because J2 produces a stronger pull toward the equatorial plane
 * for objects above/below it.
 *
 * === CENTRIFUGAL CORRECTION ===
 *
 * For ECEF-relative computations (e.g., on the launch pad before liftoff),
 * we must subtract the centrifugal acceleration due to Earth's rotation:
 *
 *   a_centrifugal = -ω × (ω × r)
 *
 * This reduces the effective g at the equator by ~0.034 m/s² compared to
 * the poles.
 *
 * @author  Prakshep GNC Team
 * @version 3.0.0
 */

#include "prakshep/physics/wgs84_gravity.hpp"
#include "prakshep/constants.hpp"
#include <cmath>
#include <utility>

namespace prakshep {
namespace physics {

// ===========================================================================
//  J2-Perturbed Gravity
// ===========================================================================

Vec3 WGS84_Gravity::compute_gravity(const Vec3& position_eci) {
    double r_mag = position_eci.norm();

    // Guard against division by zero (vehicle at Earth's centre — should never happen)
    if (r_mag < 1.0) return Vec3(0.0, 0.0, 0.0);

    double x = position_eci.x;
    double y = position_eci.y;
    double z = position_eci.z;

    double r_mag_sq = r_mag * r_mag;
    double r_mag_inv = 1.0 / r_mag;

    // Central body acceleration magnitude: μ/r²
    double mu_r2 = constants::GM / r_mag_sq;

    // Normalised z-component
    double z_r = z * r_mag_inv;
    double z_r_sq = z_r * z_r;

    // J2 perturbation factor: 1.5 * J2 * (R_eq / r)²
    double r_ratio = A_EQUATORIAL * r_mag_inv;
    double j2_factor = 1.5 * constants::J2 * r_ratio * r_ratio;

    // Gravitational acceleration components (ECI frame)
    // The x,y components share the same J2 correction term (5·z_r² - 1)
    // The z component uses (5·z_r² - 3) — this asymmetry is the J2 signature
    double xy_correction = 1.0 - j2_factor * (5.0 * z_r_sq - 1.0);
    double z_correction  = 1.0 - j2_factor * (5.0 * z_r_sq - 3.0);

    return Vec3(
        -mu_r2 * (x * r_mag_inv) * xy_correction,
        -mu_r2 * (y * r_mag_inv) * xy_correction,
        -mu_r2 * (z * r_mag_inv) * z_correction
    );
}

Vec3 WGS84_Gravity::compute_effective_gravity(const Vec3& position_eci) {
    // Pure gravitational acceleration
    Vec3 g = compute_gravity(position_eci);

    // Centrifugal correction: a_cf = ω²·r_perp (directed outward from spin axis)
    // ω × (ω × r) for ω = (0, 0, ω_earth):
    //   = (ω²·x, ω²·y, 0)  [outward from spin axis]
    // Effective gravity subtracts this (we're in the rotating frame):
    double omega_sq = constants::EARTH_OMEGA * constants::EARTH_OMEGA;
    g.x -= omega_sq * position_eci.x;
    g.y -= omega_sq * position_eci.y;
    // z-component unaffected (along spin axis)

    return g;
}

// ===========================================================================
//  Geodetic ↔ ECI Conversions
// ===========================================================================

double WGS84_Gravity::radius_of_curvature(double latitude) {
    double sin_lat = std::sin(latitude);
    return A_EQUATORIAL / std::sqrt(1.0 - E_SQ * sin_lat * sin_lat);
}

Vec3 WGS84_Gravity::geodetic_to_eci(const GeodeticCoord& coord, double gmst) {
    double sin_lat = std::sin(coord.latitude);
    double cos_lat = std::cos(coord.latitude);

    // Radius of curvature in the prime vertical
    double N = radius_of_curvature(coord.latitude);

    // ECEF position
    double x_ecef = (N + coord.altitude) * cos_lat * std::cos(coord.longitude);
    double y_ecef = (N + coord.altitude) * cos_lat * std::sin(coord.longitude);
    double z_ecef = (N * (1.0 - E_SQ) + coord.altitude) * sin_lat;

    // Rotate ECEF → ECI by GMST angle (rotation about Z-axis)
    double cos_gmst = std::cos(gmst);
    double sin_gmst = std::sin(gmst);

    return Vec3(
        x_ecef * cos_gmst - y_ecef * sin_gmst,
        x_ecef * sin_gmst + y_ecef * cos_gmst,
        z_ecef
    );
}

GeodeticCoord WGS84_Gravity::eci_to_geodetic(const Vec3& position_eci, double gmst) {
    // Rotate ECI → ECEF
    double cos_gmst = std::cos(gmst);
    double sin_gmst = std::sin(gmst);

    double x_ecef =  position_eci.x * cos_gmst + position_eci.y * sin_gmst;
    double y_ecef = -position_eci.x * sin_gmst + position_eci.y * cos_gmst;
    double z_ecef =  position_eci.z;

    // Longitude is straightforward
    double longitude = std::atan2(y_ecef, x_ecef);

    // Iterative Bowring method for latitude and altitude
    double p = std::sqrt(x_ecef * x_ecef + y_ecef * y_ecef);
    double latitude = std::atan2(z_ecef, p * (1.0 - E_SQ)); // Initial guess

    for (int i = 0; i < 10; ++i) {
        double sin_lat = std::sin(latitude);
        double N = radius_of_curvature(latitude);
        double new_lat = std::atan2(z_ecef + E_SQ * N * sin_lat, p);

        if (std::abs(new_lat - latitude) < 1e-12) break;
        latitude = new_lat;
    }

    double sin_lat = std::sin(latitude);
    double cos_lat = std::cos(latitude);
    double N = radius_of_curvature(latitude);

    double altitude;
    if (std::abs(cos_lat) > 1e-10) {
        altitude = p / cos_lat - N;
    } else {
        altitude = std::abs(z_ecef) / std::abs(sin_lat) - N * (1.0 - E_SQ);
    }

    return {latitude, longitude, altitude};
}

// ===========================================================================
//  SLP Launch Pad Initialisation
// ===========================================================================

std::pair<Vec3, Vec3> WGS84_Gravity::compute_slp_initial_state(double gmst) {
    GeodeticCoord slp;
    slp.latitude  = SLP_LATITUDE_DEG  * constants::DEG_TO_RAD;
    slp.longitude = SLP_LONGITUDE_DEG * constants::DEG_TO_RAD;
    slp.altitude  = SLP_ALTITUDE_M;

    Vec3 pos = geodetic_to_eci(slp, gmst);

    // Velocity due to Earth's rotation: v = ω × r
    // ω = (0, 0, EARTH_OMEGA), so ω × r = (-ω·y, ω·x, 0)
    Vec3 vel(
        -constants::EARTH_OMEGA * pos.y,
         constants::EARTH_OMEGA * pos.x,
         0.0
    );

    return {pos, vel};
}

// ===========================================================================
//  Time Conversions
// ===========================================================================

double WGS84_Gravity::calendar_to_julian_date(int year, int month, int day,
                                               int hour, int minute, double second) {
    // Algorithm from Meeus, "Astronomical Algorithms" 2nd ed., Chapter 7
    if (month <= 2) {
        year -= 1;
        month += 12;
    }

    int A = year / 100;
    int B = 2 - A + A / 4;

    double JD = std::floor(365.25 * (year + 4716))
              + std::floor(30.6001 * (month + 1))
              + day + B - 1524.5;

    // Add fractional day
    JD += (hour + minute / 60.0 + second / 3600.0) / 24.0;

    return JD;
}

double WGS84_Gravity::julian_date_to_gmst(double jd) {
    // GMST at 0h UT1: see Meeus Ch. 12 / IERS conventions
    double T = (jd - 2451545.0) / 36525.0; // Julian centuries from J2000.0

    // GMST in seconds of time at 0h UT1
    double gmst_sec = 67310.54841
                    + (876600.0 * 3600.0 + 8640184.812866) * T
                    + 0.093104 * T * T
                    - 6.2e-6 * T * T * T;

    // Convert to radians (1 revolution = 86400 seconds of sidereal time)
    double gmst_rad = std::fmod(gmst_sec, 86400.0) / 86400.0 * 2.0 * M_PI;

    // Normalise to [0, 2π)
    if (gmst_rad < 0.0) gmst_rad += 2.0 * M_PI;

    return gmst_rad;
}

} // namespace physics
} // namespace prakshep
