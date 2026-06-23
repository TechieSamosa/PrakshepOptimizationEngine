#pragma once

/**
 * @file flight_computer.hpp
 * @brief Autonomous flight computer (GNC) for open-loop pitch programme
 *        and TVC override commands.
 *
 * The flight computer generates throttle and gimbal commands each tick.
 * In the default mode it follows a pre-programmed pitch schedule;
 * when a TVC override is active (e.g. from the RL agent or manual input)
 * the override values are passed through directly.
 *
 * The pitch programme is a simplified version of the gravity-turn
 * manoeuvre used by real launch vehicles:
 *   - Vertical ascent for the first ~15 seconds
 *   - Initiate pitch-over (kick) at ~15 s
 *   - Smooth pitch ramp toward horizontal (gravity turn)
 *   - Near-zero angle-of-attack above ~100 km
 *
 * @author  Prakshep GNC Team
 * @version 1.0.0
 */

#include "types.hpp"

namespace prakshep {

/**
 * @struct FlightCommand
 * @brief  Commands issued by the flight computer to the vehicle actuators.
 */
struct FlightCommand {
    double throttle     = 1.0;   ///< Throttle level [0, 1] (1 = full thrust)
    double gimbal_pitch = 0.0;   ///< TVC pitch deflection (radians)
    double gimbal_yaw   = 0.0;   ///< TVC yaw deflection (radians)
    bool   stage_separate = false; ///< Command stage separation this tick
    bool   abort        = false; ///< Command mission abort
};

/**
 * @class FlightComputer
 * @brief Generates flight commands (throttle, TVC, staging) each simulation tick.
 *
 * Supports two modes:
 *   1. **Autonomous**: Follows a pre-programmed pitch schedule and staging logic.
 *   2. **TVC Override**: An external agent (RL policy, human operator) directly
 *      commands throttle and gimbal angles.
 */
class FlightComputer {
public:
    /**
     * @brief Construct a flight computer for a given vehicle.
     * @param config  Reference to the vehicle configuration (must outlive this object).
     */
    explicit FlightComputer(const RocketConfig& config);

    /**
     * @brief Compute flight commands for the current state.
     *
     * If a TVC override is active, the override values are returned directly.
     * Otherwise the autonomous pitch programme generates commands.
     *
     * @param state         Current vehicle state.
     * @param current_stage Index of the active stage.
     * @return FlightCommand for this tick.
     */
    FlightCommand compute(const StateVector& state, int current_stage);

    /**
     * @brief Activate TVC override mode.
     *
     * When active, the flight computer's autonomous logic is bypassed
     * and the supplied gimbal/throttle values are used directly.
     *
     * @param gimbal_pitch  Desired pitch gimbal angle (radians).
     * @param gimbal_yaw    Desired yaw gimbal angle (radians).
     * @param throttle      Desired throttle level [0, 1].
     */
    void set_tvc_override(double gimbal_pitch, double gimbal_yaw, double throttle);

    /**
     * @brief Deactivate TVC override mode.
     *
     * Returns control to the autonomous pitch programme.
     */
    void clear_tvc_override();

private:
    const RocketConfig* config_;  ///< Pointer to vehicle configuration

    // -- TVC override state --------------------------------------------------
    bool   tvc_override_active_   = false; ///< True when external agent is commanding TVC
    double override_gimbal_pitch_ = 0.0;   ///< Override pitch gimbal (radians)
    double override_gimbal_yaw_   = 0.0;   ///< Override yaw gimbal (radians)
    double override_throttle_     = 1.0;   ///< Override throttle [0, 1]
    bool   is_pad_3_              = false; ///< True if launching from Kulashekarapatnam Pad 3

    /**
     * @brief Compute the target pitch angle from the pre-programmed schedule.
     *
     * The pitch programme defines the desired vehicle pitch as a function
     * of mission elapsed time and altitude.  It implements a simplified
     * gravity-turn profile:
     *
     *   t < 15 s:            90° (vertical)
     *   15 s ≤ t < 30 s:     Pitch kick — ramp from 90° to ~85°
     *   30 s ≤ t:            Gradual pitch-down toward horizontal
     *                        (rate decreases with altitude)
     *
     * @param time     Mission elapsed time (s).
     * @param altitude Geometric altitude above MSL (m).
     * @return Target pitch angle (radians from local horizontal).
     */
    double compute_pitch_angle(double time, double altitude) const;
};

} // namespace prakshep
