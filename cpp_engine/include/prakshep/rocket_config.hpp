#pragma once

/**
 * @file rocket_config.hpp
 * @brief Factory functions for ISRO launch vehicle configurations.
 *
 * Provides pre-built RocketConfig instances for PSLV-XL, GSLV Mk II,
 * and LVM3 with realistic stage parameters, drag tables, and geometry.
 *
 * All mass, thrust, and Isp values are based on publicly available ISRO
 * mission data.  Where exact values are not published, engineering estimates
 * are used.
 *
 * @author  Prakshep GNC Team
 * @version 1.0.0
 */

#include "types.hpp"

namespace prakshep {
namespace rocket_config {

/**
 * @brief Create a PSLV-XL configuration.
 *
 * The PSLV-XL is ISRO's workhorse polar-orbit launcher.  It is a
 * four-stage vehicle with 6 strap-on solid boosters (PSOM-XL):
 *
 *   - Stage 0: 6× PSOM-XL solid strap-ons (4 ground-lit, 2 air-lit)
 *   - Stage 1: PS1 — S139 solid motor (core first stage)
 *   - Stage 2: PS2 — Vikas liquid engine (L40 stage)
 *   - Stage 3: PS3 — S7 solid motor (third stage)
 *   - Stage 4: PS4 — 2× L-2-5 liquid engines (fourth stage)
 *
 * @param payload_mass  Payload mass in kg (default: 1750 kg to SSO).
 * @return Complete RocketConfig for PSLV-XL.
 */
RocketConfig create_pslv_xl(double payload_mass = 1750.0);

/**
 * @brief Create a GSLV Mk II configuration.
 *
 * The GSLV Mk II is ISRO's medium-lift launcher for GTO payloads.
 * It uses a cryogenic upper stage (CUS) with an indigenous CE-7.5 engine.
 *
 *   - Stage 0: 4× L40 liquid strap-on boosters
 *   - Stage 1: S139 solid motor (core first stage)
 *   - Stage 2: GS2 — Vikas liquid engine
 *   - Stage 3: CUS — CE-7.5 cryogenic engine (LOX/LH2)
 *
 * @param payload_mass  Payload mass in kg (default: 2500 kg to GTO).
 * @return Complete RocketConfig for GSLV Mk II.
 */
RocketConfig create_gslv_mk2(double payload_mass = 2500.0);

/**
 * @brief Create an LVM3 (GSLV Mk III) configuration.
 *
 * LVM3 is ISRO's heavy-lift launch vehicle, featuring two solid strap-on
 * boosters (S200), a liquid core stage (L110), and a cryogenic upper
 * stage (C25) with the CE-20 engine.
 *
 *   - Stage 0: 2× S200 solid strap-on boosters
 *   - Stage 1: L110 — 2× Vikas engines (liquid core)
 *   - Stage 2: C25 — CE-20 cryogenic engine (LOX/LH2)
 *
 * @param payload_mass  Payload mass in kg (default: 4000 kg to GTO).
 * @return Complete RocketConfig for LVM3.
 */
RocketConfig create_lvm3(double payload_mass = 4000.0);

/**
 * @brief Create a rocket configuration by type enum.
 *
 * Dispatches to the appropriate factory function based on the RocketType.
 *
 * @param type          Desired launch vehicle type.
 * @param payload_mass  Payload mass in kg.
 * @return Complete RocketConfig for the requested vehicle.
 */
RocketConfig create(RocketType type, double payload_mass);

} // namespace rocket_config
} // namespace prakshep
