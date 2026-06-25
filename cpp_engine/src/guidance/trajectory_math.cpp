/**
 * @file trajectory_math.cpp
 * @brief Orbital mechanics computations for trajectory targeting and stage budgeting.
 *
 * This module answers the question: "Given a target orbit (apogee/perigee)
 * and a payload mass, how much ΔV is needed, and can a specific stage
 * deliver its portion?"
 *
 * === THE VIS-VIVA EQUATION ===
 *
 * The cornerstone of Keplerian orbital mechanics:
 *
 *     v² = μ × (2/r - 1/a)
 *
 * This gives the speed of an object at any point in its orbit.
 *   - μ = GM = 3.986 × 10¹⁴ m³/s²  (Earth's gravitational parameter)
 *   - r = distance from Earth's centre (m)
 *   - a = semi-major axis of the orbit (m)
 *
 * For a **circular** orbit (a = r): v = √(μ/r)
 * For an **elliptical** transfer orbit: a = (r_perigee + r_apogee) / 2
 *
 * === HOHMANN TRANSFER BUDGET ===
 *
 * A Hohmann transfer is the minimum-energy two-impulse manoeuvre between
 * two co-planar circular orbits. Even when the target orbit is elliptical,
 * we decompose the problem into:
 *
 *   1. ΔV₁: Burn at perigee to enter the transfer ellipse
 *   2. ΔV₂: Burn at apogee to circularise (or achieve the final ellipse)
 *
 * In practice, a launch vehicle's ascent trajectory is NOT a pure Hohmann
 * transfer because:
 *   - The vehicle starts at rest on the ground (not in a circular orbit)
 *   - It must fight gravity the entire time (gravity losses)
 *   - It must punch through the atmosphere (drag losses)
 *
 * We estimate these losses empirically based on payload mass and target altitude.
 *
 * === TSIOLKOVSKY ROCKET EQUATION ===
 *
 * The ideal rocket equation determines how much ΔV a stage can deliver:
 *
 *     Δv = Isp × g₀ × ln(m₀ / m_f)
 *
 * where:
 *   - Isp = specific impulse (s)
 *   - g₀ = 9.80665 m/s²
 *   - m₀ = initial mass (propellant + structural + payload above)
 *   - m_f = final mass (structural + payload above)
 *
 * @author  Prakshep GNC Team
 * @version 2.0.0
 */

#include "prakshep/guidance/trajectory_math.hpp"
#include "prakshep/constants.hpp"
#include <cmath>
#include <algorithm>

