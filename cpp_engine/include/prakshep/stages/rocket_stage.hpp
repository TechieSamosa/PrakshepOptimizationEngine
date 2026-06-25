/**
 * @file rocket_stage.hpp
 * @brief Abstract base class defining the modular interface for all rocket stages.
 *
 * Every stage in the Prakshep engine — solid, liquid, cryo, hybrid — must inherit
 * from this interface. The flight computer treats every stage as a generic
 * "attachable module" and queries it for three canonical outputs:
 *
 *   1. current_mass     — total instantaneous mass (structural + remaining propellant)
 *   2. thrust_vector     — 3-component force in the body frame (N)
 *   3. com_offset        — centre-of-mass offset from the geometric attach-point (m)
 *
 * This decouples the integrator from any stage-specific propulsion physics
 * (grain regression, turbopump spool-up, SITVC, gimbal TVC, etc.) and allows
 * hot-swapping stages at the UI level.
 *
 * @author  Prakshep GNC Team
 * @version 2.0.0
 */

#pragma once

#include "prakshep/types.hpp"

namespace prakshep {
namespace stages {

/**
 * @class RocketStage
 * @brief Pure virtual interface for a modular, swappable rocket stage.
 *
 * Lifecycle:
 *   1. Construct with configuration constants.
 *   2. Call ignite() to transition from IDLE → BURNING.
 *   3. The flight computer calls update(dt) every integration step.
 *   4. When propellant is exhausted the stage transitions to BURNOUT.
 *   5. The flight computer calls separate() before dropping the stage.
 *
 * Subclasses must implement all pure virtual methods.
 */
class RocketStage {
public:
    // -----------------------------------------------------------------------
    //  Stage lifecycle states
    // -----------------------------------------------------------------------
    enum class State {
        IDLE,       ///< Pre-ignition — stage is inert
        BURNING,    ///< Active propulsive burn
        BURNOUT,    ///< Propellant exhausted, awaiting separation
        SEPARATED   ///< Physically detached from the stack
    };

    virtual ~RocketStage() = default;

    // -----------------------------------------------------------------------
    //  Core physics interface (must be implemented by every stage)
    // -----------------------------------------------------------------------

    /**
     * @brief Advance the stage physics by one time step.
     *
     * This is where mass depletion, thrust curve evaluation, TVC deflection,
     * and any stage-specific physics (grain regression, turbopump transients,
     * ullage settling, etc.) are computed.
     *
     * @param dt              Integration time step (s)
     * @param pitch_command   Normalised pitch steering command [-1, +1]
     * @param yaw_command     Normalised yaw steering command [-1, +1]
     * @param altitude        Current altitude ASL (m), for Isp altitude correction
     */
    virtual void update(double dt,
                        double pitch_command,
                        double yaw_command,
                        double altitude) = 0;

    /**
     * @brief Ignite the stage — transition from IDLE to BURNING.
     *
     * For solid motors this is irreversible (you cannot shut down a solid).
     * For liquid stages the engine may be shut down and re-ignited.
     */
    virtual void ignite() = 0;

    /**
     * @brief Notify the stage that it has been physically separated.
     */
    virtual void separate() = 0;

    // -----------------------------------------------------------------------
    //  Canonical output ports (flight computer reads these)
    // -----------------------------------------------------------------------

    /** @brief Total instantaneous mass: structural + remaining propellant (kg). */
    virtual double get_mass() const = 0;

    /**
     * @brief Thrust force vector in the **body frame** (N).
     *
     * For an un-deflected motor, thrust is purely along +X_body (axial).
     * TVC / SITVC deflection introduces Y and Z components.
     */
    virtual Vec3 get_thrust_vector() const = 0;

    /**
     * @brief Centre-of-mass offset from the stage's geometric attach point (m).
     *
     * As propellant is consumed the CoM migrates — solid motors regress from
     * the fore-end, liquids drain from the bottom. The flight computer uses
     * this to compute the total-vehicle CoM for torque calculations.
     */
    virtual Vec3 get_com_offset() const = 0;

    /** @brief Current lifecycle state. */
    virtual State get_state() const = 0;

    /** @brief Propellant remaining as a fraction [0.0, 1.0]. */
    virtual double get_propellant_fraction() const = 0;

    /** @brief Human-readable stage name (e.g. "PS1 - S139"). */
    virtual std::string get_name() const = 0;
};

} // namespace stages
} // namespace prakshep
