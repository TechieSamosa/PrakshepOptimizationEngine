'use client';

import React from 'react';
import { useTelemetryStore } from '@/store/telemetryStore';
import { Activity, AlertTriangle, CheckCircle, Navigation, ShieldAlert, Wind } from 'lucide-react';
import clsx from 'clsx';

function DataCard({ title, value, unit, icon, alert = false }: { title: string, value: string | number, unit: string, icon: React.ReactNode, alert?: boolean }) {
  return (
    <div className={clsx(
      "flex flex-col p-4 bg-[var(--color-panel)] border rounded-lg transition-colors",
      alert ? "border-[var(--color-accent-red)] shadow-[0_0_15px_rgba(255,42,42,0.3)]" : "border-[var(--color-panel-border)]"
    )}>
      <div className="flex items-center justify-between mb-2">
        <span className="text-xs tracking-widest text-gray-400 font-semibold uppercase">{title}</span>
        <span className={alert ? "text-[var(--color-accent-red)]" : "text-[var(--color-accent-cyan)]"}>
          {icon}
        </span>
      </div>
      <div className="flex items-baseline space-x-1">
        <span className={clsx(
          "text-2xl font-mono tabular-nums font-bold",
          alert ? "text-[var(--color-accent-red)]" : "text-[var(--color-foreground)]"
        )}>
          {value}
        </span>
        <span className="text-sm font-mono text-gray-500">{unit}</span>
      </div>
    </div>
  );
}

export default function TelemetryDashboard() {
  const data = useTelemetryStore((state) => state.data);
  const isConnected = useTelemetryStore((state) => state.isConnected);

  if (!data) {
    return (
      <div className="w-full h-full flex items-center justify-center bg-[var(--color-background)]">
        <div className="flex flex-col items-center space-y-4">
          <div className="w-8 h-8 border-4 border-[var(--color-accent-cyan)] border-t-transparent rounded-full animate-spin"></div>
          <p className="font-mono text-sm tracking-widest text-[var(--color-accent-cyan)]">
            {isConnected ? "AWAITING TELEMETRY STREAM..." : "CONNECTING TO ENGINE..."}
          </p>
        </div>
      </div>
    );
  }

  // Formatting for display
  const altitudeKm = (data.altitude / 1000).toFixed(2);
  const velocityMach = data.mach_number.toFixed(2);
  const qDynkPa = (data.dynamic_pressure / 1000).toFixed(2);
  
  const isMaxQ = data.max_q_reached;
  const isAnomaly = data.structural_integrity < 0.8;
  const tMinus = data.time.toFixed(1);

  return (
    <div className="w-full h-full flex flex-col pointer-events-none p-4 justify-between">
      
      {/* Top Bar: Status & Time */}
      <div className="flex justify-between items-start w-full">
        <div className="flex items-center space-x-3 bg-[var(--color-panel)] border border-[var(--color-panel-border)] px-4 py-2 rounded-lg pointer-events-auto">
          <div className={clsx("w-3 h-3 rounded-full animate-pulse", isConnected ? "bg-[var(--color-accent-green)] shadow-[0_0_8px_#39FF14]" : "bg-red-500")} />
          <span className="font-mono text-sm tracking-widest text-white">60HZ DATALINK</span>
        </div>
        
        <div className="flex items-center bg-[var(--color-panel)] border border-[var(--color-panel-border)] px-6 py-2 rounded-lg text-center pointer-events-auto">
          <span className="text-xs tracking-widest text-gray-400 font-semibold uppercase mr-4">Mission Time</span>
          <span className="font-mono text-xl tabular-nums font-bold text-white">T+ {tMinus}s</span>
        </div>
      </div>

      {/* Right Side: Gauges */}
      <div className="absolute right-4 top-20 bottom-4 w-64 flex flex-col space-y-4 pointer-events-auto overflow-y-auto pr-2 pb-4">
        
        <DataCard 
          title="Altitude" 
          value={altitudeKm} 
          unit="km" 
          icon={<Navigation size={18} />} 
        />
        
        <DataCard 
          title="Velocity" 
          value={velocityMach} 
          unit="Mach" 
          icon={<Activity size={18} />} 
        />
        
        <DataCard 
          title="Dynamic Pressure" 
          value={qDynkPa} 
          unit="kPa" 
          icon={<Wind size={18} />} 
          alert={isMaxQ}
        />

        <DataCard 
          title="Propellant" 
          value={(data.propellant_remaining * 100).toFixed(1)} 
          unit="%" 
          icon={<CheckCircle size={18} />} 
          alert={data.propellant_remaining < 0.05 && data.thrust > 0}
        />
        
        {/* LSTM Anomaly Prediction Panel */}
        <div className={clsx(
          "flex flex-col p-4 bg-[var(--color-panel)] border rounded-lg transition-colors mt-8",
          isAnomaly ? "border-[var(--color-accent-red)] bg-red-900/20" : "border-[var(--color-panel-border)]"
        )}>
          <div className="flex items-center justify-between mb-3">
            <span className="text-xs tracking-widest text-gray-400 font-semibold uppercase">LSTM Anomaly Pred</span>
            <ShieldAlert size={18} className={isAnomaly ? "text-[var(--color-accent-red)] animate-pulse" : "text-gray-500"} />
          </div>
          
          <div className="w-full bg-gray-800 rounded-full h-2.5 mb-2">
            <div 
              className={clsx("h-2.5 rounded-full transition-all duration-300", isAnomaly ? "bg-[var(--color-accent-red)]" : "bg-[var(--color-accent-green)]")} 
              style={{ width: `${Math.max(0, Math.min(100, data.structural_integrity * 100))}%` }}
            ></div>
          </div>
          <div className="flex justify-between text-xs font-mono">
            <span className="text-gray-500">Critical</span>
            <span className="text-white">{(data.structural_integrity * 100).toFixed(1)}%</span>
          </div>
          
          {isAnomaly && (
            <div className="mt-3 flex items-center space-x-2 text-[var(--color-accent-red)] font-mono text-xs animate-pulse">
              <AlertTriangle size={14} />
              <span>STRUCTURAL FAILURE IMMINENT</span>
            </div>
          )}
        </div>

        {/* Action Logger */}
        <div className="flex flex-col p-4 bg-[var(--color-panel)] border border-[var(--color-panel-border)] rounded-lg">
           <span className="text-xs tracking-widest text-gray-400 font-semibold uppercase mb-2">Stage Status</span>
           <span className="font-mono text-sm text-[var(--color-accent-amber)]">{data.event}</span>
        </div>
      </div>
      
    </div>
  );
}
