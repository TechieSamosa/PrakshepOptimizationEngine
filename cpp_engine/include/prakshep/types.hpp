#pragma once

/**
 * @file types.hpp
 * @brief Core mathematical types and data structures for the Prakshep rocket simulation engine.
 *
 * This header defines the fundamental building blocks used throughout the simulation:
 *   - Vec3:           3D vector with full operator overloading and geometric methods
 *   - Quaternion:     Unit quaternion for attitude representation (Hamilton convention)
 *   - Enumerations:   RocketType, FailureType, SimEvent
 *   - StageConfig:    Per-stage propulsion and structural parameters
 *   - RocketConfig:   Complete vehicle configuration (multi-stage + aerodynamics)
 *   - StateVector:    Full 6-DOF state of the vehicle at a point in time
 *   - AtmosphereState:Local atmospheric conditions
 *   - TelemetryFrame: Downlink-style telemetry snapshot for visualization/logging
 *
 * Design notes:
 *   - All types live in namespace prakshep.
 *   - Vec3 and Quaternion are fully inline (header-only) for performance.
 *   - SI units throughout (metres, seconds, kilograms, radians, Newtons, Pascals).
 *   - No external dependencies beyond the C++17 standard library.
 *
 * @author  Prakshep GNC Team
 * @version 1.0.0
 */

#include <cmath>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace prakshep {

// ============================================================================
//  Vec3 — 3-component Cartesian vector
// ============================================================================

/**
 * @struct Vec3
 * @brief  A 3-component Cartesian vector with common linear-algebra operations.
 *
 * Used for positions (ECI metres), velocities (m/s), accelerations (m/s²),
 * angular velocities (rad/s), forces (N), etc.  All arithmetic operators are
 * element-wise except where noted (dot, cross).
 */
struct Vec3 {
    double x = 0.0; ///< X component
    double y = 0.0; ///< Y component
    double z = 0.0; ///< Z component

    // -- Constructors --------------------------------------------------------

    /** @brief Default constructor — zero vector. */
    Vec3() = default;

    /** @brief Construct from three scalar components. */
    Vec3(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}

    // -- Arithmetic operators ------------------------------------------------

    /** @brief Element-wise addition. */
    Vec3 operator+(const Vec3& rhs) const {
        return {x + rhs.x, y + rhs.y, z + rhs.z};
    }

    /** @brief Element-wise subtraction. */
    Vec3 operator-(const Vec3& rhs) const {
        return {x - rhs.x, y - rhs.y, z - rhs.z};
    }

    /** @brief Scalar multiplication (vector * scalar). */
    Vec3 operator*(double s) const {
        return {x * s, y * s, z * s};
    }

    /** @brief Scalar division. */
    Vec3 operator/(double s) const {
        double inv = 1.0 / s;
        return {x * inv, y * inv, z * inv};
    }

    /** @brief In-place element-wise addition. */
    Vec3& operator+=(const Vec3& rhs) {
        x += rhs.x;
        y += rhs.y;
        z += rhs.z;
        return *this;
    }

    /** @brief In-place element-wise subtraction. */
    Vec3& operator-=(const Vec3& rhs) {
        x -= rhs.x;
        y -= rhs.y;
        z -= rhs.z;
        return *this;
    }

    /** @brief In-place scalar multiplication. */
    Vec3& operator*=(double s) {
        x *= s;
        y *= s;
        z *= s;
        return *this;
    }

    /** @brief Unary negation. */
    Vec3 operator-() const {
        return {-x, -y, -z};
    }

    // -- Geometric methods ---------------------------------------------------

    /** @brief Dot (inner) product: a · b = ax*bx + ay*by + az*bz. */
    double dot(const Vec3& rhs) const {
        return x * rhs.x + y * rhs.y + z * rhs.z;
    }

    /**
     * @brief Cross (vector) product: a × b.
     *
     * Returns a vector perpendicular to both operands following the
     * right-hand rule.
     */
    Vec3 cross(const Vec3& rhs) const {
        return {
            y * rhs.z - z * rhs.y,
            z * rhs.x - x * rhs.z,
            x * rhs.y - y * rhs.x
        };
    }

