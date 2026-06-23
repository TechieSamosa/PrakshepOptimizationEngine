/**
 * @file aerodynamics.cpp
 * @brief Implementation of aerodynamic force computations.
 *
 * == Drag Model Overview ==
 *
 * Aerodynamic drag is the dominant external disturbance during the atmospheric
 * phase of ascent (roughly 0–80 km altitude).  The drag force magnitude is:
 *
 *     F_drag = q · Cd · A_ref
 *
 * where:
 *     q     = ½ ρ v²_rel        (dynamic pressure, Pa)
 *     Cd    = f(Mach)            (drag coefficient, dimensionless)
 *     A_ref                      (reference cross-sectional area, m²)
 *     v_rel                      (velocity relative to the co-rotating atmosphere)
 *
 * The drag force acts opposite to the velocity vector relative to the
 * atmosphere (not the inertial velocity).  The atmosphere co-rotates with
 * the Earth, so we subtract ω_E × r from the inertial velocity to get
 * the airspeed.
 *
 * == Cd(Mach) Interpolation ==
 *
 * The drag coefficient varies significantly with Mach number.  A typical
 * launch vehicle sees:
 *   - Subsonic Cd ≈ 0.2–0.3  (streamlined body)
 *   - Transonic peak near Mach 1.0–1.2, Cd ≈ 0.4–0.6
 *   - Supersonic Cd drops, stabilising around 0.2 by Mach 3+
 *
 * We store this as a piecewise-linear table in RocketConfig and interpolate.
 *
 * @author  Prakshep GNC Team
 * @version 1.0.0
 */

#include "prakshep/aerodynamics.hpp"
#include "prakshep/constants.hpp"

#include <cmath>

