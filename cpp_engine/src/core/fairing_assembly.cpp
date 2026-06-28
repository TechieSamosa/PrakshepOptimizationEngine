/**
 * @file fairing_assembly.cpp
 * @brief Implementation of payload fairing jettison dynamics.
 *
 * @author  Prakshep GNC Team
 * @version 2.0.0
 */

#include "prakshep/core/fairing_assembly.hpp"

namespace prakshep {
namespace core {

bool FairingAssembly::check_jettison_condition(double dynamic_pressure, double altitude) {
    if (m_jettisoned) return false;

    // Both altitude and q conditions must be met.
    // The altitude guard prevents shedding during an anomalous low-q event
    // early in flight (e.g. going through a high-altitude wind shear null).
    if (altitude >= MIN_JETTISON_ALTITUDE && dynamic_pressure <= SAFE_DYNAMIC_PRESSURE_PA) {
        return true;
    }
    return false;
}

void FairingAssembly::executeJettison(Vec3& out_vel_half_1, Vec3& out_vel_half_2) {
    if (m_jettisoned) {
        out_vel_half_1 = Vec3(0,0,0);
        out_vel_half_2 = Vec3(0,0,0);
        return;
    }

    // Mark as jettisoned and zero the mass contribution to the vehicle
    m_jettisoned = true;
    m_mass = 0.0;

    // Simulate zip-cord pyrotechnic lateral separation.
    // The fairing splits vertically into two halves.
    // We assume the seam lies in the X-Z plane, so the halves separate
    // along the +/- Y body axis.
    
    // Half 1 (+Y direction)
    out_vel_half_1 = Vec3(0.0, SEPARATION_VELOCITY, 0.0);
    
    // Half 2 (-Y direction)
    out_vel_half_2 = Vec3(0.0, -SEPARATION_VELOCITY, 0.0);
}

} // namespace core
} // namespace prakshep
