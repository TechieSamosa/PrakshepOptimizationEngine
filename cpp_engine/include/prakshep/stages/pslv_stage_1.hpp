/**
 * @file pslv_stage_1.hpp
 * @brief PSLV 1st Stage (PS1) — S139 Solid Motor with SITVC steering.
 *
 * The PS1 is the first core stage of the Polar Satellite Launch Vehicle.
 * It houses the S139 solid rocket motor — the fourth-largest solid propellant
 * booster in the world — loaded with 138,200 kg of Hydroxyl-Terminated
 * Polybutadiene (HTPB) composite propellant in a star-grain geometry.
 *
 * Steering during the PS1 burn phase is NOT accomplished through mechanical
 * gimbal actuation (as in liquid engines). Instead, ISRO uses Secondary
 * Injection Thrust Vector Control (SITVC):
 *
 *   → Strontium Perchlorate solution is injected through ports in the nozzle
 *     divergent section. The injected liquid creates an asymmetric shock
 *     interaction in the supersonic exhaust flow, producing a lateral force
 *     component that deflects the effective thrust vector.
 *
 *   → The deflection angle δ is proportional to the ratio of secondary
 *     injection mass flow rate to the primary core mass flow rate:
 *
 *         δ = k_sitvc * (ṁ_injectant / ṁ_core)
 *
 *     where k_sitvc is an empirically-calibrated injection efficiency
 *     constant (~0.15 rad per unit flow ratio for PSLV).
 *
 * Physical constants sourced from ISRO PSLV User Manual (Issue 7, Rev 0)
 * and publicly available VSSC/LPSC technical documentation.
 *
 * @author  Prakshep GNC Team
 * @version 2.0.0
 */

#pragma once

#include "prakshep/stages/rocket_stage.hpp"
#include "prakshep/types.hpp"
#include <vector>
#include <string>
#include <cmath>

namespace prakshep {
namespace stages {

class PSLV_Stage1 : public RocketStage {
public:
    // =======================================================================
    //  S139 Motor Physical Constants (ISRO PSLV User Manual)
    // =======================================================================

    /// Propellant: HTPB-based composite solid propellant (star grain)
    static constexpr double PROPELLANT_MASS       = 138200.0;    // kg

    /// Structural (dry) mass of the S139 casing + nozzle assembly
    static constexpr double STRUCTURAL_MASS       = 30200.0;     // kg

    /// Nominal burn time of the S139 motor
    static constexpr double BURN_TIME             = 109.0;       // seconds

    /// Sea-level thrust (average over burn)
    static constexpr double THRUST_SEA_LEVEL      = 4846.9e3;    // N (4846.9 kN)

    /// Vacuum thrust
    static constexpr double THRUST_VACUUM         = 5150.0e3;    // N (5150 kN)

    /// Specific impulse at sea level
    static constexpr double ISP_SEA_LEVEL         = 237.0;       // seconds

    /// Specific impulse in vacuum
    static constexpr double ISP_VACUUM            = 269.0;       // seconds

    /// Stage length (for geometry and CoM calculations)
    static constexpr double STAGE_LENGTH          = 20.34;       // metres

    /// Stage outer diameter
    static constexpr double STAGE_DIAMETER        = 2.8;         // metres

    /// Nozzle exit diameter
    static constexpr double NOZZLE_EXIT_DIAMETER  = 2.1;         // metres

    // =======================================================================
    //  SITVC Parameters
    // =======================================================================

    /// SITVC injection efficiency constant.
    /// Empirically: ~0.15 radians of thrust deflection per unit (ṁ_inj / ṁ_core).
    /// This means if injection flow is 10% of core flow, deflection is ~0.015 rad (~0.86°).
    static constexpr double K_SITVC               = 0.15;        // rad per flow ratio

    /// Maximum available SITVC injection mass flow rate (kg/s).
    /// Each of the two pitch + two yaw injectors can deliver up to this rate.
    static constexpr double MAX_INJECTION_FLOW     = 18.0;       // kg/s per axis

    /// Total strontium perchlorate solution available for SITVC
    static constexpr double SITVC_FLUID_MASS       = 3400.0;     // kg

    /// Maximum thrust deflection achievable through SITVC (safety clamp)
    static constexpr double MAX_DEFLECTION_ANGLE   = 0.035;      // ~2.0 degrees in radians

