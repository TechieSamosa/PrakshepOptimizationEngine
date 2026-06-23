'use client';

import React, { useState } from 'react';
import clsx from 'clsx';
import { Rocket, MapPin, Database, CloudRainWind } from 'lucide-react';

interface SetupPanelProps {
  onInitialize: (config: MissionConfig) => void;
}

export interface MissionConfig {
  vehicle: string;
  launchPad: string;
  payloadMass: number;
  targetOrbit: string;
  weatherProfile: string;
}

const VEHICLES = [
  { id: 'PSLV-XL', name: 'PSLV-XL (6 Strap-ons)', agency: 'ISRO' },
  { id: 'GSLV-MK2', name: 'GSLV Mk II', agency: 'ISRO' },
  { id: 'LVM3', name: 'LVM3', agency: 'ISRO' },
  { id: 'FALCON-9', name: 'Falcon 9', agency: 'SpaceX' },
  { id: 'AGNIBAAN', name: 'Agnibaan', agency: 'Agnikul' },
];

const PADS = [
  { id: 'SDSC-1', name: 'SDSC-SHAR Pad 1', coords: '13.733° N, 80.235° E' },
  { id: 'SDSC-2', name: 'SDSC-SHAR Pad 2', coords: '13.719° N, 80.230° E' },
  { id: 'KULASHEK', name: 'Kulashekarapatnam Pad 3', coords: '8.375° N, 78.051° E' },
];

const ORBITS = ['LEO (Low Earth Orbit)', 'SSO (Sun-Synchronous)', 'GTO (Geosynchronous Transfer)'];
const WEATHER = ['Nominal', 'High Wind Shear', 'High Atmospheric Density Perturbation'];

