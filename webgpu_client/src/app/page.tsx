'use client';

import React, { useEffect, useState } from 'react';
import dynamic from 'next/dynamic';
import { useTelemetryStore } from '@/store/telemetryStore';
import TelemetryDashboard from '@/components/dashboard/TelemetryDashboard';
import SetupPanel, { MissionConfig } from '@/components/control-room/SetupPanel';
import { Layers } from 'lucide-react';
import clsx from 'clsx';

// Dynamically import 3D and Map components to avoid SSR issues with Canvas and Cesium
const RocketScene = dynamic(() => import('@/components/3d/RocketScene'), { ssr: false });
const CesiumMap = dynamic(() => import('@/components/map/CesiumMap'), { ssr: false });

export default function MissionControl() {
  const { connect, disconnect, setMissionConfig } = useTelemetryStore();
  const [viewMode, setViewMode] = useState<'3D' | 'MAP'>('3D');
  const [isLaunched, setIsLaunched] = useState(false);

  // Initialize websocket on mount
  useEffect(() => {
    connect();
    return () => disconnect();
  }, [connect, disconnect]);

  const handleIgnition = async (config: MissionConfig) => {
    try {
      setMissionConfig(config);
      const isLocalhost = typeof window !== 'undefined' && (window.location.hostname === 'localhost' || window.location.hostname === '127.0.0.1');
      const defaultApiUrl = isLocalhost ? 'http://127.0.0.1:8000/api' : 'https://prakshep-api.onrender.com/api';
      const apiUrl = process.env.NEXT_PUBLIC_API_URL || defaultApiUrl;
      // Post the configuration to the backend
      const response = await fetch(`${apiUrl}/simulation/start`, { 
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(config)
      });
      
      if (!response.ok) {
        throw new Error(`Backend responded with status: ${response.status}`);
      }
      
      setIsLaunched(true);
    } catch (error) {
      console.error("FATAL: Mission initialization failed:", error);
      alert(`Failed to initialize data link: ${error instanceof Error ? error.message : 'Backend offline'}`);
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
        {isLaunched && (
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
        )}
      </div>
      
      {/* Pre-Launch Setup Modal */}
      {!isLaunched && <SetupPanel onInitialize={handleIgnition} />}
    </main>
  );
}
