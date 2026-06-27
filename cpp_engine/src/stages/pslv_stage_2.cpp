/**
 * @file pslv_stage_2.cpp
 * @brief Implementation of the PSLV 2nd Stage (PS2) — Vikas Engine.
 *
 * This file implements the physics for a liquid hypergolic engine.
 * Key differences from solid motors:
 *   1. Thrust is nominally constant, but features a start-up transient
 *      (exponential rise) rather than an instant step.
 *   2. Steering is achieved by mechanically gimbaling the entire nozzle,
 *      which requires modelling actuator lag (a first-order filter).
 *   3. Roll control requires a separate system (HRCS) because a single
 *      axisymmetric nozzle cannot generate roll torque.
 *
 * @author  Prakshep GNC Team
 * @version 2.0.0
 */

#include "prakshep/stages/pslv_stage_2.hpp"
#include "prakshep/constants.hpp"
#include <algorithm>
#include <cmath>

namespace prakshep {
namespace stages {

// ===========================================================================
//  Constructor
// ===========================================================================

PSLV_Stage2::PSLV_Stage2()
    : m_state(State::IDLE)
    , m_propellant_remaining(PROPELLANT_MASS)
    , m_burn_elapsed(0.0)
    , m_current_thrust_vector(0.0, 0.0, 0.0)
    , m_current_com_offset(0.0, 0.0, 0.0)
    , m_current_mass_flow_rate(0.0)
    , m_current_gimbal_angles(0.0, 0.0, 0.0)
    , m_commanded_gimbal_angles(0.0, 0.0, 0.0)
    , m_hrcs_torque(0.0, 0.0, 0.0)
{
}

// ===========================================================================
//  Lifecycle transitions
// ===========================================================================

void PSLV_Stage2::ignite() {
    if (m_state == State::IDLE) {
        m_state = State::BURNING;
        m_burn_elapsed = 0.0;
        // The liquid engine can technically be shut down, but PS2 is a
        // single-burn stage.
    }
}

void PSLV_Stage2::separate() {
    m_state = State::SEPARATED;
    m_current_thrust_vector = Vec3(0.0, 0.0, 0.0);
    m_hrcs_torque = Vec3(0.0, 0.0, 0.0);
}

// ===========================================================================
//  Core physics update
// ===========================================================================

void PSLV_Stage2::update(double dt, double pitch_command, double yaw_command, double altitude) {
    update_with_roll(dt, pitch_command, yaw_command, 0.0, altitude);
}

void PSLV_Stage2::update_with_roll(double dt,
                                   double pitch_command,
                                   double yaw_command,
                                   double roll_command,
                                   double altitude)
{
    if (m_state != State::BURNING) {
        m_current_thrust_vector = Vec3(0.0, 0.0, 0.0);
        m_current_mass_flow_rate = 0.0;
        m_hrcs_torque = Vec3(0.0, 0.0, 0.0);
        return;
    }

    // -----------------------------------------------------------------------
    //  Step 1: Advance burn clock
    // -----------------------------------------------------------------------

    m_burn_elapsed += dt;

    // Check for burnout
    if (m_propellant_remaining <= 0.0 || m_burn_elapsed >= BURN_TIME) {
        m_state = State::BURNOUT;
        m_propellant_remaining = 0.0;
        m_current_thrust_vector = Vec3(0.0, 0.0, 0.0);
        m_current_mass_flow_rate = 0.0;
        m_hrcs_torque = Vec3(0.0, 0.0, 0.0);
        return;
    }

    // -----------------------------------------------------------------------
    //  Step 2: Engine Start-up Transient
    //
    //  Hypergolic engines ignite when valves open and propellants mix.
    //  Thrust does not reach 100% instantly. We model an exponential rise.
    // -----------------------------------------------------------------------

    double thrust_multiplier = 1.0 - std::exp(-m_burn_elapsed / IGNITION_TIME_CONSTANT);

    // -----------------------------------------------------------------------
    //  Step 3: Altitude-dependent Isp and Base Thrust
    //
    //  The Vikas engine is a vacuum-optimised upper stage engine. While it
    //  does experience some backpressure during early PS2 flight, it's mostly
    //  in near-vacuum. We use a constant vacuum thrust for simplicity, or
    //  you could add an atmospheric pressure correction.
    // -----------------------------------------------------------------------

    // For PS2, altitude is generally > 60km, so pressure is near zero.
    // We'll use the vacuum thrust directly.
    double thrust_magnitude = THRUST_VACUUM * thrust_multiplier;
    double isp = ISP_VACUUM;

    // Mass flow rate: ṁ = F / (Isp × g₀)
    m_current_mass_flow_rate = thrust_magnitude / (isp * constants::G0);

    // -----------------------------------------------------------------------
    //  Step 4: Propellant Depletion
    // -----------------------------------------------------------------------

    double mass_consumed = m_current_mass_flow_rate * dt;
    m_propellant_remaining = std::max(0.0, m_propellant_remaining - mass_consumed);

    // -----------------------------------------------------------------------
    //  Step 5: TVC Gimbal Actuator Dynamics
    //
    //  The flight computer provides commands in [-1, 1].
    //  Map these to target angles, then apply a first-order lag to
    //  simulate the hydraulic/electromechanical actuator delay.
    // -----------------------------------------------------------------------

    // Clamp commands
    pitch_command = std::clamp(pitch_command, -1.0, 1.0);
    yaw_command   = std::clamp(yaw_command,   -1.0, 1.0);

    // Target angles
    double target_pitch = pitch_command * MAX_GIMBAL_ANGLE;
    double target_yaw   = yaw_command   * MAX_GIMBAL_ANGLE;
    m_commanded_gimbal_angles = Vec3(0.0, target_pitch, target_yaw);

    // First-order lag: y(k) = y(k-1) + (dt / (tau + dt)) * (u(k) - y(k-1))
    double alpha = dt / (GIMBAL_TIME_CONSTANT + dt);

    m_current_gimbal_angles.y += alpha * (target_pitch - m_current_gimbal_angles.y);
    m_current_gimbal_angles.z += alpha * (target_yaw - m_current_gimbal_angles.z);

    // -----------------------------------------------------------------------
    //  Step 6: Compose Thrust Vector
    //
    //  The thrust vector is deflected by the gimbal angles.
    //  Note: A positive pitch angle of the nozzle directs thrust downward
    //  relative to the local vertical, pitching the nose UP.
    // -----------------------------------------------------------------------

    double gp = m_current_gimbal_angles.y;
    double gy = m_current_gimbal_angles.z;

    // Small angle approximation is valid, but full rotation matrix is safer
    // The thrust acts in the +X direction of the nozzle frame.
    // Rotate by yaw (Z) then pitch (Y).
    m_current_thrust_vector = Vec3(
        thrust_magnitude * std::cos(gp) * std::cos(gy),
        thrust_magnitude * std::sin(gp),
        thrust_magnitude * std::sin(gy)
    );

    // -----------------------------------------------------------------------
    //  Step 7: HRCS Roll Control
    //
    //  Roll command [-1, 1] determines thrust from the HRCS thrusters.
    //  Torque = Force × Moment Arm
    //  This torque acts purely about the X axis (roll).
    // -----------------------------------------------------------------------

    roll_command = std::clamp(roll_command, -1.0, 1.0);
    double roll_force = roll_command * HRCS_THRUST;
    m_hrcs_torque = Vec3(roll_force * HRCS_MOMENT_ARM, 0.0, 0.0);

    // -----------------------------------------------------------------------
    //  Step 8: Centre-of-Mass Tracking
    //
    //  Unlike solid motors, liquid tanks drain from the bottom (typically).
    //  The PS2 has two tanks: N2O4 and UH25.
    //  As they empty, the CoM moves. For a simple model, we assume a linear
    //  shift from a full state to an empty state.
    // -----------------------------------------------------------------------

    double burn_fraction = 1.0 - (m_propellant_remaining / PROPELLANT_MASS);
    // Rough estimate: CoM starts slightly aft of centre, moves further aft
    double com_position = STAGE_LENGTH * (0.55 + 0.1 * burn_fraction);
    m_current_com_offset = Vec3(com_position - STAGE_LENGTH * 0.5, 0.0, 0.0);
}

// ===========================================================================
//  Canonical Accessors
// ===========================================================================

double PSLV_Stage2::get_mass() const {
    return STRUCTURAL_MASS + m_propellant_remaining;
}

Vec3 PSLV_Stage2::get_thrust_vector() const {
    return m_current_thrust_vector;
}

Vec3 PSLV_Stage2::get_com_offset() const {
    return m_current_com_offset;
}

RocketStage::State PSLV_Stage2::get_state() const {
    return m_state;
}

double PSLV_Stage2::get_propellant_fraction() const {
    return m_propellant_remaining / PROPELLANT_MASS;
}

std::string PSLV_Stage2::get_name() const {
    return "PS2 - Vikas Engine";
}

} // namespace stages
} // namespace prakshep
