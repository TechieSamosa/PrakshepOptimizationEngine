'use client';

import React, { useMemo, useState, useEffect } from 'react';
import { useTelemetryStore } from '@/store/telemetryStore';
import clsx from 'clsx';
import { LineChart, Line, XAxis, YAxis, CartesianGrid, ResponsiveContainer, Tooltip, ScatterChart, Scatter, ZAxis } from 'recharts';

export default function TelemetryDashboard() {
  const { data, history, nominalTrajectory, isConnected } = useTelemetryStore();
  const [flashingAlert, setFlashingAlert] = useState<string | null>(null);

  // Downsample history and merge with nominal trajectory for Recharts
  const chartData = useMemo(() => {
    // Determine the max time to display
    const maxLiveTime = history.length > 0 ? history[history.length - 1].time : 0;
    const maxNominalTime = nominalTrajectory.length > 0 ? nominalTrajectory[nominalTrajectory.length - 1].time : 0;
    const maxTime = Math.max(maxLiveTime, maxNominalTime);
    
    // We will build a unified array mapped by time
    const merged: Record<number, any> = {};
    
    // Add nominal data (downsampled)
    if (nominalTrajectory && nominalTrajectory.length > 0) {
      const step = Math.max(1, Math.floor(nominalTrajectory.length / 200));
      for (let i = 0; i < nominalTrajectory.length; i += step) {
        const frame = nominalTrajectory[i];
        const t = Math.floor(frame.time);
        merged[t] = {
          time: t,
          nominalAlt: frame.altitude / 1000,
          nominalVel: frame.velocity_magnitude / 1000,
          nominalRange: frame.downrange_distance / 1000,
          nominalLat: frame.latitude,
          nominalLon: frame.longitude,
          nominalGForce: frame.acceleration_magnitude / 9.80665,
          nominalQ: frame.dynamic_pressure / 1000,
        };
      }
    }
    
    // Merge live data (downsampled)
    if (history && history.length > 0) {
      const step = Math.max(1, Math.floor(history.length / 200));
      for (let i = 0; i < history.length; i += step) {
        const frame = history[i];
        const t = Math.floor(frame.time);
        if (!merged[t]) merged[t] = { time: t };
        merged[t].liveAlt = frame.altitude / 1000;
        merged[t].liveVel = frame.velocity_magnitude / 1000;
        merged[t].liveRange = frame.downrange_distance / 1000;
        merged[t].liveLat = frame.latitude;
        merged[t].liveLon = frame.longitude;
        merged[t].liveGForce = frame.acceleration_magnitude / 9.80665;
        merged[t].liveQ = frame.dynamic_pressure / 1000;
      }
    }
    
    return Object.values(merged).sort((a, b) => a.time - b.time);
  }, [history, nominalTrajectory]);

  // Handle Flashing Alerts based on event changes
  useEffect(() => {
    if (data && data.event && data.event !== 'NOMINAL FLIGHT') {
      setFlashingAlert(data.event);
      const timer = setTimeout(() => setFlashingAlert(null), 3000);
      return () => clearTimeout(timer);
    }
  }, [data?.event]);

  if (!data) {
    return (
      <div className="w-full h-full flex items-center justify-center bg-[#00000A]/80 backdrop-blur-md pointer-events-none">
        <div className="flex flex-col items-center space-y-4">
          <div className="w-8 h-8 border-4 border-yellow-500 border-t-transparent rounded-full animate-spin"></div>
          <p className="font-mono text-sm tracking-widest text-yellow-500 font-bold">
            {isConnected ? "AWAITING LIFTOFF TELEMETRY..." : "ESTABLISHING RADAR DATALINK..."}
          </p>
        </div>
      </div>
    );
  }

  // Formatting for display
  const tMinus = data.time.toFixed(1).padStart(5, '0');
  const altitudeKm = (data.altitude / 1000).toFixed(2).padStart(6, '0');
  const rangeKm = (data.downrange_distance / 1000).toFixed(2).padStart(6, '0');
  const velocityKms = (data.velocity_magnitude / 1000).toFixed(3).padStart(6, '0');
  const azimuthDeg = data.yaw.toFixed(2).padStart(6, '0');

  return (
    <div className="w-full h-full flex flex-col pointer-events-none font-mono selection:bg-transparent overflow-hidden">
      
      {/* Top Banner */}
      <div className="w-full bg-[#000005]/90 border-b-2 border-green-800 p-2 flex justify-between items-center z-10 shadow-[0_4px_20px_rgba(0,0,0,0.8)]">
        <div className="flex items-center space-x-4">
          <div className="text-yellow-400 font-bold tracking-widest text-lg">ISRO MCC / SHAR</div>
          <div className="text-green-500 text-xs tracking-widest border border-green-800 px-2 py-1 bg-green-900/20">DATALINK NOMINAL [60HZ]</div>
        </div>
        <div className="flex items-center space-x-4">
          <div className="text-white text-xs tracking-widest bg-blue-900/30 px-3 py-1 border border-blue-800">WGS84 RADAR TRACK</div>
        </div>
      </div>

      {/* Main 4-Panel Layout */}
      <div className="flex-1 w-full grid grid-cols-4 gap-2 p-2 z-10">
        
        {/* Panel 1: Time vs Alt & Time vs Vel */}
        <div className="bg-[#000015]/80 backdrop-blur-md border border-gray-800 rounded p-2 flex flex-col relative overflow-hidden pointer-events-auto shadow-lg">
          <div className="absolute inset-0 bg-[linear-gradient(rgba(0,255,255,0.03)_1px,transparent_1px),linear-gradient(90deg,rgba(0,255,255,0.03)_1px,transparent_1px)] bg-[size:20px_20px] pointer-events-none" />
          <div className="text-yellow-500 text-xs font-bold tracking-widest mb-2 z-10 border-b border-gray-800 pb-1">FLIGHT PROFILE (TIME VS ALT/VEL)</div>
          <div className="flex-1 w-full h-full z-10">
            <ResponsiveContainer width="100%" height="100%">
              <LineChart data={chartData} margin={{ top: 5, right: 5, left: -20, bottom: 5 }}>
                <CartesianGrid strokeDasharray="3 3" stroke="#1f2937" />
                <XAxis dataKey="time" type="number" domain={['auto', 'auto']} tickFormatter={(v) => `T+${Math.floor(v)}s`} stroke="#4b5563" tick={{fontSize: 10, fill: '#9ca3af'}} />
                <YAxis yAxisId="left" stroke="#3b82f6" tick={{fontSize: 10, fill: '#3b82f6'}} />
                <YAxis yAxisId="right" orientation="right" stroke="#eab308" tick={{fontSize: 10, fill: '#eab308'}} />
                <Tooltip contentStyle={{backgroundColor: '#000015', border: '1px solid #374151', fontSize: '10px'}} labelStyle={{color: '#9ca3af'}} />
                <Line yAxisId="left" type="monotone" dataKey="nominalAlt" stroke="#3b82f6" strokeDasharray="3 3" dot={false} strokeWidth={1} name="Nominal Alt (km)" isAnimationActive={false} />
                <Line yAxisId="left" type="monotone" dataKey="liveAlt" stroke="#60a5fa" dot={false} strokeWidth={2} name="Live Alt (km)" isAnimationActive={false} />
                
                <Line yAxisId="right" type="monotone" dataKey="nominalVel" stroke="#eab308" strokeDasharray="3 3" dot={false} strokeWidth={1} name="Nominal Vel (km/s)" isAnimationActive={false} />
                <Line yAxisId="right" type="monotone" dataKey="liveVel" stroke="#fef08a" dot={false} strokeWidth={2} name="Live Vel (km/s)" isAnimationActive={false} />
              </LineChart>
            </ResponsiveContainer>
          </div>
        </div>

        {/* Panel 2: Range vs Alt */}
        <div className="bg-[#000015]/80 backdrop-blur-md border border-gray-800 rounded p-2 flex flex-col relative overflow-hidden pointer-events-auto shadow-lg">
          <div className="absolute inset-0 bg-[linear-gradient(rgba(0,255,255,0.03)_1px,transparent_1px),linear-gradient(90deg,rgba(0,255,255,0.03)_1px,transparent_1px)] bg-[size:20px_20px] pointer-events-none" />
          <div className="text-yellow-500 text-xs font-bold tracking-widest mb-2 z-10 border-b border-gray-800 pb-1">TRAJECTORY (RANGE VS ALT)</div>
          <div className="flex-1 w-full h-full z-10">
            <ResponsiveContainer width="100%" height="100%">
              <LineChart data={chartData} margin={{ top: 5, right: 5, left: -20, bottom: 5 }}>
                <CartesianGrid strokeDasharray="3 3" stroke="#1f2937" />
                <XAxis dataKey="liveRange" type="number" domain={['auto', 'auto']} stroke="#4b5563" tick={{fontSize: 10, fill: '#9ca3af'}} />
                <YAxis dataKey="liveAlt" stroke="#22c55e" tick={{fontSize: 10, fill: '#22c55e'}} />
                <Tooltip contentStyle={{backgroundColor: '#000015', border: '1px solid #374151', fontSize: '10px'}} labelStyle={{color: '#9ca3af'}} />
                <Line type="monotone" dataKey="nominalAlt" stroke="#22c55e" strokeDasharray="3 3" dot={false} strokeWidth={1} name="Nominal Alt" isAnimationActive={false} />
                <Line type="monotone" dataKey="liveAlt" stroke="#4ade80" dot={false} strokeWidth={2} name="Live Alt" isAnimationActive={false} />
              </LineChart>
            </ResponsiveContainer>
          </div>
        </div>

        {/* Panel 3: G-Force vs Q-Pressure (Dual Axis) */}
        <div className="bg-[#000015]/80 backdrop-blur-md border border-gray-800 rounded p-2 flex flex-col relative overflow-hidden pointer-events-auto shadow-lg">
          <div className="absolute inset-0 bg-[linear-gradient(rgba(0,255,255,0.03)_1px,transparent_1px),linear-gradient(90deg,rgba(0,255,255,0.03)_1px,transparent_1px)] bg-[size:20px_20px] pointer-events-none" />
          <div className="text-yellow-500 text-xs font-bold tracking-widest mb-2 z-10 border-b border-gray-800 pb-1">AERODYNAMICS (G-FORCE VS Q-PRESSURE)</div>
          <div className="flex-1 w-full h-full z-10">
            <ResponsiveContainer width="100%" height="100%">
              <LineChart data={chartData} margin={{ top: 5, right: 5, left: -20, bottom: 5 }}>
                <CartesianGrid strokeDasharray="3 3" stroke="#1f2937" />
                <XAxis dataKey="time" type="number" domain={['auto', 'auto']} tickFormatter={(v) => `T+${Math.floor(v)}s`} stroke="#4b5563" tick={{fontSize: 10, fill: '#9ca3af'}} />
                <YAxis yAxisId="left" stroke="#ef4444" tick={{fontSize: 10, fill: '#ef4444'}} />
                <YAxis yAxisId="right" orientation="right" stroke="#8b5cf6" tick={{fontSize: 10, fill: '#8b5cf6'}} />
                <Tooltip contentStyle={{backgroundColor: '#000015', border: '1px solid #374151', fontSize: '10px'}} labelStyle={{color: '#9ca3af'}} />
                <Line yAxisId="left" type="monotone" dataKey="nominalGForce" stroke="#ef4444" strokeDasharray="3 3" dot={false} strokeWidth={1} name="Nominal G" isAnimationActive={false} />
                <Line yAxisId="left" type="monotone" dataKey="liveGForce" stroke="#f87171" dot={false} strokeWidth={2} name="Live G" isAnimationActive={false} />
                
                <Line yAxisId="right" type="monotone" dataKey="nominalQ" stroke="#8b5cf6" strokeDasharray="3 3" dot={false} strokeWidth={1} name="Nominal Q" isAnimationActive={false} />
                <Line yAxisId="right" type="monotone" dataKey="liveQ" stroke="#a78bfa" dot={false} strokeWidth={2} name="Live Q" isAnimationActive={false} />
              </LineChart>
            </ResponsiveContainer>
          </div>
        </div>

        {/* Panel 4: Ground Trace (Lat vs Lon) */}
        <div className="bg-[#000015]/80 backdrop-blur-md border border-gray-800 rounded p-2 flex flex-col relative overflow-hidden pointer-events-auto shadow-lg">
          <div className="absolute inset-0 bg-[linear-gradient(rgba(0,255,255,0.03)_1px,transparent_1px),linear-gradient(90deg,rgba(0,255,255,0.03)_1px,transparent_1px)] bg-[size:20px_20px] pointer-events-none" />
          <div className="text-yellow-500 text-xs font-bold tracking-widest mb-2 z-10 border-b border-gray-800 pb-1">GROUND TRACE (WGS84 PROJECTION)</div>
          <div className="flex-1 w-full h-full z-10">
            <ResponsiveContainer width="100%" height="100%">
              <ScatterChart margin={{ top: 5, right: 5, left: -20, bottom: 5 }}>
                <CartesianGrid strokeDasharray="3 3" stroke="#1f2937" />
                <XAxis dataKey="liveLon" type="number" domain={['auto', 'auto']} stroke="#4b5563" tick={{fontSize: 10, fill: '#9ca3af'}} name="Longitude" />
                <YAxis dataKey="liveLat" type="number" domain={['auto', 'auto']} stroke="#f97316" tick={{fontSize: 10, fill: '#f97316'}} name="Latitude" />
                <ZAxis range={[10, 10]} />
                <Tooltip cursor={{strokeDasharray: '3 3'}} contentStyle={{backgroundColor: '#000015', border: '1px solid #374151', fontSize: '10px'}} />
                <Scatter name="Nominal" data={chartData} dataKey="nominalLat" fill="#f97316" shape="circle" line={{strokeDasharray: '3 3', strokeWidth: 1}} isAnimationActive={false} />
                <Scatter name="Live Trace" data={chartData} dataKey="liveLat" fill="#fdba74" shape="circle" line={{strokeWidth: 2}} isAnimationActive={false} />
              </ScatterChart>
            </ResponsiveContainer>
          </div>
        </div>

      </div>

      {/* Bottom HUD Data Banner */}
      <div className="w-full bg-[#000005]/95 border-t-2 border-gray-800 p-4 z-10 flex flex-col shadow-[0_-4px_20px_rgba(0,0,0,0.8)] pointer-events-auto">
        
        {/* Alerts Row */}
        <div className="w-full h-6 mb-2 flex justify-center items-center">
          {flashingAlert && (
            <div className="text-red-500 font-bold tracking-[0.3em] animate-pulse bg-red-900/20 px-8 border border-red-800">
              !!! {flashingAlert} !!!
            </div>
          )}
        </div>

        {/* Telemetry Numbers */}
        <div className="flex justify-between items-center w-full px-8 tabular-nums">
          
          <div className="flex flex-col items-center">
            <span className="text-gray-500 text-xs tracking-widest mb-1">TIME (s)</span>
            <span className="text-white text-3xl font-bold tracking-wider">{tMinus}</span>
          </div>

          <div className="flex flex-col items-center">
            <span className="text-gray-500 text-xs tracking-widest mb-1">RANGE (km)</span>
            <span className="text-green-500 text-3xl font-bold tracking-wider">{rangeKm}</span>
          </div>

          <div className="flex flex-col items-center">
            <span className="text-gray-500 text-xs tracking-widest mb-1">ALTITUDE (km)</span>
            <span className="text-blue-500 text-3xl font-bold tracking-wider">{altitudeKm}</span>
          </div>

          <div className="flex flex-col items-center">
            <span className="text-gray-500 text-xs tracking-widest mb-1">REL. VELOCITY (km/s)</span>
            <span className="text-yellow-500 text-3xl font-bold tracking-wider">{velocityKms}</span>
          </div>

          <div className="flex flex-col items-center relative">
            <span className="text-gray-500 text-xs tracking-widest mb-1">HEADING (deg)</span>
            
            {/* SVG Directional Dial */}
            <div className="relative w-16 h-16 flex items-center justify-center mb-1">
              <svg width="64" height="64" viewBox="0 0 100 100" className="absolute">
                <circle cx="50" cy="50" r="45" fill="none" stroke="#1f2937" strokeWidth="4" />
                {/* Compass ticks */}
                <line x1="50" y1="5" x2="50" y2="15" stroke="#4b5563" strokeWidth="2" />
                <line x1="50" y1="85" x2="50" y2="95" stroke="#4b5563" strokeWidth="2" />
                <line x1="5" y1="50" x2="15" y2="50" stroke="#4b5563" strokeWidth="2" />
                <line x1="85" y1="50" x2="95" y2="50" stroke="#4b5563" strokeWidth="2" />
                
                {/* Rotating Needle */}
                <g style={{ transform: `rotate(${data.yaw}deg)`, transformOrigin: '50px 50px', transition: 'transform 0.1s linear' }}>
                  <polygon points="50,10 60,50 50,45 40,50" fill="#22c55e" />
                  <polygon points="50,90 55,50 50,45 45,50" fill="#374151" />
                </g>
              </svg>
            </div>

            <span className="text-white text-2xl font-bold tracking-wider">{azimuthDeg}</span>
          </div>

        </div>
      </div>

    </div>
  );
}
