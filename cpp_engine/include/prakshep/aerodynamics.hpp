#pragma once

/**
 * @file aerodynamics.hpp
 * @brief Aerodynamic force computation interface.
 *
 * Provides drag coefficient lookup (piecewise-linear interpolation over
 * Mach number) and drag-force computation including the correction for
 * atmospheric co-rotation.
 *
 * @see aerodynamics.cpp for the full implementation.
 *
 * @author  Prakshep GNC Team
 * @version 1.0.0
 */

#include "types.hpp"

namespace prakshep {
namespace aerodynamics {

/**
 * @brief Look up the drag coefficient for a given Mach number.
 *
 * Performs piecewise-linear interpolation on the vehicle's Mach/Cd
 * breakpoint table.  Values below the first breakpoint return the first
 * Cd; values above the last breakpoint return the last Cd.
 *
 * @param mach   Local Mach number.
 * @param config Vehicle configuration containing the Cd table.
 * @return Drag coefficient (dimensionless).
 */
double get_drag_coefficient(double mach, const RocketConfig& config);


/**
 * @brief Compute the aerodynamic drag force vector in ECI.
 *
 * Accounts for atmospheric co-rotation by subtracting the local
 * atmosphere velocity (ω × r) from the inertial velocity.
 * Also accounts for reverse atmospheric entry mechanics.
 *
 * @param state  Current vehicle state vector.
 * @param config Vehicle configuration.
 * @param atm    Local atmospheric conditions.
 * @return Drag force vector in ECI (N), opposing the velocity-relative-to-air.
 */
Vec3 compute_drag(const StateVector& state, const RocketConfig& config,
                  const AtmosphereState& atm);

/**
 * @brief Compute lift and drag forces from deployed grid fins during reverse atmospheric entry.
 * 
 * @param state     Current vehicle state vector.
 * @param atm       Local atmospheric conditions.
 * @param grid_fins Grid fin configuration.
 * @param angle_of_attack The effective angle of attack of the grid fins.
 * @return Force vector in ECI (N) applied by the grid fins.
 */
Vec3 compute_grid_fin_force(const StateVector& state, const AtmosphereState& atm, 
                            const GridFinConfig& grid_fins, double angle_of_attack);

/**
 * @brief Compute dynamic pressure q = ½ ρ v².
 *
 * @param density       Air density (kg/m³).
 * @param velocity_mag  Speed relative to the atmosphere (m/s).
 * @return Dynamic pressure (Pa).
 */
double compute_dynamic_pressure(double density, double velocity_mag);

} // namespace aerodynamics
} // namespace prakshep
