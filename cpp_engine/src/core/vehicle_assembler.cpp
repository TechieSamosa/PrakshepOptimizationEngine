/**
 * @file vehicle_assembler.cpp
 * @brief Factory implementation for assembling PSLV vehicle variants.
 *
 * === ASSEMBLY PROCESS ===
 *
 * The assembler performs the following sequence for each PSLV variant:
 *
 *   1. Instantiate the PS1 core stage (S139 solid motor)
 *   2. Determine the number and type of strap-on boosters
 *   3. For each booster:
 *      a. Instantiate a PSLV_StrapOn object
 *      b. Compute its angular mounting position around the core
 *      c. Set its radial distance from the core axis (2.4m for PSOM-XL)
 *      d. Set its axial height offset (aligned with lower third of PS1)
 *      e. Assign its ignition group (ground-lit or air-lit)
 *   4. Compute the total vehicle mass budget
 *
 * === PSLV-XL BOOSTER LAYOUT ===
 *
 * Looking down the vehicle axis (from the fairing toward the nozzle),
 * the 6 PSOM-XL boosters are arranged as:
 *
 *              45° (Air-Lit)
 *       0° (G)        90° (G)
 *
 *      315° (G)       135° (G)
 *             225° (Air-Lit)
 *
 * (G) = Ground-Lit (ignites at T-0)
 * The two air-lit boosters are placed diametrically opposite to maintain
 * symmetry when they ignite 25 seconds into flight.
 *
 * === MASS BUDGET ===
 *
 * Total wet mass = PS1 mass + Σ(booster mass) + upper stages + payload + fairing
 * Total dry mass = PS1 structural + Σ(booster structural) + upper stages + payload
 *
 * For now, upper stages (PS2, PS3, PS4) are represented as a lump-sum
 * estimate. They will be replaced with individual modular stage objects
 * in subsequent phases.
 *
 * @author  Prakshep GNC Team
 * @version 2.0.0
 */

#include "prakshep/core/vehicle_assembler.hpp"
#include "prakshep/stages/pslv_stage_1.hpp"
#include "prakshep/stages/pslv_strap_on.hpp"
#include <cmath>
#include <stdexcept>

