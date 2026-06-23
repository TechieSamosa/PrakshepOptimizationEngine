#pragma once

/**
 * @file simulation.hpp
 * @brief Top-level simulation engine for launch trajectory propagation.
 *
 * The Simulation class orchestrates all subsystems — gravity, atmosphere,
 * aerodynamics, propulsion, flight computer, and integrator — to advance
 * the rocket's 6-DOF state through time.
 *
 * Usage pattern:
 * @code
 *   prakshep::Simulation sim(prakshep::RocketType::PSLV_XL, 1750.0);
 *   while (!sim.is_complete()) {
 *       auto telem = sim.tick(1.0 / 60.0);
 *       // ... send telem to frontend or log it
 *   }
 * @endcode
 *
 * Features:
 *   - Tick-based deterministic simulation (fixed or variable dt)
 *   - Failure injection for Monte-Carlo analysis
 *   - TVC override for RL agent control
 *   - Automatic staging, fairing jettison, and abort detection
 *   - Rich telemetry output for visualisation
 *
 * @author  Prakshep GNC Team
 * @version 1.0.0
 */

#include "types.hpp"
#include "flight_computer.hpp"

#include <string>

namespace prakshep {

/**
 * @class Simulation
 * @brief Main simulation engine — integrates the full 6-DOF trajectory.
 *
 * Encapsulates the vehicle state, flight computer, staging logic, failure
 * model, and telemetry generation.  Each call to tick() advances the
 * simulation by dt seconds and returns a TelemetryFrame suitable for
 * real-time visualisation or logging.
 */
class Simulation {
public:
    // -- Construction & lifecycle -------------------------------------------

    /**
     * @brief Construct a simulation for the specified vehicle type.
     *
     * Initialises the vehicle on the Sriharikota launch pad in the
     * ECI frame, pointed vertically, at rest relative to the Earth's surface.
     *
     * @param type          Launch vehicle type (PSLV_XL, GSLV_MK2, LVM3).
     * @param payload_mass  Payload mass in kg (0 = use vehicle default).
     */
    Simulation(RocketType type, double payload_mass = 0);

    /**
     * @brief Advance the simulation by one time step.
     *
     * Executes the full simulation loop:
     *   1. Flight computer generates commands
     *   2. Integrator propagates state (RK4)
     *   3. Staging logic checks for separation
     *   4. Failure model applies perturbations
     *   5. Telemetry frame is constructed
     *
     * @param dt  Time step in seconds (default: 1/60 s for 60 Hz).
     * @return TelemetryFrame snapshot at the new time.
     */
    TelemetryFrame tick(double dt = 1.0 / 60.0);

    /**
     * @brief Reset the simulation to initial conditions.
     *
     * Returns the vehicle to the launch pad with all propellant and
     * clears any active failures.
     */
    void reset();

    // -- Failure injection --------------------------------------------------

    /**
     * @brief Inject a failure mode into the simulation.
     *
     * @param type     Type of failure to inject.
     * @param severity Severity factor [0, 1] where 1 = maximum effect.
     */
    void inject_failure(FailureType type, double severity = 1.0);

    /** @brief Clear all active failures (return to nominal). */
    void clear_failures();

    // -- TVC override (for RL agent or manual control) ----------------------

    /**
     * @brief Set TVC override values.
     *
     * Bypasses the autonomous flight computer's pitch programme.
     *
     * @param gimbal_pitch  Pitch gimbal angle (radians).
     * @param gimbal_yaw    Yaw gimbal angle (radians).
     * @param throttle      Throttle level [0, 1].
     */
    void set_tvc_override(double gimbal_pitch, double gimbal_yaw, double throttle);

    /** @brief Clear TVC override (return to autonomous mode). */
    void clear_tvc_override();

    // -- Accessors ----------------------------------------------------------

    /** @brief Get the current state vector (read-only). */
    const StateVector& get_state() const;

    /** @brief Check if the simulation has ended (orbit insertion, abort, or crash). */
    bool is_complete() const;

private:
    // -- Core state ---------------------------------------------------------
    RocketConfig   config_;           ///< Vehicle configuration
    StateVector    state_;            ///< Current 6-DOF state
    FlightComputer flight_computer_;  ///< Autonomous GNC system

    // -- Staging & progress -------------------------------------------------
    int    current_stage_       = 0;     ///< Index of the currently active stage
    double max_q_               = 0.0;   ///< Peak dynamic pressure seen so far (Pa)
    bool   max_q_reached_       = false; ///< True once Max-Q has been identified
    bool   simulation_complete_ = false; ///< True when the sim has terminated

    /**
     * @brief Per-stage consumed propellant tracking (kg).
     *
     * Array of 10 slots supports vehicles with up to 10 stages.
     * consumed_propellant_[i] accumulates the mass of propellant burned
     * by stage i.  When consumed_propellant_[i] >= stage.propellant_mass,
     * stage separation is triggered.
     */
    double consumed_propellant_[10] = {};

    // -- Failure model ------------------------------------------------------
    FailureType active_failure_  = FailureType::NONE; ///< Currently active failure
    double      failure_severity_ = 0.0;               ///< Failure severity [0, 1]

    // -- Internal methods ---------------------------------------------------

    /**
     * @brief Build a TelemetryFrame from the current state.
     *
     * Converts ECI state to geodetic coordinates, computes display angles,
     * Mach number, dynamic pressure, etc.
     *
     * @param acceleration  Net acceleration vector from the last integration step (m/s²).
     * @return Populated TelemetryFrame.
     */
    TelemetryFrame build_telemetry(const Vec3& acceleration);

    /**
     * @brief Handle staging logic (propellant depletion, separation).
     *
     * Checks if the current stage's propellant is exhausted and advances
     * to the next stage, updating mass and propulsion parameters.
     */
    void handle_staging();

    /**
     * @brief Convert geodetic coordinates to ECI position vector.
     *
     * Uses the WGS84 ellipsoid.  This is the inverse of eci_to_geodetic.
     *
     * @param lat_rad Geodetic latitude (radians).
     * @param lon_rad Geodetic longitude (radians).
     * @param alt_m   Altitude above the ellipsoid (m).
     * @return ECI position vector (m).
     */
    Vec3 eci_from_geodetic(double lat_rad, double lon_rad, double alt_m);

    /**
     * @brief Convert ECI position to geodetic coordinates.
     *
     * Uses an iterative (Bowring) algorithm on the WGS84 ellipsoid.
     *
     * @param pos  ECI position vector (m).
     * @param lat  [out] Geodetic latitude (radians).
     * @param lon  [out] Geodetic longitude (radians).
     * @param alt  [out] Altitude above the ellipsoid (m).
     */
    void eci_to_geodetic(const Vec3& pos, double& lat, double& lon, double& alt);
};

} // namespace prakshep
