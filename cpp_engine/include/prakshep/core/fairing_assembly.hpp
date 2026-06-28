/**
 * @file fairing_assembly.hpp
 * @brief Payload Fairing Dynamics for the PSLV.
 *
 * The payload fairing (heat shield) protects the spacecraft from aerodynamic
 * and thermal loads during ascent through the atmosphere.
 *
 * It is jettisoned once the vehicle has cleared the dense atmosphere and
 * the aerodynamic heat flux / dynamic pressure drops below a critical threshold
 * (typically at ~110-115 km altitude for PSLV).
 *
 * This class tracks the fairing state, polls the environment, and manages
 * the instantaneous mass shedding and lateral separation impulse of the two halves.
 *
 * @author  Prakshep GNC Team
 * @version 2.0.0
 */

#pragma once

#include "prakshep/types.hpp"

namespace prakshep {
namespace core {

class FairingAssembly {
public:
    /// Fairing mass for PSLV-XL standard payload fairing (kg)
    static constexpr double MASS = 1130.0;

    /// Threshold dynamic pressure below which it is safe to jettison (Pa)
    static constexpr double SAFE_DYNAMIC_PRESSURE_PA = 0.1;

    /// Minimum altitude constraint to prevent premature jettison (m)
    static constexpr double MIN_JETTISON_ALTITUDE = 110000.0;

    /// Lateral kick velocity imparted by the zip-cord / pyrotechnics (m/s)
    static constexpr double SEPARATION_VELOCITY = 4.5;

    FairingAssembly() : m_jettisoned(false), m_mass(MASS) {}

    /**
     * @brief Poll the environmental conditions to see if jettison is required.
     *
     * @param dynamic_pressure  Current aerodynamic pressure q (Pa)
     * @param altitude          Current altitude ASL (m)
     * @return True if the condition is met and jettison was just triggered.
     */
    bool check_jettison_condition(double dynamic_pressure, double altitude);

    /**
     * @brief Execute the pyrotechnic separation.
     *
     * Sets mass to 0. Returns the separation velocity vectors for both halves
     * to the flight computer (for potential debris tracking / visualization).
     *
     * @param out_vel_half_1  Output: lateral velocity of +Y half (m/s)
     * @param out_vel_half_2  Output: lateral velocity of -Y half (m/s)
     */
    void executeJettison(Vec3& out_vel_half_1, Vec3& out_vel_half_2);

    /** @brief Check if the fairing has been shed. */
    bool is_jettisoned() const { return m_jettisoned; }

    /** @brief Get current mass (MASS if attached, 0 if jettisoned). */
    double get_mass() const { return m_mass; }

private:
    bool   m_jettisoned;
    double m_mass;
};

} // namespace core
} // namespace prakshep
