/**
 * @file vehicle_assembler.hpp
 * @brief Factory for assembling complete launch vehicles from modular stages.
 *
 * The VehicleAssembly is a runtime data structure representing a fully
 * stacked launch vehicle. It holds:
 *   - An ordered vector of core stages (PS1, PS2, PS3, PS4)
 *   - A vector of booster modules (PSOMs) attached to the first core stage
 *   - Precomputed total mass budgets (wet, dry, propellant)
 *
 * The assembly factory functions (create_pslv_xl, create_pslv_ca, etc.)
 * construct the correct configuration by:
 *   1. Instantiating the PS1 core stage
 *   2. Attaching the correct number of PSOM boosters
 *   3. Configuring staggered ignition schedules
 *   4. Computing the mounting angles and radial distances
 *   5. Calculating total vehicle mass
 *
 * @author  Prakshep GNC Team
 * @version 2.0.0
 */

#pragma once

#include "prakshep/types.hpp"
#include "prakshep/stages/rocket_stage.hpp"
#include "prakshep/stages/booster_module.hpp"
#include <vector>
#include <memory>
#include <string>

namespace prakshep {
namespace core {

/**
 * @struct VehicleAssembly
 * @brief  Runtime representation of a fully stacked launch vehicle.
 */
struct VehicleAssembly {
    std::string name;                                       ///< Vehicle designation
    std::string variant;                                    ///< Variant (CA, DL, QL, XL)

    std::vector<std::unique_ptr<stages::RocketStage>> core_stages;   ///< Ordered core stages
    std::vector<std::unique_ptr<stages::BoosterModule>> boosters;    ///< Radial strap-on boosters

    double payload_mass   = 0.0;    ///< Payload mass (kg)
    double fairing_mass   = 0.0;    ///< Fairing mass (kg)

    // Precomputed mass budget
    double total_wet_mass = 0.0;    ///< Total mass at T-0 (kg)
    double total_dry_mass = 0.0;    ///< Total mass with all propellant expended (kg)
    double total_propellant_mass = 0.0; ///< Total propellant across all stages + boosters (kg)

    int num_boosters      = 0;      ///< Number of strap-on boosters
    int num_ground_lit    = 0;      ///< Number of ground-lit boosters
    int num_air_lit       = 0;      ///< Number of air-lit boosters

    /**
     * @brief Compute the composite centre of mass of the entire vehicle.
     *
     * Iterates over all core stages and boosters, weighting each module's
     * CoM by its current mass to find the vehicle-level CoM.
     *
     * @return CoM position in the vehicle body frame (m).
     */
    Vec3 compute_composite_com() const;

    /**
     * @brief Get the current total mass of the vehicle (all stages + boosters).
     *
     * @return Total instantaneous mass (kg).
     */
    double get_current_mass() const;

    /**
     * @brief Get the total thrust from all currently-burning modules.
     *
     * @return Composite thrust vector in body frame (N).
     */
    Vec3 get_total_thrust() const;

    /**
     * @brief Get the net torque from all booster off-axis contributions.
     *
     * Computes τ = Σ(r_i × F_i) for all burning boosters, using the
     * current composite CoM as the reference point.
     *
     * @return Net torque in body frame (N·m).
     */
    Vec3 get_net_booster_torque() const;
};

// ===========================================================================
//  Factory functions
// ===========================================================================

/**
 * @brief Assemble a PSLV-XL vehicle (6 PSOM-XL strap-ons).
 *
 * Configuration:
 *   - 4 ground-lit boosters at 0°, 90°, 180°, 270°
 *   - 2 air-lit boosters at 45°, 225° (ignite at T+25s)
 *   - PS1 core stage (S139)
 *
 * @param payload_mass  Payload mass (kg)
 * @return Fully assembled VehicleAssembly.
 */
VehicleAssembly assemble_pslv_xl(double payload_mass);

/**
 * @brief Assemble a PSLV-QL vehicle (4 PSOM-XL strap-ons, all ground-lit).
 *
 * @param payload_mass  Payload mass (kg)
 */
VehicleAssembly assemble_pslv_ql(double payload_mass);

/**
 * @brief Assemble a PSLV-DL vehicle (2 PSOM-XL strap-ons, both ground-lit).
 *
 * @param payload_mass  Payload mass (kg)
 */
VehicleAssembly assemble_pslv_dl(double payload_mass);

/**
 * @brief Assemble a PSLV-CA vehicle (Core Alone, no strap-ons).
 *
 * @param payload_mass  Payload mass (kg)
 */
VehicleAssembly assemble_pslv_ca(double payload_mass);

/**
 * @brief Top-level factory — dispatches to the correct variant assembler.
 *
 * @param variant       Vehicle variant string ("PSLV-XL", "PSLV-CA", etc.)
 * @param payload_mass  Payload mass (kg)
 * @return Fully assembled VehicleAssembly.
 */
VehicleAssembly assemble_vehicle(const std::string& variant, double payload_mass);

} // namespace core
} // namespace prakshep
