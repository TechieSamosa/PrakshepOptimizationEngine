'use client';

/**
 * PS4_and_Payload.tsx — PSLV 4th Stage (Twin Engine) & Satellite WebGPU Component.
 *
 * This component visualizes the final orbital injection stage of the PSLV.
 * Key visualization features:
 *
 *   1. PS4 Rigging: Tank structure with twin L-2.5 engines.
 *   2. Reaction Control System (RCS): Particle emitters around the upper ring
 *      that fire dynamically based on attitude stabilization telemetry 
 *      (pitch/yaw/roll commands).
 *   3. Payload Separation: On a `sat_sep` telemetry event, the satellite 
 *      physically detaches from the adapter and translates along the positive 
 *      Z-axis, deploying into orbit.
 */

import React, { useRef, useState, useEffect } from 'react';
import { useFrame } from '@react-three/fiber';
import { Sparkles, Cylinder, Box } from '@react-three/drei';
import * as THREE from 'three';
import { useTelemetryStore } from '@/store/telemetryStore';

// ===========================================================================
//  Physical dimensions
// ===========================================================================

const PS4_LENGTH       = 2.6;
const PS4_RADIUS       = 1.0;
const ENGINE_LENGTH    = 0.6;
const ENGINE_OFFSET_X  = 0.4; // Engines mounted side-by-side
const PAYLOAD_RADIUS   = 1.0;
const PAYLOAD_HEIGHT   = 3.0;

// Separation Physics
const SAT_SEP_VEL      = 1.2; // m/s forward

interface PS4PayloadProps {
  mountingPosition?: [number, number, number];
  isActive?: boolean;
}

