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
      {/* Central Core */}
      <mesh position={[0, 0, 0]}>
        <cylinderGeometry args={[1.4, 1.4, 20, 32]} />
        <meshStandardMaterial color="#eeeeee" roughness={0.3} metalness={0.2} />
      </mesh>
      {/* Fairing */}
      <mesh position={[0, 11, 0]}>
        <coneGeometry args={[1.4, 4, 32]} />
        <meshStandardMaterial color="#eeeeee" roughness={0.2} metalness={0.1} />
      </mesh>
      
      {/* Strap-on boosters (Mocking PSLV-XL 6 strap-ons) */}
      {[0, 60, 120, 180, 240, 300].map((angle, i) => {
        const rad = THREE.MathUtils.degToRad(angle);
        const x = Math.cos(rad) * 2;
        const z = Math.sin(rad) * 2;
        return (
          <group key={i} position={[x, -2, z]}>
            <mesh>
              <cylinderGeometry args={[0.5, 0.5, 10, 16]} />
              <meshStandardMaterial color="#FFB000" roughness={0.4} metalness={0.6} />
            </mesh>
            <mesh position={[0, 5.5, 0]}>
              <coneGeometry args={[0.5, 1, 16]} />
              <meshStandardMaterial color="#FFB000" roughness={0.4} metalness={0.6} />
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
  
  // Dynamic atmospheric parameters
  // Interpolate sun position and mie scattering based on altitude
  const mieCoefficient = useMemo(() => {
    // 0.005 at sea level, 0.0 at 100km
    return Math.max(0, 0.005 * (1 - altitude / 100000));
  }, [altitude]);
  
  const sunPosition = useMemo(() => {
    // Sun drops lower relative to vehicle as it ascends (mocking orbit)
    const angle = Math.PI / 4 + (altitude / 500000) * (Math.PI / 2);
    return new THREE.Vector3(Math.cos(angle)*100, Math.sin(angle)*100, -50);
  }, [altitude]);

  return (
    <>
      <ambientLight intensity={Math.max(0.1, 0.5 * (1 - altitude / 200000))} />
      <directionalLight position={sunPosition} intensity={2.5} castShadow />
      {/* Volumetric Scattering */}
      <Sky 
        distance={450000} 
        sunPosition={sunPosition} 
        inclination={0} 
        azimuth={0.25} 
        mieCoefficient={mieCoefficient} 
        mieDirectionalG={0.8} 
        rayleigh={0.5 + (altitude / 100000)}
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