namespace prakshep {
namespace aerodynamics {

// ============================================================================
//  Cd(Mach) lookup — piecewise linear interpolation
// ============================================================================

double get_drag_coefficient(double mach, const RocketConfig& config) {
    const auto& mb = config.mach_breakpoints;
    const auto& cd = config.cd_values;

    // ---------------------------------------------------------------
    // Guard: if the table is empty or has mismatched sizes, return a
    // conservative default.  This should never happen with a correctly
    // configured vehicle.
    // ---------------------------------------------------------------
    if (mb.empty() || cd.empty() || mb.size() != cd.size()) {
        return 0.3; // Fallback — typical subsonic Cd
    }

    // ---------------------------------------------------------------
    // Below the first breakpoint: clamp to the first Cd value.
    // ---------------------------------------------------------------
    if (mach <= mb.front()) {
        return cd.front();
    }

    // ---------------------------------------------------------------
    // Above the last breakpoint: clamp to the last Cd value.
    // ---------------------------------------------------------------
    if (mach >= mb.back()) {
        return cd.back();
    }

    // ---------------------------------------------------------------
    // Find the bounding breakpoints and linearly interpolate.
    //
    // For a small table (typically 8–15 entries) a linear search is
    // faster than a binary search due to cache locality.
    // ---------------------------------------------------------------
    for (size_t i = 0; i < mb.size() - 1; ++i) {
        if (mach >= mb[i] && mach <= mb[i + 1]) {
            // Compute interpolation fraction t ∈ [0, 1]
            double t = (mach - mb[i]) / (mb[i + 1] - mb[i]);
            // Linear blend: Cd = Cd_i + t · (Cd_{i+1} − Cd_i)
            return cd[i] + t * (cd[i + 1] - cd[i]);
        }
    }

    // Should be unreachable, but return last value as safety
    return cd.back();
}

// ============================================================================
//  Drag force computation
// ============================================================================

Vec3 compute_drag(const StateVector& state, const RocketConfig& config,
                  const AtmosphereState& atm) {
    // ---------------------------------------------------------------
    // Step 1: Compute the atmospheric velocity at the vehicle's position.
    //
    // The atmosphere co-rotates with the Earth.  At position r in ECI,
    // the local atmosphere velocity is:
    //
    //     v_atm = ω_E × r
    //
    // where ω_E = (0, 0, Ω_E) is Earth's angular velocity vector
    // aligned with the ECI Z-axis (true pole).
    //
    // Cross product:  (0, 0, Ω) × (x, y, z) = (−Ω·y, Ω·x, 0)
    // ---------------------------------------------------------------
    Vec3 omega_cross_r(
        -constants::EARTH_OMEGA * state.position.y,
         constants::EARTH_OMEGA * state.position.x,
         0.0
    );

    // ---------------------------------------------------------------
    // Step 2: Velocity relative to the atmosphere.
    //
    //     v_rel = v_inertial − v_atmosphere
    //
    // This is the airspeed vector that determines aerodynamic loads.
    // ---------------------------------------------------------------
    Vec3 v_rel = state.velocity - omega_cross_r;
    double v_rel_mag = v_rel.norm();

    // ---------------------------------------------------------------
    // Step 3: Check for negligible airspeed.
    //
    // At very low speeds (e.g. on the pad before liftoff or in the
    // near-vacuum above 80 km) the drag is effectively zero.  We skip
    // the computation to avoid division by zero.
    // ---------------------------------------------------------------
    if (v_rel_mag < 1e-6) {
        return {0.0, 0.0, 0.0};
    }

    // ---------------------------------------------------------------
    // Step 4: Local Mach number and drag coefficient.
    //
    //     Mach = v_rel / a
    //
    // where a is the local speed of sound from the atmosphere model.
    // ---------------------------------------------------------------
    double mach = v_rel_mag / atm.speed_of_sound;
    double Cd = get_drag_coefficient(mach, config);

    // ---------------------------------------------------------------
    // Step 5: Dynamic pressure and drag magnitude.
    //
    //     q      = ½ ρ v²_rel
    //     F_drag = q · Cd · A_ref
    //
    // Dynamic pressure is the key structural load parameter.  Maximum
    // dynamic pressure (Max-Q) occurs around 60–80 seconds into flight
    // for most launch vehicles, at ~11–14 km altitude.
    // ---------------------------------------------------------------
    double q = 0.5 * atm.density * v_rel_mag * v_rel_mag;
    double drag_mag = q * Cd * config.reference_area;

    // ---------------------------------------------------------------
    // Step 6: Drag force vector (opposing the relative velocity).
    //
    //     F⃗_drag = −F_drag · (v_rel / |v_rel|)
    //
    // The negative sign ensures drag always decelerates the vehicle
    // relative to the atmosphere.
    // 
    // For reverse atmospheric entry (retrograde motion), v_rel handles the 
    // inversion automatically since drag directly opposes the velocity vector.
    // ---------------------------------------------------------------
    return v_rel * (-drag_mag / v_rel_mag);
}

// ============================================================================
//  Grid Fin Aerodynamics (Reverse Entry Control)
// ============================================================================

Vec3 compute_grid_fin_force(const StateVector& state, const AtmosphereState& atm, 
                            const GridFinConfig& grid_fins, double angle_of_attack) {
    if (!grid_fins.deployed || grid_fins.area <= 0.0) {
        return {0.0, 0.0, 0.0};
    }

    Vec3 omega_cross_r(
        -constants::EARTH_OMEGA * state.position.y,
         constants::EARTH_OMEGA * state.position.x,
         0.0
    );

    Vec3 v_rel = state.velocity - omega_cross_r;
    double v_rel_mag = v_rel.norm();

    if (v_rel_mag < 1.0) {
        return {0.0, 0.0, 0.0};
    }

    double mach = v_rel_mag / atm.speed_of_sound;
    double q = 0.5 * atm.density * v_rel_mag * v_rel_mag;

    // Simplified Grid Fin Aerodynamic model
    // Supersonic grid fins have massive wave drag, but generate lift based on angle of attack.
    // Cd peaks strongly at transonic speeds (Mach 1.0 - 1.3)
    double base_cd = (mach > 0.8 && mach < 1.5) ? 1.5 : (mach >= 1.5 ? 0.8 : 0.4);
    
    // Induced drag and lift from angle of attack
    double lift_coefficient = 2.0 * M_PI * angle_of_attack; // Thin airfoil theory approx
    if (mach > 1.0) {
        lift_coefficient = 4.0 * angle_of_attack / std::sqrt(mach * mach - 1.0); // Supersonic linear theory
    }
    
    double drag_coefficient = base_cd + std::abs(angle_of_attack) * 0.5;

    double lift_mag = q * lift_coefficient * grid_fins.area;
    double drag_mag = q * drag_coefficient * grid_fins.area;

    Vec3 drag_vec = v_rel * (-drag_mag / v_rel_mag);
    
    // Lift acts perpendicular to velocity. We approximate by mapping it to body-local transverse.
    // In a full 6-DOF, we'd use the attitude quaternion to compute the lift vector direction.
    // Here we provide the magnitude along the appropriate local axis.
    // We assume the flight computer handles roll to orient the lift vector.
    // For now, we apply lift vertically relative to the velocity vector to simulate pitch control.
    Vec3 lift_dir = Vec3(0, 0, 1).cross(v_rel).normalized(); // Simplified orthogonal vector
    if (lift_dir.norm_squared() < 0.1) {
        lift_dir = Vec3(1, 0, 0);
    }
    Vec3 lift_vec = lift_dir * lift_mag;

    return drag_vec + lift_vec;
}

// ============================================================================
//  Dynamic pressure utility
// ============================================================================

double compute_dynamic_pressure(double density, double velocity_mag) {
    /*
     * Dynamic pressure:  q = ½ ρ v²
     *
     * This is a fundamental aerodynamic quantity:
     *   - Determines aerodynamic loads on the vehicle structure
     *   - Throttle-back may be required near Max-Q to limit structural loads
     *   - Used by the flight computer to schedule pitch programme manoeuvres
     */
    return 0.5 * density * velocity_mag * velocity_mag;
}

} // namespace aerodynamics
} // namespace prakshep