export default function PS4_and_Payload({
  mountingPosition = [0, 0, 0],
  isActive = false,
}: PS4PayloadProps) {
  const groupRef = useRef<THREE.Group>(null);
  const satelliteRef = useRef<THREE.Group>(null);

  const data = useTelemetryStore((state) => state.data);

  // Separation State
  const [isDeployed, setIsDeployed] = useState(false);

  // Monitor telemetry for satellite separation event
  useEffect(() => {
    if (!data || !data.event) return;

    if (data.event.toLowerCase().includes('sat_sep') && !isDeployed) {
      setIsDeployed(true);
    }
  }, [data?.event, isDeployed]);

  // Fallback trigger: Auto-deploy if altitude > 500km and coasting
  useEffect(() => {
    if (!data) return;
    if (!isDeployed && data.altitude > 500000 && data.thrust === 0 && data.current_stage === 4) {
      setIsDeployed(true);
    }
  }, [data?.altitude, data?.thrust, data?.current_stage, isDeployed]);

  // Animation Loop
  useFrame((_, delta) => {
    // Satellite Separation Translation
    // Moves forward along the local Y axis (which points "up" in this mesh orientation)
    if (isDeployed && satelliteRef.current) {
      satelliteRef.current.position.y += SAT_SEP_VEL * delta;
      // Slight tumble given to payload for thermal management (barbecue roll)
      satelliteRef.current.rotateOnAxis(new THREE.Vector3(0, 1, 0), 0.1 * delta);
    }
  });

  // RCS logic flags based on telemetry commands
  // Assuming data provides normalized commands [-1, 1]
  const pitchCmd = data?.pitch || 0;
  const yawCmd = data?.yaw || 0;
  const rollCmd = data?.roll || 0;

  // Thresholds to trigger visual RCS firing
  const rcsPitchPos = isActive && pitchCmd > 0.1;
  const rcsPitchNeg = isActive && pitchCmd < -0.1;
  const rcsYawPos   = isActive && yawCmd > 0.1;
  const rcsYawNeg   = isActive && yawCmd < -0.1;
  const rcsRollPos  = isActive && rollCmd > 0.1;
  const rcsRollNeg  = isActive && rollCmd < -0.1;

  // A generic RCS Thruster sub-component
  const RCS_Thruster = ({ position, rotation, active }: { position: [number,number,number], rotation: [number,number,number], active: boolean }) => (
    <group position={position} rotation={rotation}>
      <mesh>
        <cylinderGeometry args={[0.02, 0.04, 0.1, 8]} />
        <meshStandardMaterial color="#444444" roughness={0.8} />
      </mesh>
      {active && (
        <Sparkles position={[0, 0.1, 0]} count={15} scale={0.2} size={3} speed={6} color="#ffffff" opacity={0.8} />
      )}
    </group>
  );

  return (
    <group ref={groupRef} position={mountingPosition}>

      {/* ================================================================= */}
      {/* PS4 Tank Structure (MMH/MON3)                                     */}
      {/* ================================================================= */}
      <group position={[0, PS4_LENGTH / 2, 0]}>
        <mesh>
          <cylinderGeometry args={[PS4_RADIUS, PS4_RADIUS, PS4_LENGTH, 32]} />
          {/* PS4 is typically wrapped in golden Kapton multi-layer insulation (MLI) */}
          <meshStandardMaterial color="#d4af37" roughness={0.6} metalness={0.5} />
        </mesh>
        
        {/* Payload Adapter Ring */}
        <mesh position={[0, PS4_LENGTH / 2 + 0.2, 0]}>
          <cylinderGeometry args={[PS4_RADIUS - 0.2, PS4_RADIUS, 0.4, 32]} />
          <meshStandardMaterial color="#aaaaaa" roughness={0.4} metalness={0.6} />
        </mesh>

        {/* ================================================================= */}
        {/* Reaction Control System (RCS) Modules                             */}
        {/* Placed around the top rim of the PS4 tank.                        */}
        {/* ================================================================= */}
        {/* Pitch RCS (mounted on +/- Z axis, firing along Y) */}
        <RCS_Thruster position={[0, PS4_LENGTH/2 - 0.2, PS4_RADIUS]} rotation={[Math.PI/2, 0, 0]} active={rcsPitchPos || rcsRollPos} />
        <RCS_Thruster position={[0, PS4_LENGTH/2 - 0.2, -PS4_RADIUS]} rotation={[-Math.PI/2, 0, 0]} active={rcsPitchNeg || rcsRollPos} />
        
        {/* Yaw RCS (mounted on +/- X axis, firing along Y) */}
        <RCS_Thruster position={[PS4_RADIUS, PS4_LENGTH/2 - 0.2, 0]} rotation={[0, 0, -Math.PI/2]} active={rcsYawPos || rcsRollNeg} />
        <RCS_Thruster position={[-PS4_RADIUS, PS4_LENGTH/2 - 0.2, 0]} rotation={[0, 0, Math.PI/2]} active={rcsYawNeg || rcsRollNeg} />
      </group>


      {/* ================================================================= */}
      {/* Twin L-2.5 Liquid Engines                                         */}
      {/* ================================================================= */}
      {/* Engine 1 (+X offset) */}
      <group position={[ENGINE_OFFSET_X, -ENGINE_LENGTH / 2, 0]}>
        <mesh>
          <cylinderGeometry args={[0.2, 0.05, ENGINE_LENGTH, 16]} />
          <meshStandardMaterial color="#2a2a2a" roughness={0.7} metalness={0.6} />
        </mesh>
        {isActive && data?.current_stage === 4 && data?.thrust > 0 && (
          <group position={[0, -ENGINE_LENGTH, 0]}>
            <Sparkles count={100} scale={[0.5, 4, 0.5]} size={8} speed={3} opacity={0.6} color="#ff66cc" />
          </group>
        )}
      </group>

      {/* Engine 2 (-X offset) */}
      <group position={[-ENGINE_OFFSET_X, -ENGINE_LENGTH / 2, 0]}>
        <mesh>
          <cylinderGeometry args={[0.2, 0.05, ENGINE_LENGTH, 16]} />
          <meshStandardMaterial color="#2a2a2a" roughness={0.7} metalness={0.6} />
        </mesh>
        {isActive && data?.current_stage === 4 && data?.thrust > 0 && (
          <group position={[0, -ENGINE_LENGTH, 0]}>
            <Sparkles count={100} scale={[0.5, 4, 0.5]} size={8} speed={3} opacity={0.6} color="#ff66cc" />
          </group>
        )}
      </group>


      {/* ================================================================= */}
      {/* Satellite Payload                                                 */}
      {/* ================================================================= */}
      <group ref={satelliteRef} position={[0, PS4_LENGTH + 0.4 + PAYLOAD_HEIGHT / 2, 0]}>
        
        {/* Satellite Body (Gold foil box) */}
        <mesh>
          <boxGeometry args={[PAYLOAD_RADIUS * 1.5, PAYLOAD_HEIGHT, PAYLOAD_RADIUS * 1.5]} />
          <meshStandardMaterial color="#ffd700" metalness={0.8} roughness={0.3} /> 
        </mesh>

        {/* Solar Panels (Folded state) */}
        <mesh position={[PAYLOAD_RADIUS * 0.76, 0, 0]}>
          <boxGeometry args={[0.05, PAYLOAD_HEIGHT * 0.8, PAYLOAD_RADIUS * 1.4]} />
          <meshStandardMaterial color="#1a3b5c" metalness={0.5} roughness={0.1} /> 
        </mesh>
        <mesh position={[-PAYLOAD_RADIUS * 0.76, 0, 0]}>
          <boxGeometry args={[0.05, PAYLOAD_HEIGHT * 0.8, PAYLOAD_RADIUS * 1.4]} />
          <meshStandardMaterial color="#1a3b5c" metalness={0.5} roughness={0.1} /> 
        </mesh>

      </group>

    </group>
  );
}
