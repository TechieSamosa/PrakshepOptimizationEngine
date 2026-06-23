/**
 * @file atmosphere.cpp
 * @brief Implementation of the US Standard Atmosphere 1976 model.
 *
 * The US Standard Atmosphere 1976 divides the atmosphere below 86 km
 * geometric (~84.852 km geopotential) into 7 layers, each characterised by
 * a constant temperature lapse rate.  Within a layer the temperature varies
 * linearly with geopotential altitude:
 *
 *     T(H) = T_base + λ · (H − H_base)
 *
 * Pressure is derived from the hydrostatic equation integrated over each
 * layer.  Two cases arise:
 *
 *   Gradient layer (λ ≠ 0):
 *       P = P_base · (T / T_base)^(−g₀ / (R · λ))
 *
 *   Isothermal layer (λ = 0):
 *       P = P_base · exp(−g₀ · (H − H_base) / (R · T_base))
 *
 * Density follows from the ideal gas law:
 *       ρ = P / (R · T)
 *
 * Speed of sound:
 *       a = √(γ · R · T)
 *
 * Above the model ceiling (84852 m geopotential) we switch to an exponential
 * density decay with a 6500 m scale height — a simple but reasonable
 * approximation for ascent-trajectory drag computations (aerodynamic loads
 * become negligible above ~80 km anyway).
 *
 * Reference:
 *   "US Standard Atmosphere, 1976", NOAA / NASA / USAF, October 1976.
 *
 * @author  Prakshep GNC Team
 * @version 1.0.0
 */

#include "prakshep/atmosphere.hpp"
#include "prakshep/constants.hpp"

#include <array>
#include <cmath>

