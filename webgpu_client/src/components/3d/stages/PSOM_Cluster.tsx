'use client';

/**
 * PSOM_Cluster.tsx — PSLV Strap-On Motor Cluster WebGPU/R3F Component
 *
 * This component dynamically instantiates the correct number of PSOM-XL
 * booster meshes based on the selected PSLV variant configuration:
 *
 *   PSLV-CA: 0 boosters (component renders nothing)
 *   PSLV-DL: 2 boosters at 0°, 180°
 *   PSLV-QL: 4 boosters at 0°, 90°, 180°, 270°
 *   PSLV-XL: 6 boosters at 0°, 90°, 180°, 270° (ground-lit) + 45°, 225° (air-lit)
 *
 * === PROCEDURAL SEPARATION ANIMATION ===
 *
 * When the backend fires a `booster_sep_ground` or `booster_sep_air` event
 * via WebSocket telemetry, the specific booster instances decouple from the
 * core and receive a lateral kick velocity + tumble angular velocity.
 *
 * The separation animation runs entirely on the GPU frame clock using
 * useFrame() — no React state updates during the tumbling phase.
 *
 * === MOUNTING ===
 *
 * Each PSOM mesh is snapped to a radial position around the PS1 core
 * at the mount radius (2.4m from core axis) and aligned with the core's
 * axial direction. The `clusterMountOffset` prop allows the parent
 * vehicle assembler to position the entire cluster relative to the PS1.
 */

import React, { useRef, useMemo, useState, useEffect } from 'react';
import { useFrame } from '@react-three/fiber';
import { Sparkles } from '@react-three/drei';
import * as THREE from 'three';
import { useTelemetryStore } from '@/store/telemetryStore';

// ===========================================================================
//  Physical dimensions (matching C++ PSLV_StrapOn constants)
// ===========================================================================

const PSOM_LENGTH      = 12.19;   // Booster length (m)
const PSOM_RADIUS      = 0.5;    // Outer radius (diameter = 1.0m)
const NOSE_CONE_LENGTH = 1.5;    // Nose cone length (m)
const NOZZLE_LENGTH    = 1.2;    // Nozzle divergent section
const NOZZLE_EXIT_R    = 0.4;    // Nozzle exit radius
const NOZZLE_THROAT_R  = 0.2;    // Nozzle throat radius
const MOUNT_RADIUS     = 2.4;    // Radial distance from core axis (m)
const MOUNT_HEIGHT     = -2.0;   // Axial offset below PS1 centre (m)

// Separation physics (matching C++ constants)
const SEP_VEL_RADIAL = 3.0;     // m/s outward
const SEP_VEL_AXIAL  = -1.5;    // m/s aft (negative Y in scene space)
const SEP_TUMBLE     = 0.4;     // rad/s

// ===========================================================================
//  PSLV variant booster configurations
// ===========================================================================

interface BoosterConfig {
  id: number;
  angleDeg: number;
  group: 'ground' | 'air';
}

const VARIANT_CONFIGS: Record<string, BoosterConfig[]> = {
  'PSLV-CA': [],
  'PSLV-DL': [
    { id: 0, angleDeg: 0,   group: 'ground' },
    { id: 1, angleDeg: 180, group: 'ground' },
  ],
  'PSLV-QL': [
    { id: 0, angleDeg: 0,   group: 'ground' },
    { id: 1, angleDeg: 90,  group: 'ground' },
    { id: 2, angleDeg: 180, group: 'ground' },
    { id: 3, angleDeg: 270, group: 'ground' },
  ],
  'PSLV-XL': [
    { id: 0, angleDeg: 0,   group: 'ground' },
    { id: 1, angleDeg: 90,  group: 'ground' },
    { id: 2, angleDeg: 180, group: 'ground' },
    { id: 3, angleDeg: 270, group: 'ground' },
    { id: 4, angleDeg: 45,  group: 'air' },
    { id: 5, angleDeg: 225, group: 'air' },
  ],
};

// ===========================================================================
//  Props
// ===========================================================================

interface PSOM_ClusterProps {
  /** PSLV variant — determines number and arrangement of boosters */
  variant?: string;

  /** Position offset for the entire cluster relative to parent */
  clusterMountOffset?: [number, number, number];
}

