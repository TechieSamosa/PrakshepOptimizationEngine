/**
 * @file pslv_stage_2.hpp
 * @brief PSLV 2nd Stage (PS2) — Liquid Hypergolic Vikas Engine.
 *
 * The PS2 is the second stage of the Polar Satellite Launch Vehicle. It is
 * powered by a single Vikas engine, which burns hypergolic liquid propellants:
 * UH25 (a mixture of 75% UDMH and 25% Hydrazine) as fuel, and Nitrogen
 * Tetroxide (N2O4) as oxidiser.
 *
 * === LIQUID PROPULSION MATH ===
 * Unlike solid motors, liquid engines exhibit start-up and shutdown transients
 * due to turbopump spool-up and valve sequencing. This class models an ignition
 * delay followed by a first-order exponential thrust rise to nominal levels.
 *
 * === GIMBAL TVC (Thrust Vector Control) ===
 * The Vikas engine nozzle is mechanically gimbaled in two axes (pitch and yaw)
 * to steer the vehicle.
 *   - Hardware limit: ±4.0 degrees.
 *   - Actuator dynamics: Modeled with a first-order lag filter to simulate
 *     hydraulic actuation delays (i.e., the nozzle cannot instantly snap to
 *     a new commanded angle).
 *
 * === HRCS (Hot Gas Reaction Control System) ===
 * A single gimbaled engine can control pitch and yaw, but cannot generate
 * roll torque. The PS2 uses a separate Hot Gas Reaction Control System (HRCS)
 * comprising small thrusters mounted radially to provide roll authority.
 * We model this as a binary on/off torque contribution about the longitudinal axis.
 *
 * Physical constants sourced from ISRO PSLV technical documentation.
 *
 * @author  Prakshep GNC Team
 * @version 2.0.0
 */

#pragma once

#include "prakshep/stages/rocket_stage.hpp"
#include "prakshep/types.hpp"
#include <string>

namespace prakshep {
namespace stages {

class PSLV_Stage2 : public RocketStage {
public:
    // =======================================================================
    //  PS2 (Vikas Engine) Physical Constants
    // =======================================================================

    /// Total propellant mass (UH25 + N2O4)
    static constexpr double PROPELLANT_MASS       = 41700.0;     // kg

    /// Structural (dry) mass of the stage
    static constexpr double STRUCTURAL_MASS       = 4600.0;      // kg

    /// Nominal burn time
    static constexpr double BURN_TIME             = 133.0;       // seconds

    /// Vacuum thrust of the Vikas engine
    static constexpr double THRUST_VACUUM         = 803.7e3;     // N (803.7 kN)

    /// Specific impulse in vacuum
    static constexpr double ISP_VACUUM            = 293.0;       // seconds

    /// Stage length
    static constexpr double STAGE_LENGTH          = 12.8;        // metres

    /// Stage diameter
    static constexpr double STAGE_DIAMETER        = 2.8;         // metres

    // =======================================================================
    //  Gimbal TVC Constants
    // =======================================================================

    /// Maximum gimbal deflection angle (radians) - corresponds to ±4.0 degrees
    static constexpr double MAX_GIMBAL_ANGLE      = 4.0 * (M_PI / 180.0); // ~0.0698 rad

    /// Gimbal actuator time constant for first-order lag (seconds)
    static constexpr double GIMBAL_TIME_CONSTANT  = 0.15;

    // =======================================================================
    //  HRCS (Roll Control) Constants
    // =======================================================================

    /// Thrust per HRCS thruster (N)
    static constexpr double HRCS_THRUST           = 500.0;       // N

    /// Radial distance of HRCS thrusters from centerline (m)
    static constexpr double HRCS_MOMENT_ARM       = 1.4;         // m (half of STAGE_DIAMETER)

    // =======================================================================
    //  Ignition Transient Constants
    // =======================================================================

    /// Time for thrust to reach 63% of nominal during start-up (seconds)
    static constexpr double IGNITION_TIME_CONSTANT = 0.5;

    // =======================================================================
    //  Constructor
    // =======================================================================

    PSLV_Stage2();

    // =======================================================================
    //  RocketStage interface implementation
    // =======================================================================

    /**
     * @brief Advance the PS2 liquid engine physics by one time step.
     *
     * In addition to pitch/yaw commands, this stage also accepts a roll_command
     * which drives the HRCS system.
     *
     * @param dt              Integration time step (s)
     * @param pitch_command   Normalised pitch steering command [-1, +1]
     * @param yaw_command     Normalised yaw steering command [-1, +1]
     * @param roll_command    Normalised roll steering command [-1, +1]
     * @param altitude        Current altitude ASL (m)
     */
    void update_with_roll(double dt,
                          double pitch_command,
                          double yaw_command,
                          double roll_command,
                          double altitude);

    // Overloaded to satisfy the base class interface (assumes 0 roll command)
    void update(double dt, double pitch_command, double yaw_command, double altitude) override;

    void   ignite() override;
    void   separate() override;

    double      get_mass() const override;
    Vec3        get_thrust_vector() const override;
    Vec3        get_com_offset() const override;
    State       get_state() const override;
    double      get_propellant_fraction() const override;
    std::string get_name() const override;

    // =======================================================================
    //  PS2-specific accessors
    // =======================================================================

    /** @brief Get the current physical gimbal angles (pitch, yaw) in radians. */
    Vec3 get_gimbal_angles() const { return m_current_gimbal_angles; }

    /** @brief Get the torque generated by the HRCS for roll control. */
    Vec3 get_hrcs_torque() const { return m_hrcs_torque; }

private:
    // =======================================================================
    //  State variables
    // =======================================================================

    State  m_state;                     ///< Current lifecycle state

    double m_propellant_remaining;      ///< Remaining propellant mass (kg)
    double m_burn_elapsed;              ///< Time since ignition (s)

    Vec3   m_current_thrust_vector;     ///< Thrust vector in body frame (N)
    Vec3   m_current_com_offset;        ///< CoM offset from geometric centre (m)
    double m_current_mass_flow_rate;    ///< Instantaneous ṁ (kg/s)

    // TVC State
    Vec3   m_current_gimbal_angles;     ///< Actual gimbal deflection (0, pitch_rad, yaw_rad)
    Vec3   m_commanded_gimbal_angles;   ///< Commanded gimbal deflection (0, pitch_rad, yaw_rad)

    // HRCS State
    Vec3   m_hrcs_torque;               ///< Roll torque from HRCS (N*m)
};

} // namespace stages
} // namespace prakshep
