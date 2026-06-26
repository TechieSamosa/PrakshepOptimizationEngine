/**
 * @file pslv_strap_on.cpp
 * @brief Implementation of the PSLV Strap-On Motor (PSOM-XL / S12M).
 *
 * === OFF-AXIS THRUST KINEMATICS ===
 *
 * A strap-on booster mounted at radial distance d from the core axis
 * generates thrust along its own longitudinal axis. Because the booster
 * is constrained to the rigid body of the vehicle, its thrust vector
 * is parallel to the core's +X_body axis (axial).
 *
 * However, the point of thrust application is offset from the core axis,
 * which creates a moment (torque) about the vehicle's centre of mass:
 *
 *     τ = r_mount × F_thrust
 *
 * where r_mount is the vector from the vehicle CoM to the booster's
 * thrust application point.
 *
 * When all boosters are burning symmetrically, these torques cancel.
 * When asymmetric (e.g., ground-lit burnout while air-lit still firing),
 * a net torque is produced that the GNC system must actively counteract
 * using the PS1's SITVC system.
 *
 * === STAGGERED IGNITION STATE MACHINE ===
 *
 *   IDLE ──(mission_time >= delay)──> BURNING ──(burnout)──> BURNOUT
 *                                                                │
 *                                              triggerDecouple() │
 *                                                                ▼
 *                                                           SEPARATED
 *
 * The flight computer calls check_ignition_schedule(mission_time) every
 * timestep. Ground-lit boosters ignite at T-0 (delay = 0). Air-lit
 * boosters ignite when the timer reaches their programmed delay.
 *
 * === SEPARATION IMPULSE ===
 *
 * At burnout, triggerDecouple() computes a lateral kick velocity:
 *
 *     Δv = v_radial * r̂_mount + v_axial * (-x̂_body)
 *
 * where:
 *   v_radial = 3.0 m/s (separation springs, directed radially outward)
 *   v_axial  = 1.5 m/s (retro-rockets, directed aft)
 *   r̂_mount  = unit vector from core axis to booster centre
 *
 * The frontend reads this vector to animate the tumbling debris.
 *
 * @author  Prakshep GNC Team
 * @version 2.0.0
 */

#include "prakshep/stages/pslv_strap_on.hpp"
#include "prakshep/constants.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>

