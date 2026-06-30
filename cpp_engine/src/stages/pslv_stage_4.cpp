/**
 * @file pslv_stage_4.cpp
 * @brief Implementation of the PSLV 4th Stage (PS4).
 *
 * @author  Prakshep GNC Team
 * @version 2.0.0
 */

#include "prakshep/stages/pslv_stage_4.hpp"
#include "prakshep/constants.hpp"
#include <algorithm>
#include <cmath>

namespace prakshep {
namespace stages {

PSLV_Stage4::PSLV_Stage4(double payload_mass)
    : m_state(State::IDLE)
    , m_propellant_remaining(PROPELLANT_MASS)
    , m_payload_mass(payload_mass)
    , m_payload_deployed(false)
    , m_current_thrust_vector(0.0, 0.0, 0.0)
    , m_current_com_offset(0.0, 0.0, 0.0)
{
}

void PSLV_Stage4::ignite() {
    // Support restart: can transition from IDLE or COASTING to BURNING
    if (m_state == State::IDLE || m_state == State::COASTING) {
        if (m_propellant_remaining > 0.0) {
            m_state = State::BURNING;
        }
    }
}

void PSLV_Stage4::shutdown_engines() {
    if (m_state == State::BURNING) {
        m_state = State::COASTING;
        m_current_thrust_vector = Vec3(0.0, 0.0, 0.0);
    }
}

void PSLV_Stage4::separate() {
    // PS4 doesn't technically separate from the vehicle stack since it IS the top of the stack,
    // but standardising the interface allows it to detach from PS3.
    m_state = State::SEPARATED; 
}

Vec3 PSLV_Stage4::deploy_payload() {
    if (!m_payload_deployed) {
        m_payload_deployed = true;
        m_payload_mass = 0.0;
        
        // Payload is pushed forward along the local Z axis (assuming Z is longitudinal axis here,
        // or X depending on body frame convention. Usually body X is forward).
        // Using +X as forward:
        return Vec3(PAYLOAD_SEPARATION_VEL, 0.0, 0.0); 
    }
    return Vec3(0.0, 0.0, 0.0);
}

void PSLV_Stage4::update(double dt, double pitch_command, double yaw_command, double altitude) {
    if (m_state != State::BURNING) {
        m_current_thrust_vector = Vec3(0.0, 0.0, 0.0);
        return;
    }

    if (m_propellant_remaining <= 0.0) {
        m_state = State::BURNOUT;
        m_propellant_remaining = 0.0;
        m_current_thrust_vector = Vec3(0.0, 0.0, 0.0);
        return;
    }

    // Mass flow rate for both engines combined
    double mass_flow_rate = THRUST_VACUUM / (ISP_VACUUM * constants::G0);
    double mass_consumed = mass_flow_rate * dt;
    m_propellant_remaining = std::max(0.0, m_propellant_remaining - mass_consumed);

    // Twin engines are gimbaled for pitch/yaw, similar to PS2 but smaller deflection.
    // Simplified TVC rotation:
    pitch_command = std::clamp(pitch_command, -1.0, 1.0);
    yaw_command   = std::clamp(yaw_command,   -1.0, 1.0);
    
    double gp = pitch_command * 0.05; // ~3 degrees max
    double gy = yaw_command * 0.05;

    // Body +X is forward
    m_current_thrust_vector = Vec3(
        THRUST_VACUUM * std::cos(gp) * std::cos(gy),
        THRUST_VACUUM * std::sin(gp),
        THRUST_VACUUM * std::sin(gy)
    );

    // CoM shift
    double burn_fraction = 1.0 - (m_propellant_remaining / PROPELLANT_MASS);
    double com_position = STAGE_LENGTH * (0.6 + 0.1 * burn_fraction);
    m_current_com_offset = Vec3(com_position - STAGE_LENGTH * 0.5, 0.0, 0.0);
}

double PSLV_Stage4::get_mass() const {
    return STRUCTURAL_MASS + m_propellant_remaining + m_payload_mass;
}

Vec3 PSLV_Stage4::get_thrust_vector() const { return m_current_thrust_vector; }
Vec3 PSLV_Stage4::get_com_offset() const { return m_current_com_offset; }
RocketStage::State PSLV_Stage4::get_state() const { return m_state; }
double PSLV_Stage4::get_propellant_fraction() const { return m_propellant_remaining / PROPELLANT_MASS; }
std::string PSLV_Stage4::get_name() const { return "PS4 - Twin Engines"; }

} // namespace stages
} // namespace prakshep
