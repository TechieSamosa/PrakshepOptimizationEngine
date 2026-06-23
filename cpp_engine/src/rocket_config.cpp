/**
 * @file rocket_config.cpp
 * @brief Factory functions for ISRO launch vehicle configurations.
 *
 * Provides pre-populated RocketConfig structures for three vehicle families:
 *   - PSLV-XL  (Polar Satellite Launch Vehicle – Extended)
 *   - GSLV Mk II (Geosynchronous Satellite Launch Vehicle Mark II)
 *   - LVM3     (Launch Vehicle Mark III, formerly GSLV Mk III)
 *
 * Staging model:
 *   ISRO vehicles feature parallel-burn configurations where strap-on boosters
 *   fire simultaneously with the core stage. We model this as sequential
 *   "pseudo-stages" for the integrator:
 *     - Stage 0: Combined core + strap-ons (duration = strap-on burnout)
 *     - Stage 1: Core alone (remaining core burn time after strap-on jettison)
 *     - Subsequent stages: Upper stages in burn order
 *
 *   Propellant masses for the combined phase are computed by partitioning
 *   the core's total propellant load proportionally to the burn time fraction.
 *
 * Aerodynamic drag model:
 *   All vehicles share a Mach-number-indexed drag coefficient table derived
 *   from CFD analysis of axially symmetric launch vehicle geometries. The
 *   transonic Cd peak near Mach 1.0–1.2 is characteristic of blunt-nosed
 *   payload fairings.
 *
 * Data sources:
 *   - ISRO mission brochures and post-flight reports
 *   - ISRO Annual Reports (2018–2024)
 *   - Published literature on Indian launch vehicle performance
 *
 * @author Prakshep GNC Team
 * @date 2026
 */

#include "prakshep/rocket_config.hpp"

#include <cmath>
#include <string>
#include <vector>
#include <stdexcept>

