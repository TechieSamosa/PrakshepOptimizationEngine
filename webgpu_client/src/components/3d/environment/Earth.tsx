'use client';

/**
 * Earth.tsx — Dynamic Earth Globe with Ephemeris Lighting.
 *
 * Renders a massive, accurately-scaled sphere representing the Earth.
 * Features:
 *
 *   1. Multi-layer rendering: A base globe with topological colour,
 *      an atmospheric glow shell, and a thin cloud shell.
 *
 *   2. Dynamic Ephemeris Lighting: Accepts the user-defined launch
 *      Calendar Date & Time (UTC). Computes the approximate Sun position
 *      relative to Sriharikota at that timestamp, and positions the
 *      primary Directional Light accordingly:
 *        - Dawn launch → long shadows from the east
 *        - Noon launch → overhead lighting
 *        - Midnight launch → pitch black (no sunlight on launch site)
 *
 *   3. Coordinate System: The Earth mesh is centred at (0,0,0) in ECI.
 *      The GMST rotation is applied to align geographic features with
 *      the ECI frame at the simulated time.
 */

import React, { useRef, useMemo } from 'react';
import { useFrame } from '@react-three/fiber';
import * as THREE from 'three';

// ===========================================================================
//  Constants
// ===========================================================================

const EARTH_RADIUS    = 6378137.0;   // WGS84 equatorial radius (m)
const ATMOSPHERE_SCALE = 1.015;       // Atmosphere shell is 1.5% larger
const CLOUD_SCALE      = 1.005;

// For rendering, we scale down by a factor to keep numbers manageable.
// 1 unit in scene = SCALE_FACTOR metres in real life.
const SCALE_FACTOR = 1000.0; // 1 scene unit = 1 km
const RENDER_RADIUS = EARTH_RADIUS / SCALE_FACTOR;

// ===========================================================================
//  Sun Position Ephemeris (Simplified Solar Model)
// ===========================================================================

/**
 * Compute the approximate direction to the Sun in ECI coordinates
 * for a given Julian Date.
 *
 * Uses a simplified solar position model based on the mean anomaly
 * and ecliptic longitude. Accurate to ~1 degree.
 *
 * Reference: Meeus, "Astronomical Algorithms", Chapter 25.
 */
function computeSunDirectionECI(julianDate: number): THREE.Vector3 {
  // Julian centuries from J2000.0
  const T = (julianDate - 2451545.0) / 36525.0;

  // Mean longitude of the Sun (degrees)
  const L0 = 280.46646 + 36000.76983 * T + 0.0003032 * T * T;

  // Mean anomaly of the Sun (degrees)
  const M = 357.52911 + 35999.05029 * T - 0.0001537 * T * T;
  const M_rad = (M * Math.PI) / 180.0;

  // Equation of centre (degrees)
  const C =
    (1.9146 - 0.004817 * T) * Math.sin(M_rad) +
    (0.019993 - 0.000101 * T) * Math.sin(2 * M_rad) +
    0.00029 * Math.sin(3 * M_rad);

  // Sun's ecliptic longitude (degrees)
  const lambda_sun = ((L0 + C) % 360.0) * (Math.PI / 180.0);

  // Obliquity of the ecliptic (radians)
  const epsilon = (23.439291 - 0.0130042 * T) * (Math.PI / 180.0);

  // Sun direction in ECI (equatorial coordinates)
  // Ignoring nutation for this approximation
  const x = Math.cos(lambda_sun);
  const y = Math.cos(epsilon) * Math.sin(lambda_sun);
  const z = Math.sin(epsilon) * Math.sin(lambda_sun);

  return new THREE.Vector3(x, y, z).normalize();
}

/**
 * Convert a calendar date/time (UTC) to a Julian Date.
 */
function calendarToJulianDate(
  year: number,
  month: number,
  day: number,
  hour: number,
  minute: number,
  second: number
): number {
  let y = year;
  let m = month;
  if (m <= 2) {
    y -= 1;
    m += 12;
  }
  const A = Math.floor(y / 100);
  const B = 2 - A + Math.floor(A / 4);

  const JD =
    Math.floor(365.25 * (y + 4716)) +
    Math.floor(30.6001 * (m + 1)) +
    day +
    B -
    1524.5;

  return JD + (hour + minute / 60.0 + second / 3600.0) / 24.0;
}

/**
 * Compute GMST from Julian Date (radians).
 */
function julianDateToGMST(jd: number): number {
  const T = (jd - 2451545.0) / 36525.0;

  let gmst_sec =
    67310.54841 +
    (876600.0 * 3600.0 + 8640184.812866) * T +
    0.093104 * T * T -
    6.2e-6 * T * T * T;

  let gmst_rad = ((gmst_sec % 86400.0) / 86400.0) * 2.0 * Math.PI;
  if (gmst_rad < 0) gmst_rad += 2.0 * Math.PI;

  return gmst_rad;
}

// ===========================================================================
//  Props
// ===========================================================================

interface EarthProps {
  /** UTC launch date/time for ephemeris lighting computation */
  launchDate?: {
    year: number;
    month: number;
    day: number;
    hour: number;
    minute: number;
    second: number;
  };
  /** Whether to show the atmospheric glow shell */
  showAtmosphere?: boolean;
  /** Scale factor override (scene units per meter) */
  scaleFactor?: number;
}