    /** @brief Squared Euclidean norm (avoids sqrt for comparisons). */
    double norm_squared() const {
        return x * x + y * y + z * z;
    }

    /** @brief Euclidean (L2) norm. */
    double norm() const {
        return std::sqrt(norm_squared());
    }

    /**
     * @brief Return the unit vector in the same direction.
     * @warning If the vector is zero-length this returns (0,0,0).
     */
    Vec3 normalized() const {
        double n = norm();
        if (n < 1e-15) return {0.0, 0.0, 0.0};
        return *this / n;
    }

    // -- Friend free functions -----------------------------------------------

    /** @brief Scalar * vector (allows `2.0 * v` syntax). */
    friend Vec3 operator*(double s, const Vec3& v) {
        return {s * v.x, s * v.y, s * v.z};
    }

    /** @brief Stream insertion for debugging / telemetry logging. */
    friend std::ostream& operator<<(std::ostream& os, const Vec3& v) {
        os << "(" << v.x << ", " << v.y << ", " << v.z << ")";
        return os;
    }
};

// ============================================================================
//  Quaternion — Hamilton-convention unit quaternion for attitude
// ============================================================================

/**
 * @struct Quaternion
 * @brief  Unit quaternion representing a 3D rotation (Hamilton convention).
 *
 * Storage order is (w, x, y, z) where w is the scalar part.
 * Multiplication follows the Hamilton product rule:
 *   q1 * q2  !=  q2 * q1  (non-commutative)
 *
 * The quaternion maps body-frame vectors into the ECI frame via:
 *   v_eci = q * v_body * q_conj
 *
 * Convention: ZYX intrinsic Euler angles (yaw-pitch-roll, aerospace standard).
 */
struct Quaternion {
    double w = 1.0; ///< Scalar (real) part
    double x = 0.0; ///< i component
    double y = 0.0; ///< j component
    double z = 0.0; ///< k component

    // -- Constructors --------------------------------------------------------

    /** @brief Default constructor — identity rotation (no rotation). */
    Quaternion() = default;

    /** @brief Construct from four scalar components. */
    Quaternion(double w_, double x_, double y_, double z_)
        : w(w_), x(x_), y(y_), z(z_) {}

    // -- Operators -----------------------------------------------------------

    /**
     * @brief Hamilton product (quaternion multiplication).
     *
     * Given q1 = (w1, v1) and q2 = (w2, v2):
     *   q1*q2 = (w1*w2 - v1·v2,  w1*v2 + w2*v1 + v1×v2)
     *
     * Expanded:
     *   w = w1*w2 - x1*x2 - y1*y2 - z1*z2
     *   x = w1*x2 + x1*w2 + y1*z2 - z1*y2
     *   y = w1*y2 - x1*z2 + y1*w2 + z1*x2
     *   z = w1*z2 + x1*y2 - y1*x2 + z1*w2
     */
    Quaternion operator*(const Quaternion& rhs) const {
        return {
            w * rhs.w - x * rhs.x - y * rhs.y - z * rhs.z,
            w * rhs.x + x * rhs.w + y * rhs.z - z * rhs.y,
            w * rhs.y - x * rhs.z + y * rhs.w + z * rhs.x,
            w * rhs.z + x * rhs.y - y * rhs.x + z * rhs.w
        };
    }

    // -- Methods -------------------------------------------------------------

    /**
     * @brief Quaternion conjugate: q* = (w, -x, -y, -z).
     *
     * For unit quaternions, the conjugate equals the inverse.
     */
    Quaternion conjugate() const {
        return {w, -x, -y, -z};
    }

    /** @brief Quaternion norm (should be ~1.0 for unit quaternions). */
    double norm() const {
        return std::sqrt(w * w + x * x + y * y + z * z);
    }

    /** @brief Return a normalized (unit) copy of this quaternion. */
    Quaternion normalized() const {
        double n = norm();
        if (n < 1e-15) return identity();
        double inv = 1.0 / n;
        return {w * inv, x * inv, y * inv, z * inv};
    }

