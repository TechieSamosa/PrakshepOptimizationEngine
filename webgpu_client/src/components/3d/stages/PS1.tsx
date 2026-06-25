'use client';

/**
 * PS1.tsx — PSLV 1st Stage (S139 Solid Motor) WebGPU/R3F Component
 *
 * This is a modular, swappable 3D component representing the PS1 core stage
 * of the Polar Satellite Launch Vehicle. It is designed to be dynamically
 * stacked with other stage components via the `mountingPosition` prop.
 *
 * === GEOMETRY ===
 *   - S139 core cylinder: 2.8m diameter, 20.34m length (white casing)
 *   - Fixed nozzle assembly: convergent-divergent bell, 2.1m exit diameter
 *   - 6× PSOM strap-on booster attach hardpoints (null objects/refs)
 *   - Fore-end interstage adapter ring
 *
 * === SITVC VISUAL EFFECTS ===
 *   When the backend sends non-zero pitch/yaw commands during the PS1 burn,
 *   the exhaust plume visually deflects to represent the SITVC fluid injection.
 *   A secondary "shock diamond" particle stream appears asymmetrically on the
 *   side where strontium perchlorate is being injected.
 *
 * === SWAPPABLE ARCHITECTURE ===
 *   This component exposes `strapOnMountRefs` — six THREE.Group references
 *   positioned around the core cylinder at the exact angular offsets where
 *   the PSOMs (or PSLVs strap-on solid boosters) attach.
 */

import React, { useRef, useMemo, useEffect } from 'react';
import { useFrame } from '@react-three/fiber';
import { Sparkles } from '@react-three/drei';
import * as THREE from 'three';
import { useTelemetryStore } from '@/store/telemetryStore';

// ===========================================================================
//  Physical dimensions (metres, matching the C++ S139 constants)
// ===========================================================================

const S139_LENGTH       = 20.34;    // Core stage length (m)
const S139_RADIUS       = 1.4;     // Outer radius (diameter = 2.8m)
const NOZZLE_EXIT_R     = 1.05;    // Nozzle exit radius (diameter = 2.1m)
const NOZZLE_THROAT_R   = 0.45;    // Nozzle throat radius
const NOZZLE_LENGTH     = 3.5;     // Nozzle divergent section length
const INTERSTAGE_HEIGHT = 1.2;     // Fore-end interstage adapter height
const STRAP_ON_RADIUS   = 2.4;     // Radial distance from core axis to PSOM centre

// Angular positions of the 6 PSOM strap-on boosters (degrees)
const PSOM_ANGLES = [0, 60, 120, 180, 240, 300];

// ===========================================================================
//  Props interface
// ===========================================================================

interface PS1Props {
  /** 3D position offset for stacking this stage in a vehicle assembly */
  mountingPosition?: [number, number, number];

  /** Whether this stage is currently the active (burning) stage */
  isActive?: boolean;

  /** Callback to provide strap-on mount refs to the parent assembler */
  onMountsReady?: (refs: React.RefObject<THREE.Group | null>[]) => void;
}

// ===========================================================================
//  SITVC Exhaust Plume Sub-Component
// ===========================================================================

/**
 * Renders the S139 exhaust plume with SITVC-driven asymmetric deflection.
 *
 * The plume cone tilts based on the pitch/yaw telemetry data to visually
 * represent the oblique shock interaction from strontium perchlorate injection.
 */