namespace prakshep {
namespace stages {

// ===========================================================================
//  Constructor
// ===========================================================================

PSLV_StrapOn::PSLV_StrapOn(int booster_id)
    : m_booster_id(booster_id)
    , m_state(State::IDLE)
    , m_propellant_remaining(PROPELLANT_MASS)
    , m_burn_elapsed(0.0)
    , m_current_mass_flow_rate(0.0)
    , m_current_thrust_vector(0.0, 0.0, 0.0)
    , m_mount_angle(0.0)
    , m_radial_distance(2.4)          // Default: 2.4m from core axis
    , m_height_offset(-2.0)           // Default: 2m below core centre
    , m_mount_position(0.0, 0.0, 0.0)
    , m_ignition_group(IgnitionGroup::GROUND_LIT)
    , m_ignition_delay(0.0)
    , m_separation_velocity(0.0, 0.0, 0.0)
    , m_separation_tumble_rate(0.0)
{
    // Compute initial mount position from default geometry
    m_mount_position = Vec3(
        m_height_offset,
        std::cos(m_mount_angle) * m_radial_distance,
        std::sin(m_mount_angle) * m_radial_distance
    );
}

// ===========================================================================
//  Mounting geometry
// ===========================================================================

void PSLV_StrapOn::set_mount_geometry(double angle_rad,
                                       double radial_distance_m,
                                       double height_offset_m)
{
    m_mount_angle      = angle_rad;
    m_radial_distance  = radial_distance_m;
    m_height_offset    = height_offset_m;

    // Recompute the mount position in the core body frame.
    //
    // Body frame convention:
    //   +X = axial (thrust direction, "up")
    //   +Y = lateral ("north" at 0° azimuth)
    //   +Z = lateral ("east" at 90° azimuth)
    //
    // The booster is positioned at:
    //   x = height_offset (axial position along core)
    //   y = cos(angle) * radial_distance
    //   z = sin(angle) * radial_distance
    m_mount_position = Vec3(
        m_height_offset,
        std::cos(m_mount_angle) * m_radial_distance,
        std::sin(m_mount_angle) * m_radial_distance
    );
}

Vec3 PSLV_StrapOn::get_mount_position() const {
    return m_mount_position;
}

double PSLV_StrapOn::get_mount_angle() const {
    return m_mount_angle;
}

// ===========================================================================
//  Staggered ignition scheduling
// ===========================================================================

void PSLV_StrapOn::set_ignition_schedule(IgnitionGroup group,
                                          double delay_seconds)
{
    m_ignition_group = group;
    m_ignition_delay = delay_seconds;

    // Sanity: ground-lit must have delay = 0
    if (group == IgnitionGroup::GROUND_LIT) {
        m_ignition_delay = 0.0;
    }
}

BoosterModule::IgnitionGroup PSLV_StrapOn::get_ignition_group() const {
    return m_ignition_group;
}

double PSLV_StrapOn::get_ignition_delay() const {
    return m_ignition_delay;
}

void PSLV_StrapOn::check_ignition_schedule(double mission_time) {
    // Auto-ignite when the mission clock reaches our scheduled time
    if (m_state == State::IDLE && mission_time >= m_ignition_delay) {
        ignite();
    }
}

// ===========================================================================
//  Lifecycle transitions
// ===========================================================================

void PSLV_StrapOn::ignite() {
    if (m_state == State::IDLE) {
        m_state = State::BURNING;
        m_burn_elapsed = 0.0;
        // Solid motor ignition is irreversible
    }
}

void PSLV_StrapOn::separate() {
    m_state = State::SEPARATED;
    m_current_thrust_vector = Vec3(0.0, 0.0, 0.0);
}

// ===========================================================================
//  Core physics update
// ===========================================================================

void PSLV_StrapOn::update(double dt,
                           double pitch_command,
                           double yaw_command,
                           double altitude)
{
    // pitch/yaw commands are ignored — strap-ons have no TVC.
    // They are fixed-nozzle solid motors. All steering is done by the PS1's SITVC.
    (void)pitch_command;
    (void)yaw_command;

    if (m_state != State::BURNING) {
        m_current_thrust_vector = Vec3(0.0, 0.0, 0.0);
        m_current_mass_flow_rate = 0.0;
        return;
    }

    // -----------------------------------------------------------------------
    //  Step 1: Advance burn clock
    // -----------------------------------------------------------------------

    m_burn_elapsed += dt;
    double normalised_time = m_burn_elapsed / BURN_TIME;

    // Check for burnout
    if (normalised_time >= 1.0 || m_propellant_remaining <= 0.0) {
        m_state = State::BURNOUT;
        m_propellant_remaining = 0.0;
        m_current_thrust_vector = Vec3(0.0, 0.0, 0.0);
        m_current_mass_flow_rate = 0.0;
        return;
    }

    // -----------------------------------------------------------------------
    //  Step 2: Sample the near-neutral thrust curve
    //
    //  The PSOM-XL has a much simpler grain geometry than the PS1.
    //  The thrust profile is nearly flat (neutral burn) with a short
    //  ignition transient and a brief tail-off at the end.
    // -----------------------------------------------------------------------

    double thrust_multiplier = sample_thrust_curve(normalised_time);

    // -----------------------------------------------------------------------
    //  Step 3: Altitude-dependent Isp
    // -----------------------------------------------------------------------

    double isp = interpolate_isp(altitude);

    // -----------------------------------------------------------------------
    //  Step 4: Instantaneous thrust
    //
    //  The base thrust transitions from sea-level to vacuum with altitude.
    //  The thrust_multiplier then shapes it according to the grain profile.
    // -----------------------------------------------------------------------

    double pressure_ratio = std::exp(-altitude / 8500.0);
    double base_thrust = THRUST_SEA_LEVEL * pressure_ratio
                       + THRUST_VACUUM * (1.0 - pressure_ratio);
    double thrust_magnitude = base_thrust * thrust_multiplier;

    // Mass flow rate: ṁ = F / (Isp × g₀)
    m_current_mass_flow_rate = thrust_magnitude / (isp * constants::G0);

    // -----------------------------------------------------------------------
    //  Step 5: Propellant depletion
    // -----------------------------------------------------------------------

    double mass_consumed = m_current_mass_flow_rate * dt;
    m_propellant_remaining = std::max(0.0, m_propellant_remaining - mass_consumed);

    // -----------------------------------------------------------------------
    //  Step 6: Compose thrust vector in body frame
    //
    //  Strap-on boosters have FIXED nozzles — no TVC capability.
    //  Thrust is purely axial (+X_body direction).
    //
    //  The off-axis torque contribution is computed separately in
    //  compute_torque() which the flight computer calls.
    // -----------------------------------------------------------------------

    m_current_thrust_vector = Vec3(thrust_magnitude, 0.0, 0.0);
}

// ===========================================================================
//  Separation dynamics
// ===========================================================================

Vec3 PSLV_StrapOn::triggerDecouple()
{
    // -----------------------------------------------------------------------
    //  Compute the lateral separation impulse.
    //
    //  The separation system consists of:
    //    1. Pyrotechnic bolt-cutters that sever the structural attach points
    //    2. Compression springs at 3 points along the booster length
    //    3. Two small retro-rockets on the nose cone
    //
    //  The combined effect is a velocity kick with two components:
    //    - Radial (outward from core axis): pushes casing clear of the vehicle
    //    - Axial (aft): ensures the casing falls behind, never forward
    //
    //  The radial direction is determined by the booster's mounting angle.
    //  In body frame coordinates:
    //    r̂_radial = (0, cos(mount_angle), sin(mount_angle))
    //    x̂_aft    = (-1, 0, 0)
    //
    //  Δv = SEP_VELOCITY_RADIAL × r̂_radial + SEP_VELOCITY_AXIAL × x̂_aft
    // -----------------------------------------------------------------------

    Vec3 radial_unit(
        0.0,
        std::cos(m_mount_angle),
        std::sin(m_mount_angle)
    );

    Vec3 axial_aft(-1.0, 0.0, 0.0);

    m_separation_velocity = radial_unit * SEP_VELOCITY_RADIAL
                          + axial_aft   * SEP_VELOCITY_AXIAL;

    m_separation_tumble_rate = SEP_TUMBLE_RATE;

    // Transition to SEPARATED state
    m_state = State::SEPARATED;
    m_current_thrust_vector = Vec3(0.0, 0.0, 0.0);
    m_current_mass_flow_rate = 0.0;

    return m_separation_velocity;
}

Vec3 PSLV_StrapOn::get_separation_velocity() const {
    return m_separation_velocity;
}

// ===========================================================================
//  Off-axis torque computation
// ===========================================================================

Vec3 PSLV_StrapOn::compute_torque(const Vec3& vehicle_com) const
{
    // -----------------------------------------------------------------------
    //  Torque = r × F
    //
    //  r = vector from vehicle CoM to the booster's thrust application point.
    //      The thrust acts at the booster's nozzle exit, which is at the
    //      aft end of the booster: mount_position + (0, 0, 0) since the
    //      booster nozzle is aligned with the core nozzle plane.
    //
    //  F = booster thrust vector (axial, +X_body).
    //
    //  When all boosters burn symmetrically, the cross products sum to zero.
    //  When asymmetric (e.g., 2 air-lit still burning, 4 ground-lit burnt out),
    //  a net torque appears that the GNC must counteract.
    // -----------------------------------------------------------------------

    if (m_state != State::BURNING) {
        return Vec3(0.0, 0.0, 0.0);
    }

    // Lever arm from vehicle CoM to booster thrust point
    Vec3 r = m_mount_position - vehicle_com;

    // Torque = r × F
    return r.cross(m_current_thrust_vector);
}

// ===========================================================================
//  Thrust curve interpolation
// ===========================================================================

double PSLV_StrapOn::sample_thrust_curve(double normalised_time) const
{
    constexpr int N = sizeof(THRUST_CURVE_TIME) / sizeof(THRUST_CURVE_TIME[0]);

    normalised_time = std::clamp(normalised_time, 0.0, 1.0);

    if (normalised_time <= THRUST_CURVE_TIME[0])     return THRUST_CURVE_MULT[0];
    if (normalised_time >= THRUST_CURVE_TIME[N - 1]) return THRUST_CURVE_MULT[N - 1];

    for (int i = 0; i < N - 1; ++i) {
        if (normalised_time >= THRUST_CURVE_TIME[i] &&
            normalised_time <  THRUST_CURVE_TIME[i + 1])
        {
            double t0 = THRUST_CURVE_TIME[i];
            double t1 = THRUST_CURVE_TIME[i + 1];
            double m0 = THRUST_CURVE_MULT[i];
            double m1 = THRUST_CURVE_MULT[i + 1];

            double alpha = (normalised_time - t0) / (t1 - t0);
            return m0 + alpha * (m1 - m0);
        }
    }

    return 1.0;
}

// ===========================================================================
//  Isp interpolation
// ===========================================================================

double PSLV_StrapOn::interpolate_isp(double altitude) const
{
    double altitude_factor = 1.0 - std::exp(-altitude / 8500.0);
    return ISP_SEA_LEVEL + (ISP_VACUUM - ISP_SEA_LEVEL) * altitude_factor;
}

// ===========================================================================
//  Canonical accessors
// ===========================================================================

double PSLV_StrapOn::get_mass() const {
    return STRUCTURAL_MASS + m_propellant_remaining;
}

Vec3 PSLV_StrapOn::get_thrust_vector() const {
    return m_current_thrust_vector;
}

Vec3 PSLV_StrapOn::get_com_offset() const {
    // PSOM has a simple cylindrical grain — CoM stays roughly centred.
    // Minor forward shift (~5%) as propellant burns from the aft end.
    double burn_fraction = 1.0 - (m_propellant_remaining / PROPELLANT_MASS);
    double com_axial = BOOSTER_LENGTH * (0.48 + 0.04 * burn_fraction);
    return Vec3(com_axial - BOOSTER_LENGTH * 0.5, 0.0, 0.0);
}

RocketStage::State PSLV_StrapOn::get_state() const {
    return m_state;
}

double PSLV_StrapOn::get_propellant_fraction() const {
    return m_propellant_remaining / PROPELLANT_MASS;
}

std::string PSLV_StrapOn::get_name() const {
    std::ostringstream oss;
    oss << "PSOM-XL #" << m_booster_id;
    if (m_ignition_group == IgnitionGroup::GROUND_LIT) {
        oss << " (Ground-Lit)";
    } else {
        oss << " (Air-Lit, T+" << m_ignition_delay << "s)";
    }
    return oss.str();
}

} // namespace stages
} // namespace prakshep
