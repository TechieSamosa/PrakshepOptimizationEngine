/**
 * @file pslv_strap_on.hpp
 * @brief PSLV Strap-On Motor (PSOM / PSOM-XL) — S12M Solid Booster.
 *
 * The PSLV uses PSOM-XL (S12M) strap-on solid rocket motors mounted
 * radially around the PS1 core stage. Depending on the PSLV variant:
 *
 *   PSLV-CA: 0 strap-ons (Core Alone)
 *   PSLV-DL: 2 strap-ons (Dual Launch, both ground-lit)
 *   PSLV-QL: 4 strap-ons (Quad Launch, all ground-lit)
 *   PSLV-XL: 6 strap-ons (Extended, 4 ground-lit + 2 air-lit)
 *
 * === PSOM-XL (S12M) SPECIFICATIONS ===
 *
 * Each PSOM-XL is a monolithic solid motor loaded with 12,200 kg of HTPB
 * composite propellant in a simple cylindrical grain geometry (end-burning
 * with a central bore for structural integrity).
 *
 * Unlike the PS1's complex star grain, the PSOM has a nearly flat (neutral)
 * thrust profile — it burns at roughly constant thrust for its full duration.
 *
 * === STAGGERED IGNITION (PSLV-XL) ===
 *
 * In the PSLV-XL configuration:
 *   - Boosters at 0°, 90°, 180°, 270° are GROUND-LIT (T-0)
 *   - Boosters at 45° and 225° are AIR-LIT (T+25s)
 *
 * The ground-lit boosters burn out at ~T+70s.
 * The air-lit boosters burn out at ~T+95s.
 *
 * This staggered separation prevents a sudden 6-booster mass drop that
 * would cause an unacceptable spike in longitudinal acceleration.
 *
 * === SEPARATION DYNAMICS ===
 *
 * At burnout, each booster is decoupled via pyrotechnic bolt-cutters.
 * Separation springs at the attach points plus small retro-rockets on the
 * booster nose cones push the empty casings radially outward and aft
 * to clear the vehicle.
 *
 * Typical separation parameters:
 *   - Radial kick: ~3.0 m/s (outward from core axis)
 *   - Axial kick: ~1.5 m/s (aft, away from the vehicle)
 *   - Tumble rate: ~15-30 °/s (induced by asymmetric retro-rocket placement)
 *
 * @author  Prakshep GNC Team
 * @version 2.0.0
 */

#pragma once

#include "prakshep/stages/booster_module.hpp"
#include "prakshep/types.hpp"
#include <string>
#include <cmath>

namespace prakshep {
namespace stages {

class PSLV_StrapOn : public BoosterModule {
public:
    // =======================================================================
    //  PSOM-XL (S12M) Physical Constants
    //  Source: ISRO PSLV User Manual, VSSC Technical Reports
    // =======================================================================

    /// HTPB composite propellant mass per booster
    static constexpr double PROPELLANT_MASS       = 12200.0;     // kg

    /// Structural (dry) mass of the booster casing + nozzle
    static constexpr double STRUCTURAL_MASS       = 2570.0;      // kg

    /// Nominal burn time
    static constexpr double BURN_TIME             = 50.0;        // seconds

    /// Sea-level thrust (average)
    static constexpr double THRUST_SEA_LEVEL      = 719.5e3;     // N (719.5 kN)

    /// Vacuum thrust
    static constexpr double THRUST_VACUUM         = 803.7e3;     // N (803.7 kN)

    /// Specific impulse at sea level
    static constexpr double ISP_SEA_LEVEL         = 262.0;       // seconds

    /// Specific impulse in vacuum
    static constexpr double ISP_VACUUM            = 282.0;       // seconds

    /// Booster length
    static constexpr double BOOSTER_LENGTH        = 12.19;       // metres

    /// Booster diameter
    static constexpr double BOOSTER_DIAMETER      = 1.0;         // metres

    /// Nose cone length
    static constexpr double NOSE_CONE_LENGTH      = 1.5;         // metres

    // =======================================================================
    //  Separation Spring / Retro-Rocket Constants
    // =======================================================================