    /**
     * @brief Rotate a vector by this quaternion.
     *
     * Computes v' = q * v_pure * q_conj  where v_pure = (0, vx, vy, vz).
     *
     * Optimized form avoiding full quaternion multiplications:
     *   t = 2 * (q_xyz × v)
     *   v' = v + w*t + q_xyz × t
     */
    Vec3 rotate(const Vec3& v) const {
        // Rodrigues-like optimized rotation via quaternion
        Vec3 q_vec(x, y, z);
        Vec3 t = 2.0 * q_vec.cross(v);
        return v + w * t + q_vec.cross(t);
    }

    /**
     * @brief Convert Euler angles (roll, pitch, yaw) to quaternion.
     *
     * Uses the ZYX intrinsic convention (aerospace standard):
     *   1. Yaw   (ψ) about Z
     *   2. Pitch (θ) about Y'
     *   3. Roll  (φ) about X''
     *
     * @param roll  Roll angle φ in radians (rotation about X-body axis)
     * @param pitch Pitch angle θ in radians (rotation about Y-body axis)
     * @param yaw   Yaw angle ψ in radians (rotation about Z-body axis)
     * @return Unit quaternion representing the combined rotation.
     */
    static Quaternion from_euler(double roll, double pitch, double yaw) {
        // Half-angles
        double cr = std::cos(roll * 0.5);
        double sr = std::sin(roll * 0.5);
        double cp = std::cos(pitch * 0.5);
        double sp = std::sin(pitch * 0.5);
        double cy = std::cos(yaw * 0.5);
        double sy = std::sin(yaw * 0.5);

        return {
            cr * cp * cy + sr * sp * sy,   // w
            sr * cp * cy - cr * sp * sy,   // x
            cr * sp * cy + sr * cp * sy,   // y
            cr * cp * sy - sr * sp * cy    // z
        };
    }

    /**
     * @brief Construct a quaternion from an axis-angle representation.
     *
     * @param axis  Unit vector defining the rotation axis.
     * @param angle Rotation angle in radians (right-hand rule).
     * @return Unit quaternion.
     */
    static Quaternion from_axis_angle(const Vec3& axis, double angle) {
        double half = angle * 0.5;
        double s = std::sin(half);
        Vec3 a = axis.normalized();
        return {std::cos(half), a.x * s, a.y * s, a.z * s};
    }

    /** @brief Return the identity (no-rotation) quaternion (1, 0, 0, 0). */
    static Quaternion identity() {
        return {1.0, 0.0, 0.0, 0.0};
    }

    /**
     * @brief Extract Euler angles (roll, pitch, yaw) from this quaternion.
     *
     * ZYX intrinsic convention (matches from_euler).
     *
     * @return Vec3(roll, pitch, yaw) in radians.
     */
    Vec3 to_euler() const {
        // Roll (x-axis rotation)
        double sinr_cosp = 2.0 * (w * x + y * z);
        double cosr_cosp = 1.0 - 2.0 * (x * x + y * y);
        double roll = std::atan2(sinr_cosp, cosr_cosp);

        // Pitch (y-axis rotation) — clamped to avoid NaN near ±90°
        double sinp = 2.0 * (w * y - z * x);
        double pitch;
        if (std::fabs(sinp) >= 1.0) {
            pitch = std::copysign(M_PI / 2.0, sinp); // gimbal lock
        } else {
            pitch = std::asin(sinp);
        }

        // Yaw (z-axis rotation)
        double siny_cosp = 2.0 * (w * z + x * y);
        double cosy_cosp = 1.0 - 2.0 * (y * y + z * z);
        double yaw = std::atan2(siny_cosp, cosy_cosp);

        return {roll, pitch, yaw};
    }
};

// ============================================================================
//  Enumerations
// ============================================================================

/**
 * @enum RocketType
 * @brief Supported ISRO launch vehicle variants.
 */
enum class RocketType {
    PSLV_XL,    ///< Polar Satellite Launch Vehicle — XL variant (6 strap-on boosters)
    GSLV_MK2,   ///< Geosynchronous Satellite Launch Vehicle — Mk II (cryo upper stage)
    LVM3         ///< Launch Vehicle Mark 3 (formerly GSLV Mk III, heavy-lift)
};

/**
 * @enum FailureType
 * @brief Catalogue of injectable failure modes for Monte-Carlo robustness testing.
 */