function SITVCExhaust({ isActive }: { isActive: boolean }) {
  const plumeRef = useRef<THREE.Group>(null);
  const shockRef = useRef<THREE.Group>(null);
  const data = useTelemetryStore((state) => state.data);

  useFrame(() => {
    if (!plumeRef.current || !data) return;

    if (isActive && data.current_stage <= 1) {
      // SITVC deflection: tilt the plume opposite to the steering direction.
      // When SITVC injects on one side, the exhaust deflects the other way.
      // The thrust vector tilts toward the injection, but the visible plume
      // deflects AWAY from the injection (the shock pushes the flow laterally).
      const pitchDeflection = -data.pitch * 0.02;  // Scale down for visual
      const yawDeflection   = -data.yaw   * 0.02;

      plumeRef.current.rotation.x = THREE.MathUtils.lerp(
        plumeRef.current.rotation.x,
        pitchDeflection,
        0.15  // Smooth interpolation (SITVC response is not instant)
      );
      plumeRef.current.rotation.z = THREE.MathUtils.lerp(
        plumeRef.current.rotation.z,
        yawDeflection,
        0.15
      );

      // Shock diamond injection point — appears on the side of injection
      if (shockRef.current) {
        const injectionIntensity = Math.abs(data.pitch) + Math.abs(data.yaw);
        const normalised = Math.min(injectionIntensity / 5.0, 1.0);
        shockRef.current.scale.setScalar(normalised);
        shockRef.current.visible = normalised > 0.05;

        // Position the shock effect on the injection side
        shockRef.current.position.x = data.pitch * 0.03;
        shockRef.current.position.z = data.yaw * 0.03;
      }
    } else {
      // Stage not burning — collapse plume
      plumeRef.current.rotation.x = 0;
      plumeRef.current.rotation.z = 0;
      if (shockRef.current) shockRef.current.visible = false;
    }
  });

  if (!isActive) return null;

  return (
    <>
      {/* Main exhaust plume cone (deflectable) */}
      <group ref={plumeRef} position={[0, -(S139_LENGTH / 2 + NOZZLE_LENGTH + 2), 0]}>
        {/* Core plume — bright orange/yellow */}
        <Sparkles
          count={600}
          scale={[3, 12, 3]}
          size={18}
          speed={3.5}
          opacity={0.9}
          color="#FFB300"
        />
        {/* Outer plume envelope — dimmer, wider */}
        <Sparkles
          count={300}
          scale={[5, 18, 5]}
          size={10}
          speed={2.0}
          opacity={0.4}
          color="#FF6600"
        />
      </group>

      {/* SITVC injection shock diamonds (asymmetric, appears during steering) */}
      <group ref={shockRef} position={[0, -(S139_LENGTH / 2 + NOZZLE_LENGTH + 1), 0]}>
        <Sparkles
          count={150}
          scale={[1.5, 4, 1.5]}
          size={8}
          speed={5.0}
          opacity={0.7}
          color="#FFFFFF"
        />
      </group>
    </>
  );
}

// ===========================================================================
//  Main PS1 Component
// ===========================================================================

