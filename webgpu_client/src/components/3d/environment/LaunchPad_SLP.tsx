'use client';

/**
 * LaunchPad_SLP.tsx — SDSC-SHAR Second Launch Pad (SLP) WebGPU Component.
 *
 * Renders the structural elements of the PSLV/LVM3 launch pad:
 *
 *   1. Concrete Launch Pedestal: The elevated platform on which the rocket
 *      sits during pre-launch.
 *   2. Flame Trench: An angled channel below the pedestal that redirects
 *      exhaust gases laterally during liftoff.
 *   3. Umbilical Tower: A tall service tower with swing-arm connections
 *      to the rocket that retract at T-0.
 *   4. T-0 Disconnect Animation: At exactly T-0, the swing arms physically
 *      retract backward into the tower as the rocket lifts off.
 *   5. Exhaust Collision Plane: A transparent angled plane inside the flame
 *      trench for particle collision redirection.
 */

import React, { useRef, useState, useEffect, useMemo } from 'react';
import { useFrame } from '@react-three/fiber';
import * as THREE from 'three';
import { useTelemetryStore } from '@/store/telemetryStore';

// ===========================================================================
//  Launch Pad Dimensions (approximate, meters)
// ===========================================================================

const PEDESTAL_WIDTH    = 20.0;
const PEDESTAL_DEPTH    = 20.0;
const PEDESTAL_HEIGHT   = 8.0;

const TRENCH_WIDTH      = 6.0;
const TRENCH_DEPTH      = 30.0;
const TRENCH_HEIGHT     = 6.0;

const TOWER_WIDTH       = 6.0;
const TOWER_DEPTH       = 6.0;
const TOWER_HEIGHT      = 76.0;  // Mobile Service Tower (MST) height

const ARM_LENGTH        = 12.0;
const ARM_THICKNESS     = 0.4;

// ===========================================================================
//  Swing Arm sub-component
// ===========================================================================

interface SwingArmProps {
  height: number;
  isRetracted: boolean;
  retractSpeed?: number;
}

function SwingArm({ height, isRetracted, retractSpeed = 2.0 }: SwingArmProps) {
  const armRef = useRef<THREE.Group>(null);
  const currentAngle = useRef(0); // 0 = connected, -PI/2 = fully retracted

  useFrame((_, delta) => {
    if (!armRef.current) return;

    const targetAngle = isRetracted ? -Math.PI / 2 : 0;
    const diff = targetAngle - currentAngle.current;

    if (Math.abs(diff) > 0.001) {
      currentAngle.current += Math.sign(diff) * retractSpeed * delta;
      // Clamp
      if (isRetracted && currentAngle.current < targetAngle) currentAngle.current = targetAngle;
      if (!isRetracted && currentAngle.current > targetAngle) currentAngle.current = targetAngle;
    }

    // Pivot is at the tower base of the arm
    armRef.current.rotation.y = currentAngle.current;
  });

  return (
    <group ref={armRef} position={[TOWER_WIDTH / 2, height, 0]}>
      {/* Arm beam */}
      <mesh position={[ARM_LENGTH / 2, 0, 0]}>
        <boxGeometry args={[ARM_LENGTH, ARM_THICKNESS, ARM_THICKNESS * 2]} />
        <meshStandardMaterial color="#cc4400" roughness={0.6} metalness={0.4} />
      </mesh>
      {/* Connector tip (the part that touches the rocket) */}
      <mesh position={[ARM_LENGTH, 0, 0]}>
        <sphereGeometry args={[0.4, 8, 8]} />
        <meshStandardMaterial color="#666666" roughness={0.5} metalness={0.8} />
      </mesh>
    </group>
  );
}

// ===========================================================================
//  Main LaunchPad Component
// ===========================================================================

interface LaunchPadProps {
  position?: [number, number, number];
}

