'use client';

/**
 * PS3_and_Fairing.tsx — PSLV 3rd Stage (S7) and Payload Fairing WebGPU Component.
 *
 * This modular component represents the upper-stage assembly of the PSLV.
 * Key visualization features:
 *
 *   1. PS3 (S7 Motor): A Kevlar-epoxy composite casing with a submerged
 *      flex-seal nozzle.
 *   2. Flex-Seal TVC Animation: The nozzle pivots dynamically based on
 *      pitch/yaw telemetry, simulating the elastomeric bearing displacement.
 *   3. Symmetrical Payload Fairing Jettison: Two fairing halves enclose a
 *      mock satellite payload. On a `fairing_sep` telemetry event, the
 *      halves split and tumble laterally outward using procedural physics
 *      in the `useFrame` render loop.
 */

import React, { useRef, useState, useEffect } from 'react';
import { useFrame } from '@react-three/fiber';
import { Sparkles, Sphere, Cylinder } from '@react-three/drei';
import * as THREE from 'three';
import { useTelemetryStore } from '@/store/telemetryStore';

// ===========================================================================
//  Physical dimensions (matching C++ constants)
// ===========================================================================

// PS3 (S7)
const PS3_LENGTH      = 3.6;
const PS3_RADIUS      = 1.0;
const NOZZLE_LENGTH   = 0.8;
const NOZZLE_EXIT_R   = 0.4;
const NOZZLE_THROAT_R = 0.15;

// Fairing Assembly (Approximate for PSLV-XL)
const FAIRING_RADIUS  = 1.6;
const FAIRING_HEIGHT  = 8.3;
const PAYLOAD_RADIUS  = 1.0;
const PAYLOAD_HEIGHT  = 3.0;

// Fairing Separation Physics
const FAIRING_SEP_VEL = 4.5;    // m/s lateral
const FAIRING_TUMBLE  = 0.8;    // rad/s

interface PS3FairingProps {
  mountingPosition?: [number, number, number];
  isActive?: boolean;
}