export default function SetupPanel({ onInitialize }: SetupPanelProps) {
  const [config, setConfig] = useState<MissionConfig>({
    vehicle: 'PSLV-XL',
    launchPad: 'SDSC-1',
    payloadMass: 1750,
    targetOrbit: 'SSO (Sun-Synchronous)',
    weatherProfile: 'Nominal',
  });

  return (
    <div className="absolute inset-0 z-50 flex items-center justify-center bg-black/80 backdrop-blur-md p-6 font-sans">
      <div className="w-full max-w-4xl bg-[#000015] border border-cyan-900/50 rounded-xl shadow-[0_0_50px_rgba(0,255,255,0.1)] overflow-hidden">
        
        {/* Header */}
        <div className="bg-gradient-to-r from-cyan-900/40 to-transparent p-6 border-b border-cyan-900/50 flex items-center justify-between">
          <div>
            <h1 className="text-2xl font-bold text-white tracking-widest uppercase">Mission Configuration Console</h1>
            <p className="text-cyan-500 text-sm tracking-widest font-mono mt-1">MCC PRE-LAUNCH INITIALIZATION</p>
          </div>
          <div className="w-4 h-4 rounded-full bg-red-500 animate-pulse shadow-[0_0_10px_red]" />
        </div>

        {/* Form Body */}
        <div className="p-8 grid grid-cols-1 md:grid-cols-2 gap-8">
          
          {/* Left Column */}
          <div className="space-y-6">
            
            <div>
              <label className="flex items-center text-xs font-bold text-cyan-400 uppercase tracking-widest mb-3">
                <Rocket size={14} className="mr-2" /> Launch Vehicle Profile
              </label>
              <div className="space-y-2">
                {VEHICLES.map((v) => (
                  <button
                    key={v.id}
                    onClick={() => setConfig({ ...config, vehicle: v.id })}
                    className={clsx(
                      "w-full flex justify-between items-center px-4 py-3 border rounded-lg text-sm transition-all duration-200",
                      config.vehicle === v.id 
                        ? "bg-cyan-900/30 border-cyan-400 text-white shadow-[0_0_15px_rgba(0,255,255,0.2)]" 
                        : "border-gray-800 text-gray-400 hover:border-gray-600 hover:text-gray-200"
                    )}
                  >
                    <span className="font-bold">{v.name}</span>
                    <span className="text-xs font-mono tracking-widest opacity-60">{v.agency}</span>
                  </button>
                ))}
              </div>
            </div>

            <div>
              <label className="flex items-center text-xs font-bold text-cyan-400 uppercase tracking-widest mb-3 mt-8">
                <MapPin size={14} className="mr-2" /> Launch Pad Coordinates
              </label>
              <div className="space-y-2">
                {PADS.map((pad) => (
                  <button
                    key={pad.id}
                    onClick={() => setConfig({ ...config, launchPad: pad.id })}
                    className={clsx(
                      "w-full flex justify-between items-center px-4 py-3 border rounded-lg text-sm transition-all duration-200",
                      config.launchPad === pad.id 
                        ? "bg-cyan-900/30 border-cyan-400 text-white shadow-[0_0_15px_rgba(0,255,255,0.2)]" 
                        : "border-gray-800 text-gray-400 hover:border-gray-600 hover:text-gray-200"
                    )}
                  >
                    <span className="font-bold">{pad.name}</span>
                    <span className="text-xs font-mono text-cyan-500/70">{pad.coords}</span>
                  </button>
                ))}
              </div>
            </div>

          </div>

          {/* Right Column */}
          <div className="space-y-6">
            
            <div>
              <label className="flex items-center text-xs font-bold text-cyan-400 uppercase tracking-widest mb-3">
                <Database size={14} className="mr-2" /> Payload Mass (kg)
              </label>
              <div className="bg-[#000022] border border-gray-800 rounded-lg p-4">
                <div className="flex justify-between items-center mb-4">
                  <span className="text-4xl font-mono font-bold text-white">{config.payloadMass}</span>
                  <span className="text-sm font-mono text-gray-500">KILOGRAMS</span>
                </div>
                <input 
                  type="range" 
                  min="0" 
                  max="10000" 
                  step="50"
                  value={config.payloadMass}
                  onChange={(e) => setConfig({ ...config, payloadMass: parseInt(e.target.value) })}
                  className="w-full accent-cyan-400 h-2 bg-gray-800 rounded-lg appearance-none cursor-pointer"
                />
              </div>
            </div>

            <div>
              <label className="flex items-center text-xs font-bold text-cyan-400 uppercase tracking-widest mb-3">
                <Rocket size={14} className="mr-2" /> Target Orbit
              </label>
              <div className="grid grid-cols-1 gap-2">
                {ORBITS.map((orbit) => (
                  <button
                    key={orbit}
                    onClick={() => setConfig({ ...config, targetOrbit: orbit })}
                    className={clsx(
                      "w-full text-left px-4 py-3 border rounded-lg text-sm font-bold transition-all duration-200",
                      config.targetOrbit === orbit 
                        ? "bg-cyan-900/30 border-cyan-400 text-white shadow-[0_0_15px_rgba(0,255,255,0.2)]" 
                        : "border-gray-800 text-gray-400 hover:border-gray-600 hover:text-gray-200"
                    )}
                  >
                    {orbit}
                  </button>
                ))}
              </div>
            </div>

            <div>
              <label className="flex items-center text-xs font-bold text-cyan-400 uppercase tracking-widest mb-3 mt-4">
                <CloudRainWind size={14} className="mr-2" /> Weather Profile
              </label>
              <select 
                value={config.weatherProfile}
                onChange={(e) => setConfig({ ...config, weatherProfile: e.target.value })}
                className="w-full bg-[#000022] border border-gray-800 text-white text-sm rounded-lg p-3 outline-none focus:border-cyan-400"
              >
                {WEATHER.map(w => <option key={w} value={w}>{w}</option>)}
              </select>
            </div>

          </div>

        </div>

        {/* Footer Action */}
        <div className="bg-[#00000a] p-6 border-t border-cyan-900/50 flex justify-end">
          <button 
            onClick={() => onInitialize(config)}
            className="group relative px-8 py-4 bg-cyan-600 hover:bg-cyan-500 text-black font-bold tracking-widest uppercase rounded overflow-hidden transition-all shadow-[0_0_20px_rgba(0,255,255,0.4)]"
          >
            <div className="absolute inset-0 w-full h-full bg-white/20 transform -translate-x-full group-hover:translate-x-full transition-transform duration-500" />
            Initialize Telemetry Link
          </button>
        </div>

      </div>
    </div>
  );
}
