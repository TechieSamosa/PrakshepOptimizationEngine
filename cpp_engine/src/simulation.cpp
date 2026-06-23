#include "prakshep/simulation.hpp"
#include "prakshep/constants.hpp"
#include "prakshep/atmosphere.hpp"
#include "prakshep/integrator.hpp"
#include "prakshep/aerodynamics.hpp"
#include "prakshep/rocket_config.hpp"

#include <cmath>

namespace prakshep {

Simulation::Simulation(RocketType type, double payload_mass)
    : config_(rocket_config::create(type, payload_mass)),
      flight_computer_(config_) {
    reset();
}

void Simulation::reset() {
    state_.time = 0.0;
    
    // Convert launch pad geodetic to ECI position
    state_.position = eci_from_geodetic(
        constants::LAUNCH_LATITUDE * constants::DEG_TO_RAD,
        constants::LAUNCH_LONGITUDE * constants::DEG_TO_RAD,
        constants::LAUNCH_ALTITUDE
    );
    
    // Initial velocity is due to Earth's rotation
    Vec3 omega(0.0, 0.0, constants::EARTH_OMEGA);
    state_.velocity = omega.cross(state_.position);
    
    // Point straight up
    // Normal to the ellipsoid approximation:
    Vec3 up = state_.position.normalized(); // Approximation for vertical
    // Create quaternion that points +Z along 'up'
    Vec3 z_axis(0, 0, 1);
    Vec3 axis = z_axis.cross(up);
    double angle = std::acos(z_axis.dot(up));
    if (axis.norm_squared() > 1e-12) {
        state_.attitude = Quaternion::from_axis_angle(axis.normalized(), angle);
    } else {
        state_.attitude = Quaternion::identity();
    }
    
    state_.angular_velocity = Vec3(0, 0, 0);
    state_.mass = config_.total_mass();
    
    current_stage_ = 0;
    max_q_ = 0.0;
    max_q_reached_ = false;
    simulation_complete_ = false;
    
    for (int i = 0; i < 10; ++i) {
        consumed_propellant_[i] = 0.0;
    }
    
    clear_failures();
    clear_tvc_override();
}

void Simulation::inject_failure(FailureType type, double severity) {
    active_failure_ = type;
    failure_severity_ = std::fmax(0.0, std::fmin(1.0, severity));
}

void Simulation::clear_failures() {
    active_failure_ = FailureType::NONE;
    failure_severity_ = 0.0;
}

void Simulation::set_tvc_override(double gimbal_pitch, double gimbal_yaw, double throttle) {
    flight_computer_.set_tvc_override(gimbal_pitch, gimbal_yaw, throttle);
}

void Simulation::clear_tvc_override() {
    flight_computer_.clear_tvc_override();
}

const StateVector& Simulation::get_state() const {
    return state_;
}

bool Simulation::is_complete() const {
    return simulation_complete_;
}

TelemetryFrame Simulation::tick(double dt) {
    if (simulation_complete_) {
        return build_telemetry(Vec3(0,0,0));
    }
    
    FlightCommand cmd = flight_computer_.compute(state_, current_stage_);
    
    // Apply failure effects
    if (active_failure_ == FailureType::ENGINE_UNDERPERFORMANCE) {
        cmd.throttle *= (1.0 - failure_severity_ * 0.5);
    } else if (active_failure_ == FailureType::TVC_FAILURE) {
        cmd.gimbal_pitch = 0.0;
        cmd.gimbal_yaw = 0.0;
    }
    // CROSSWIND is modelled as an attitude disturbance, applied in integrator/simulation step if needed. For now, we affect structural integrity later.
    
    Vec3 old_velocity = state_.velocity;
    double old_mass = state_.mass;
    
    double lat, lon, geodetic_altitude;
    eci_to_geodetic(state_.position, lat, lon, geodetic_altitude);

    // ----------------------------------------------------------------------
    // State Vector Handoff: 
    // Transition to pure J2 oblate gravity orbital propagator if above 150km
    // or atmospheric density is negligible (handles stable orbit/BAS docking)
    // ----------------------------------------------------------------------
    if (geodetic_altitude > 150000.0) {
        // Fast RK4 specialized for pure J2 orbital propagation (zero thrust, zero drag)
        auto orbit_deriv = [](const Vec3& pos, const Vec3& vel, Vec3& dpos, Vec3& dvel) {
            dpos = vel;
            dvel = gravity::compute(pos); // J2 oblate gravity
        };
        
        Vec3 k1_p, k1_v, k2_p, k2_v, k3_p, k3_v, k4_p, k4_v;
        orbit_deriv(state_.position, state_.velocity, k1_p, k1_v);
        orbit_deriv(state_.position + k1_p * (dt/2.0), state_.velocity + k1_v * (dt/2.0), k2_p, k2_v);
        orbit_deriv(state_.position + k2_p * (dt/2.0), state_.velocity + k2_v * (dt/2.0), k3_p, k3_v);
        orbit_deriv(state_.position + k3_p * dt, state_.velocity + k3_v * dt, k4_p, k4_v);
        
        state_.position += (k1_p + k2_p * 2.0 + k3_p * 2.0 + k4_p) * (dt / 6.0);
        state_.velocity += (k1_v + k2_v * 2.0 + k3_v * 2.0 + k4_v) * (dt / 6.0);
        state_.time += dt;
        
        // Attitude control in orbit (RCS approximation)
        if (cmd.gimbal_pitch != 0 || cmd.gimbal_yaw != 0) {
             // RCS thrust not modelled here, but attitude changes would be tracked
        }
    } else {
        // Atmospheric integration via 6-DOF
        state_ = integrator::rk4_step(state_, config_, current_stage_, cmd.throttle, cmd.gimbal_pitch, cmd.gimbal_yaw, dt);
    }
    
    // Acceleration estimate
    Vec3 acceleration = (state_.velocity - old_velocity) / dt;
    
    // Update propellant
    double mass_burned = old_mass - state_.mass;
    if (mass_burned > 0) {
        consumed_propellant_[current_stage_] += mass_burned;
    }
    
    handle_staging();
    
    eci_to_geodetic(state_.position, lat, lon, geodetic_altitude);
    
    if (state_.time > 10.0 && geodetic_altitude < 0.0) {
        simulation_complete_ = true; // Crash
    } else if (geodetic_altitude > 200000.0 && state_.velocity.norm() > 7400.0) {
        simulation_complete_ = true; // Orbit achieved
    }
    
    return build_telemetry(acceleration);
}

TelemetryFrame Simulation::build_telemetry(const Vec3& acceleration) {
    TelemetryFrame t;
    t.time = state_.time;
    eci_to_geodetic(state_.position, t.latitude, t.longitude, t.geodetic_altitude);
    t.altitude = t.geodetic_altitude;
    
    // Relative velocity
    Vec3 omega(0.0, 0.0, constants::EARTH_OMEGA);
    Vec3 v_rel = state_.velocity - omega.cross(state_.position);
    t.velocity_magnitude = v_rel.norm();
    t.acceleration_magnitude = acceleration.norm();
    
    auto atm = atmosphere::compute(t.altitude);
    t.mach_number = t.velocity_magnitude / atm.speed_of_sound;
    t.dynamic_pressure = aerodynamics::compute_dynamic_pressure(atm.density, t.velocity_magnitude);
    
    if (t.dynamic_pressure > max_q_) {
        max_q_ = t.dynamic_pressure;
    } else if (t.dynamic_pressure < max_q_ * 0.95 && max_q_ > 10000.0) {
        max_q_reached_ = true;
    }
    t.max_q_reached = max_q_reached_;
    t.max_q_value = max_q_;
    
    t.latitude *= constants::RAD_TO_DEG;
    t.longitude *= constants::RAD_TO_DEG;
    
    t.position_eci = state_.position;
    t.velocity_eci = state_.velocity;
    
    Vec3 euler = state_.attitude.to_euler();
    t.roll = euler.x * constants::RAD_TO_DEG;
    t.pitch = euler.y * constants::RAD_TO_DEG;
    t.yaw = euler.z * constants::RAD_TO_DEG;
    
    t.current_stage = current_stage_;
    t.mass = state_.mass;
    
    if (current_stage_ < (int)config_.stages.size()) {
        double total_prop = config_.stages[current_stage_].propellant_mass;
        t.propellant_remaining = total_prop > 0 ? (1.0 - consumed_propellant_[current_stage_] / total_prop) : 0.0;
        t.thrust = 0.0; // Needs detailed thrust reporting, set to 0 for telemetry abstraction
    } else {
        t.propellant_remaining = 0.0;
        t.thrust = 0.0;
    }
    
    t.structural_integrity = 1.0;
    t.crash_probability = 0.0;
    
    if (active_failure_ != FailureType::NONE) {
        t.structural_integrity -= failure_severity_ * 0.5;
        if (active_failure_ == FailureType::CROSSWIND) {
            t.crash_probability = failure_severity_ * 0.3;
        }
    }
    
    t.event = simulation_complete_ ? (t.altitude < 0 ? "CRASH" : "ORBIT_INSERTION") : "NOMINAL";
    
    // Great circle distance
    double lat1 = constants::LAUNCH_LATITUDE * constants::DEG_TO_RAD;
    double lon1 = constants::LAUNCH_LONGITUDE * constants::DEG_TO_RAD;
    double lat2 = t.latitude * constants::DEG_TO_RAD;
    double lon2 = t.longitude * constants::DEG_TO_RAD;
    double dlon = lon2 - lon1;
    double dlat = lat2 - lat1;
    double a = std::pow(std::sin(dlat/2), 2) + std::cos(lat1) * std::cos(lat2) * std::pow(std::sin(dlon/2), 2);
    double c = 2 * std::atan2(std::sqrt(a), std::sqrt(1-a));
    t.downrange_distance = constants::R_EARTH * c;
    
    return t;
}

void Simulation::handle_staging() {
    if (current_stage_ >= (int)config_.stages.size()) {
        simulation_complete_ = true;
        return;
    }
    
    const auto& stage = config_.stages[current_stage_];
    if (consumed_propellant_[current_stage_] >= stage.propellant_mass) {
        state_.mass -= stage.structural_mass * stage.count;
        current_stage_++;
        if (current_stage_ >= (int)config_.stages.size()) {
            simulation_complete_ = true;
        }
    }
}

Vec3 Simulation::eci_from_geodetic(double lat_rad, double lon_rad, double alt_m) {
    double e2 = 1.0 - std::pow(constants::R_POLAR / constants::R_EARTH, 2);
    double N = constants::R_EARTH / std::sqrt(1.0 - e2 * std::pow(std::sin(lat_rad), 2));
    
    double x = (N + alt_m) * std::cos(lat_rad) * std::cos(lon_rad);
    double y = (N + alt_m) * std::cos(lat_rad) * std::sin(lon_rad);
    double z = (N * (1.0 - e2) + alt_m) * std::sin(lat_rad);
    
    return Vec3(x, y, z);
}

void Simulation::eci_to_geodetic(const Vec3& pos, double& lat, double& lon, double& alt) {
    double e2 = 1.0 - std::pow(constants::R_POLAR / constants::R_EARTH, 2);
    double p = std::sqrt(pos.x * pos.x + pos.y * pos.y);
    
    lon = std::atan2(pos.y, pos.x);
    lat = std::atan2(pos.z, p * (1.0 - e2));
    
    double N = 0;
    for (int i = 0; i < 5; ++i) {
        N = constants::R_EARTH / std::sqrt(1.0 - e2 * std::pow(std::sin(lat), 2));
        lat = std::atan2(pos.z + e2 * N * std::sin(lat), p);
    }
    alt = p / std::cos(lat) - N;
}

} // namespace prakshep
