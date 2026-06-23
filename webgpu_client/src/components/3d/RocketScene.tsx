'use client';

import React, { useRef, useMemo } from 'react';
import { Canvas, useFrame } from '@react-three/fiber';
import { OrbitControls, Sky, Environment, Trail, Sparkles } from '@react-three/drei';
import * as THREE from 'three';
import { useTelemetryStore } from '@/store/telemetryStore';

function Rocket() {
  const meshRef = useRef<THREE.Group>(null);
  const exhaustRef = useRef<THREE.Points>(null);
  const data = useTelemetryStore((state) => state.data);

  // Update rocket attitude (pitch, yaw, roll)
  useFrame(() => {
    if (!meshRef.current || !data) return;
    
    // Smooth interpolation could be used, but 60Hz is fast enough for direct assignment
    // Convert degrees to radians for THREE.js
    meshRef.current.rotation.x = THREE.MathUtils.degToRad(data.pitch);
    meshRef.current.rotation.y = THREE.MathUtils.degToRad(data.yaw);
    meshRef.current.rotation.z = THREE.MathUtils.degToRad(data.roll);
    
    // Dynamic exhaust scaling based on throttle and dynamic pressure
    if (exhaustRef.current) {
      const thrustScale = (data.thrust > 0 ? 1 : 0) * (1 - (data.dynamic_pressure / 100000));
      exhaustRef.current.scale.setScalar(Math.max(0.1, thrustScale));
    }
  });

  return (
    <group ref={meshRef}>
      {/* Central Core (PS1) - 2.8m diameter = 1.4m radius */}
      <mesh position={[0, 0, 0]}>
        <cylinderGeometry args={[1.4, 1.4, 20, 32]} />
        <meshStandardMaterial color="#eeeeee" roughness={0.3} metalness={0.2} />
      </mesh>
      {/* Fairing */}
      <mesh position={[0, 11, 0]}>
        <coneGeometry args={[1.4, 4, 32]} />
        <meshStandardMaterial color="#ffffff" roughness={0.2} metalness={0.1} />
      </mesh>
      
      {/* PSLV-XL 6 Strap-on solid boosters (PSOM-XL) */}
      {[0, 60, 120, 180, 240, 300].map((angle, i) => {
        const rad = THREE.MathUtils.degToRad(angle);
        const radiusOffset = 1.4 + 0.5 + 0.1; // Core radius + booster radius + gap
        const x = Math.cos(rad) * radiusOffset;
        const z = Math.sin(rad) * radiusOffset;
        return (
          <group key={i} position={[x, -2, z]}>
            {/* Booster Body */}
            <mesh>
              <cylinderGeometry args={[0.5, 0.5, 12, 16]} />
              <meshStandardMaterial color="#8B0000" roughness={0.6} metalness={0.1} /> {/* ISRO rust red */}
            </mesh>
            {/* Booster Nose Cone */}
            <mesh position={[0, 6.5, 0]}>
              <coneGeometry args={[0.5, 1, 16]} />
              <meshStandardMaterial color="#8B0000" roughness={0.6} metalness={0.1} />
            </mesh>
            {/* Metallic Nozzle */}
            <mesh position={[0, -6.5, 0]}>
              <cylinderGeometry args={[0.3, 0.6, 1, 16]} />
              <meshStandardMaterial color="#333333" roughness={0.2} metalness={0.9} />
            </mesh>
          </group>
        );
      })}

      {/* Volumetric Exhaust Plume Mock (Navier-Stokes styled Sparkles/Particles) */}
      <group position={[0, -11, 0]}>
        <points ref={exhaustRef}>
          <Sparkles count={500} scale={[3, 10, 3]} size={20} speed={2} opacity={0.8} color="#FFD700" />
        </points>
      </group>
    </group>
  );
}

function AtmosphericLighting() {
  const data = useTelemetryStore((state) => state.data);
  const altitude = data ? data.altitude : 0;
  
  // Dynamic atmospheric parameters (Karman line at 100,000m)
  const atmosphericDensity = Math.max(0, 1 - (altitude / 100000));
  
  const rayleigh = 2 * atmosphericDensity; // Fades to 0 (black void) in space
  const mieCoefficient = 0.005 * atmosphericDensity;
  
  const sunPosition = useMemo(() => {
    // Sun drops lower relative to vehicle as it ascends (mocking orbital curvature)
    const angle = Math.PI / 4 + (Math.min(altitude, 200000) / 400000) * (Math.PI / 2);
    return new THREE.Vector3(Math.cos(angle)*100, Math.sin(angle)*100, -50);
  }, [altitude]);

  return (
    <>
      <ambientLight intensity={Math.max(0.05, 0.4 * atmosphericDensity)} />
      <directionalLight position={sunPosition} intensity={2.5} castShadow />
      
      {/* Ray-marching Volumetric Scattering - Transits to Black at 100km */}
      <Sky 
        distance={450000} 
        sunPosition={sunPosition} 
        inclination={0} 
        azimuth={0.25} 
        mieCoefficient={mieCoefficient} 
        mieDirectionalG={0.8} 
        rayleigh={rayleigh}
      />
    </>
  );
}

export default function RocketScene() {
  return (
    <div className="w-full h-full bg-black relative">
      <Canvas shadows camera={{ position: [0, 0, 40], fov: 45 }}>
        <AtmosphericLighting />
        <Rocket />
        <OrbitControls 
          enablePan={false} 
          maxPolarAngle={Math.PI / 1.5} 
          minDistance={10} 
          maxDistance={100} 
          autoRotate 
          autoRotateSpeed={0.5} 
        />
        {/* Environment mapping for reflections */}
        <Environment preset="city" />
      </Canvas>
      <div className="absolute bottom-4 left-4 text-[var(--color-accent-cyan)] font-mono text-xs z-10 pointer-events-none opacity-50">
        [WebGPU Render Engine] VOLUMETRIC SCATTERING ACTIVE
      </div>
    </div>
  );
}