export default function PS1({
  mountingPosition = [0, 0, 0],
  isActive = false,
  onMountsReady,
}: PS1Props) {
  const groupRef = useRef<THREE.Group>(null);

  // Create refs for the 6 PSOM strap-on hardpoints
  const strapOnMountRefs = useMemo(() => {
    return PSOM_ANGLES.map(() => React.createRef<THREE.Group>());
  }, []);

  // Notify parent that mount refs are ready
  useEffect(() => {
    if (onMountsReady) {
      onMountsReady(strapOnMountRefs);
    }
  }, [onMountsReady, strapOnMountRefs]);

  // Propellant depletion visual: darken the casing as propellant burns
  const data = useTelemetryStore((state) => state.data);
  const propRemaining = data && data.current_stage <= 1
    ? data.propellant_remaining
    : 1.0;

  // Casing colour transitions from white (full) to slightly grey (depleted)
  const casingColor = useMemo(() => {
    const brightness = 0.7 + 0.3 * propRemaining;
    return new THREE.Color(brightness, brightness, brightness);
  }, [propRemaining]);

  return (
    <group ref={groupRef} position={mountingPosition}>

      {/* ================================================================= */}
      {/* S139 Core Cylinder (White HTPB Casing)                            */}
      {/* ================================================================= */}
      <mesh position={[0, 0, 0]}>
        <cylinderGeometry args={[S139_RADIUS, S139_RADIUS, S139_LENGTH, 32]} />
        <meshStandardMaterial
          color={casingColor}
          roughness={0.35}
          metalness={0.15}
        />
      </mesh>

      {/* Maroon band markings (2 bands typical on S139) */}
      <mesh position={[0, S139_LENGTH * 0.2, 0]}>
        <cylinderGeometry args={[S139_RADIUS + 0.01, S139_RADIUS + 0.01, 0.3, 32]} />
        <meshStandardMaterial color="#8B0000" roughness={0.5} />
      </mesh>
      <mesh position={[0, -S139_LENGTH * 0.15, 0]}>
        <cylinderGeometry args={[S139_RADIUS + 0.01, S139_RADIUS + 0.01, 0.3, 32]} />
        <meshStandardMaterial color="#8B0000" roughness={0.5} />
      </mesh>

      {/* ================================================================= */}
      {/* Fixed Nozzle Assembly (Convergent-Divergent Bell)                  */}
      {/* ================================================================= */}

      {/* Nozzle convergent section (narrows from casing to throat) */}
      <mesh position={[0, -(S139_LENGTH / 2 + 0.5), 0]}>
        <cylinderGeometry args={[NOZZLE_THROAT_R, S139_RADIUS, 1.0, 32]} />
        <meshStandardMaterial color="#333333" roughness={0.7} metalness={0.6} />
      </mesh>

      {/* Nozzle divergent section (expands from throat to exit) */}
      <mesh position={[0, -(S139_LENGTH / 2 + 1.0 + NOZZLE_LENGTH / 2), 0]}>
        <cylinderGeometry args={[NOZZLE_EXIT_R, NOZZLE_THROAT_R, NOZZLE_LENGTH, 32]} />
        <meshStandardMaterial color="#2a2a2a" roughness={0.8} metalness={0.5} />
      </mesh>

      {/* Nozzle exit ring (bright metal lip) */}
      <mesh position={[0, -(S139_LENGTH / 2 + 1.0 + NOZZLE_LENGTH), 0]}>
        <torusGeometry args={[NOZZLE_EXIT_R, 0.05, 8, 32]} />
        <meshStandardMaterial color="#999999" metalness={0.8} roughness={0.2} />
      </mesh>

      {/* ================================================================= */}
      {/* Fore-End Interstage Adapter Ring                                   */}
      {/* Connects PS1 to the PS2 (2nd stage) above.                        */}
      {/* ================================================================= */}
      <mesh position={[0, S139_LENGTH / 2 + INTERSTAGE_HEIGHT / 2, 0]}>
        <cylinderGeometry args={[S139_RADIUS + 0.05, S139_RADIUS + 0.05, INTERSTAGE_HEIGHT, 32]} />
        <meshStandardMaterial color="#555555" roughness={0.6} metalness={0.4} />
      </mesh>

      {/* ================================================================= */}
      {/* 6× PSOM Strap-On Booster Hardpoint Mounts                         */}
      {/*                                                                   */}
      {/* These are empty <group> nodes positioned at the exact angular      */}
      {/* offsets where the PSOM solid boosters will attach. A parent         */}
      {/* vehicle assembler component can bind PSOM stage components to      */}
      {/* these refs for full modular composition.                           */}
      {/* ================================================================= */}
      {PSOM_ANGLES.map((angleDeg, i) => {
        const rad = THREE.MathUtils.degToRad(angleDeg);
        const x = Math.cos(rad) * STRAP_ON_RADIUS;
        const z = Math.sin(rad) * STRAP_ON_RADIUS;
        return (
          <group
            key={`psom-mount-${i}`}
            ref={strapOnMountRefs[i]}
            position={[x, -S139_LENGTH * 0.1, z]}
          >
            {/* Visual hardpoint indicator (small cylinder) */}
            <mesh>
              <cylinderGeometry args={[0.08, 0.08, 0.4, 8]} />
              <meshStandardMaterial color="#444444" metalness={0.7} roughness={0.3} />
            </mesh>
          </group>
        );
      })}

      {/* ================================================================= */}
      {/* SITVC Exhaust Visual FX                                           */}
      {/* ================================================================= */}
      <SITVCExhaust isActive={isActive} />

    </group>
  );
}
