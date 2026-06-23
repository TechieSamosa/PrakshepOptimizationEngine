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
  targetOrbitPreset: string;
  targetApogee: number;
  targetPerigee: number;
  targetInclination: number;
  weatherProfile: string;
  weatherDate?: string;
  relandingLocation: string;
}

const VEHICLES = [
  { id: 'PSLV-XL', name: 'PSLV-XL (6 Strap-ons)', agency: 'ISRO' },
  { id: 'GSLV-MK2', name: 'GSLV Mk II', agency: 'ISRO' },
  { id: 'LVM3', name: 'LVM3', agency: 'ISRO' },
  { id: 'SSLV', name: 'SSLV', agency: 'ISRO' },
  { id: 'HSLV', name: 'HSLV', agency: 'ISRO' },
  { id: 'NGLV', name: 'NGLV', agency: 'ISRO' },
  { id: 'VIKRAM', name: 'Vikram I', agency: 'Skyroot' },
  { id: 'AGNIBAAN', name: 'Agnibaan', agency: 'Agnikul' },
  { id: 'FALCON-9', name: 'Falcon 9 Block 5', agency: 'SpaceX' },
];

const PADS = [
  { id: 'SDSC-1', name: 'SDSC-SHAR Pad 1', coords: '13.733° N, 80.235° E' },
  { id: 'SDSC-2', name: 'SDSC-SHAR Pad 2', coords: '13.719° N, 80.230° E' },
  { id: 'KULASHEK', name: 'Kulashekarapatnam Pad 3', coords: '8.375° N, 78.051° E' },
];

const ORBITS = [
  { id: 'LEO', name: 'LEO', ap: 300, pe: 300, inc: 28.5 },
  { id: 'SSO', name: 'SSO', ap: 500, pe: 500, inc: 97.4 },
  { id: 'GTO', name: 'GTO', ap: 35786, pe: 250, inc: 18.0 },
  { id: 'CUSTOM', name: 'CUSTOM', ap: 0, pe: 0, inc: 0 }
];

const WEATHER = ['Nominal', 'High Wind Shear', 'High Atmospheric Density Perturbation', 'Predictive Model (Live API)', 'Predictive Model (Future Date)'];
const RELANDING = ['Expendable (No Landing)', 'ASDS (Drone Ship - Bay of Bengal)', 'LZ-1 (SDSC-SHAR Pad 4)', 'LZ-2 (Kulashekarapatnam)'];

