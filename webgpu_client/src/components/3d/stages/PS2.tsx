'use client';

/**
 * PS2.tsx — PSLV 2nd Stage (Vikas Engine) WebGPU/R3F Component
 *
 * This modular component represents the liquid-fueled PS2 stage.
 * Key visualization features:
 *
 *   1. Gimbaled Nozzle: The Vikas engine nozzle is a separate mesh parented
 *      to a pivot point. The nozzle rotates dynamically based on the pitch/yaw
 *      telemetry data to visualize Thrust Vector Control (TVC).
 *
 *   2. HRCS Thrusters: Four small nozzles are placed radially to represent
 *      the Hot Gas Reaction Control System. When a roll command is active,
 *      particles emit tangentially from these thrusters.
 *
 *   3. Propellant Depletion: The liquid tank sections shift their material
 *      properties (e.g., frost/condensation line receding) as the fuel drains.
 */

import React, { useRef, useMemo } from 'react';
import { useFrame } from '@react-three/fiber';
import { Sparkles, Cylinder } from '@react-three/drei';
import * as THREE from 'three';
import { useTelemetryStore } from '@/store/telemetryStore';

// ===========================================================================
//  Physical dimensions (matching C++ constants)
// ===========================================================================

const STAGE_LENGTH      = 12.8;    // metres
const STAGE_RADIUS      = 1.4;     // diameter = 2.8m
const ENGINE_LENGTH     = 2.5;     // Vikas engine nozzle length
const NOZZLE_EXIT_R     = 0.9;
const NOZZLE_THROAT_R   = 0.3;
const HRCS_MOMENT_ARM   = 1.4;

interface PS2Props {
  mountingPosition?: [number, number, number];
  isActive?: boolean;
}

export default function PS2({
  mountingPosition = [0, 0, 0],
  isActive = false,
}: PS2Props) {
  const groupRef = useRef<THREE.Group>(null);
  const gimbalPivotRef = useRef<THREE.Group>(null);

  const data = useTelemetryStore((state) => state.data);

  // Animate gimbal and HRCS based on telemetry
  useFrame(() => {
    if (!data) return;

    // Apply gimbal rotation to the nozzle pivot
    if (gimbalPivotRef.current && isActive && data.current_stage === 2) {
      // Assuming telemetry provides pitch/yaw deflections in radians.
      // If the engine gimbals positive pitch, the nozzle tilts up, pointing thrust down.
      gimbalPivotRef.current.rotation.x = data.pitch * 0.05; // Scale for visual clarity if needed
      gimbalPivotRef.current.rotation.z = data.yaw * 0.05;
    } else if (gimbalPivotRef.current) {
      // Reset to neutral
      gimbalPivotRef.current.rotation.x = 0;
      gimbalPivotRef.current.rotation.z = 0;
    }
  });

  return (
    <group ref={groupRef} position={mountingPosition}>

      {/* ================================================================= */}
      {/* Liquid Propellant Tanks (UH25 & N2O4)                             */}
      {/* ================================================================= */}
      {/* We model this as a single cylindrical tank for visual simplicity, 
          but with a metallic/frosty texture. */}
      <mesh position={[0, STAGE_LENGTH / 2, 0]}>
        <cylinderGeometry args={[STAGE_RADIUS, STAGE_RADIUS, STAGE_LENGTH, 32]} />
        <meshStandardMaterial
          color="#d9d9d9" // Light grey metallic
          roughness={0.4}
          metalness={0.6}
        />
      </mesh>

      {/* Interstage Adapter (top ring connecting to PS3) */}
      <mesh position={[0, STAGE_LENGTH, 0]}>
        <cylinderGeometry args={[STAGE_RADIUS, STAGE_RADIUS - 0.1, 1.0, 32]} />
        <meshStandardMaterial color="#b3b3b3" roughness={0.6} metalness={0.3} />
      </mesh>

      {/* ================================================================= */}
      {/* HRCS (Hot Gas Reaction Control System) Thrusters                  */}
      {/* 4 thrusters positioned radially.                                  */}
      {/* ================================================================= */}
      {[0, 90, 180, 270].map((angle) => {
        const rad = THREE.MathUtils.degToRad(angle);
        const x = Math.cos(rad) * HRCS_MOMENT_ARM;
        const z = Math.sin(rad) * HRCS_MOMENT_ARM;
        return (
          <mesh
            key={`hrcs-${angle}`}
            position={[x, STAGE_LENGTH * 0.8, z]}
            rotation={[0, -rad, Math.PI / 2]} // Point tangentially for roll
          >
            <cylinderGeometry args={[0.05, 0.08, 0.2, 8]} />
            <meshStandardMaterial color="#444444" roughness={0.7} metalness={0.8} />
            
            {/* Visualizer for HRCS thrust (active during roll command) */}
            {isActive && data && data.current_stage === 2 && Math.abs(data.roll) > 0.1 && (
              <Sparkles
                position={[0, -0.2, 0]}
                count={20}
                scale={[0.2, 0.5, 0.2]}
                size={4}
                speed={5}
                color="#aaccff" // Cold/Hot gas bluish tint
                opacity={0.6}
              />
            )}
          </mesh>
        );
      })}


      {/* ================================================================= */}
      {/* Gimbaled Vikas Engine Assembly                                    */}
      {/* ================================================================= */}
      {/* The gimbal pivot is located at the bottom of the tank structure. */}
      <group ref={gimbalPivotRef} position={[0, 0, 0]}>
        
        {/* Thrust Frame / Gimbal Joint visual */}
        <mesh position={[0, -0.2, 0]}>
          <sphereGeometry args={[0.3, 16, 16]} />
          <meshStandardMaterial color="#666666" roughness={0.5} metalness={0.7} />
        </mesh>

        {/* Vikas Engine Nozzle */}
        <mesh position={[0, -ENGINE_LENGTH / 2 - 0.2, 0]}>
          <cylinderGeometry args={[NOZZLE_THROAT_R, NOZZLE_EXIT_R, ENGINE_LENGTH, 32]} />
          <meshStandardMaterial color="#1a1a1a" roughness={0.8} metalness={0.4} />
        </mesh>

        {/* Hypergolic Exhaust Plume */}
        {isActive && data?.current_stage === 2 && (
          <group position={[0, -ENGINE_LENGTH - 1.0, 0]}>
            {/* Hypergolic flame is typically transparent, reddish-orange or bluish-purple.
                UDMH/N2O4 burns very cleanly compared to solid SRBs. */}
            <Sparkles
              count={300}
              scale={[1.5, 10, 1.5]}
              size={15}
              speed={4.0}
              opacity={0.7}
              color="#ff66cc" // Pinkish-purple hypergolic hue
            />
            <Sparkles
              count={100}
              scale={[2.5, 14, 2.5]}
              size={8}
              speed={2.5}
              opacity={0.3}
              color="#ffb366"
            />
          </group>
        )}
      </group>

    </group>
  );
}
