/**
 * @file pslv_stage_4.hpp
 * @brief PSLV 4th Stage (PS4) — Twin Earth-Storable Liquid Engines.
 *
 * The PS4 is the final stage of the PSLV. It is powered by twin L-2.5 
 * liquid engines burning Monomethylhydrazine (MMH) and Mixed Oxides 
 * of Nitrogen (MON3). 
 *
 * This stage features restart capability, allowing it to shut down 
 * its main engines (coasting phase) and re-ignite them later to 
 * circularize orbits or inject multiple payloads into different orbits.
 *
 * @author  Prakshep GNC Team
 * @version 2.0.0
 */

#pragma once

#include "prakshep/stages/rocket_stage.hpp"
#include "prakshep/types.hpp"
#include <string>

namespace prakshep {
namespace stages {

class PSLV_Stage4 : public RocketStage {
public:
    // =======================================================================
    //  PS4 Twin Engine Constants
    // =======================================================================

    /// Propellant mass (MMH + MON3) for PS4 (varies by mission, typ. 2000-2500 kg)
    static constexpr double PROPELLANT_MASS       = 2500.0;      // kg

    /// Structural dry mass (tanks, twin engines, avionics, payload adapter)
    static constexpr double STRUCTURAL_MASS       = 920.0;       // kg

    /// Vacuum thrust (COMBINED for twin engines)
    static constexpr double THRUST_VACUUM         = 14.6e3;      // N (2 x 7.3 kN)

    /// Specific impulse in vacuum
    static constexpr double ISP_VACUUM            = 308.0;       // seconds

    /// Stage length
    static constexpr double STAGE_LENGTH          = 2.6;         // metres

    /// Stage diameter
    static constexpr double STAGE_DIAMETER        = 2.0;         // metres

    // =======================================================================
    //  Payload Deployment Constants
    // =======================================================================

    /// Forward kick velocity given to the satellite upon separation (m/s)
    static constexpr double PAYLOAD_SEPARATION_VEL = 1.2;

    // =======================================================================
    //  Constructor
    // =======================================================================

    PSLV_Stage4(double payload_mass);

    // =======================================================================
    //  RocketStage interface
    // =======================================================================

    void   update(double dt, double pitch_command, double yaw_command, double altitude) override;
    void   ignite() override;
    void   separate() override;

    double      get_mass() const override;
    Vec3        get_thrust_vector() const override;
    Vec3        get_com_offset() const override;
    State       get_state() const override;
    double      get_propellant_fraction() const override;
    std::string get_name() const override;

    // =======================================================================
    //  PS4 Specific Operations
    // =======================================================================

    /** @brief Shutdown the main engines and enter COASTING state. */
    void shutdown_engines();

    /** 
     * @brief Deploy the primary payload.
     * Subtracts payload mass and returns the forward velocity kick vector.
     */
    Vec3 deploy_payload();
    
    bool is_payload_deployed() const { return m_payload_deployed; }

private:
    State  m_state;
    double m_propellant_remaining;
    double m_payload_mass;
    bool   m_payload_deployed;

    Vec3   m_current_thrust_vector;
    Vec3   m_current_com_offset;
};

} // namespace stages
} // namespace prakshep
