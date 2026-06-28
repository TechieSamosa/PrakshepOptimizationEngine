/**
 * @file pslv_stage_3.hpp
 * @brief PSLV 3rd Stage (PS3) — S7 Solid Motor with Flex-Seal TVC.
 *
 * The PS3 is a high-performance solid upper stage. Unlike the PS1 (steel casing)
 * and the PSOMs, the S7 motor uses a lightweight Kevlar-epoxy composite casing.
 *
 * === FLEX-SEAL TVC ===
 * The nozzle of the PS3 is submerged and uses an elastomeric flex-bearing
 * (flex-seal) for Thrust Vector Control, instead of a mechanical gimbal joint
 * or SITVC.
 *
 * A flex-seal provides a leak-proof pivot, but it possesses mechanical stiffness.
 * When the hydraulic actuators pivot the nozzle, they must overcome the restoring
 * spring force of the rubber/steel laminate layers.
 *
 * The GNC module models this as a second-order spring-damper system:
 *     I_nozzle * δ'' + C_damp * δ' + K_flex * δ = T_actuator
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

class PSLV_Stage3 : public RocketStage {
public:
    // =======================================================================
    //  S7 Motor Physical Constants
    // =======================================================================

    /// Propellant: HTPB composite solid propellant
    static constexpr double PROPELLANT_MASS       = 7600.0;      // kg

    /// Structural mass (Kevlar-epoxy casing + nozzle)
    static constexpr double STRUCTURAL_MASS       = 840.0;       // kg

    /// Nominal burn time
    static constexpr double BURN_TIME             = 112.0;       // seconds

    /// Vacuum thrust (average)
    static constexpr double THRUST_VACUUM         = 240.0e3;     // N (240 kN)

    /// Specific impulse in vacuum
    static constexpr double ISP_VACUUM            = 294.0;       // seconds

    /// Stage length
    static constexpr double STAGE_LENGTH          = 3.6;         // metres

    /// Stage diameter
    static constexpr double STAGE_DIAMETER        = 2.0;         // metres

    // =======================================================================
    //  Flex-Seal TVC Dynamics
    // =======================================================================

    /// Maximum commanded deflection angle (radians, ~±2.0 degrees)
    static constexpr double MAX_DEFLECTION_ANGLE  = 2.0 * (M_PI / 180.0);

    /// Flex-seal rotational stiffness (N*m/rad)
    static constexpr double FLEX_STIFFNESS        = 25000.0;

    /// Flex-seal structural damping coefficient (N*m*s/rad)
    static constexpr double FLEX_DAMPING          = 1500.0;

    /// Nozzle moment of inertia about the pivot (kg*m^2)
    static constexpr double NOZZLE_INERTIA        = 450.0;

    /// Maximum torque the hydraulic actuators can apply (N*m)
    static constexpr double MAX_ACTUATOR_TORQUE   = 12000.0;

    // =======================================================================
    //  Thrust Curve Breakpoints
    // =======================================================================

    static constexpr double THRUST_CURVE_TIME[] = {
        0.00, 0.02, 0.10, 0.50, 0.85, 0.95, 1.00
    };
    static constexpr double THRUST_CURVE_MULT[] = {
        0.50, 1.10, 1.05, 1.00, 0.95, 0.40, 0.00
    };

    // =======================================================================
    //  Constructor
    // =======================================================================

    PSLV_Stage3();

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
    //  PS3-specific accessors
    // =======================================================================

    Vec3 get_nozzle_angles() const { return m_nozzle_angles; }

private:
    double sample_thrust_curve(double normalised_time) const;
    void   update_flex_seal_dynamics(double dt, double pitch_cmd, double yaw_cmd);

    // =======================================================================
    //  State variables
    // =======================================================================

    State  m_state;

    double m_propellant_remaining;
    double m_burn_elapsed;
    double m_current_mass_flow_rate;
    Vec3   m_current_thrust_vector;
    Vec3   m_current_com_offset;

    // Second-order TVC state
    Vec3   m_nozzle_angles;         ///< (0, pitch_rad, yaw_rad)
    Vec3   m_nozzle_rates;          ///< (0, pitch_rad/s, yaw_rad/s)
};

} // namespace stages
} // namespace prakshep
