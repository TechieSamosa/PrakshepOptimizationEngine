import { create } from 'zustand';
import { wsClient, ConnectionStatus } from '@/lib/websocket';

export interface TelemetryFrame {
  time: number;
  altitude: number;
  velocity_magnitude: number;
  acceleration_magnitude: number;
  downrange_distance: number;
  mach_number: number;
  dynamic_pressure: number;
  max_q_reached: boolean;
  max_q_value: number;
  latitude: number;
  longitude: number;
  geodetic_altitude: number;
  position_eci: [number, number, number];
  velocity_eci: [number, number, number];
  pitch: number;
  yaw: number;
  roll: number;
  current_stage: number;
  thrust: number;
  mass: number;
  propellant_remaining: number;
  structural_integrity: number;
  crash_probability: number;
  event: string;
}

interface TelemetryState {
  data: TelemetryFrame | null;
  history: TelemetryFrame[];
  status: ConnectionStatus;
  isConnected: boolean;
  missionConfig: any | null;
  nominalTrajectory: TelemetryFrame[];
  connect: () => void;
  disconnect: () => void;
  setMissionConfig: (config: any) => void;
}

// Global buffer outside React state to decouple ingestion from rendering
let telemetryBuffer: TelemetryFrame[] = [];
let lastRenderedFrame: TelemetryFrame | null = null;
let animationFrameId: number | null = null;

export const useTelemetryStore = create<TelemetryState>((set) => {
  // Bind external websocket events to store state
  wsClient.addStatusHandler((status: ConnectionStatus) => {
    set({ status, isConnected: status === 'CONNECTED' });
  });

  const commitBuffer = () => {
    if (telemetryBuffer.length > 0) {
      set((state) => {
        const newHistory = [...state.history, ...telemetryBuffer];
        lastRenderedFrame = telemetryBuffer[telemetryBuffer.length - 1];
        
        // Clear buffer after committing
        telemetryBuffer = [];
        
        return { 
          data: lastRenderedFrame,
          history: newHistory
        };
      });
    }
    animationFrameId = requestAnimationFrame(commitBuffer);
  };

  // Start the rAF loop
  if (typeof window !== 'undefined' && !animationFrameId) {
    animationFrameId = requestAnimationFrame(commitBuffer);
  }

  wsClient.addMessageHandler((msg: any) => {
    // Flatten the new nested schema from the backend
    const data: TelemetryFrame = {
      ...(msg.raw || {}),
      ...(msg.analysed || {}),
      ...(msg.events || {})
    };

    // Push to rapid ingestion buffer, do NOT call set() directly
    telemetryBuffer.push(data);
  });

  return {
    data: null,
    history: [],
    nominalTrajectory: [],
    status: wsClient.status,
    isConnected: wsClient.status === 'CONNECTED',
    missionConfig: null,
    
    connect: () => {
      wsClient.connect();
    },
    
    disconnect: () => {
      wsClient.disconnect();
      telemetryBuffer = [];
      set({ data: null, history: [], nominalTrajectory: [] });
    },

    setMissionConfig: (config: any) => {
      // Generate a mock nominal trajectory for visualization baseline
      // A true app would fetch this from an API based on config.vehicle and config.payloadMass
      const mockNominal: TelemetryFrame[] = [];
      for (let i = 0; i < 300; i++) {
        const time = i;
        mockNominal.push({
          time,
          altitude: i < 150 ? (i * i) * 8 : 180000 + (i - 150) * 100,
          velocity_magnitude: i * 25,
          acceleration_magnitude: 9.8 + (i * 0.1),
          downrange_distance: (i * i) * 12,
          mach_number: (i * 25) / 343,
          dynamic_pressure: i < 75 ? i * 500 : Math.max(0, 37500 - (i - 75) * 600),
          latitude: 13.733 - (i * 0.05),
          longitude: 80.235,
          // Unused fields for mock
          max_q_reached: false, max_q_value: 0, geodetic_altitude: 0,
          position_eci: [0,0,0], velocity_eci: [0,0,0],
          pitch: Math.max(0, 90 - i * 0.3), yaw: 0, roll: 0,
          current_stage: i < 120 ? 1 : 2, thrust: 0, mass: 0, propellant_remaining: 1.0,
          structural_integrity: 1.0, crash_probability: 0.0, event: 'NOMINAL'
        });
      }
      set({ missionConfig: config, nominalTrajectory: mockNominal });
    }
  };
});