namespace prakshep {
namespace atmosphere {

// ============================================================================
//  Layer table — US Standard Atmosphere 1976
// ============================================================================

/**
 * @struct Layer
 * @brief  Descriptor for one atmospheric layer: base altitude, base temperature,
 *         and temperature lapse rate.
 */
struct Layer {
    double h_base;     ///< Base geopotential altitude (m)
    double t_base;     ///< Base temperature at h_base (K)
    double lapse_rate; ///< Temperature lapse rate dT/dH (K/m), negative = cooling
};

/**
 * Seven layers from the surface to 84.852 km geopotential altitude.
 *
 * | Index | Name          | H_base (m) | T_base (K) | Lapse (K/m)  |
 * |-------|---------------|-----------|-----------|-------------|
 * |   0   | Troposphere   |     0     |  288.15   |  −0.0065    |
 * |   1   | Tropopause    | 11 000    |  216.65   |   0.0       |
 * |   2   | Stratosphere  | 20 000    |  216.65   |  +0.001     |
 * |   3   | Stratosphere  | 32 000    |  228.65   |  +0.0028    |
 * |   4   | Stratopause   | 47 000    |  270.65   |   0.0       |
 * |   5   | Mesosphere    | 51 000    |  270.65   |  −0.0028    |
 * |   6   | Mesosphere    | 71 000    |  214.65   |  −0.002     |
 */
static constexpr std::array<Layer, 7> LAYERS = {{
    {     0.0, 288.15, -0.0065 },   // Layer 0: Troposphere
    { 11000.0, 216.65,  0.0    },   // Layer 1: Tropopause (isothermal)
    { 20000.0, 216.65,  0.001  },   // Layer 2: Lower stratosphere
    { 32000.0, 228.65,  0.0028 },   // Layer 3: Upper stratosphere
    { 47000.0, 270.65,  0.0    },   // Layer 4: Stratopause (isothermal)
    { 51000.0, 270.65, -0.0028 },   // Layer 5: Lower mesosphere
    { 71000.0, 214.65, -0.002  }    // Layer 6: Upper mesosphere
}};

/** @brief Number of layers in the model. */
static constexpr int NUM_LAYERS = 7;

/** @brief Upper geopotential altitude limit of the layer model (m). */
static constexpr double H_UPPER = constants::ATMOSPHERE_UPPER_LIMIT; // 84852.0 m

/** @brief Scale height for exponential density decay above 84852 m (m). */
static constexpr double EXPONENT_SCALE_HEIGHT = 6500.0;

// ============================================================================
//  Geopotential altitude conversion
// ============================================================================

double geometric_to_geopotential(double z) {
    /*
     * Geopotential altitude accounts for the decrease of gravitational
     * acceleration with altitude.  The conversion is:
     *
     *   H = (R_geo · z) / (R_geo + z)
     *
     * where R_geo ≈ 6356766 m is the effective Earth radius chosen so
     * that the geopotential matches the geometric altitude at sea level.
     */
    return (constants::R_EARTH_GEOPOTENTIAL * z) /
           (constants::R_EARTH_GEOPOTENTIAL + z);
}

// ============================================================================
//  Main compute function
// ============================================================================

AtmosphereState compute(double geometric_altitude_m) {
    // Clamp negative altitudes to sea level (e.g. if rocket is on the pad
    // at a site slightly below the geoid — shouldn't happen at Sriharikota).
    if (geometric_altitude_m < 0.0) {
        geometric_altitude_m = 0.0;
    }

    // ---------------------------------------------------------------
    // Step 1: Convert geometric altitude to geopotential altitude.
    // ---------------------------------------------------------------
    double H = geometric_to_geopotential(geometric_altitude_m);

    // ---------------------------------------------------------------
    // Step 2: Pre-compute the base pressure at each layer boundary.
    //
    // We walk through the layers from the surface upward, applying
    // the hydrostatic equation at each boundary to obtain P_base[i].
    // This is necessary because the table only stores temperature
    // and lapse rate — pressure must be derived.
    // ---------------------------------------------------------------
    std::array<double, NUM_LAYERS> p_base;
    p_base[0] = constants::P0; // Sea-level standard pressure

    for (int i = 0; i < NUM_LAYERS - 1; ++i) {
        double dH = LAYERS[i + 1].h_base - LAYERS[i].h_base;
        double Tb = LAYERS[i].t_base;
        double lapse = LAYERS[i].lapse_rate;

        if (std::fabs(lapse) > 1e-10) {
            // Gradient layer: P = Pb * (T_top / Tb) ^ (-g0 / (R * lapse))
            double T_top = Tb + lapse * dH;
            double exponent = -constants::G0 / (constants::R_AIR * lapse);
            p_base[i + 1] = p_base[i] * std::pow(T_top / Tb, exponent);
        } else {
            // Isothermal layer: P = Pb * exp(-g0 * dH / (R * Tb))
            p_base[i + 1] = p_base[i] *
                            std::exp(-constants::G0 * dH / (constants::R_AIR * Tb));
        }
    }

    // ---------------------------------------------------------------
    // Step 3: Determine the layer containing the target altitude.
    // ---------------------------------------------------------------
    AtmosphereState state;

    if (H >= H_UPPER) {
        // ----- Above the model ceiling: exponential density decay -----
        //
        // Compute conditions at the top of the last modelled layer first,
        // then decay density exponentially.  Temperature and pressure are
        // set to the top-of-atmosphere values for consistency.

        // Conditions at the top of layer 6 (H = 84852 m)
        double lapse6  = LAYERS[6].lapse_rate;
        double Tb6     = LAYERS[6].t_base;
        double Hb6     = LAYERS[6].h_base;
        double dH_top  = H_UPPER - Hb6;
        double T_top   = Tb6 + lapse6 * dH_top;

        double P_top;
        if (std::fabs(lapse6) > 1e-10) {
            double exponent = -constants::G0 / (constants::R_AIR * lapse6);
            P_top = p_base[6] * std::pow(T_top / Tb6, exponent);
        } else {
            P_top = p_base[6] *
                    std::exp(-constants::G0 * dH_top / (constants::R_AIR * Tb6));
        }

        double rho_top = P_top / (constants::R_AIR * T_top);

        // Exponential decay above the ceiling
        double dH_above = H - H_UPPER;
        double rho = rho_top * std::exp(-dH_above / EXPONENT_SCALE_HEIGHT);

        // Use the top temperature for speed of sound (approximation)
        state.temperature    = T_top;
        state.pressure       = rho * constants::R_AIR * T_top; // Consistent with ideal gas
        state.density        = rho;
        state.speed_of_sound = std::sqrt(constants::GAMMA_AIR * constants::R_AIR * T_top);

        return state;
    }

    // ----- Within the 7-layer model -----

    // Find the layer containing H (search from top to bottom)
    int layer_idx = 0;
    for (int i = NUM_LAYERS - 1; i >= 0; --i) {
        if (H >= LAYERS[i].h_base) {
            layer_idx = i;
            break;
        }
    }

    double Tb    = LAYERS[layer_idx].t_base;
    double Hb    = LAYERS[layer_idx].h_base;
    double lapse = LAYERS[layer_idx].lapse_rate;
    double Pb    = p_base[layer_idx];
    double dH    = H - Hb;

    // ---------------------------------------------------------------
    // Step 4: Compute temperature and pressure within the layer.
    // ---------------------------------------------------------------
    double T, P;

    if (std::fabs(lapse) > 1e-10) {
        // --- Gradient layer ---
        //
        // Temperature:  T = Tb + λ·(H − Hb)
        // Pressure:     P = Pb · (T/Tb)^(−g₀/(R·λ))
        //
        // The exponent arises from integrating the hydrostatic equation
        // dp = −ρ·g·dH with ρ = P/(R·T) and T linear in H.
        T = Tb + lapse * dH;
        double exponent = -constants::G0 / (constants::R_AIR * lapse);
        P = Pb * std::pow(T / Tb, exponent);
    } else {
        // --- Isothermal layer ---
        //
        // Temperature is constant:  T = Tb
        // Pressure decays exponentially: P = Pb · exp(−g₀·dH/(R·Tb))
        T = Tb;
        P = Pb * std::exp(-constants::G0 * dH / (constants::R_AIR * Tb));
    }

    // ---------------------------------------------------------------
    // Step 5: Derive density from the ideal gas law:  ρ = P / (R·T)
    // ---------------------------------------------------------------
    double rho = P / (constants::R_AIR * T);

    // ---------------------------------------------------------------
    // Step 6: Speed of sound:  a = √(γ · R · T)
    //
    // This assumes air behaves as a calorically perfect gas with
    // constant γ = 1.4 (valid below ~80 km).
    // ---------------------------------------------------------------
    double a = std::sqrt(constants::GAMMA_AIR * constants::R_AIR * T);

    // ---------------------------------------------------------------
    // Pack results
    // ---------------------------------------------------------------
    state.temperature    = T;
    state.pressure       = P;
    state.density        = rho;
    state.speed_of_sound = a;

    return state;
}

} // namespace atmosphere
} // namespace prakshep
