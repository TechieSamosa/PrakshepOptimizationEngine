'use client';

import React, { useEffect, useState } from 'react';
import dynamic from 'next/dynamic';
import { useTelemetryStore } from '@/store/telemetryStore';
import TelemetryDashboard from '@/components/dashboard/TelemetryDashboard';
import { Layers } from 'lucide-react';
import clsx from 'clsx';

// Dynamically import 3D and Map components to avoid SSR issues with Canvas and Cesium
const RocketScene = dynamic(() => import('@/components/3d/RocketScene'), { ssr: false });
const CesiumMap = dynamic(() => import('@/components/map/CesiumMap'), { ssr: false });

export default function MissionControl() {
  const connect = useTelemetryStore((state) => state.connect);
  const disconnect = useTelemetryStore((state) => state.disconnect);
  const [viewMode, setViewMode] = useState<'3D' | 'MAP'>('3D');
  const [isLaunched, setIsLaunched] = useState(false);

  // Initialize websocket on mount
  useEffect(() => {
    connect();
    return () => disconnect();
  }, [connect, disconnect]);

  const handleIgnition = async () => {
    try {
      const apiUrl = process.env.NEXT_PUBLIC_API_URL || '/api';
      await fetch(`${apiUrl}/simulation/start`, { method: 'POST' });
      setIsLaunched(true);
    } catch (e) {
      console.error("Failed to start simulation", e);
    }
  };

  return (
    <main className="w-screen h-screen overflow-hidden bg-[var(--color-background)] relative font-sans">
      
      {/* Background Viewport (3D or Map) */}
      <div className="absolute inset-0 z-0">
        {viewMode === '3D' ? <RocketScene /> : <CesiumMap />}
      </div>

      {/* Overlay Dashboard UI */}
      <div className="absolute inset-0 z-10 pointer-events-none">
        <TelemetryDashboard />
      </div>

      {/* View Toggle and Ignition */}
      <div className="absolute bottom-4 left-1/2 -translate-x-1/2 z-20 flex space-x-4 pointer-events-auto">
        <div className="flex bg-[var(--color-panel)] border border-[var(--color-panel-border)] rounded-full p-1 shadow-lg">
          <button
            onClick={() => setViewMode('3D')}
            className={clsx(
              "px-6 py-2 rounded-full text-xs font-bold tracking-widest transition-all",
              viewMode === '3D' ? "bg-[var(--color-accent-cyan)] text-black" : "text-gray-400 hover:text-white"
            )}
          >
            WEBGPU 3D
          </button>
          <button
            onClick={() => setViewMode('MAP')}
            className={clsx(
              "flex items-center space-x-2 px-6 py-2 rounded-full text-xs font-bold tracking-widest transition-all",
              viewMode === 'MAP' ? "bg-[var(--color-accent-amber)] text-black" : "text-gray-400 hover:text-white"
            )}
          >
            <Layers size={14} />
            <span>GLOBAL TRACK</span>
          </button>
        </div>

        {!isLaunched && (
          <button
            onClick={handleIgnition}
            className="bg-red-600 hover:bg-red-500 text-white border-2 border-red-400 px-8 py-2 rounded-full text-sm font-bold tracking-widest transition-all animate-pulse shadow-[0_0_15px_rgba(255,0,0,0.5)]"
          >
            IGNITION SEQUENCE START
          </button>
        )}
      </div>
      
    </main>
  );
}