export default function SetupPanel({ onInitialize }: SetupPanelProps) {
  const [config, setConfig] = useState<MissionConfig>({
    vehicle: 'PSLV-XL',
    launchPad: 'SDSC-1',
    payloadMass: 1750,
    targetOrbitPreset: 'SSO',
    targetApogee: 500,
    targetPerigee: 500,
    targetInclination: 97.4,
    weatherProfile: 'Nominal',
    weatherDate: new Date().toISOString().slice(0, 16), // YYYY-MM-DDTHH:mm
    relandingLocation: 'Expendable (No Landing)',
  });

  const handleOrbitPreset = (presetId: string) => {
    if (presetId === 'CUSTOM') {
      setConfig({ ...config, targetOrbitPreset: 'CUSTOM' });
      return;
    }
    const o = ORBITS.find(x => x.id === presetId);
    if (o) {
      setConfig({ ...config, targetOrbitPreset: presetId, targetApogee: o.ap, targetPerigee: o.pe, targetInclination: o.inc });
    }
  };

  const handleOrbitCustom = (field: 'targetApogee'|'targetPerigee'|'targetInclination', val: number) => {
    setConfig({ ...config, [field]: val, targetOrbitPreset: 'CUSTOM' });
  };

  return (
    <div className="absolute inset-0 z-50 flex items-center justify-center bg-black/90 backdrop-blur-md font-sans">
      <div className="w-full max-w-5xl h-screen max-h-[768px] bg-[#000015] border border-cyan-900/50 rounded-xl shadow-[0_0_50px_rgba(0,255,255,0.1)] overflow-hidden flex flex-col">
        
        {/* Header */}
        <div className="bg-gradient-to-r from-cyan-900/40 to-transparent p-6 border-b border-cyan-900/50 flex items-center justify-between">
          <div>
            <h1 className="text-2xl font-bold text-white tracking-widest uppercase">Mission Configuration Console</h1>
            <p className="text-cyan-500 text-sm tracking-widest font-mono mt-1">MCC PRE-LAUNCH INITIALIZATION</p>
          </div>
          <div className="w-4 h-4 rounded-full bg-red-500 animate-pulse shadow-[0_0_10px_red]" />
        </div>

        {/* Form Body - Dense 3-column layout */}
        <div className="flex-1 p-6 grid grid-cols-3 gap-6 overflow-hidden">
          
          {/* Column 1: Vehicle & Pad */}
          <div className="flex flex-col space-y-4">
            <div>
              <label className="flex items-center text-[10px] font-bold text-cyan-400 uppercase tracking-widest mb-2">
                <Rocket size={12} className="mr-2" /> Launch Vehicle Profile
              </label>
              <div className="space-y-1 h-[220px] overflow-y-auto custom-scrollbar pr-2">
                {VEHICLES.map((v) => (
                  <button
                    key={v.id}
                    onClick={() => setConfig({ ...config, vehicle: v.id })}
                    className={clsx(
                      "w-full flex justify-between items-center px-3 py-2 border rounded text-xs transition-all duration-200",
                      config.vehicle === v.id 
                        ? "bg-cyan-900/30 border-cyan-400 text-white shadow-[0_0_10px_rgba(0,255,255,0.2)]" 
                        : "border-gray-800 text-gray-400 hover:border-gray-600 hover:text-gray-200"
                    )}
                  >
                    <span className="font-bold">{v.name}</span>
                    <span className="text-[9px] font-mono tracking-widest opacity-60">{v.agency}</span>
                  </button>
                ))}
              </div>
            </div>

            <div className="flex-1">
              <label className="flex items-center text-[10px] font-bold text-cyan-400 uppercase tracking-widest mb-2">
                <MapPin size={12} className="mr-2" /> Launch Pad Coordinates
              </label>
              <div className="space-y-1">
                {PADS.map((pad) => (
                  <button
                    key={pad.id}
                    onClick={() => setConfig({ ...config, launchPad: pad.id })}
                    className={clsx(
                      "w-full flex justify-between items-center px-3 py-2 border rounded text-xs transition-all duration-200",
                      config.launchPad === pad.id 
                        ? "bg-cyan-900/30 border-cyan-400 text-white shadow-[0_0_10px_rgba(0,255,255,0.2)]" 
                        : "border-gray-800 text-gray-400 hover:border-gray-600 hover:text-gray-200"
                    )}
                  >
                    <span className="font-bold">{pad.name}</span>
                  </button>
                ))}
              </div>
            </div>
          </div>

          {/* Column 2: Payload & Advanced Orbit */}
          <div className="flex flex-col space-y-4">
            <div>
              <label className="flex items-center text-[10px] font-bold text-cyan-400 uppercase tracking-widest mb-2">
                <Database size={12} className="mr-2" /> Payload Mass (kg)
              </label>
              <div className="bg-[#000022] border border-gray-800 rounded p-3">
                <div className="flex justify-between items-center mb-2">
                  <span className="text-2xl font-mono font-bold text-white">{config.payloadMass}</span>
                  <span className="text-[10px] font-mono text-gray-500">KG</span>
                </div>
                <input 
                  type="range" min="0" max="50000" step="50"
                  value={config.payloadMass}
                  onChange={(e) => setConfig({ ...config, payloadMass: parseInt(e.target.value) })}
                  className="w-full accent-cyan-400 h-1 bg-gray-800 rounded appearance-none cursor-pointer"
                />
              </div>
            </div>

            <div>
              <label className="flex items-center text-[10px] font-bold text-cyan-400 uppercase tracking-widest mb-2">
                <Rocket size={12} className="mr-2" /> Target Orbit
              </label>
              <div className="flex space-x-2 mb-3">
                {ORBITS.map((orbit) => (
                  <button
                    key={orbit.id}
                    onClick={() => handleOrbitPreset(orbit.id)}
                    className={clsx(
                      "flex-1 text-center py-1 border rounded text-xs font-bold transition-all duration-200",
                      config.targetOrbitPreset === orbit.id 
                        ? "bg-cyan-900/30 border-cyan-400 text-white shadow-[0_0_10px_rgba(0,255,255,0.2)]" 
                        : "border-gray-800 text-gray-400 hover:border-gray-600 hover:text-gray-200"
                    )}
                  >
                    {orbit.id}
                  </button>
                ))}
              </div>
              <div className="grid grid-cols-2 gap-2">
                <div>
                  <label className="text-[9px] font-bold text-cyan-400 uppercase tracking-widest mb-1 block">Apogee (km)</label>
                  <input 
                    type="number" value={config.targetApogee} 
                    onChange={(e) => handleOrbitCustom('targetApogee', parseFloat(e.target.value))}
                    className="w-full bg-[#000022] border border-gray-800 text-white font-mono p-1.5 text-xs rounded outline-none focus:border-cyan-400"
                  />
                </div>
                <div>
                  <label className="text-[9px] font-bold text-cyan-400 uppercase tracking-widest mb-1 block">Perigee (km)</label>
                  <input 
                    type="number" value={config.targetPerigee} 
                    onChange={(e) => handleOrbitCustom('targetPerigee', parseFloat(e.target.value))}
                    className="w-full bg-[#000022] border border-gray-800 text-white font-mono p-1.5 text-xs rounded outline-none focus:border-cyan-400"
                  />
                </div>
                <div className="col-span-2">
                  <label className="text-[9px] font-bold text-cyan-400 uppercase tracking-widest mb-1 block">Inclination (°)</label>
                  <input 
                    type="number" value={config.targetInclination} 
                    onChange={(e) => handleOrbitCustom('targetInclination', parseFloat(e.target.value))}
                    className="w-full bg-[#000022] border border-gray-800 text-white font-mono p-1.5 text-xs rounded outline-none focus:border-cyan-400"
                  />
                </div>
              </div>
            </div>
          </div>

          {/* Column 3: Advanced Environment & Landing */}
          <div className="flex flex-col space-y-4">
            <div>
              <label className="flex items-center text-[10px] font-bold text-cyan-400 uppercase tracking-widest mb-2">
                <CloudRainWind size={12} className="mr-2" /> Weather Profile
              </label>
              <select 
                value={config.weatherProfile}
                onChange={(e) => setConfig({ ...config, weatherProfile: e.target.value })}
                className="w-full bg-[#000022] border border-gray-800 text-white text-xs rounded p-2 outline-none focus:border-cyan-400 mb-2"
              >
                {WEATHER.map(w => <option key={w} value={w}>{w}</option>)}
              </select>
              {config.weatherProfile === 'Predictive Model (Future Date)' && (
                <div>
                  <label className="text-[9px] font-bold text-cyan-400 uppercase tracking-widest mb-1 block">Forecast Date & Time</label>
                  <input 
                    type="datetime-local" 
                    value={config.weatherDate}
                    onChange={(e) => setConfig({...config, weatherDate: e.target.value})}
                    className="w-full bg-[#000022] border border-gray-800 text-white font-mono p-1.5 text-xs rounded outline-none focus:border-cyan-400"
                  />
                </div>
              )}
            </div>

            <div className="flex-1">
              <label className="flex items-center text-[10px] font-bold text-cyan-400 uppercase tracking-widest mb-2">
                <MapPin size={12} className="mr-2" /> Relanding Target Zone
              </label>
              <div className="space-y-1">
                {RELANDING.map((loc) => (
                  <button
                    key={loc}
                    onClick={() => setConfig({ ...config, relandingLocation: loc })}
                    className={clsx(
                      "w-full flex justify-between items-center px-3 py-2 border rounded text-xs transition-all duration-200",
                      config.relandingLocation === loc 
                        ? "bg-amber-900/40 border-amber-500 text-white shadow-[0_0_10px_rgba(255,176,0,0.2)]" 
                        : "border-gray-800 text-gray-400 hover:border-gray-600 hover:text-gray-200"
                    )}
                  >
                    <span className="font-bold">{loc}</span>
                  </button>
                ))}
              </div>
            </div>
          </div>
        </div>

        {/* Footer Action */}
        <div className="bg-[#00000a] p-4 border-t border-cyan-900/50 flex justify-end shrink-0">
          <button 
            onClick={() => onInitialize(config)}
            className="group relative px-6 py-3 bg-cyan-600 hover:bg-cyan-500 text-black text-sm font-bold tracking-widest uppercase rounded overflow-hidden transition-all shadow-[0_0_15px_rgba(0,255,255,0.4)]"
          >
            <div className="absolute inset-0 w-full h-full bg-white/20 transform -translate-x-full group-hover:translate-x-full transition-transform duration-500" />
            Initialize Telemetry Link
          </button>
        </div>

      </div>
    </div>
  );
}