// ===========================================================================
//  Individual PSOM Booster Mesh
// ===========================================================================

interface PSOMBoosterProps {
  config: BoosterConfig;
  isSeparated: boolean;
  separationTime: number;  // seconds since separation event
  isBurning: boolean;
}

function PSOMBooster({ config, isSeparated, separationTime, isBurning }: PSOMBoosterProps) {
  const meshRef = useRef<THREE.Group>(null);

  // Pre-compute the mount position
  const mountPos = useMemo(() => {
    const rad = THREE.MathUtils.degToRad(config.angleDeg);
    return new THREE.Vector3(
      Math.cos(rad) * MOUNT_RADIUS,
      MOUNT_HEIGHT,
      Math.sin(rad) * MOUNT_RADIUS
    );
  }, [config.angleDeg]);

  // Pre-compute separation velocity direction (radially outward + aft)
  const sepVelocity = useMemo(() => {
    const rad = THREE.MathUtils.degToRad(config.angleDeg);
    return new THREE.Vector3(
      Math.cos(rad) * SEP_VEL_RADIAL,
      SEP_VEL_AXIAL,
      Math.sin(rad) * SEP_VEL_RADIAL
    );
  }, [config.angleDeg]);

  // Tumble axis (perpendicular to separation direction)
  const tumbleAxis = useMemo(() => {
    return new THREE.Vector3(
      Math.sin(THREE.MathUtils.degToRad(config.angleDeg)),
      0.3,
      -Math.cos(THREE.MathUtils.degToRad(config.angleDeg))
    ).normalize();
  }, [config.angleDeg]);

  useFrame((_, delta) => {
    if (!meshRef.current) return;

    if (isSeparated) {
      // Procedural tumbling animation:
      // Position drifts along separation velocity vector
      meshRef.current.position.addScaledVector(sepVelocity, delta);

      // Tumble rotation
      meshRef.current.rotateOnAxis(tumbleAxis, SEP_TUMBLE * delta);
    }
  });

  // Casing colour: white with maroon stripe
  const casingColor = config.group === 'air' ? '#E8E8E8' : '#FFFFFF';

  return (
    <group
      ref={meshRef}
      position={isSeparated ? undefined : [mountPos.x, mountPos.y, mountPos.z]}
    >
      {/* PSOM-XL cylindrical casing */}
      <mesh>
        <cylinderGeometry args={[PSOM_RADIUS, PSOM_RADIUS, PSOM_LENGTH, 16]} />
        <meshStandardMaterial color={casingColor} roughness={0.35} metalness={0.15} />
      </mesh>

      {/* Maroon identity stripe */}
      <mesh position={[0, PSOM_LENGTH * 0.3, 0]}>
        <cylinderGeometry args={[PSOM_RADIUS + 0.005, PSOM_RADIUS + 0.005, 0.3, 16]} />
        <meshStandardMaterial color="#8B0000" roughness={0.5} />
      </mesh>

      {/* Nose cone */}
      <mesh position={[0, PSOM_LENGTH / 2 + NOSE_CONE_LENGTH / 2, 0]}>
        <coneGeometry args={[PSOM_RADIUS, NOSE_CONE_LENGTH, 16]} />
        <meshStandardMaterial color={casingColor} roughness={0.3} metalness={0.15} />
      </mesh>

      {/* Nozzle convergent */}
      <mesh position={[0, -(PSOM_LENGTH / 2 + 0.3), 0]}>
        <cylinderGeometry args={[NOZZLE_THROAT_R, PSOM_RADIUS, 0.6, 16]} />
        <meshStandardMaterial color="#333333" roughness={0.7} metalness={0.6} />
      </mesh>

      {/* Nozzle divergent bell */}
      <mesh position={[0, -(PSOM_LENGTH / 2 + 0.6 + NOZZLE_LENGTH / 2), 0]}>
        <cylinderGeometry args={[NOZZLE_EXIT_R, NOZZLE_THROAT_R, NOZZLE_LENGTH, 16]} />
        <meshStandardMaterial color="#2a2a2a" roughness={0.8} metalness={0.5} />
      </mesh>

      {/* Attach struts (3 diagonal struts connecting to core) */}
      {[0, 120, 240].map((strutAngle) => {
        const sRad = THREE.MathUtils.degToRad(strutAngle);
        return (
          <mesh
            key={`strut-${strutAngle}`}
            position={[
              Math.cos(sRad) * 0.2,
              PSOM_LENGTH * 0.15,
              Math.sin(sRad) * 0.2
            ]}
            rotation={[0, 0, 0.3]}
          >
            <cylinderGeometry args={[0.03, 0.03, 1.5, 4]} />
            <meshStandardMaterial color="#555555" metalness={0.7} roughness={0.3} />
          </mesh>
        );
      })}

      {/* Exhaust plume (visible only while burning and not separated) */}
      {isBurning && !isSeparated && (
        <group position={[0, -(PSOM_LENGTH / 2 + NOZZLE_LENGTH + 2), 0]}>
          <Sparkles
            count={200}
            scale={[1.5, 8, 1.5]}
            size={12}
            speed={3.0}
            opacity={0.85}
            color="#FFB300"
          />
          <Sparkles
            count={100}
            scale={[2.5, 12, 2.5]}
            size={6}
            speed={1.5}
            opacity={0.3}
            color="#FF6600"
          />
        </group>
      )}
    </group>
  );
}