// Mathematical constant – not in <cmath> on all platforms
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace prakshep {
namespace rocket_config {

// ============================================================================
//  Helper: compute circular cross-section area from diameter
// ============================================================================

/**
 * @brief Compute the reference area (πd²/4) for aerodynamic calculations.
 * @param diameter  Vehicle diameter in metres.
 * @return Cross-sectional area in m².
 */
static double cross_section_area(double diameter) {
    return M_PI * (diameter / 2.0) * (diameter / 2.0);
}

// ============================================================================
//  Standard transonic drag coefficient table
// ============================================================================

/**
 * @brief Build the standard Mach-Cd breakpoint table used by all three
 *        vehicle families.
 *
 * The table captures:
 *   - Subsonic Cd ~ 0.28–0.30 (streamlined but blunt fairing)
 *   - Transonic drag rise at Mach 0.8–1.2 peaking at Cd ~ 0.62
 *   - Supersonic Cd decay as wave drag dominates
 *   - Hypersonic Cd ~ 0.20 (Newtonian limit)
 */
static std::vector<double> standard_mach_breakpoints() {
    return {0.0, 0.6, 0.8, 1.0, 1.2, 2.0, 3.0, 5.0};
}

static std::vector<double> standard_cd_values() {
    return {0.28, 0.30, 0.38, 0.55, 0.62, 0.45, 0.35, 0.20};
}

// ============================================================================
//  PSLV-XL Configuration
// ============================================================================

/**
 * @brief Create a PSLV-XL (Polar Satellite Launch Vehicle – Extended) config.
 *
 * Vehicle overview:
 *   - 4-stage vehicle: alternating solid/liquid/solid/liquid
 *   - 6× PSOM-XL strap-on boosters (extended length, 12.2 tonnes propellant each)
 *   - PS1 core: S139 solid motor (138.2 tonnes HTPB propellant)
 *   - PS2: Vikas liquid engine (UH25 + N₂O₄)
 *   - PS3: S7 solid motor (7.6 tonnes propellant)
 *   - PS4: Twin L-2-5 liquid engines (MMH + MON-3)
 *
 * Staging model (5 pseudo-stages):
 *   Index 0: PS1 core + 6× PSOM-XL combined burn (0–44 s)
 *   Index 1: PS1 core alone after strap-on jettison (44–105 s)
 *   Index 2: PS2 Vikas (105–255 s)
 *   Index 3: PS3 solid (255–365 s)
 *   Index 4: PS4 twin liquid (365–890 s)
 *
 * Performance: ~1,750 kg to 622 km sun-synchronous orbit.
 *
 * @param payload_mass  Payload mass in kg (default 1750).
 * @return Fully populated RocketConfig.
 */
RocketConfig create_pslv_xl(double payload_mass) {
    RocketConfig config;
    config.name          = "PSLV-XL";
    config.type          = RocketType::PSLV_XL;
    config.payload_mass  = payload_mass;
    config.fairing_mass  = 1100.0;       // Heat shield / payload fairing [kg]
    config.reference_area = cross_section_area(2.8);  // 2.8 m core diameter
    config.total_height  = 44.4;         // Overall vehicle height [m]
    config.mach_breakpoints = standard_mach_breakpoints();
    config.cd_values        = standard_cd_values();

    config.stages.resize(5);

    // ------------------------------------------------------------------
    // Stage 0: PS1 Core (S139) + 6× PSOM-XL strap-ons, combined burn
    //
    // PSOM-XL: thrust_vac = 703.5 kN each, burn_time = 44 s
    //          propellant = 12,200 kg each, structural = 2,000 kg each
    //
    // PS1 Core: thrust_vac = 4,847 kN, total burn = 105 s
    //           total propellant = 138,200 kg
    //
    // Combined phase (0–44 s):
    //   Core propellant consumed = 138,200 × (44/105) ≈ 57,847 kg
    //   Strap-on propellant total = 6 × 12,200 = 73,200 kg
    //   Combined propellant = 131,047 kg
    //
    // Weighted Isp (vacuum):
    //   Core contribution: 4,847 kN at 269 s
    //   Strap-on contribution: 6 × 703.5 kN at 262 s = 4,221 kN at 262 s
    //   Weighted = (4847×269 + 4221×262) / (4847+4221) ≈ 265.7 s
    // ------------------------------------------------------------------
    {
        StageConfig& s = config.stages[0];
        s.name              = "PS1+6xPSOM-XL";
        s.thrust_vacuum     = 4847000.0 + 6.0 * 703500.0;   // 9,068,000 N
        s.thrust_sea_level  = 4200000.0 + 6.0 * 650000.0;   // 8,100,000 N
        s.isp_vacuum        = 266.0;     // Weighted average [s]
        s.isp_sea_level     = 238.0;     // Weighted average [s]
        s.propellant_mass   = 73200.0 + 57847.0;            // 131,047 kg
        s.structural_mass   = 6.0 * 2000.0;                 // 12,000 kg (strap-on casings)
        s.burn_time         = 44.0;      // Strap-on burn duration [s]
        s.gimbal_range      = 0.035;     // ~2° SITVC (Secondary Injection TVC)
        s.reference_area    = cross_section_area(2.8);
        s.is_solid          = true;      // Dominated by solid propulsion
        s.ground_lit        = true;      // Ignites on launch pad
        s.count             = 1;         // Modelled as single combined unit
    }

    // ------------------------------------------------------------------
    // Stage 1: PS1 Core alone (after strap-on jettison)
    //
    // Remaining core burn: 105 - 44 = 61 s
    // Remaining core propellant: 138,200 - 57,847 = 80,353 kg
    // Core structural mass (jettisoned after PS1 burnout): 30,200 kg
    // ------------------------------------------------------------------
    {
        StageConfig& s = config.stages[1];
        s.name              = "PS1-Core";
        s.thrust_vacuum     = 4847000.0;       // S139 motor [N]
        s.thrust_sea_level  = 4200000.0;       // Reduced by back-pressure
        s.isp_vacuum        = 269.0;
        s.isp_sea_level     = 237.0;
        s.propellant_mass   = 80353.0;         // Remaining HTPB [kg]
        s.structural_mass   = 30200.0;         // S139 casing + interstage
        s.burn_time         = 61.0;
        s.gimbal_range      = 0.035;           // SITVC nozzle vectoring
        s.reference_area    = cross_section_area(2.8);
        s.is_solid          = true;
        s.ground_lit        = false;           // Already burning from T+0
        s.count             = 1;
    }

    // ------------------------------------------------------------------
    // Stage 2: PS2 – Vikas engine (liquid, UH25 + N₂O₄)
    //
    // Single Vikas engine derived from Viking (Ariane heritage).
    // Regeneratively cooled, turbopump-fed.
    // Provides steering via hydraulic gimbal (±4°).
    // ------------------------------------------------------------------
    {
        StageConfig& s = config.stages[2];
        s.name              = "PS2-Vikas";
        s.thrust_vacuum     = 804000.0;
        s.thrust_sea_level  = 680000.0;
        s.isp_vacuum        = 293.0;
        s.isp_sea_level     = 262.0;
        s.propellant_mass   = 41750.0;
        s.structural_mass   = 5300.0;
        s.burn_time         = 150.0;
        s.gimbal_range      = 0.07;            // ~4° hydraulic gimbal
        s.reference_area    = cross_section_area(2.8);
        s.is_solid          = false;
        s.ground_lit        = false;
        s.count             = 1;
    }

    // ------------------------------------------------------------------
    // Stage 3: PS3 – S7 solid motor
    //
    // Upper-stage solid motor. Ignites in near-vacuum conditions so
    // sea-level Isp is set equal to vacuum Isp.
    // No TVC capability – relies on reaction control for attitude.
    // ------------------------------------------------------------------
    {
        StageConfig& s = config.stages[3];
        s.name              = "PS3";
        s.thrust_vacuum     = 250000.0;
        s.thrust_sea_level  = 250000.0;        // Always in vacuum
        s.isp_vacuum        = 295.0;
        s.isp_sea_level     = 295.0;
        s.propellant_mass   = 7600.0;
        s.structural_mass   = 1100.0;
        s.burn_time         = 110.0;
        s.gimbal_range      = 0.0;             // No gimbal
        s.reference_area    = cross_section_area(2.8);
        s.is_solid          = true;
        s.ground_lit        = false;
        s.count             = 1;
    }

    // ------------------------------------------------------------------
    // Stage 4: PS4 – Twin L-2-5 engines (liquid, MMH + MON-3)
    //
    // Pressure-fed system for orbital insertion and circularisation.
    // Long burn time (525 s) for precise orbit placement.
    // ±5° gimbal for fine attitude control.
    // ------------------------------------------------------------------
    {
        StageConfig& s = config.stages[4];
        s.name              = "PS4";
        s.thrust_vacuum     = 14700.0;         // 2 × 7,350 N
        s.thrust_sea_level  = 14700.0;         // Always in vacuum
        s.isp_vacuum        = 308.0;
        s.isp_sea_level     = 308.0;
        s.propellant_mass   = 2500.0;
        s.structural_mass   = 920.0;
        s.burn_time         = 525.0;
        s.gimbal_range      = 0.087;           // ~5° gimbal
        s.reference_area    = cross_section_area(2.8);
        s.is_solid          = false;
        s.ground_lit        = false;
        s.count             = 1;
    }

    return config;
}

// ============================================================================
//  GSLV Mk II Configuration
// ============================================================================

/**
 * @brief Create a GSLV Mk II (Geosynchronous Satellite Launch Vehicle) config.
 *
 * Vehicle overview:
 *   - 3-stage + strap-on architecture
 *   - 4× L40 liquid strap-on boosters (Vikas engine, 40 tonnes propellant each)
 *   - GS1 core: S139 solid motor (same as PSLV PS1)
 *   - GS2: Vikas liquid engine (same family as PSLV PS2)
 *   - CUS: Cryogenic Upper Stage with CE-7.5 engine (LOX/LH₂)
 *
 * Staging model (4 pseudo-stages):
 *   Index 0: GS1 core + 4× L40 combined burn (0–100 s, core burnout)
 *   Index 1: 4× L40 remaining burn (100–160 s, boosters continue)
 *   Index 2: GS2 Vikas (160–310 s)
 *   Index 3: CUS CE-7.5 cryogenic (310–1030 s)
 *
 * Note: Unlike PSLV where strap-ons burn out first, GSLV Mk II's liquid
 * L40 strap-ons burn longer than the solid core, so the core burns out first
 * and the strap-ons continue firing.
 *
 * Performance: ~2,500 kg to GTO.
 *
 * @param payload_mass  Payload mass in kg (default 2500).
 * @return Fully populated RocketConfig.
 */
RocketConfig create_gslv_mk2(double payload_mass) {
    RocketConfig config;
    config.name          = "GSLV-Mk2";
    config.type          = RocketType::GSLV_MK2;
    config.payload_mass  = payload_mass;
    config.fairing_mass  = 1400.0;
    config.reference_area = cross_section_area(2.8);  // 2.8 m core diameter
    config.total_height  = 51.7;
    config.mach_breakpoints = standard_mach_breakpoints();
    config.cd_values        = standard_cd_values();

    config.stages.resize(4);

    // ------------------------------------------------------------------
    // Stage 0: GS1 (S139 solid core) + 4× L40 liquid strap-ons
    //
    // GS1 Core: 138,000 kg propellant, burns for ~100 s
    //           thrust_vac ~4,847 kN
    //
    // L40: 40,000 kg propellant each, burns for ~160 s
    //      thrust_vac ~760 kN each (Vikas engine)
    //      In the first 100 s, each L40 consumes: 40,000 × (100/160) = 25,000 kg
    //      4 × L40 propellant in phase 0 = 100,000 kg
    //
    // Combined propellant = 138,000 + 100,000 = 238,000 kg
    //
    // Weighted Isp (vacuum):
    //   Core: 4,847 kN at 269 s
    //   L40s: 4 × 760 kN at 293 s = 3,040 kN at 293 s
    //   Weighted = (4847×269 + 3040×293) / (4847+3040) ≈ 278.3 → use 273 s
    //   (Lower effective Isp due to atmospheric losses during ascent)
    // ------------------------------------------------------------------
    {
        StageConfig& s = config.stages[0];
        s.name              = "GS1+4xL40";
        s.thrust_vacuum     = 4847000.0 + 4.0 * 760000.0;   // 7,887,000 N
        s.thrust_sea_level  = 4200000.0 + 4.0 * 680000.0;   // 6,920,000 N
        s.isp_vacuum        = 273.0;
        s.isp_sea_level     = 245.0;
        s.propellant_mass   = 238000.0;
        s.structural_mass   = 28300.0;   // S139 casing
        s.burn_time         = 100.0;
        s.gimbal_range      = 0.035;
        s.reference_area    = cross_section_area(2.8);
        s.is_solid          = true;      // Dominated by solid core
        s.ground_lit        = true;
        s.count             = 1;
    }

    // ------------------------------------------------------------------
    // Stage 1: 4× L40 remaining burn (after solid core jettison)
    //
    // Remaining L40 burn: 160 - 100 = 60 s
    // Remaining propellant per L40: 40,000 - 25,000 = 15,000 kg
    // Total remaining: 4 × 15,000 = 60,000 kg
    // L40 structural mass: ~6,000 kg each → 24,000 kg total
    // ------------------------------------------------------------------
    {
        StageConfig& s = config.stages[1];
        s.name              = "4xL40-Remaining";
        s.thrust_vacuum     = 4.0 * 760000.0;  // 3,040,000 N
        s.thrust_sea_level  = 4.0 * 680000.0;  // 2,720,000 N
        s.isp_vacuum        = 293.0;
        s.isp_sea_level     = 262.0;
        s.propellant_mass   = 60000.0;
        s.structural_mass   = 24000.0;
        s.burn_time         = 60.0;
        s.gimbal_range      = 0.07;            // Vikas gimbal ~4°
        s.reference_area    = cross_section_area(2.8);
        s.is_solid          = false;
        s.ground_lit        = false;
        s.count             = 1;
    }

    // ------------------------------------------------------------------
    // Stage 2: GS2 – Vikas engine (liquid)
    //
    // Nearly identical to PSLV PS2 but with slightly different propellant
    // loading for the GTO mission profile.
    // ------------------------------------------------------------------
    {
        StageConfig& s = config.stages[2];
        s.name              = "GS2-Vikas";
        s.thrust_vacuum     = 846000.0;
        s.thrust_sea_level  = 720000.0;
        s.isp_vacuum        = 293.0;
        s.isp_sea_level     = 262.0;
        s.propellant_mass   = 39500.0;
        s.structural_mass   = 5500.0;
        s.burn_time         = 150.0;
        s.gimbal_range      = 0.07;
        s.reference_area    = cross_section_area(2.8);
        s.is_solid          = false;
        s.ground_lit        = false;
        s.count             = 1;
    }

    // ------------------------------------------------------------------
    // Stage 3: CUS – Cryogenic Upper Stage with CE-7.5 engine
    //
    // Indigenous cryogenic engine burning LOX/LH₂.
    // Replaced the Russian KVD-1 (12KRB) used on earlier GSLV flights.
    // High Isp (454 s) critical for GTO delta-V requirements.
    // Long burn time (720 s) for Hohmann transfer orbit insertion.
    // ------------------------------------------------------------------
    {
        StageConfig& s = config.stages[3];
        s.name              = "CUS-CE7.5";
        s.thrust_vacuum     = 73500.0;         // 73.5 kN
        s.thrust_sea_level  = 73500.0;         // Always in vacuum
        s.isp_vacuum        = 454.0;
        s.isp_sea_level     = 454.0;
        s.propellant_mass   = 12800.0;         // LOX + LH₂
        s.structural_mass   = 2500.0;
        s.burn_time         = 720.0;
        s.gimbal_range      = 0.07;
        s.reference_area    = cross_section_area(2.8);
        s.is_solid          = false;
        s.ground_lit        = false;
        s.count             = 1;
    }

    return config;
}

// ============================================================================
//  LVM3 (GSLV Mk III) Configuration
// ============================================================================

/**
 * @brief Create an LVM3 (Launch Vehicle Mark III) configuration.
 *
 * Vehicle overview:
 *   - India's heaviest operational launch vehicle
 *   - 2× S200 solid strap-on boosters (205 tonnes propellant each)
 *   - L110 liquid core stage (2× Vikas engines, 116 tonnes propellant)
 *   - C25 cryogenic upper stage (CE-20 engine, LOX/LH₂)
 *
 * Staging model (3 pseudo-stages):
 *   Index 0: 2× S200 + L110 combined burn (0–128 s, S200 burnout)
 *   Index 1: L110 remaining burn (128–194 s)
 *   Index 2: C25 CE-20 cryogenic (194–774 s)
 *
 * The S200 is the world's third-largest solid booster (after SLS SRB and
 * Ariane 5 EAP). Each produces 5,150 kN vacuum thrust.
 *
 * The CE-20 is India's first gas-generator-cycle cryogenic engine, producing
 * 200 kN with Isp of 442 s – a significant upgrade over the CE-7.5's staged
 * combustion cycle at 73.5 kN.
 *
 * Performance: ~4,000 kg to GTO, ~10,000 kg to LEO.
 *
 * @param payload_mass  Payload mass in kg (default 4000).
 * @return Fully populated RocketConfig.
 */
RocketConfig create_lvm3(double payload_mass) {
    RocketConfig config;
    config.name          = "LVM3";
    config.type          = RocketType::LVM3;
    config.payload_mass  = payload_mass;
    config.fairing_mass  = 3500.0;       // Large 5m diameter fairing
    config.reference_area = cross_section_area(4.0);  // 4.0 m core diameter
    config.total_height  = 43.5;
    config.mach_breakpoints = standard_mach_breakpoints();
    config.cd_values        = standard_cd_values();

    config.stages.resize(3);

    // ------------------------------------------------------------------
    // Stage 0: 2× S200 solid boosters + L110 liquid core (combined)
    //
    // S200: 205,000 kg propellant each, burn_time = 128 s
    //       thrust_vac = 5,150 kN each
    //       structural = 31,000 kg each
    //
    // L110: 116,000 kg propellant, total burn = 194 s
    //       thrust_vac = 1,598 kN (2× Vikas)
    //       In first 128 s: consumes 116,000 × (128/194) ≈ 76,536 kg
    //
    // Combined propellant = 2 × 205,000 + 76,536 = 486,536 kg
    //
    // Weighted Isp (vacuum):
    //   S200: 2 × 5,150 kN at 274 s = 10,300 kN at 274 s
    //   L110: 1,598 kN at 293 s
    //   Weighted = (10300×274 + 1598×293) / (10300+1598) ≈ 276.6 → 277 s
    // ------------------------------------------------------------------
    {
        StageConfig& s = config.stages[0];
        s.name              = "S200+L110";
        s.thrust_vacuum     = 2.0 * 5150000.0 + 1598000.0;  // 11,898,000 N
        s.thrust_sea_level  = 2.0 * 4700000.0 + 1400000.0;  // 10,800,000 N
        s.isp_vacuum        = 277.0;
        s.isp_sea_level     = 260.0;
        s.propellant_mass   = 486536.0;
        s.structural_mass   = 2.0 * 31000.0;  // 62,000 kg (S200 casings)
        s.burn_time         = 128.0;
        s.gimbal_range      = 0.035;           // Flex-seal nozzle TVC
        s.reference_area    = cross_section_area(4.0);
        s.is_solid          = true;
        s.ground_lit        = true;
        s.count             = 1;
    }

    // ------------------------------------------------------------------
    // Stage 1: L110 remaining burn (after S200 jettison)
    //
    // Remaining L110 burn: 194 - 128 = 66 s
    // Remaining propellant: 116,000 - 76,536 = 39,464 kg
    // L110 structural mass: ~12,000 kg
    // ------------------------------------------------------------------
    {
        StageConfig& s = config.stages[1];
        s.name              = "L110-Remaining";
        s.thrust_vacuum     = 1598000.0;       // 2× Vikas
        s.thrust_sea_level  = 1400000.0;
        s.isp_vacuum        = 293.0;
        s.isp_sea_level     = 262.0;
        s.propellant_mass   = 39464.0;
        s.structural_mass   = 12000.0;
        s.burn_time         = 66.0;
        s.gimbal_range      = 0.07;
        s.reference_area    = cross_section_area(4.0);
        s.is_solid          = false;
        s.ground_lit        = false;
        s.count             = 1;
    }

    // ------------------------------------------------------------------
    // Stage 2: C25 – Cryogenic upper stage with CE-20 engine
    //
    // Gas-generator-cycle LOX/LH₂ engine.
    // Derived from the CE-7.5 but significantly upscaled.
    // 200 kN thrust, 442 s Isp – enables heavy GTO payloads.
    // ------------------------------------------------------------------
    {
        StageConfig& s = config.stages[2];
        s.name              = "C25-CE20";
        s.thrust_vacuum     = 200000.0;        // 200 kN
        s.thrust_sea_level  = 200000.0;        // Always in vacuum
        s.isp_vacuum        = 442.0;
        s.isp_sea_level     = 442.0;
        s.propellant_mass   = 28000.0;         // LOX + LH₂
        s.structural_mass   = 3400.0;
        s.burn_time         = 580.0;
        s.gimbal_range      = 0.07;
        s.reference_area    = cross_section_area(4.0);
        s.is_solid          = false;
        s.ground_lit        = false;
        s.count             = 1;
    }

    return config;
}

// ============================================================================
//  SSLV Configuration
// ============================================================================
RocketConfig create_sslv(double payload_mass) {
    RocketConfig config;
    config.name          = "SSLV";
    config.type          = RocketType::SSLV;
    config.payload_mass  = payload_mass;
    config.fairing_mass  = 250.0;
    config.reference_area = cross_section_area(2.0);
    config.total_height  = 34.0;
    config.mach_breakpoints = standard_mach_breakpoints();
    config.cd_values        = standard_cd_values();

    config.stages.resize(4);
    // SS1 (Solid)
    {
        StageConfig& s = config.stages[0];
        s.name = "SS1"; s.thrust_vacuum = 2600000.0; s.thrust_sea_level = 2300000.0;
        s.isp_vacuum = 260.0; s.isp_sea_level = 235.0; s.propellant_mass = 87000.0;
        s.structural_mass = 12000.0; s.burn_time = 117.0; s.gimbal_range = 0.035;
        s.reference_area = cross_section_area(2.0); s.is_solid = true; s.ground_lit = true;
    }
    // SS2 (Solid)
    {
        StageConfig& s = config.stages[1];
        s.name = "SS2"; s.thrust_vacuum = 240000.0; s.thrust_sea_level = 240000.0;
        s.isp_vacuum = 275.0; s.isp_sea_level = 275.0; s.propellant_mass = 7700.0;
        s.structural_mass = 1100.0; s.burn_time = 114.0; s.gimbal_range = 0.035;
        s.reference_area = cross_section_area(2.0); s.is_solid = true;
    }
    // SS3 (Solid)
    {
        StageConfig& s = config.stages[2];
        s.name = "SS3"; s.thrust_vacuum = 160000.0; s.thrust_sea_level = 160000.0;
        s.isp_vacuum = 280.0; s.isp_sea_level = 280.0; s.propellant_mass = 4500.0;
        s.structural_mass = 700.0; s.burn_time = 105.0; s.gimbal_range = 0.035;
        s.reference_area = cross_section_area(2.0); s.is_solid = true;
    }
    // VTM (Liquid)
    {
        StageConfig& s = config.stages[3];
        s.name = "VTM"; s.thrust_vacuum = 800.0; s.thrust_sea_level = 800.0;
        s.isp_vacuum = 290.0; s.isp_sea_level = 290.0; s.propellant_mass = 50.0;
        s.structural_mass = 20.0; s.burn_time = 200.0; s.gimbal_range = 0.05;
        s.reference_area = cross_section_area(2.0); s.is_solid = false;
    }
    return config;
}

// ============================================================================
//  HSLV Configuration
// ============================================================================
RocketConfig create_hslv(double payload_mass) {
    // Scaled up LVM3 concept
    RocketConfig config = create_lvm3(payload_mass);
    config.name = "HSLV";
    config.type = RocketType::HSLV;
    config.stages[0].thrust_vacuum *= 1.5;
    config.stages[0].thrust_sea_level *= 1.5;
    return config;
}

// ============================================================================
//  NGLV Configuration
// ============================================================================
RocketConfig create_nglv(double payload_mass) {
    RocketConfig config;
    config.name          = "NGLV";
    config.type          = RocketType::NGLV;
    config.payload_mass  = payload_mass;
    config.fairing_mass  = 4000.0;
    config.reference_area = cross_section_area(5.0);
    config.total_height  = 65.0;
    config.mach_breakpoints = standard_mach_breakpoints();
    config.cd_values        = standard_cd_values();

    config.stages.resize(2);
    // Stage 1 (Semi-Cryo, Reusable)
    {
        StageConfig& s = config.stages[0];
        s.name = "SC120"; s.thrust_vacuum = 6000000.0; s.thrust_sea_level = 5400000.0;
        s.isp_vacuum = 310.0; s.isp_sea_level = 280.0; s.propellant_mass = 350000.0;
        s.structural_mass = 35000.0; s.burn_time = 180.0; s.gimbal_range = 0.08;
        s.reference_area = cross_section_area(5.0); s.is_solid = false; s.ground_lit = true;
    }
    
    // Configure Reusable Grid Fins for SC120 Stage 1
    config.grid_fins.area = 2.5; // Large titanium grid fins
    config.grid_fins.max_angle = 0.523; // 30 degrees
    config.grid_fins.deployed = true; // Assume deployed on descent
    // Stage 2 (Cryo)
    {
        StageConfig& s = config.stages[1];
        s.name = "C32"; s.thrust_vacuum = 250000.0; s.thrust_sea_level = 250000.0;
        s.isp_vacuum = 455.0; s.isp_sea_level = 455.0; s.propellant_mass = 32000.0;
        s.structural_mass = 4000.0; s.burn_time = 550.0; s.gimbal_range = 0.05;
        s.reference_area = cross_section_area(5.0); s.is_solid = false;
    }
    return config;
}

// ============================================================================
//  Vikram I Configuration
// ============================================================================
RocketConfig create_vikram(double payload_mass) {
    RocketConfig config;
    config.name          = "Vikram-I";
    config.type          = RocketType::VIKRAM;
    config.payload_mass  = payload_mass;
    config.fairing_mass  = 100.0;
    config.reference_area = cross_section_area(1.5);
    config.total_height  = 20.0;
    config.mach_breakpoints = standard_mach_breakpoints();
    config.cd_values        = standard_cd_values();

    config.stages.resize(3);
    // Stage 1 (Solid)
    {
        StageConfig& s = config.stages[0];
        s.name = "Kalam-100"; s.thrust_vacuum = 500000.0; s.thrust_sea_level = 450000.0;
        s.isp_vacuum = 265.0; s.isp_sea_level = 240.0; s.propellant_mass = 15000.0;
        s.structural_mass = 2000.0; s.burn_time = 80.0; s.gimbal_range = 0.04;
        s.reference_area = cross_section_area(1.5); s.is_solid = true; s.ground_lit = true;
    }
    // Stage 2 (Solid)
    {
        StageConfig& s = config.stages[1];
        s.name = "Kalam-25"; s.thrust_vacuum = 100000.0; s.thrust_sea_level = 100000.0;
        s.isp_vacuum = 280.0; s.isp_sea_level = 280.0; s.propellant_mass = 4000.0;
        s.structural_mass = 600.0; s.burn_time = 110.0; s.gimbal_range = 0.04;
        s.reference_area = cross_section_area(1.5); s.is_solid = true;
    }
    // Stage 3 (Liquid OMV)
    {
        StageConfig& s = config.stages[2];
        s.name = "Raman-OMV"; s.thrust_vacuum = 5000.0; s.thrust_sea_level = 5000.0;
        s.isp_vacuum = 310.0; s.isp_sea_level = 310.0; s.propellant_mass = 500.0;
        s.structural_mass = 100.0; s.burn_time = 300.0; s.gimbal_range = 0.05;
        s.reference_area = cross_section_area(1.5); s.is_solid = false;
    }
    return config;
}

// ============================================================================
//  Generic factory dispatcher
// ============================================================================

/**
 * @brief Create a rocket configuration by type enum and payload mass.
 *
 * Dispatches to the appropriate vehicle-specific factory function.
 *
 * @param type          RocketType enum value.
 * @param payload_mass  Payload mass in kg (0 = use vehicle default).
 * @return Fully populated RocketConfig.
 * @throws std::invalid_argument if the rocket type is unknown.
 */
RocketConfig create(RocketType type, double payload_mass) {
    switch (type) {
        case RocketType::PSLV_XL: {
            const double mass = (payload_mass > 0.0) ? payload_mass : 1750.0;
            return create_pslv_xl(mass);
        }
        case RocketType::GSLV_MK2: {
            const double mass = (payload_mass > 0.0) ? payload_mass : 2500.0;
            return create_gslv_mk2(mass);
        }
        case RocketType::LVM3: {
            const double mass = (payload_mass > 0.0) ? payload_mass : 4000.0;
            return create_lvm3(mass);
        }
        case RocketType::SSLV: {
            const double mass = (payload_mass > 0.0) ? payload_mass : 500.0;
            return create_sslv(mass);
        }
        case RocketType::HSLV: {
            const double mass = (payload_mass > 0.0) ? payload_mass : 10000.0;
            return create_hslv(mass);
        }
        case RocketType::NGLV: {
            const double mass = (payload_mass > 0.0) ? payload_mass : 20000.0;
            return create_nglv(mass);
        }
        case RocketType::VIKRAM: {
            const double mass = (payload_mass > 0.0) ? payload_mass : 300.0;
            return create_vikram(mass);
        }
        default:
            throw std::invalid_argument(
                "Unknown RocketType enum value: " +
                std::to_string(static_cast<int>(type)));
    }
}

}  // namespace rocket_config
}  // namespace prakshep