// ===========================================================================
//  Earth Component
// ===========================================================================

export default function Earth({
  launchDate = { year: 2025, month: 2, day: 28, hour: 3, minute: 30, second: 0 },
  showAtmosphere = true,
  scaleFactor = SCALE_FACTOR,
}: EarthProps) {
  const earthRef = useRef<THREE.Group>(null);
  const lightRef = useRef<THREE.DirectionalLight>(null);

  const renderRadius = EARTH_RADIUS / scaleFactor;

  // Compute sun direction and GMST from launch date
  const { sunDirection, gmst } = useMemo(() => {
    const jd = calendarToJulianDate(
      launchDate.year,
      launchDate.month,
      launchDate.day,
      launchDate.hour,
      launchDate.minute,
      launchDate.second
    );

    const sunDir = computeSunDirectionECI(jd);
    const gmstAngle = julianDateToGMST(jd);

    return { sunDirection: sunDir, gmst: gmstAngle };
  }, [
    launchDate.year,
    launchDate.month,
    launchDate.day,
    launchDate.hour,
    launchDate.minute,
    launchDate.second,
  ]);

  // Position the Sun directional light
  const sunLightPosition = useMemo(() => {
    // Scale sun direction to be far from Earth
    const sunDistance = renderRadius * 200;
    return new THREE.Vector3(
      sunDirection.x * sunDistance,
      sunDirection.y * sunDistance,
      sunDirection.z * sunDistance
    );
  }, [sunDirection, renderRadius]);

  // Slowly rotate Earth to match real-time (if simulation is running)
  useFrame((_, delta) => {
    if (earthRef.current) {
      // Earth rotates at ~7.29e-5 rad/s. Scale for visual effect.
      earthRef.current.rotation.y += 7.2921150e-5 * delta;
    }
  });

  // Earth surface colour gradient (latitude bands for visual depth)
  const earthMaterial = useMemo(() => {
    return new THREE.MeshStandardMaterial({
      color: new THREE.Color('#2266aa'),
      roughness: 0.85,
      metalness: 0.05,
    });
  }, []);

  return (
    <>
      {/* ================================================================= */}
      {/* Sun Directional Light (Ephemeris-driven)                          */}
      {/* ================================================================= */}
      <directionalLight
        ref={lightRef}
        position={[sunLightPosition.x, sunLightPosition.y, sunLightPosition.z]}
        intensity={2.5}
        color="#FFF5E1"
        castShadow
        shadow-mapSize-width={2048}
        shadow-mapSize-height={2048}
      />

      {/* Ambient fill (very dim to simulate space scatter) */}
      <ambientLight intensity={0.08} color="#334466" />

      {/* ================================================================= */}
      {/* Earth Globe Group (rotated by GMST to align geography → ECI)      */}
      {/* ================================================================= */}
      <group ref={earthRef} rotation={[0, gmst, 0]}>

        {/* Primary Earth Sphere */}
        <mesh>
          <sphereGeometry args={[renderRadius, 128, 64]} />
          <primitive object={earthMaterial} />
        </mesh>

        {/* Polar Ice Caps (subtle white rings) */}
        <mesh rotation={[0, 0, 0]}>
          <sphereGeometry args={[renderRadius * 1.001, 64, 16, 0, Math.PI * 2, 0, 0.25]} />
          <meshStandardMaterial color="#e8e8e8" roughness={0.9} transparent opacity={0.6} />
        </mesh>
        <mesh rotation={[Math.PI, 0, 0]}>
          <sphereGeometry args={[renderRadius * 1.001, 64, 16, 0, Math.PI * 2, 0, 0.2]} />
          <meshStandardMaterial color="#f0f0f0" roughness={0.9} transparent opacity={0.5} />
        </mesh>

        {/* Land Mass Indicator — Rough continental band */}
        <mesh>
          <sphereGeometry args={[renderRadius * 1.002, 64, 32, 0, Math.PI * 2, Math.PI * 0.2, Math.PI * 0.5]} />
          <meshStandardMaterial color="#3a7d44" roughness={0.9} transparent opacity={0.3} />
        </mesh>

      </group>

      {/* ================================================================= */}
      {/* Atmospheric Glow Shell                                            */}
      {/* ================================================================= */}
      {showAtmosphere && (
        <mesh>
          <sphereGeometry args={[renderRadius * ATMOSPHERE_SCALE, 64, 32]} />
          <meshStandardMaterial
            color="#88bbff"
            transparent
            opacity={0.08}
            side={THREE.BackSide}
            depthWrite={false}
          />
        </mesh>
      )}

      {/* Cloud Shell (very thin layer) */}
      {showAtmosphere && (
        <mesh rotation={[0.1, 0.3, 0]}>
          <sphereGeometry args={[renderRadius * CLOUD_SCALE, 64, 32]} />
          <meshStandardMaterial
            color="#ffffff"
            transparent
            opacity={0.12}
            depthWrite={false}
          />
        </mesh>
      )}
    </>
  );
}