namespace prakshep {
namespace guidance {

// ===========================================================================
//  Orbital velocity helpers
// ===========================================================================

double orbital_velocity(double altitude_km, double semi_major_axis_km) {
    // Convert to metres from Earth centre
    double r = (altitude_km * 1000.0) + constants::R_EARTH;      // m
    double a = (semi_major_axis_km * 1000.0) + constants::R_EARTH; // m — wait, SMA is already from centre

    // Actually: semi_major_axis_km is given as distance from Earth centre in km.
    // Let's be explicit: the caller passes SMA in km measured from Earth centre.
    a = semi_major_axis_km * 1000.0;

    // vis-viva: v² = μ × (2/r - 1/a)
    double v_squared = constants::GM * (2.0 / r - 1.0 / a);

    // Guard against negative (shouldn't happen for bound orbits but protect anyway)
    if (v_squared < 0.0) return 0.0;

    return std::sqrt(v_squared);
}

double circular_velocity(double altitude_km) {
    // r = altitude + Earth radius
    double r = (altitude_km * 1000.0) + constants::R_EARTH;

    // v_circ = √(μ / r)
    return std::sqrt(constants::GM / r);
}

// ===========================================================================
//  Delta-V budget calculator
// ===========================================================================

DeltaVBudget calculate_required_delta_v(double target_apogee_km,
                                        double target_perigee_km,
                                        double payload_mass_kg)
{
    DeltaVBudget budget{};

    // -----------------------------------------------------------------------
    //  Orbital radii (from Earth centre, in metres)
    // -----------------------------------------------------------------------

    double r_perigee = (target_perigee_km * 1000.0) + constants::R_EARTH;  // m
    double r_apogee  = (target_apogee_km  * 1000.0) + constants::R_EARTH;  // m

    // Semi-major axis of the target orbit
    double a_target = (r_perigee + r_apogee) / 2.0;  // m

    // -----------------------------------------------------------------------
    //  Velocity at perigee of the target orbit (vis-viva)
    //
    //  This is the velocity the vehicle must achieve at perigee altitude
    //  to naturally coast up to the target apogee.
    //
    //  v_perigee² = μ × (2/r_perigee - 1/a_target)
    // -----------------------------------------------------------------------

    double v_perigee = std::sqrt(
        constants::GM * (2.0 / r_perigee - 1.0 / a_target)
    );

    // -----------------------------------------------------------------------
    //  Velocity at apogee of the target orbit
    //
    //  v_apogee² = μ × (2/r_apogee - 1/a_target)
    // -----------------------------------------------------------------------

    double v_apogee = std::sqrt(
        constants::GM * (2.0 / r_apogee - 1.0 / a_target)
    );

    // -----------------------------------------------------------------------
    //  Circular velocity at the target apogee altitude
    //  (for circularisation burn, if the target orbit is circular)
    // -----------------------------------------------------------------------

    double v_circ_apogee = std::sqrt(constants::GM / r_apogee);

    // -----------------------------------------------------------------------
    //  ΔV breakdown
    //
    //  1. Ascent ΔV: The vehicle starts at rest and must achieve v_perigee
    //     at the injection altitude. This is roughly v_perigee minus the
    //     Earth's rotational velocity at the launch latitude.
    //
    //  2. Circularisation ΔV: If target is a circular orbit at apogee altitude,
    //     the vehicle must add (v_circ - v_apogee) at apogee.
    //     If target is already elliptical (perigee ≠ apogee), this is 0.
    // -----------------------------------------------------------------------

    // Earth's surface velocity at Sriharikota (13.73° N)
    double v_earth_rotation = constants::EARTH_OMEGA * constants::R_EARTH *
                              std::cos(constants::LAUNCH_LATITUDE * constants::DEG_TO_RAD);
    // ~410 m/s at 13.73° N

    // Ascent ΔV: achieve orbital velocity from rest (minus Earth rotation assist)
    budget.dv_ascent = v_perigee - v_earth_rotation;

    // Circularisation: only needed if target is circular (apogee ≈ perigee)
    if (std::abs(target_apogee_km - target_perigee_km) < 10.0) {
        // Circular orbit: no circularisation needed beyond the ascent
        budget.dv_circularisation = 0.0;
    } else {
        // Elliptical target: circularisation burn at apogee
        budget.dv_circularisation = std::abs(v_circ_apogee - v_apogee);
    }

    // -----------------------------------------------------------------------
    //  Loss estimates
    //
    //  Gravity losses: 1200–1800 m/s for typical LEO launches.
    //  Varies with thrust-to-weight ratio (higher T/W → lower gravity loss).
    //
    //  Drag losses: 50–200 m/s depending on vehicle shape and trajectory.
    //  Heavier payloads have proportionally lower drag losses (higher ballistic
    //  coefficient).
    // -----------------------------------------------------------------------

    // Empirical gravity loss model: ~1500 m/s for PSLV-class (T/W ≈ 1.3-1.5)
    // Scales slightly with orbit altitude (higher orbit → longer burn → more loss)
    budget.dv_losses_gravity = 1400.0 + (target_perigee_km / 1000.0) * 100.0;

    // Empirical drag loss: ~100 m/s for PSLV-class, lower for heavier payloads
    budget.dv_losses_drag = 120.0 - std::min(50.0, payload_mass_kg / 100.0);
    budget.dv_losses_drag = std::max(50.0, budget.dv_losses_drag);

    // -----------------------------------------------------------------------
    //  Total ΔV budget
    // -----------------------------------------------------------------------

    budget.dv_total = budget.dv_ascent
                    + budget.dv_circularisation
                    + budget.dv_losses_gravity
                    + budget.dv_losses_drag;

    return budget;
}

// ===========================================================================
//  Stage sufficiency evaluator
// ===========================================================================

StageSufficiency evaluate_stage_sufficiency(double isp_s,
                                            double propellant_mass_kg,
                                            double structural_mass_kg,
                                            double payload_above_kg,
                                            double required_dv_ms)
{
    StageSufficiency result{};

    // -----------------------------------------------------------------------
    //  Tsiolkovsky ideal rocket equation
    //
    //      Δv = Isp × g₀ × ln(m₀ / m_f)
    //
    //  m₀ = propellant + structural + payload above
    //  m_f = structural + payload above (propellant burned)
    // -----------------------------------------------------------------------

    double m_initial = propellant_mass_kg + structural_mass_kg + payload_above_kg;
    double m_final   = structural_mass_kg + payload_above_kg;

    // Guard: mass ratio must be > 1
    if (m_final <= 0.0 || m_initial <= m_final) {
        result.available_dv = 0.0;
        result.required_dv  = required_dv_ms;
        result.margin       = -required_dv_ms;
        result.sufficient   = false;
        result.burnout_velocity = 0.0;
        return result;
    }

    double mass_ratio  = m_initial / m_final;
    double exhaust_vel = isp_s * constants::G0;  // Effective exhaust velocity (m/s)

    result.available_dv     = exhaust_vel * std::log(mass_ratio);
    result.required_dv      = required_dv_ms;
    result.margin           = result.available_dv - required_dv_ms;
    result.sufficient       = result.margin >= 0.0;
    result.burnout_velocity = result.available_dv;  // Ideal (no losses)

    return result;
}

} // namespace guidance
} // namespace prakshep