    // =======================================================================
    //  Thrust Curve Breakpoints (normalised time vs thrust multiplier)
    //  Models the regressive star-grain burn profile of the S139
    // =======================================================================

    /// Normalised burn time breakpoints [0.0 = ignition, 1.0 = burnout]
    static constexpr double THRUST_CURVE_TIME[] = {
        0.00, 0.02, 0.05, 0.10, 0.20, 0.40, 0.60, 0.80, 0.90, 0.95, 0.98, 1.00
    };

    /// Thrust multiplier at each breakpoint (1.0 = nominal average thrust).
    /// The star grain ignites with a spike, settles into a plateau, then
    /// regresses as the web burns through and tail-off begins.
    static constexpr double THRUST_CURVE_MULT[] = {
        0.60,  // T+0.0s : Ignition transient (pressure building)
        1.12,  // T+2.2s : Ignition spike (full web exposed)
        1.08,  // T+5.5s : Settling into plateau
        1.05,  // T+10.9s: Plateau region
        1.02,  //          Stable burn
        1.00,  //          Mid-burn plateau
        0.98,  //          Slight regression begins
        0.92,  //          Web thinning
        0.80,  //          Tail-off onset
        0.55,  //          Rapid tail-off
        0.25,  //          Near burnout
        0.00   //          Burnout
    };

    // =======================================================================
    //  Constructor
    // =======================================================================

    /**
     * @brief Construct a PS1 stage module.
     *
     * The stage is initialised in the IDLE state. Call ignite() to begin
     * the S139 burn sequence.
     */
    PSLV_Stage1();

    // =======================================================================
    //  RocketStage interface implementation
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
    //  PS1-specific accessors
    // =======================================================================

    /** @brief Current SITVC deflection angles (pitch, yaw) in radians. */
    Vec3 get_sitvc_deflection() const { return m_sitvc_deflection; }

    /** @brief Remaining SITVC fluid mass (kg). */
    double get_sitvc_fluid_remaining() const { return m_sitvc_fluid_remaining; }

    /** @brief Burn elapsed time (s). */
    double get_burn_elapsed() const { return m_burn_elapsed; }

private:
    // =======================================================================
    //  Internal methods
    // =======================================================================

    /**
     * @brief Calculate the SITVC thrust deflection vector.
     *
     * Models secondary injection of strontium perchlorate solution into the
     * nozzle divergent section. The injected fluid creates an asymmetric
     * shock pattern in the supersonic exhaust, producing a lateral force.
     *
     * @param pitch_cmd  Normalised pitch command [-1, +1]
     * @param yaw_cmd    Normalised yaw command [-1, +1]
     * @param dt         Time step for fluid consumption (s)
     * @return Vec3 containing (0, pitch_deflection, yaw_deflection) in radians.
     */
    Vec3 calculateSITVC_Vector(double pitch_cmd, double yaw_cmd, double dt);

    /**
     * @brief Sample the pre-defined thrust curve at a given normalised burn time.
     *
     * Uses piecewise-linear interpolation on the star-grain regression profile.
     *
     * @param normalised_time  Burn progress [0.0 = ignition, 1.0 = burnout]
     * @return Thrust multiplier (dimensionless, ~1.0 at plateau).
     */
    double sample_thrust_curve(double normalised_time) const;

    /**
     * @brief Interpolate Isp between sea-level and vacuum values.
     *
     * Uses a simple exponential atmosphere scale-height model:
     *   Isp = Isp_sl + (Isp_vac - Isp_sl) * (1 - exp(-alt / H))
     *
     * @param altitude  Geometric altitude ASL (m)
     * @return Effective specific impulse (s).
     */
    double interpolate_isp(double altitude) const;

    // =======================================================================
    //  State variables
    // =======================================================================

    State  m_state;                     ///< Current lifecycle state

    double m_propellant_remaining;      ///< Remaining propellant mass (kg)
    double m_burn_elapsed;              ///< Time since ignition (s)

    double m_sitvc_fluid_remaining;     ///< Remaining SITVC strontium perchlorate (kg)
    Vec3   m_sitvc_deflection;          ///< Current SITVC deflection (0, pitch, yaw) rad

    Vec3   m_current_thrust_vector;     ///< Thrust vector in body frame (N)
    Vec3   m_current_com_offset;        ///< CoM offset from geometric centre (m)
    double m_current_mass_flow_rate;    ///< Instantaneous ṁ_core (kg/s)
};

} // namespace stages
} // namespace prakshep