enum class FailureType {
    NONE,                    ///< Nominal (no failure)
    ENGINE_UNDERPERFORMANCE, ///< Reduced Isp / thrust
    CROSSWIND,               ///< Persistent lateral wind disturbance
    MASS_ASYMMETRY,          ///< Centre-of-mass offset producing roll torque
    TVC_FAILURE,             ///< Thrust Vector Control servo jam / rate limit
    CRYO_VALVE_FREEZE,       ///< Cryogenic feed valve stuck (upper stages)
    LOX_BOILOFF,             ///< Liquid oxygen boil-off reducing available propellant
    IMU_DRIFT                ///< Inertial measurement unit gyro drift
};

/**
 * @enum SimEvent
 * @brief Discrete events emitted during simulation for logging / visualisation.
 */
enum class SimEvent {
    NONE,               ///< No event this tick
    LIFTOFF,            ///< T-0 — umbilical release & first motion
    MAX_Q,              ///< Maximum dynamic pressure passage
    STAGING,            ///< Stage separation event
    FAIRING_JETTISON,   ///< Payload fairing jettison (typically ~115 km)
    MECO,               ///< Main Engine Cut-Off (final stage)
    ORBIT_INSERTION,    ///< Target orbit achieved
    ABORT               ///< Mission abort triggered
};

// ============================================================================
//  StageConfig — per-stage propulsion & structural parameters
// ============================================================================

/**
 * @struct StageConfig
 * @brief  Configuration data for a single rocket stage (or a set of identical units).
 *
 * All values are in SI units.  `count` allows modelling multiple identical
 * strap-on boosters as a single StageConfig entry (e.g. 6 for PSLV-XL S200).
 */
struct StageConfig {
    std::string name;                   ///< Human-readable stage designation (e.g. "PS1", "GS1")

    double thrust_vacuum    = 0.0;      ///< Vacuum thrust per unit (N)
    double thrust_sea_level = 0.0;      ///< Sea-level thrust per unit (N)
    double isp_vacuum       = 0.0;      ///< Vacuum specific impulse (s)
    double isp_sea_level    = 0.0;      ///< Sea-level specific impulse (s)

    double propellant_mass  = 0.0;      ///< Propellant mass per unit (kg)
    double structural_mass  = 0.0;      ///< Dry (structural) mass per unit (kg)
    double burn_time        = 0.0;      ///< Nominal burn duration (s)

    double gimbal_range     = 0.0;      ///< TVC gimbal deflection limit (radians)
    double reference_area   = 0.0;      ///< Aerodynamic reference area (m²)

    bool   is_solid         = false;    ///< True if this stage uses solid propellant
    bool   ground_lit       = false;    ///< True if this stage ignites at T-0 on the pad

    int    count            = 1;        ///< Number of identical units (e.g. 6 for strap-ons)
};

// ============================================================================
//  RocketConfig — complete vehicle configuration
// ============================================================================

/**
 * @struct RocketConfig
 * @brief  Complete launch vehicle configuration: stages, payload, and aerodynamics.
 *
 * The drag coefficient is stored as a piecewise-linear function of Mach number
 * defined by paired breakpoints and Cd values.
 */
struct RocketConfig {
    std::string name;                   ///< Vehicle designation (e.g. "PSLV-XL C56")
    RocketType  type = RocketType::PSLV_XL; ///< Vehicle family

    std::vector<StageConfig> stages;    ///< Ordered list of stages (0 = first to fire)

    double payload_mass   = 0.0;        ///< Payload mass (kg)
    double fairing_mass   = 0.0;        ///< Fairing mass (kg)
    double reference_area = 0.0;        ///< Vehicle aerodynamic reference area (m²)
    double total_height   = 0.0;        ///< Vehicle height (m), used for visualisation

    // Piecewise-linear drag model: Cd = f(Mach)
    std::vector<double> mach_breakpoints; ///< Mach number breakpoints (ascending)
    std::vector<double> cd_values;        ///< Corresponding drag coefficient values

    /**
     * @brief Compute total initial mass of the fully stacked vehicle.
     *
     * Sum over all stages: (propellant + structural) * count,
     * plus payload and fairing.
     *
     * @return Total wet mass (kg).
     */
    double total_mass() const {
        double mass = payload_mass + fairing_mass;
        for (const auto& stage : stages) {
            mass += (stage.propellant_mass + stage.structural_mass) * stage.count;
        }
        return mass;
    }
};