// ===========================================================================
//  Main PSOM_Cluster Component
// ===========================================================================

export default function PSOM_Cluster({
  variant = 'PSLV-XL',
  clusterMountOffset = [0, 0, 0],
}: PSOM_ClusterProps) {
  const data = useTelemetryStore((state) => state.data);

  // Determine which boosters are configured for this variant
  const boosterConfigs = useMemo(() => {
    return VARIANT_CONFIGS[variant] || VARIANT_CONFIGS['PSLV-XL'];
  }, [variant]);

  // Track separation state for each booster
  const [separatedGroundLit, setSeparatedGroundLit] = useState(false);
  const [separatedAirLit, setSeparatedAirLit] = useState(false);
  const [groundSepTime, setGroundSepTime] = useState(0);
  const [airSepTime, setAirSepTime] = useState(0);

  // Listen for separation events from the backend telemetry
  useEffect(() => {
    if (!data || !data.event) return;

    const event = data.event.toLowerCase();

    if (event.includes('booster_sep_ground') || event.includes('ground_lit_sep')) {
      if (!separatedGroundLit) {
        setSeparatedGroundLit(true);
        setGroundSepTime(data.time);
      }
    }

    if (event.includes('booster_sep_air') || event.includes('air_lit_sep')) {
      if (!separatedAirLit) {
        setSeparatedAirLit(true);
        setAirSepTime(data.time);
      }
    }
  }, [data?.event, data?.time, separatedGroundLit, separatedAirLit]);

  // Auto-detect separation based on mission time if no explicit events
  // Ground-lit burnout: ~T+70s, Air-lit burnout: ~T+95s
  useEffect(() => {
    if (!data) return;
    if (!separatedGroundLit && data.time > 72) {
      setSeparatedGroundLit(true);
      setGroundSepTime(data.time);
    }
    if (!separatedAirLit && data.time > 97) {
      setSeparatedAirLit(true);
      setAirSepTime(data.time);
    }
  }, [data?.time, separatedGroundLit, separatedAirLit]);

  // If no boosters for this variant, render nothing
  if (boosterConfigs.length === 0) return null;

  return (
    <group position={clusterMountOffset}>
      {boosterConfigs.map((config) => {
        const isSep = config.group === 'ground' ? separatedGroundLit : separatedAirLit;
        const sepTime = config.group === 'ground' ? groundSepTime : airSepTime;
        const currentTime = data?.time || 0;

        // Determine burning state:
        // Ground-lit: burning from T-0 to ~T+50s (BURN_TIME)
        // Air-lit: burning from T+25s to ~T+75s
        const ignitionTime = config.group === 'ground' ? 0 : 25;
        const isBurning = !isSep
          && currentTime >= ignitionTime
          && currentTime < ignitionTime + 50;

        return (
          <PSOMBooster
            key={`psom-${config.id}`}
            config={config}
            isSeparated={isSep}
            separationTime={isSep ? currentTime - sepTime : 0}
            isBurning={isBurning}
          />
        );
      })}
    </group>
  );
}
