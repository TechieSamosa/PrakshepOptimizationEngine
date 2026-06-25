/**
 * @file trajectory_math.hpp
 * @brief Orbital mechanics helper functions for trajectory targeting.
 *
 * Provides analytical tools for:
 *   - Computing required Delta-V for orbit transfers using the vis-viva equation
 *   - Evaluating whether a specific stage can deliver sufficient impulse
 *     to push a payload through a required velocity change
 *   - Hohmann transfer calculations between circular and elliptical orbits
 *
 * All calculations assume a Keplerian two-body problem (no perturbations).
 * J2 corrections should be applied separately in the full simulation.
 *
 * @author  Prakshep GNC Team
 * @version 2.0.0
 */

#pragma once

#include "prakshep/types.hpp"

namespace prakshep {
namespace guidance {

/**
 * @brief Result of a Delta-V budget analysis for a given mission profile.
 */
struct DeltaVBudget {
    double dv_ascent;           ///< ΔV to exit atmosphere and achieve initial orbit velocity (m/s)
    double dv_circularisation;  ///< ΔV for the circularisation burn at apogee (m/s)
    double dv_total;            ///< Total mission ΔV requirement (m/s)
    double dv_losses_gravity;   ///< Estimated gravity loss ΔV penalty (m/s)
    double dv_losses_drag;      ///< Estimated drag loss ΔV penalty (m/s)
};

/**
 * @brief Result of a stage sufficiency evaluation.
 */
struct StageSufficiency {
    double available_dv;        ///< ΔV this stage can deliver (Tsiolkovsky) (m/s)
    double required_dv;         ///< ΔV the stage must deliver for this phase (m/s)
    double margin;              ///< Margin = available - required (m/s). Positive = sufficient.
    bool   sufficient;          ///< True if the stage can deliver the required ΔV
    double burnout_velocity;    ///< Predicted velocity at stage burnout (m/s)
};

// ===========================================================================
//  Core trajectory math functions
// ===========================================================================

/**
 * @brief Calculate the total ΔV required for a mission to the specified orbit.
 *
 * Uses the vis-viva equation to compute orbital velocities and builds a
 * complete ΔV budget including estimated gravity and drag losses.
 *
 * The vis-viva equation gives the velocity at any point in an orbit:
 *
 *     v² = μ × (2/r - 1/a)
 *
 * where:
 *   μ = GM_Earth (gravitational parameter)
 *   r = distance from Earth centre (m)
 *   a = semi-major axis of the orbit (m)
 *
 * @param target_apogee_km    Target apogee altitude above sea level (km)
 * @param target_perigee_km   Target perigee altitude above sea level (km)
 * @param payload_mass_kg     Payload mass (kg) — used for loss estimation
 * @return DeltaVBudget containing the complete ΔV breakdown.
 */
DeltaVBudget calculate_required_delta_v(double target_apogee_km,
                                        double target_perigee_km,
                                        double payload_mass_kg);

/**
 * @brief Evaluate whether a single stage can deliver the required ΔV.
 *
 * Uses the Tsiolkovsky ideal rocket equation:
 *
 *     Δv = Isp × g₀ × ln(m_initial / m_final)
 *
 * where:
 *   m_initial = total mass at ignition (stage + payload above)
 *   m_final   = mass at burnout (structural + payload above)
 *
 * @param isp_s               Effective specific impulse of the stage (s)
 * @param propellant_mass_kg  Total propellant mass in the stage (kg)
 * @param structural_mass_kg  Dry (structural) mass of the stage (kg)
 * @param payload_above_kg    Mass of everything above this stage (kg)
 * @param required_dv_ms      Required ΔV this stage must deliver (m/s)
 * @return StageSufficiency evaluation result.
 */
StageSufficiency evaluate_stage_sufficiency(double isp_s,
                                            double propellant_mass_kg,
                                            double structural_mass_kg,
                                            double payload_above_kg,
                                            double required_dv_ms);

/**
 * @brief Compute orbital velocity at a given altitude for a given orbit.
 *
 * @param altitude_km         Altitude above sea level (km)
 * @param semi_major_axis_km  Semi-major axis of the orbit (km, from Earth centre)
 * @return Orbital velocity (m/s).
 */
double orbital_velocity(double altitude_km, double semi_major_axis_km);

/**
 * @brief Compute the circular orbital velocity at a given altitude.
 *
 * Special case of vis-viva where a = r (circular orbit):
 *     v_circ = sqrt(μ / r)
 *
 * @param altitude_km  Altitude above sea level (km)
 * @return Circular orbital velocity (m/s).
 */
double circular_velocity(double altitude_km);

} // namespace guidance
} // namespace prakshep