    /// Radial separation kick velocity (outward from core axis)
    static constexpr double SEP_VELOCITY_RADIAL   = 3.0;         // m/s

    /// Axial separation kick velocity (aft, opposite to flight direction)
    static constexpr double SEP_VELOCITY_AXIAL    = 1.5;         // m/s

    /// Induced tumble angular velocity (approximate, for visualisation)
    static constexpr double SEP_TUMBLE_RATE       = 0.4;         // rad/s (~23°/s)

    // =======================================================================
    //  Thrust Curve Breakpoints (neutral burn profile with ignition transient)
    // =======================================================================

    /// The PSOM has a near-neutral (flat) thrust profile.
    /// Short ignition transient → plateau → short tail-off.
    static constexpr double THRUST_CURVE_TIME[] = {
        0.00, 0.03, 0.08, 0.15, 0.85, 0.92, 0.97, 1.00
    };
    static constexpr double THRUST_CURVE_MULT[] = {
        0.40,   // Ignition transient (pressure rising)
        0.95,   // Approaching plateau
        1.02,   // Slight overshoot
        1.00,   // Plateau (neutral burn)
        1.00,   // Plateau continues
        0.90,   // Tail-off onset
        0.45,   // Rapid tail-off
        0.00    // Burnout
    };

    // =======================================================================
    //  Constructor
    // =======================================================================

    /**
     * @brief Construct a PSOM-XL strap-on booster.
     *
     * @param booster_id  Unique identifier (0-5 for PSLV-XL)
     */
    explicit PSLV_StrapOn(int booster_id = 0);

    // =======================================================================
    //  RocketStage interface
    // =======================================================================

    void   update(double dt, double pitch_command, double yaw_command, double altitude) override;
    void   ignite() override;
    void   separate() override;

    double      get_mass() const override;
    Vec3        get_thrust_vector() const override;
    Vec3        get_com_offset() const override;
    State       get_state() const override;
    double      get_propellant_fraction() const override;
    std::string get_name() const override;

    // =======================================================================
    //  BoosterModule interface
    // =======================================================================

    void set_mount_geometry(double angle_rad,
                            double radial_distance_m,
                            double height_offset_m) override;

    Vec3   get_mount_position() const override;
    double get_mount_angle() const override;

    void set_ignition_schedule(IgnitionGroup group,
                               double delay_seconds) override;

    IgnitionGroup get_ignition_group() const override;
    double        get_ignition_delay() const override;

    void check_ignition_schedule(double mission_time) override;

    Vec3 triggerDecouple() override;
    Vec3 get_separation_velocity() const override;

    Vec3 compute_torque(const Vec3& vehicle_com) const override;

    // =======================================================================
    //  PSOM-specific accessors
    // =======================================================================

    int get_booster_id() const { return m_booster_id; }

private:
    // =======================================================================
    //  Internal methods
    // =======================================================================

    double sample_thrust_curve(double normalised_time) const;
    double interpolate_isp(double altitude) const;

    // =======================================================================
    //  State variables
    // =======================================================================

    int    m_booster_id;                ///< Unique booster index (0-5)
    State  m_state;                     ///< Lifecycle state

    // Propulsion state
    double m_propellant_remaining;      ///< Remaining propellant (kg)
    double m_burn_elapsed;              ///< Time since ignition (s)
    double m_current_mass_flow_rate;    ///< Instantaneous ṁ (kg/s)
    Vec3   m_current_thrust_vector;     ///< Body-frame thrust (N)

    // Mounting geometry
    double m_mount_angle;               ///< Azimuthal angle from +Y (radians)
    double m_radial_distance;           ///< Distance from core axis (m)
    double m_height_offset;             ///< Axial offset from core centre (m)
    Vec3   m_mount_position;            ///< Cached mount position in body frame

    // Ignition scheduling
    IgnitionGroup m_ignition_group;     ///< Ground-lit or air-lit
    double m_ignition_delay;            ///< Delay from T-0 (s)

    // Separation state
    Vec3   m_separation_velocity;       ///< Lateral kick velocity (m/s)
    double m_separation_tumble_rate;    ///< Angular velocity post-separation (rad/s)
};

} // namespace stages
} // namespace prakshep
