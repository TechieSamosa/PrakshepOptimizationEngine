/**
 * @file pslv_stage_1.cpp
 * @brief Implementation of the PSLV 1st Stage (PS1) — S139 Solid Motor with SITVC.
 *
 * This file contains the complete physics implementation for ISRO's S139 solid
 * rocket motor, the first core stage of the Polar Satellite Launch Vehicle.
 *
 * === SOLID MOTOR PHYSICS ===
 *
 * Unlike liquid engines, a solid motor CANNOT be throttled or shut down once
 * ignited. The thrust profile is entirely determined by the propellant grain
 * geometry — in the S139's case, a star-shaped internal bore pattern.
 *
 * The star grain burns radially outward. As the flame front regresses:
 *   - Initially: large exposed surface area → high thrust (ignition spike)
 *   - Mid-burn:  surface area stabilises → plateau thrust
 *   - Late burn: web burns through, area collapses → tail-off
 *
 * We model this with a pre-tabulated thrust multiplier curve sampled via
 * piecewise linear interpolation at each timestep.
 *
 * === SITVC (Secondary Injection Thrust Vector Control) ===
 *
 * The PS1 does NOT have a gimballed nozzle. Steering is accomplished by
 * injecting strontium perchlorate (SrClO4) solution through small ports
 * in the divergent section of the nozzle.
 *
 * The injected liquid vaporises in the supersonic exhaust flow and creates
 * an asymmetric oblique shock pattern. This shock deflects the exhaust
 * momentum, producing a side force that steers the vehicle.
 *
 * The deflection angle δ is modelled as:
 *
 *     δ = k_sitvc × (ṁ_injectant / ṁ_core)
 *
 * where:
 *   - k_sitvc ≈ 0.15 rad per unit flow ratio (empirical)
 *   - ṁ_injectant is the secondary injection mass flow rate (0 to MAX_INJECTION_FLOW)
 *   - ṁ_core is the primary propellant mass flow rate through the nozzle
 *
 * The SITVC system has a finite fluid reservoir (3400 kg of SrClO4 solution).
 * Once depleted, the PS1 loses all steering authority.
 *
 * === MASS DEPLETION ===
 *
 * The instantaneous mass flow rate is derived from:
 *
 *     ṁ = F(t) / (Isp(h) × g₀)
 *
 * where F(t) is the thrust at the current burn time and Isp(h) is the
 * altitude-corrected specific impulse.
 *
 * @author  Prakshep GNC Team
 * @version 2.0.0
 */

#include "prakshep/stages/pslv_stage_1.hpp"
#include "prakshep/constants.hpp"
#include <algorithm>
#include <cmath>