// ============================================================================
//  StateVector — 6-DOF vehicle state
// ============================================================================

/**
 * @struct StateVector
 * @brief  Complete 6-DOF state of the vehicle at a single instant.
 *
 * Positions and velocities are in the Earth-Centred Inertial (ECI) frame
 * (J2000 epoch, though for short ascent simulations precession is negligible).
 * Angular velocity is expressed in the body frame.
 */
struct StateVector {
    Vec3       position;           ///< ECI position (m from Earth centre)
    Vec3       velocity;           ///< ECI velocity (m/s)
    Quaternion attitude;           ///< Body-to-ECI rotation quaternion
    Vec3       angular_velocity;   ///< Body-frame angular velocity (rad/s)
    double     mass = 0.0;         ///< Current vehicle mass (kg)
    double     time = 0.0;         ///< Mission elapsed time from T-0 (s)
};

// ============================================================================
//  AtmosphereState — local atmospheric conditions
// ============================================================================

/**
 * @struct AtmosphereState
 * @brief  Atmospheric state at a given altitude, returned by the atmosphere model.
 */
struct AtmosphereState {
    double temperature    = 0.0;   ///< Static temperature (K)
    double pressure       = 0.0;   ///< Static pressure (Pa)
    double density        = 0.0;   ///< Air density (kg/m³)
    double speed_of_sound = 0.0;   ///< Local speed of sound (m/s)
};

// ============================================================================
//  TelemetryFrame — downlink snapshot
// ============================================================================

/**
 * @struct TelemetryFrame
 * @brief  A single telemetry snapshot suitable for display, logging, or WebSocket
 *         downlink to the frontend.
 *
 * All angular values for display purposes are in degrees.
 * Internal engine computations remain in radians.
 */
struct TelemetryFrame {
    // -- Temporal & kinematic scalars ----------------------------------------
    double time                   = 0.0; ///< Mission elapsed time (s)
    double altitude               = 0.0; ///< Altitude above sea level (m)
    double velocity_magnitude     = 0.0; ///< Inertial speed (m/s)
    double acceleration_magnitude = 0.0; ///< Net acceleration magnitude (m/s²)
    double downrange_distance     = 0.0; ///< Ground-track distance from pad (m)

    // -- Aerodynamic scalars -------------------------------------------------
    double mach_number      = 0.0;       ///< Local Mach number
    double dynamic_pressure = 0.0;       ///< Dynamic pressure q (Pa)
    bool   max_q_reached    = false;     ///< True once the vehicle has passed Max-Q
    double max_q_value      = 0.0;       ///< Peak dynamic pressure encountered (Pa)

    // -- Geodetic position ---------------------------------------------------
    double latitude         = 0.0;       ///< Geodetic latitude (degrees)
    double longitude        = 0.0;       ///< Geodetic longitude (degrees)
    double geodetic_altitude = 0.0;      ///< Altitude above WGS84 ellipsoid (m)

    // -- ECI state -----------------------------------------------------------
    Vec3   position_eci;                 ///< ECI position (m)
    Vec3   velocity_eci;                 ///< ECI velocity (m/s)

    // -- Attitude (display, degrees) -----------------------------------------
    double pitch = 0.0;                  ///< Pitch angle (degrees)
    double yaw   = 0.0;                  ///< Yaw angle (degrees)
    double roll  = 0.0;                  ///< Roll angle (degrees)

    // -- Propulsion & mass ---------------------------------------------------
    int    current_stage        = 0;     ///< Active stage index
    double thrust               = 0.0;  ///< Current total thrust (N)
    double mass                 = 0.0;  ///< Current vehicle mass (kg)
    double propellant_remaining = 0.0;  ///< Fraction of current-stage propellant remaining [0,1]

    // -- Structural / risk ---------------------------------------------------
    double structural_integrity = 1.0;  ///< Structural health indicator [0,1]
    double crash_probability    = 0.0;  ///< Estimated crash probability [0,1]

    // -- Event string --------------------------------------------------------
    std::string event;                   ///< Description of discrete event (empty if none)
};

} // namespace prakshep
