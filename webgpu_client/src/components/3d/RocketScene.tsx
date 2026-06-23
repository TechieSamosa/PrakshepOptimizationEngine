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
  const missionConfig = useTelemetryStore((state) => state.missionConfig);
  const vehicle = missionConfig?.vehicle || 'PSLV-XL';
  const stage = data ? data.current_stage : 0;

  // Update rocket attitude (pitch, yaw, roll)
  useFrame(() => {
    if (!meshRef.current || !data) return;
    meshRef.current.rotation.x = THREE.MathUtils.degToRad(data.pitch);
    meshRef.current.rotation.y = THREE.MathUtils.degToRad(data.yaw);
    meshRef.current.rotation.z = THREE.MathUtils.degToRad(data.roll);
    
    if (exhaustRef.current) {
      const thrustScale = (data.thrust > 0 ? 1 : 0) * (1 - (data.dynamic_pressure / 100000));
      exhaustRef.current.scale.setScalar(Math.max(0.1, thrustScale));
    }
  });

  return (
    <group ref={meshRef}>
      
      {/* ----------------- PSLV-XL / SSLV / AGNIBAAN ----------------- */}
      {(vehicle === 'PSLV-XL' || vehicle === 'SSLV' || vehicle === 'AGNIBAAN' || vehicle === 'VIKRAM') && (
        <>
          {/* Core Stage */}
          {stage <= 1 && (
            <mesh position={[0, -2, 0]}>
              <cylinderGeometry args={[1.4, 1.4, 16, 32]} />
              <meshStandardMaterial color="#eeeeee" roughness={0.3} metalness={0.2} />
            </mesh>
          )}
          {/* Upper Stages */}
          {stage <= 3 && (
            <mesh position={[0, 8, 0]}>
              <cylinderGeometry args={[1.4, 1.4, 4, 32]} />
              <meshStandardMaterial color="#d4d4d4" roughness={0.3} metalness={0.2} />
            </mesh>
          )}
          {/* Fairing */}
          <mesh position={[0, 12, 0]}>
            <coneGeometry args={[1.4, 4, 32]} />
            <meshStandardMaterial color="#ffffff" roughness={0.2} metalness={0.1} />
          </mesh>
          
          {/* PSLV-XL 6 Strap-on solid boosters (Drop at stage 1) */}
          {vehicle === 'PSLV-XL' && stage === 0 && [0, 60, 120, 180, 240, 300].map((angle, i) => {
            const rad = THREE.MathUtils.degToRad(angle);
            const x = Math.cos(rad) * 2.0;
            const z = Math.sin(rad) * 2.0;
            return (
              <group key={i} position={[x, -4, z]}>
                <mesh><cylinderGeometry args={[0.5, 0.5, 12, 16]} /><meshStandardMaterial color="#8B0000" /></mesh>
                <mesh position={[0, 6.5, 0]}><coneGeometry args={[0.5, 1, 16]} /><meshStandardMaterial color="#8B0000" /></mesh>
                <mesh position={[0, -6.5, 0]}><cylinderGeometry args={[0.3, 0.6, 1, 16]} /><meshStandardMaterial color="#333333" /></mesh>
              </group>
            );
          })}
        </>
      )}

      {/* ----------------- LVM3 / GSLV / HSLV / NGLV ----------------- */}
      {(vehicle === 'LVM3' || vehicle === 'GSLV-MK2' || vehicle === 'HSLV' || vehicle === 'NGLV') && (
        <>
          {/* Liquid Core Stage */}
          {stage <= 1 && (
            <mesh position={[0, -2, 0]}>
              <cylinderGeometry args={[2.0, 2.0, 18, 32]} />
              <meshStandardMaterial color="#b3b3b3" roughness={0.5} metalness={0.4} />
            </mesh>
          )}
          {/* Cryo Upper Stage */}
          {stage <= 2 && (
            <mesh position={[0, 8.5, 0]}>
              <cylinderGeometry args={[2.0, 2.0, 3, 32]} />
              <meshStandardMaterial color="#ffffff" roughness={0.3} metalness={0.1} />
            </mesh>
          )}
          {/* Massive Fairing */}
          <mesh position={[0, 13, 0]}>
            <coneGeometry args={[2.5, 6, 32]} />
            <meshStandardMaterial color="#ffffff" roughness={0.2} metalness={0.1} />
          </mesh>
          
          {/* Two massive solid strap-ons (S200) */}
          {stage === 0 && [-1, 1].map((side, i) => (
            <group key={i} position={[side * 2.8, -3, 0]}>
              <mesh><cylinderGeometry args={[1.6, 1.6, 20, 32]} /><meshStandardMaterial color="#ffffff" /></mesh>
              <mesh position={[0, 11, 0]}><coneGeometry args={[1.6, 2, 32]} /><meshStandardMaterial color="#ffffff" /></mesh>
            </group>
          ))}
        </>
      )}

      {/* ----------------- FALCON 9 ----------------- */}
      {(vehicle === 'FALCON-9') && (
        <>
          {/* First Stage */}
          {stage === 0 && (
            <mesh position={[0, -5, 0]}>
              <cylinderGeometry args={[1.85, 1.85, 25, 32]} />
              <meshStandardMaterial color="#ffffff" roughness={0.2} metalness={0.2} />
              {/* Grid fins (folded) */}
              <mesh position={[0, 12, 1.85]}><boxGeometry args={[1, 1, 0.1]} /><meshStandardMaterial color="#333" /></mesh>
              <mesh position={[0, 12, -1.85]}><boxGeometry args={[1, 1, 0.1]} /><meshStandardMaterial color="#333" /></mesh>
              <mesh position={[1.85, 12, 0]}><boxGeometry args={[0.1, 1, 1]} /><meshStandardMaterial color="#333" /></mesh>
              <mesh position={[-1.85, 12, 0]}><boxGeometry args={[0.1, 1, 1]} /><meshStandardMaterial color="#333" /></mesh>
            </mesh>
          )}
          {/* Second Stage & Fairing */}
          <mesh position={[0, 10, 0]}>
            <cylinderGeometry args={[1.85, 1.85, 5, 32]} />
            <meshStandardMaterial color="#ffffff" roughness={0.2} metalness={0.2} />
          </mesh>
          <mesh position={[0, 15, 0]}>
            <coneGeometry args={[1.85, 5, 32]} />
            <meshStandardMaterial color="#ffffff" roughness={0.2} metalness={0.2} />
          </mesh>
        </>
      )}

      {/* Volumetric Exhaust Plume */}
      <group position={[0, stage === 0 ? -12 : (vehicle === 'FALCON-9' ? 7 : 5), 0]}>
        <points ref={exhaustRef}>
          <Sparkles count={stage === 0 ? 800 : 300} scale={[stage === 0 ? 4 : 2, 15, stage === 0 ? 4 : 2]} size={20} speed={2} opacity={0.8} color={stage === 0 ? "#FFD700" : "#4444FF"} />
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
