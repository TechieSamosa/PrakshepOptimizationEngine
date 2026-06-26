/**
 * @file booster_module.hpp
 * @brief Abstract interface for radially-attached booster modules (strap-ons).
 *
 * BoosterModule extends the core RocketStage interface with capabilities
 * specific to strap-on boosters that are attached radially to a central
 * core stage:
 *
 *   1. **Radial Mounting**: Boosters are positioned at a specific angular
 *      position and radial distance from the core axis. Their thrust vectors
 *      pass through a line offset from the vehicle's principal axis.
 *
 *   2. **Staggered Ignition**: Not all boosters ignite at the same time.
 *      PSLV-XL ignites 4 ground-lit at T-0 and 2 air-lit at T+25s.
 *      Each booster has an ignition delay timer.
 *
 *   3. **Asymmetric Mass Effects**: As ground-lit boosters deplete faster
 *      than air-lit ones, the vehicle's global CoM shifts dynamically.
 *      The flight computer must query each booster for its current mass
 *      and mounting position to recompute the composite CoM.
 *
 *   4. **Radial Separation**: At burnout, the booster is severed from the
 *      rigid-body tree and given a lateral kick velocity (separation springs
 *      + retro-rockets) to clear the core vehicle.
 *
 * @author  Prakshep GNC Team
 * @version 2.0.0
 */

#pragma once

#include "prakshep/stages/rocket_stage.hpp"
#include "prakshep/types.hpp"

namespace prakshep {
namespace stages {

/**
 * @class BoosterModule
 * @brief Extended interface for radially-mounted strap-on boosters.
 *
 * Inherits the full RocketStage interface and adds:
 *   - Radial mounting geometry (angle, distance, height offset)
 *   - Staggered ignition scheduling
 *   - Separation dynamics with lateral impulse
 *
 * All strap-on implementations (PSOM, PSOM-XL, S200, GEM-60, etc.)
 * inherit from this class.
 */
class BoosterModule : public RocketStage {
public:
    // -----------------------------------------------------------------------
    //  Booster mounting group classification
    // -----------------------------------------------------------------------
    enum class IgnitionGroup {
        GROUND_LIT,   ///< Ignites at T-0 on the pad
        AIR_LIT       ///< Ignites after a programmed delay in flight
    };

    virtual ~BoosterModule() = default;

    // -----------------------------------------------------------------------
    //  Radial mounting interface
    // -----------------------------------------------------------------------

    /**
     * @brief Set the angular mounting position around the core stage.
     *
     * @param angle_rad  Azimuthal angle from the +Y body axis (radians).
     *                   0 = "north", π/2 = "east", π = "south", etc.
     * @param radial_distance_m  Distance from core centreline to booster
     *                           centreline (m).
     * @param height_offset_m    Axial offset from core stage geometric
     *                           centre (m). Negative = below centre.
     */
    virtual void set_mount_geometry(double angle_rad,
                                    double radial_distance_m,
                                    double height_offset_m) = 0;

    /** @brief Get the booster's mounting position in the core body frame (m). */
    virtual Vec3 get_mount_position() const = 0;

    /** @brief Get the mounting azimuth angle (radians). */
    virtual double get_mount_angle() const = 0;

    // -----------------------------------------------------------------------
    //  Staggered ignition interface
    // -----------------------------------------------------------------------

    /**
     * @brief Assign this booster to an ignition group.
     *
     * GROUND_LIT boosters ignite at T-0 (ignition_delay = 0).
     * AIR_LIT boosters ignite after a programmed delay.
     *
     * @param group           Ignition group classification
     * @param delay_seconds   Ignition delay from T-0 (s). Must be 0 for GROUND_LIT.
     */
    virtual void set_ignition_schedule(IgnitionGroup group,
                                       double delay_seconds) = 0;

    /** @brief Get this booster's ignition group. */
    virtual IgnitionGroup get_ignition_group() const = 0;

    /** @brief Get the programmed ignition delay from T-0 (s). */
    virtual double get_ignition_delay() const = 0;

    /**
     * @brief Check the mission elapsed time and auto-ignite if scheduled.
     *
     * Called by the flight computer every timestep. If mission_time >= delay
     * and the booster is still IDLE, it will self-ignite.
     *
     * @param mission_time  Current time from T-0 (s)
     */
    virtual void check_ignition_schedule(double mission_time) = 0;

    // -----------------------------------------------------------------------
    //  Separation dynamics interface
    // -----------------------------------------------------------------------

    /**
     * @brief Trigger booster decoupling from the core vehicle.
     *
     * This severs the rigid-body constraint and computes the lateral
     * separation impulse. After this call:
     *   - get_state() returns SEPARATED
     *   - get_thrust_vector() returns zero
     *   - get_separation_velocity() returns the kick velocity vector
     *
     * The separation velocity models the combined effect of:
     *   - Radial separation springs (pushing outward)
     *   - Small retro-rockets on the booster nose cone (pushing aft + outward)
     *   - Aerodynamic drag differential (if still in atmosphere)
     *
     * @return Lateral separation velocity vector in the body frame (m/s).
     */
    virtual Vec3 triggerDecouple() = 0;

    /**
     * @brief Get the separation kick velocity (valid only after triggerDecouple).
     *
     * This is the Δv_lateral applied to the booster casing at the instant
     * of separation. The frontend uses this to animate the tumbling debris.
     *
     * @return Separation velocity in body frame (m/s). Zero if not yet separated.
     */
    virtual Vec3 get_separation_velocity() const = 0;

    // -----------------------------------------------------------------------
    //  Off-axis thrust contribution
    // -----------------------------------------------------------------------

    /**
     * @brief Get the torque this booster exerts about the vehicle CoM.
     *
     * Because strap-on boosters are mounted off-axis, their thrust creates
     * a moment about the vehicle's centre of mass:
     *
     *     τ = r × F
     *
     * where r is the vector from the vehicle CoM to the booster's thrust
     * application point, and F is the booster's thrust vector in body frame.
     *
     * @param vehicle_com  Current vehicle CoM position in body frame (m)
     * @return Torque vector in body frame (N·m)
     */
    virtual Vec3 compute_torque(const Vec3& vehicle_com) const = 0;
};

} // namespace stages
} // namespace prakshep