export default function LaunchPad_SLP({ position = [0, 0, 0] }: LaunchPadProps) {
  const data = useTelemetryStore((state) => state.data);

  // Determine if T-0 has passed (arms should retract)
  const [armsRetracted, setArmsRetracted] = useState(false);

  useEffect(() => {
    if (!data) return;
    // T-0: mission time >= 0 means liftoff has begun
    if (data.time >= 0 && !armsRetracted) {
      setArmsRetracted(true);
    }
  }, [data?.time, armsRetracted]);

  // Swing arm heights (3 arms at different levels on the tower)
  const armHeights = useMemo(() => [18.0, 32.0, 48.0], []);

  return (
    <group position={position}>

      {/* ================================================================= */}
      {/* Ground Plane                                                      */}
      {/* ================================================================= */}
      <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, -0.1, 0]}>
        <planeGeometry args={[200, 200]} />
        <meshStandardMaterial color="#8B7355" roughness={0.95} />
      </mesh>

      {/* ================================================================= */}
      {/* Launch Pedestal                                                   */}
      {/* ================================================================= */}
      <mesh position={[0, PEDESTAL_HEIGHT / 2, 0]}>
        <boxGeometry args={[PEDESTAL_WIDTH, PEDESTAL_HEIGHT, PEDESTAL_DEPTH]} />
        <meshStandardMaterial color="#b0b0b0" roughness={0.8} metalness={0.1} />
      </mesh>

      {/* Exhaust aperture (dark void in the centre of the pedestal) */}
      <mesh position={[0, PEDESTAL_HEIGHT / 2, 0]}>
        <cylinderGeometry args={[2.5, 2.5, PEDESTAL_HEIGHT + 0.2, 32]} />
        <meshStandardMaterial color="#1a1a1a" roughness={0.95} />
      </mesh>

      {/* ================================================================= */}
      {/* Flame Trench                                                      */}
      {/* ================================================================= */}
      {/* The flame trench runs below and behind the pedestal to redirect   */}
      {/* exhaust gases laterally.                                          */}
      <mesh position={[0, -TRENCH_HEIGHT / 2, -TRENCH_DEPTH / 2 + PEDESTAL_DEPTH / 4]}>
        <boxGeometry args={[TRENCH_WIDTH, TRENCH_HEIGHT, TRENCH_DEPTH]} />
        <meshStandardMaterial color="#555555" roughness={0.9} metalness={0.05} />
      </mesh>

      {/* Flame Deflector (angled wedge inside the trench) */}
      <mesh
        position={[0, -TRENCH_HEIGHT / 2 + 1, -5]}
        rotation={[0.4, 0, 0]}  // Angled ~23 degrees to redirect exhaust
      >
        <boxGeometry args={[TRENCH_WIDTH - 0.5, 0.5, 8]} />
        <meshStandardMaterial color="#444444" roughness={0.8} metalness={0.3} />
      </mesh>

      {/* ================================================================= */}
      {/* Exhaust Collision Plane (Invisible)                               */}
      {/* ================================================================= */}
      {/* This transparent plane sits inside the flame trench at an angle.  */}
      {/* Particle systems can use raycasting against this plane to bounce  */}
      {/* exhaust particles laterally rather than clipping through.         */}
      <mesh
        position={[0, -1, -3]}
        rotation={[0.5, 0, 0]}
        name="exhaust_collision_plane"
      >
        <planeGeometry args={[TRENCH_WIDTH, 15]} />
        <meshStandardMaterial
          color="#ff0000"
          transparent={true}
          opacity={0.0}
          side={THREE.DoubleSide}
          depthWrite={false}
        />
      </mesh>

      {/* ================================================================= */}
      {/* Umbilical Tower (Mobile Service Tower)                            */}
      {/* ================================================================= */}
      <group position={[-PEDESTAL_WIDTH / 2 - TOWER_WIDTH / 2 + 2, 0, 0]}>
        {/* Main tower structure */}
        <mesh position={[0, TOWER_HEIGHT / 2, 0]}>
          <boxGeometry args={[TOWER_WIDTH, TOWER_HEIGHT, TOWER_DEPTH]} />
          <meshStandardMaterial color="#cc3333" roughness={0.7} metalness={0.2} />
        </mesh>

        {/* Tower lattice lines (visual detail) */}
        {[0.2, 0.4, 0.6, 0.8].map((frac) => (
          <mesh key={`lattice-h-${frac}`} position={[TOWER_WIDTH / 2 + 0.05, TOWER_HEIGHT * frac, 0]}>
            <boxGeometry args={[0.1, 0.1, TOWER_DEPTH - 0.5]} />
            <meshStandardMaterial color="#aa2222" roughness={0.6} metalness={0.3} />
          </mesh>
        ))}

        {/* Swing Arms */}
        {armHeights.map((h, i) => (
          <SwingArm key={`arm-${i}`} height={h} isRetracted={armsRetracted} retractSpeed={1.5 + i * 0.3} />
        ))}

        {/* Tower crane at the top */}
        <mesh position={[3, TOWER_HEIGHT + 1, 0]}>
          <boxGeometry args={[8, 0.5, 1.0]} />
          <meshStandardMaterial color="#cc3333" roughness={0.7} metalness={0.2} />
        </mesh>
      </group>

      {/* ================================================================= */}
      {/* Lightning Towers (4 corner poles)                                  */}
      {/* ================================================================= */}
      {[[-25, -25], [-25, 25], [25, -25], [25, 25]].map(([x, z], i) => (
        <mesh key={`lightning-${i}`} position={[x, 50, z]}>
          <cylinderGeometry args={[0.2, 0.3, 100, 8]} />
          <meshStandardMaterial color="#888888" roughness={0.6} metalness={0.5} />
        </mesh>
      ))}
    </group>
  );
}