namespace prakshep {
namespace stages {

// ===========================================================================
//  Constructor
// ===========================================================================

PSLV_Stage1::PSLV_Stage1()
    : m_state(State::IDLE)
    , m_propellant_remaining(PROPELLANT_MASS)
    , m_burn_elapsed(0.0)
    , m_sitvc_fluid_remaining(SITVC_FLUID_MASS)
    , m_sitvc_deflection(0.0, 0.0, 0.0)
    , m_current_thrust_vector(0.0, 0.0, 0.0)
    , m_current_com_offset(0.0, 0.0, 0.0)
    , m_current_mass_flow_rate(0.0)
{
}

// ===========================================================================
//  Lifecycle transitions
// ===========================================================================

void PSLV_Stage1::ignite() {
    if (m_state == State::IDLE) {
        m_state = State::BURNING;
        m_burn_elapsed = 0.0;

        // Solid motors cannot be un-ignited. This is a one-way transition.
        // The S139 ignition is initiated by a pyrotechnic igniter at the
        // fore-end of the star-grain bore.
    }
}

void PSLV_Stage1::separate() {
    m_state = State::SEPARATED;
    m_current_thrust_vector = Vec3(0.0, 0.0, 0.0);
}

// ===========================================================================
//  Core physics update — called every integration step
// ===========================================================================

/**
 * @brief Advance the S139 solid motor physics by one timestep.
 *
 * This method performs the following sequence:
 *   1. Check if the stage is actively burning.
 *   2. Evaluate the thrust curve at the current normalised burn time.
 *   3. Interpolate Isp for the current altitude.
 *   4. Calculate instantaneous thrust and mass flow rate.
 *   5. Deplete propellant mass (RK4-consistent mass loss).
 *   6. Calculate the SITVC deflection from steering commands.
 *   7. Compose the final body-frame thrust vector.
 *   8. Update the centre-of-mass offset.
 */
void PSLV_Stage1::update(double dt,
                         double pitch_command,
                         double yaw_command,
                         double altitude)
{
    // If not burning, zero all outputs
    if (m_state != State::BURNING) {
        m_current_thrust_vector = Vec3(0.0, 0.0, 0.0);
        m_current_mass_flow_rate = 0.0;
        return;
    }

    // -----------------------------------------------------------------------
    //  Step 1: Evaluate burn progress
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
    //  Step 2: Sample the star-grain thrust curve
    // -----------------------------------------------------------------------

    // The thrust multiplier encodes the grain regression profile.
    // At normalised_time=0: ignition transient (0.6×)
    // At normalised_time≈0.02: ignition spike (1.12×)
    // At normalised_time≈0.2-0.6: plateau (~1.0×)
    // At normalised_time>0.9: tail-off → 0
    double thrust_multiplier = sample_thrust_curve(normalised_time);

    // -----------------------------------------------------------------------
    //  Step 3: Altitude-dependent Isp
    // -----------------------------------------------------------------------

    // As the vehicle ascends, ambient pressure drops and nozzle expansion
    // becomes more efficient. Isp transitions from sea-level to vacuum.
    double isp = interpolate_isp(altitude);

    // -----------------------------------------------------------------------
    //  Step 4: Instantaneous thrust and mass flow rate
    // -----------------------------------------------------------------------

    // Base thrust interpolated between sea-level and vacuum based on altitude.
    // The thrust_multiplier then shapes it according to the grain geometry.
    double pressure_ratio = std::exp(-altitude / 8500.0); // Scale height ~8.5 km
    double base_thrust = THRUST_SEA_LEVEL * pressure_ratio
                       + THRUST_VACUUM * (1.0 - pressure_ratio);
    double thrust_magnitude = base_thrust * thrust_multiplier;

    // Mass flow rate from the rocket equation: ṁ = F / (Isp × g₀)
    m_current_mass_flow_rate = thrust_magnitude / (isp * constants::G0);

    // -----------------------------------------------------------------------
    //  Step 5: Propellant mass depletion (RK4-consistent)
    //
    //  For a solid motor, mass depletion is deterministic — we simply
    //  integrate ṁ over the timestep. This is exact for a constant ṁ
    //  within each dt, which is valid for small timesteps (dt ≤ 0.1s).
    // -----------------------------------------------------------------------

    double mass_consumed = m_current_mass_flow_rate * dt;
    m_propellant_remaining = std::max(0.0, m_propellant_remaining - mass_consumed);

    // -----------------------------------------------------------------------
    //  Step 6: SITVC deflection
    // -----------------------------------------------------------------------

    // The flight computer sends normalised pitch/yaw commands in [-1, +1].
    // The SITVC system translates these into fluid injection rates.
    m_sitvc_deflection = calculateSITVC_Vector(pitch_command, yaw_command, dt);

    // -----------------------------------------------------------------------
    //  Step 7: Compose body-frame thrust vector
    //
    //  Convention:
    //    +X_body = axial (thrust direction, "up" when vertical)
    //    +Y_body = pitch axis
    //    +Z_body = yaw axis
    //
    //  An un-deflected motor produces thrust purely along +X_body.
    //  SITVC deflection tilts this vector in the Y-Z plane:
    //
    //    F_x = F × cos(δ_pitch) × cos(δ_yaw)
    //    F_y = F × sin(δ_pitch)
    //    F_z = F × sin(δ_yaw)
    //
    //  For small angles (δ < 2°): cos(δ) ≈ 1, sin(δ) ≈ δ
    //  We use the exact trig functions anyway for correctness.
    // -----------------------------------------------------------------------

    double delta_pitch = m_sitvc_deflection.y;
    double delta_yaw   = m_sitvc_deflection.z;

    m_current_thrust_vector = Vec3(
        thrust_magnitude * std::cos(delta_pitch) * std::cos(delta_yaw),
        thrust_magnitude * std::sin(delta_pitch),
        thrust_magnitude * std::sin(delta_yaw)
    );

    // -----------------------------------------------------------------------
    //  Step 8: Centre-of-mass update
    //
    //  The S139 star grain burns from the inside out (radially).
    //  As propellant is consumed, the CoM shifts aft (toward the nozzle).
    //
    //  Model: CoM starts at ~40% of stage length from the fore-end
    //  and migrates toward ~55% as the grain burns through.
    // -----------------------------------------------------------------------

    double burn_fraction = 1.0 - (m_propellant_remaining / PROPELLANT_MASS);
    double com_position = STAGE_LENGTH * (0.40 + 0.15 * burn_fraction);

    // Offset from the geometric centre of the stage (at STAGE_LENGTH/2)
    m_current_com_offset = Vec3(com_position - STAGE_LENGTH * 0.5, 0.0, 0.0);
}

// ===========================================================================
//  SITVC: Secondary Injection Thrust Vector Control
// ===========================================================================

Vec3 PSLV_Stage1::calculateSITVC_Vector(double pitch_cmd, double yaw_cmd, double dt)
{
    // -----------------------------------------------------------------------
    //  If SITVC fluid is exhausted, we have zero steering authority.
    //  The vehicle becomes ballistic during the remainder of the PS1 burn.
    // -----------------------------------------------------------------------
    if (m_sitvc_fluid_remaining <= 0.0) {
        return Vec3(0.0, 0.0, 0.0);
    }

    // -----------------------------------------------------------------------
    //  Clamp commands to [-1, +1]
    // -----------------------------------------------------------------------
    pitch_cmd = std::clamp(pitch_cmd, -1.0, 1.0);
    yaw_cmd   = std::clamp(yaw_cmd,   -1.0, 1.0);

    // -----------------------------------------------------------------------
    //  Calculate injection mass flow rates for each axis.
    //
    //  The SITVC system has four injection ports:
    //    - 2 for pitch control (upper/lower)
    //    - 2 for yaw control (left/right)
    //
    //  The command magnitude [0,1] maps linearly to injection flow rate.
    //  A positive pitch command injects on the "lower" port, deflecting
    //  the exhaust downward and thus pitching the vehicle up.
    // -----------------------------------------------------------------------

    double pitch_injection_flow = std::abs(pitch_cmd) * MAX_INJECTION_FLOW;  // kg/s
    double yaw_injection_flow   = std::abs(yaw_cmd)   * MAX_INJECTION_FLOW;  // kg/s
    double total_injection_flow = pitch_injection_flow + yaw_injection_flow;

    // -----------------------------------------------------------------------
    //  Consume SITVC fluid from the reservoir
    // -----------------------------------------------------------------------

    double fluid_consumed = total_injection_flow * dt;
    if (fluid_consumed > m_sitvc_fluid_remaining) {
        // Scale down the injection proportionally if we're running dry
        double scale = m_sitvc_fluid_remaining / fluid_consumed;
        pitch_injection_flow *= scale;
        yaw_injection_flow   *= scale;
        m_sitvc_fluid_remaining = 0.0;
    } else {
        m_sitvc_fluid_remaining -= fluid_consumed;
    }

    // -----------------------------------------------------------------------
    //  Calculate deflection angles.
    //
    //  The SITVC deflection angle δ is proportional to the ratio of the
    //  secondary injection mass flow rate to the primary core mass flow rate:
    //
    //      δ = k_sitvc × (ṁ_injection / ṁ_core)
    //
    //  This models the oblique shock interaction: more injected fluid
    //  creates a stronger shock, deflecting the exhaust stream further.
    //
    //  Safety clamp to MAX_DEFLECTION_ANGLE (~2°) to prevent unrealistic
    //  deflections at low core thrust (e.g. during tail-off).
    // -----------------------------------------------------------------------

    double delta_pitch = 0.0;
    double delta_yaw   = 0.0;

    if (m_current_mass_flow_rate > 1.0) {  // Guard: avoid division by near-zero
        delta_pitch = K_SITVC * (pitch_injection_flow / m_current_mass_flow_rate);
        delta_yaw   = K_SITVC * (yaw_injection_flow   / m_current_mass_flow_rate);

        // Apply sign from the command direction
        delta_pitch *= (pitch_cmd >= 0.0 ? 1.0 : -1.0);
        delta_yaw   *= (yaw_cmd   >= 0.0 ? 1.0 : -1.0);

        // Clamp to physical limits
        delta_pitch = std::clamp(delta_pitch, -MAX_DEFLECTION_ANGLE, MAX_DEFLECTION_ANGLE);
        delta_yaw   = std::clamp(delta_yaw,   -MAX_DEFLECTION_ANGLE, MAX_DEFLECTION_ANGLE);
    }

    return Vec3(0.0, delta_pitch, delta_yaw);
}

// ===========================================================================
//  Thrust curve interpolation
// ===========================================================================

double PSLV_Stage1::sample_thrust_curve(double normalised_time) const
{
    // Number of breakpoints in the thrust curve
    constexpr int N = sizeof(THRUST_CURVE_TIME) / sizeof(THRUST_CURVE_TIME[0]);

    // Clamp to [0, 1]
    normalised_time = std::clamp(normalised_time, 0.0, 1.0);

    // Edge cases
    if (normalised_time <= THRUST_CURVE_TIME[0])     return THRUST_CURVE_MULT[0];
    if (normalised_time >= THRUST_CURVE_TIME[N - 1]) return THRUST_CURVE_MULT[N - 1];

    // Find the bounding breakpoints and linearly interpolate
    for (int i = 0; i < N - 1; ++i) {
        if (normalised_time >= THRUST_CURVE_TIME[i] &&
            normalised_time <  THRUST_CURVE_TIME[i + 1])
        {
            double t0 = THRUST_CURVE_TIME[i];
            double t1 = THRUST_CURVE_TIME[i + 1];
            double m0 = THRUST_CURVE_MULT[i];
            double m1 = THRUST_CURVE_MULT[i + 1];

            // Linear interpolation factor
            double alpha = (normalised_time - t0) / (t1 - t0);
            return m0 + alpha * (m1 - m0);
        }
    }

    return 1.0; // Fallback (should never reach here)
}

// ===========================================================================
//  Isp altitude interpolation
// ===========================================================================

double PSLV_Stage1::interpolate_isp(double altitude) const
{
    // Exponential atmosphere model for Isp transition.
    //
    // At sea level (altitude=0): Isp = ISP_SEA_LEVEL (237 s)
    // At altitude → ∞:          Isp = ISP_VACUUM (269 s)
    //
    // The transition uses a scale height of 8500 m, matching the pressure
    // e-folding height of the troposphere. By ~25 km, Isp is within 5%
    // of vacuum. By ~40 km, it's essentially vacuum.

    double altitude_factor = 1.0 - std::exp(-altitude / 8500.0);
    return ISP_SEA_LEVEL + (ISP_VACUUM - ISP_SEA_LEVEL) * altitude_factor;
}

// ===========================================================================
//  Canonical output accessors
// ===========================================================================

double PSLV_Stage1::get_mass() const {
    return STRUCTURAL_MASS + m_propellant_remaining + m_sitvc_fluid_remaining;
}

Vec3 PSLV_Stage1::get_thrust_vector() const {
    return m_current_thrust_vector;
}

Vec3 PSLV_Stage1::get_com_offset() const {
    return m_current_com_offset;
}

RocketStage::State PSLV_Stage1::get_state() const {
    return m_state;
}

double PSLV_Stage1::get_propellant_fraction() const {
    return m_propellant_remaining / PROPELLANT_MASS;
}

std::string PSLV_Stage1::get_name() const {
    return "PS1 - S139 Solid Motor";
}

} // namespace stages
} // namespace prakshep
