'use client';

import React, { useEffect, useRef } from 'react';
import { Viewer, Entity, PolylineGraphics, PointGraphics, ImageryLayer } from 'resium';
import * as Cesium from 'cesium';
import 'cesium/Build/Cesium/Widgets/widgets.css';
import { useTelemetryStore } from '@/store/telemetryStore';

// Initialize Cesium without API keys
// Using OpenStreetMap for imagery
const osmImageryProvider = new Cesium.OpenStreetMapImageryProvider({
  url: 'https://a.tile.openstreetmap.org/'
});

export default function CesiumMap() {
  const data = useTelemetryStore((state) => state.data);
  const pathPositions = useRef<Cesium.Cartesian3[]>([]);

  // Push new position to path
  useEffect(() => {
    if (data && data.latitude && data.longitude) {
      const pos = Cesium.Cartesian3.fromDegrees(
        data.longitude,
        data.latitude,
        data.geodetic_altitude || 0
      );
      pathPositions.current = [...pathPositions.current, pos];
    }
  }, [data]);

  const currentPosition = data && data.latitude && data.longitude 
    ? Cesium.Cartesian3.fromDegrees(data.longitude, data.latitude, data.geodetic_altitude || 0)
    : Cesium.Cartesian3.fromDegrees(80.2350, 13.7340, 0); // SDSC-SHAR default

  return (
    <div className="w-full h-full relative">
      <Viewer
        full
        baseLayerPicker={false}
        geocoder={false}
        homeButton={false}
        infoBox={false}
        sceneModePicker={false}
        navigationHelpButton={false}
        animation={false}
        timeline={false}
        fullscreenButton={false}
        vrButton={false}
        scene3DOnly={true}
        baseLayer={false}
      >
        {/* Base Map */}
        <ImageryLayer imageryProvider={osmImageryProvider} />
        {/* Rocket Current Position */}
        <Entity position={currentPosition} name="Rocket">
          <PointGraphics pixelSize={10} color={Cesium.Color.fromCssColorString('#39FF14')} outlineColor={Cesium.Color.BLACK} outlineWidth={2} />
        </Entity>

        {/* Rocket Ground Track */}
        {pathPositions.current.length > 1 && (
          <Entity>
            <PolylineGraphics
              positions={pathPositions.current}
              width={3}
              material={Cesium.Color.fromCssColorString('#FFB000')}
            />
          </Entity>
        )}
      </Viewer>
      
      <div className="absolute top-4 left-4 z-10 bg-[var(--color-panel)] bg-opacity-80 p-3 rounded border border-[var(--color-panel-border)] backdrop-blur-md">
        <h3 className="text-[var(--color-accent-cyan)] font-mono text-sm tracking-widest mb-1">GLOBAL TRACKING</h3>
        <p className="font-mono text-xs text-[var(--color-foreground)]">
          LAT: {data ? data.latitude.toFixed(4) : '13.7340'}°<br />
          LON: {data ? data.longitude.toFixed(4) : '80.2350'}°
        </p>
      </div>
    </div>
  );
}
