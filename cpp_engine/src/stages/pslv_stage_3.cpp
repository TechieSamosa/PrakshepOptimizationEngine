/**
 * @file pslv_stage_3.cpp
 * @brief Implementation of the PSLV 3rd Stage (PS3) — S7 Solid Motor.
 *
 * @author  Prakshep GNC Team
 * @version 2.0.0
 */

#include "prakshep/stages/pslv_stage_3.hpp"
#include "prakshep/constants.hpp"
#include <algorithm>
#include <cmath>

namespace prakshep {
namespace stages {

PSLV_Stage3::PSLV_Stage3()
    : m_state(State::IDLE)
    , m_propellant_remaining(PROPELLANT_MASS)
    , m_burn_elapsed(0.0)
    , m_current_mass_flow_rate(0.0)
    , m_current_thrust_vector(0.0, 0.0, 0.0)
    , m_current_com_offset(0.0, 0.0, 0.0)
    , m_nozzle_angles(0.0, 0.0, 0.0)
    , m_nozzle_rates(0.0, 0.0, 0.0)
{
}

void PSLV_Stage3::ignite() {
    if (m_state == State::IDLE) {
        m_state = State::BURNING;
        m_burn_elapsed = 0.0;
    }
}

void PSLV_Stage3::separate() {
    m_state = State::SEPARATED;
    m_current_thrust_vector = Vec3(0.0, 0.0, 0.0);
}

void PSLV_Stage3::update(double dt, double pitch_command, double yaw_command, double altitude) {
    if (m_state != State::BURNING) {
        m_current_thrust_vector = Vec3(0.0, 0.0, 0.0);
        m_current_mass_flow_rate = 0.0;
        return;
    }

    // Advance burn clock
    m_burn_elapsed += dt;
    double normalised_time = m_burn_elapsed / BURN_TIME;

    if (normalised_time >= 1.0 || m_propellant_remaining <= 0.0) {
        m_state = State::BURNOUT;
        m_propellant_remaining = 0.0;
        m_current_thrust_vector = Vec3(0.0, 0.0, 0.0);
        m_current_mass_flow_rate = 0.0;
        return;
    }

    // Thrust curve and mass depletion
    double thrust_multiplier = sample_thrust_curve(normalised_time);
    
    // PS3 operates exclusively in vacuum
    double thrust_magnitude = THRUST_VACUUM * thrust_multiplier;
    m_current_mass_flow_rate = thrust_magnitude / (ISP_VACUUM * constants::G0);

    double mass_consumed = m_current_mass_flow_rate * dt;
    m_propellant_remaining = std::max(0.0, m_propellant_remaining - mass_consumed);

    // Update Flex-Seal dynamics (2nd-order system)
    update_flex_seal_dynamics(dt, pitch_command, yaw_command);

    // Compose body-frame thrust vector
    double gp = m_nozzle_angles.y;
    double gy = m_nozzle_angles.z;

    m_current_thrust_vector = Vec3(
        thrust_magnitude * std::cos(gp) * std::cos(gy),
        thrust_magnitude * std::sin(gp),
        thrust_magnitude * std::sin(gy)
    );

    // CoM tracking (S7 grain burns roughly spherically)
    double burn_fraction = 1.0 - (m_propellant_remaining / PROPELLANT_MASS);
    double com_position = STAGE_LENGTH * (0.45 + 0.1 * burn_fraction);
    m_current_com_offset = Vec3(com_position - STAGE_LENGTH * 0.5, 0.0, 0.0);
}

void PSLV_Stage3::update_flex_seal_dynamics(double dt, double pitch_cmd, double yaw_cmd) {
    // -----------------------------------------------------------------------
    //  The GNC commands are mapped to target deflection angles.
    //  The actuators act as P-D controllers trying to reach that target,
    //  but they must fight the flex-seal's mechanical stiffness (K_flex)
    //  and structural damping (C_damp).
    //
    //  Actuator Torque Command = K_p * (target - current) - K_d * rate
    // -----------------------------------------------------------------------

    pitch_cmd = std::clamp(pitch_cmd, -1.0, 1.0);
    yaw_cmd   = std::clamp(yaw_cmd,   -1.0, 1.0);

    double target_pitch = pitch_cmd * MAX_DEFLECTION_ANGLE;
    double target_yaw   = yaw_cmd   * MAX_DEFLECTION_ANGLE;

    // Pseudo-controller gains for the hydraulic system
    const double Kp = 40000.0;
    const double Kd = 1500.0;

    double act_torque_p = Kp * (target_pitch - m_nozzle_angles.y) - Kd * m_nozzle_rates.y;
    double act_torque_y = Kp * (target_yaw   - m_nozzle_angles.z) - Kd * m_nozzle_rates.z;

    // Clamp to physical actuator limits
    act_torque_p = std::clamp(act_torque_p, -MAX_ACTUATOR_TORQUE, MAX_ACTUATOR_TORQUE);
    act_torque_y = std::clamp(act_torque_y, -MAX_ACTUATOR_TORQUE, MAX_ACTUATOR_TORQUE);

    // Rotational equations of motion: I*alpha = Sum(Torques)
    // Sum(Torques) = T_actuator - K_flex * theta - C_damp * omega
    double alpha_p = (act_torque_p - FLEX_STIFFNESS * m_nozzle_angles.y - FLEX_DAMPING * m_nozzle_rates.y) / NOZZLE_INERTIA;
    double alpha_y = (act_torque_y - FLEX_STIFFNESS * m_nozzle_angles.z - FLEX_DAMPING * m_nozzle_rates.z) / NOZZLE_INERTIA;

    // Explicit Euler integration (dt is assumed small, e.g. 0.01s)
    m_nozzle_rates.y += alpha_p * dt;
    m_nozzle_rates.z += alpha_y * dt;

    m_nozzle_angles.y += m_nozzle_rates.y * dt;
    m_nozzle_angles.z += m_nozzle_rates.z * dt;
}

double PSLV_Stage3::sample_thrust_curve(double normalised_time) const {
    constexpr int N = sizeof(THRUST_CURVE_TIME) / sizeof(THRUST_CURVE_TIME[0]);
    normalised_time = std::clamp(normalised_time, 0.0, 1.0);

    if (normalised_time <= THRUST_CURVE_TIME[0]) return THRUST_CURVE_MULT[0];
    if (normalised_time >= THRUST_CURVE_TIME[N - 1]) return THRUST_CURVE_MULT[N - 1];

    for (int i = 0; i < N - 1; ++i) {
        if (normalised_time >= THRUST_CURVE_TIME[i] && normalised_time < THRUST_CURVE_TIME[i + 1]) {
            double t0 = THRUST_CURVE_TIME[i];
            double t1 = THRUST_CURVE_TIME[i + 1];
            double m0 = THRUST_CURVE_MULT[i];
            double m1 = THRUST_CURVE_MULT[i + 1];
            double alpha = (normalised_time - t0) / (t1 - t0);
            return m0 + alpha * (m1 - m0);
        }
    }
    return 1.0;
}

double PSLV_Stage3::get_mass() const { return STRUCTURAL_MASS + m_propellant_remaining; }
Vec3 PSLV_Stage3::get_thrust_vector() const { return m_current_thrust_vector; }
Vec3 PSLV_Stage3::get_com_offset() const { return m_current_com_offset; }
RocketStage::State PSLV_Stage3::get_state() const { return m_state; }
double PSLV_Stage3::get_propellant_fraction() const { return m_propellant_remaining / PROPELLANT_MASS; }
std::string PSLV_Stage3::get_name() const { return "PS3 - S7 Motor"; }

} // namespace stages
} // namespace prakshep