export default function PS3_and_Fairing({
  mountingPosition = [0, 0, 0],
  isActive = false,
}: PS3FairingProps) {
  const groupRef = useRef<THREE.Group>(null);
  const flexSealPivotRef = useRef<THREE.Group>(null);
  
  // Fairing refs
  const fairingHalf1Ref = useRef<THREE.Group>(null);
  const fairingHalf2Ref = useRef<THREE.Group>(null);

  const data = useTelemetryStore((state) => state.data);

  // Separation State
  const [isJettisoned, setIsJettisoned] = useState(false);
  const [jettisonTime, setJettisonTime] = useState(0);

  // Monitor telemetry for fairing separation event
  useEffect(() => {
    if (!data || !data.event) return;

    if (data.event.toLowerCase().includes('fairing_sep') && !isJettisoned) {
      setIsJettisoned(true);
      setJettisonTime(data.time);
    }
  }, [data?.event, data?.time, isJettisoned]);

  // Fallback trigger: Auto-jettison if alt > 115km (simulated time trigger)
  // In a full sim, the backend will send this event exactly when dynamic pressure drops.
  useEffect(() => {
    if (!data) return;
    if (!isJettisoned && data.altitude > 115000) {
      setIsJettisoned(true);
      setJettisonTime(data.time);
    }
  }, [data?.altitude, isJettisoned]);

  useFrame((_, delta) => {
    if (!data) return;

    // 1. Flex-Seal TVC Animation
    if (flexSealPivotRef.current && isActive && data.current_stage === 3) {
      // Actuator angles
      flexSealPivotRef.current.rotation.x = data.pitch * 0.1; 
      flexSealPivotRef.current.rotation.z = data.yaw * 0.1;
    }

    // 2. Procedural Fairing Separation Animation
    if (isJettisoned && fairingHalf1Ref.current && fairingHalf2Ref.current) {
      // Half 1 (+X direction)
      fairingHalf1Ref.current.position.x += FAIRING_SEP_VEL * delta;
      fairingHalf1Ref.current.rotateOnAxis(new THREE.Vector3(0, 0, -1), FAIRING_TUMBLE * delta);

      // Half 2 (-X direction)
      fairingHalf2Ref.current.position.x -= FAIRING_SEP_VEL * delta;
      fairingHalf2Ref.current.rotateOnAxis(new THREE.Vector3(0, 0, 1), FAIRING_TUMBLE * delta);
    }
  });

  return (
    <group ref={groupRef} position={mountingPosition}>

      {/* ================================================================= */}
      {/* PS3 Core Stage (S7 Kevlar-Epoxy Motor)                          */}
      {/* ================================================================= */}
      <group position={[0, PS3_LENGTH / 2, 0]}>
        {/* Main spherical/cylindrical casing */}
        <mesh>
          <cylinderGeometry args={[PS3_RADIUS, PS3_RADIUS, PS3_LENGTH, 32]} />
          {/* Kevlar-epoxy has a darker, slightly textured composite look */}
          <meshStandardMaterial color="#807b6f" roughness={0.9} metalness={0.1} />
        </mesh>
        
        {/* Forward Dome */}
        <mesh position={[0, PS3_LENGTH / 2, 0]}>
          <sphereGeometry args={[PS3_RADIUS, 32, 16, 0, Math.PI * 2, 0, Math.PI / 2]} />
          <meshStandardMaterial color="#807b6f" roughness={0.9} metalness={0.1} />
        </mesh>
        
        {/* Aft Dome (Submerged nozzle base) */}
        <mesh position={[0, -PS3_LENGTH / 2, 0]} rotation={[Math.PI, 0, 0]}>
          <sphereGeometry args={[PS3_RADIUS, 32, 16, 0, Math.PI * 2, 0, Math.PI / 2]} />
          <meshStandardMaterial color="#807b6f" roughness={0.9} metalness={0.1} />
        </mesh>
      </group>


      {/* ================================================================= */}
      {/* Flex-Seal Nozzle Assembly                                         */}
      {/* ================================================================= */}
      <group ref={flexSealPivotRef} position={[0, -0.2, 0]}>
        {/* Flex Bearing Joint */}
        <mesh position={[0, 0, 0]}>
          <sphereGeometry args={[0.2, 16, 16]} />
          <meshStandardMaterial color="#444444" roughness={0.8} />
        </mesh>

        {/* Nozzle Divergent Cone */}
        <mesh position={[0, -NOZZLE_LENGTH / 2 - 0.1, 0]}>
          <cylinderGeometry args={[NOZZLE_EXIT_R, NOZZLE_THROAT_R, NOZZLE_LENGTH, 32]} />
          <meshStandardMaterial color="#222222" roughness={0.8} metalness={0.3} />
        </mesh>

        {/* Exhaust Plume (Solid Motor) */}
        {isActive && data?.current_stage === 3 && (
          <group position={[0, -NOZZLE_LENGTH - 0.5, 0]}>
            <Sparkles count={200} scale={[1, 6, 1]} size={15} speed={4} opacity={0.9} color="#FF9900" />
            <Sparkles count={100} scale={[2, 8, 2]} size={10} speed={2} opacity={0.4} color="#FF3300" />
          </group>
        )}
      </group>


      {/* ================================================================= */}
      {/* Payload Fairing & Satellite                                       */}
      {/* ================================================================= */}
      <group position={[0, PS3_LENGTH + 2.0, 0]}>
        
        {/* Internal Payload (always visible, but hidden inside fairing initially) */}
        <mesh position={[0, PAYLOAD_HEIGHT / 2, 0]}>
          <boxGeometry args={[PAYLOAD_RADIUS*1.5, PAYLOAD_HEIGHT, PAYLOAD_RADIUS*1.5]} />
          <meshStandardMaterial color="#cca43b" metalness={0.9} roughness={0.2} /> {/* Gold foil */}
        </mesh>

        {/* --- FAIRING HALF 1 (+X direction) --- */}
        <group ref={fairingHalf1Ref}>
          <mesh position={[0, FAIRING_HEIGHT / 2, 0]}>
            {/* Custom half-cylinder. thetaLength = PI */}
            <cylinderGeometry args={[FAIRING_RADIUS, FAIRING_RADIUS, FAIRING_HEIGHT * 0.6, 32, 1, false, 0, Math.PI]} />
            <meshStandardMaterial color="#eeeeee" roughness={0.5} metalness={0.2} side={THREE.DoubleSide} />
          </mesh>
          <mesh position={[0, FAIRING_HEIGHT * 0.6 + (FAIRING_HEIGHT * 0.4) / 2, 0]}>
            {/* Half-cone for the nose */}
            <cylinderGeometry args={[0.01, FAIRING_RADIUS, FAIRING_HEIGHT * 0.4, 32, 1, false, 0, Math.PI]} />
            <meshStandardMaterial color="#eeeeee" roughness={0.5} metalness={0.2} side={THREE.DoubleSide} />
          </mesh>
        </group>

        {/* --- FAIRING HALF 2 (-X direction) --- */}
        <group ref={fairingHalf2Ref}>
          <mesh position={[0, FAIRING_HEIGHT / 2, 0]}>
            {/* Custom half-cylinder. thetaStart = PI, thetaLength = PI */}
            <cylinderGeometry args={[FAIRING_RADIUS, FAIRING_RADIUS, FAIRING_HEIGHT * 0.6, 32, 1, false, Math.PI, Math.PI]} />
            <meshStandardMaterial color="#eeeeee" roughness={0.5} metalness={0.2} side={THREE.DoubleSide} />
          </mesh>
          <mesh position={[0, FAIRING_HEIGHT * 0.6 + (FAIRING_HEIGHT * 0.4) / 2, 0]}>
            {/* Half-cone for the nose */}
            <cylinderGeometry args={[0.01, FAIRING_RADIUS, FAIRING_HEIGHT * 0.4, 32, 1, false, Math.PI, Math.PI]} />
            <meshStandardMaterial color="#eeeeee" roughness={0.5} metalness={0.2} side={THREE.DoubleSide} />
          </mesh>
        </group>

      </group>

    </group>
  );
}