namespace prakshep {
namespace core {

// ===========================================================================
//  Constants for the standard PSOM-XL mounting geometry
// ===========================================================================

/// Radial distance from core axis to PSOM centreline (m)
static constexpr double PSOM_RADIAL_DISTANCE = 2.4;

/// Axial offset: PSOMs are mounted with their aft end roughly aligned
/// with the PS1 nozzle. This places them ~2m below the PS1 geometric centre.
static constexpr double PSOM_HEIGHT_OFFSET = -2.0;

/// Air-lit ignition delay (PSLV-XL only)
static constexpr double AIR_LIT_DELAY = 25.0;  // seconds after T-0

/// Fairing mass (standard PSLV fairing)
static constexpr double STANDARD_FAIRING_MASS = 1130.0;  // kg

/// Upper stages mass estimate (PS2 + PS3 + PS4, will be replaced with modules)
static constexpr double UPPER_STAGES_MASS_ESTIMATE = 8560.0;  // kg total

// ===========================================================================
//  VehicleAssembly methods
// ===========================================================================

Vec3 VehicleAssembly::compute_composite_com() const
{
    // -----------------------------------------------------------------------
    //  Weighted average of all module CoMs:
    //
    //      CoM_vehicle = Σ(m_i × r_i) / Σ(m_i)
    //
    //  where m_i is each module's current mass and r_i is its CoM position
    //  in the vehicle body frame.
    // -----------------------------------------------------------------------

    Vec3 weighted_sum(0.0, 0.0, 0.0);
    double total_mass = 0.0;

    // Core stages — their CoM offset is relative to their own geometric centre.
    // For now, PS1 is at the origin of the body frame.
    for (const auto& stage : core_stages) {
        double m = stage->get_mass();
        Vec3 com = stage->get_com_offset();  // Already in body frame
        weighted_sum += com * m;
        total_mass += m;
    }

    // Boosters — their CoM is relative to their mount position
    for (const auto& booster : boosters) {
        if (booster->get_state() == stages::RocketStage::State::SEPARATED) {
            continue;  // Separated boosters no longer contribute
        }
        double m = booster->get_mass();
        Vec3 mount_pos = booster->get_mount_position();
        Vec3 booster_com = mount_pos + booster->get_com_offset();
        weighted_sum += booster_com * m;
        total_mass += m;
    }

    // Payload + fairing (sitting at the top of the stack)
    double upper_mass = payload_mass + fairing_mass + UPPER_STAGES_MASS_ESTIMATE;
    Vec3 upper_com(15.0, 0.0, 0.0);  // Approximate: 15m above core centre
    weighted_sum += upper_com * upper_mass;
    total_mass += upper_mass;

    if (total_mass < 1.0) return Vec3(0.0, 0.0, 0.0);

    return weighted_sum / total_mass;
}

double VehicleAssembly::get_current_mass() const
{
    double mass = payload_mass + fairing_mass + UPPER_STAGES_MASS_ESTIMATE;

    for (const auto& stage : core_stages) {
        mass += stage->get_mass();
    }

    for (const auto& booster : boosters) {
        if (booster->get_state() != stages::RocketStage::State::SEPARATED) {
            mass += booster->get_mass();
        }
    }

    return mass;
}

Vec3 VehicleAssembly::get_total_thrust() const
{
    Vec3 total(0.0, 0.0, 0.0);

    for (const auto& stage : core_stages) {
        total += stage->get_thrust_vector();
    }

    for (const auto& booster : boosters) {
        if (booster->get_state() == stages::RocketStage::State::BURNING) {
            total += booster->get_thrust_vector();
        }
    }

    return total;
}

Vec3 VehicleAssembly::get_net_booster_torque() const
{
    Vec3 com = compute_composite_com();
    Vec3 total_torque(0.0, 0.0, 0.0);

    for (const auto& booster : boosters) {
        total_torque += booster->compute_torque(com);
    }

    return total_torque;
}

// ===========================================================================
//  Internal helper: attach boosters to a vehicle assembly
// ===========================================================================

static void attach_boosters(VehicleAssembly& assembly,
                            int num_boosters,
                            int num_ground_lit,
                            int num_air_lit,
                            const double* angles_deg)
{
    assembly.num_boosters   = num_boosters;
    assembly.num_ground_lit = num_ground_lit;
    assembly.num_air_lit    = num_air_lit;

    for (int i = 0; i < num_boosters; ++i) {
        auto booster = std::make_unique<stages::PSLV_StrapOn>(i);

        // Convert angle from degrees to radians
        double angle_rad = angles_deg[i] * (M_PI / 180.0);

        // Set the physical mounting position
        booster->set_mount_geometry(angle_rad, PSOM_RADIAL_DISTANCE, PSOM_HEIGHT_OFFSET);

        // Assign ignition schedule
        if (i < num_ground_lit) {
            booster->set_ignition_schedule(
                stages::BoosterModule::IgnitionGroup::GROUND_LIT, 0.0);
        } else {
            booster->set_ignition_schedule(
                stages::BoosterModule::IgnitionGroup::AIR_LIT, AIR_LIT_DELAY);
        }

        assembly.boosters.push_back(std::move(booster));
    }
}

// ===========================================================================
//  Internal helper: compute the mass budget
// ===========================================================================

static void compute_mass_budget(VehicleAssembly& assembly)
{
    // Wet mass: everything at T-0
    assembly.total_wet_mass = 0.0;
    assembly.total_dry_mass = 0.0;
    assembly.total_propellant_mass = 0.0;

    for (const auto& stage : assembly.core_stages) {
        assembly.total_wet_mass += stage->get_mass();
        // Dry mass = structural only (no propellant)
        // For PS1: structural = 30200 kg
        assembly.total_dry_mass += stages::PSLV_Stage1::STRUCTURAL_MASS;
        assembly.total_propellant_mass += stages::PSLV_Stage1::PROPELLANT_MASS;
    }

    for (const auto& booster : assembly.boosters) {
        assembly.total_wet_mass += booster->get_mass();
        assembly.total_dry_mass += stages::PSLV_StrapOn::STRUCTURAL_MASS;
        assembly.total_propellant_mass += stages::PSLV_StrapOn::PROPELLANT_MASS;
    }

    // Add upper stages, payload, fairing
    assembly.total_wet_mass += UPPER_STAGES_MASS_ESTIMATE
                             + assembly.payload_mass
                             + assembly.fairing_mass;
    assembly.total_dry_mass += UPPER_STAGES_MASS_ESTIMATE
                             + assembly.payload_mass;
}

// ===========================================================================
//  PSLV-XL: 6 strap-ons (4 ground-lit + 2 air-lit)
// ===========================================================================

VehicleAssembly assemble_pslv_xl(double payload_mass)
{
    VehicleAssembly assembly;
    assembly.name = "PSLV-XL";
    assembly.variant = "XL";
    assembly.payload_mass = payload_mass;
    assembly.fairing_mass = STANDARD_FAIRING_MASS;

    // PS1 core stage
    assembly.core_stages.push_back(std::make_unique<stages::PSLV_Stage1>());

    // 6 boosters: indices 0-3 are ground-lit, 4-5 are air-lit
    // Angular positions: 0°, 90°, 180°, 270° (ground), 45°, 225° (air)
    static constexpr double angles[] = {0.0, 90.0, 180.0, 270.0, 45.0, 225.0};
    attach_boosters(assembly, 6, 4, 2, angles);

    compute_mass_budget(assembly);
    return assembly;
}

// ===========================================================================
//  PSLV-QL: 4 strap-ons (all ground-lit)
// ===========================================================================

VehicleAssembly assemble_pslv_ql(double payload_mass)
{
    VehicleAssembly assembly;
    assembly.name = "PSLV-QL";
    assembly.variant = "QL";
    assembly.payload_mass = payload_mass;
    assembly.fairing_mass = STANDARD_FAIRING_MASS;

    assembly.core_stages.push_back(std::make_unique<stages::PSLV_Stage1>());

    static constexpr double angles[] = {0.0, 90.0, 180.0, 270.0};
    attach_boosters(assembly, 4, 4, 0, angles);

    compute_mass_budget(assembly);
    return assembly;
}

// ===========================================================================
//  PSLV-DL: 2 strap-ons (both ground-lit)
// ===========================================================================

VehicleAssembly assemble_pslv_dl(double payload_mass)
{
    VehicleAssembly assembly;
    assembly.name = "PSLV-DL";
    assembly.variant = "DL";
    assembly.payload_mass = payload_mass;
    assembly.fairing_mass = STANDARD_FAIRING_MASS;

    assembly.core_stages.push_back(std::make_unique<stages::PSLV_Stage1>());

    // 2 boosters diametrically opposite
    static constexpr double angles[] = {0.0, 180.0};
    attach_boosters(assembly, 2, 2, 0, angles);

    compute_mass_budget(assembly);
    return assembly;
}

// ===========================================================================
//  PSLV-CA: Core Alone (no strap-ons)
// ===========================================================================

VehicleAssembly assemble_pslv_ca(double payload_mass)
{
    VehicleAssembly assembly;
    assembly.name = "PSLV-CA";
    assembly.variant = "CA";
    assembly.payload_mass = payload_mass;
    assembly.fairing_mass = STANDARD_FAIRING_MASS;

    assembly.core_stages.push_back(std::make_unique<stages::PSLV_Stage1>());

    // No boosters
    assembly.num_boosters = 0;
    assembly.num_ground_lit = 0;
    assembly.num_air_lit = 0;

    compute_mass_budget(assembly);
    return assembly;
}

// ===========================================================================
//  Top-level dispatch factory
// ===========================================================================

VehicleAssembly assemble_vehicle(const std::string& variant, double payload_mass)
{
    if (variant == "PSLV-XL" || variant == "pslv-xl") {
        return assemble_pslv_xl(payload_mass);
    } else if (variant == "PSLV-QL" || variant == "pslv-ql") {
        return assemble_pslv_ql(payload_mass);
    } else if (variant == "PSLV-DL" || variant == "pslv-dl") {
        return assemble_pslv_dl(payload_mass);
    } else if (variant == "PSLV-CA" || variant == "pslv-ca") {
        return assemble_pslv_ca(payload_mass);
    }

    // Default fallback: PSLV-XL
    return assemble_pslv_xl(payload_mass);
}

} // namespace core
} // namespace prakshep
