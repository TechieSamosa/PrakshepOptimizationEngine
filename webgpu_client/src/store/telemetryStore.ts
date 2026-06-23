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
  connect: () => void;
  disconnect: () => void;
  setMissionConfig: (config: any) => void;
}

export const useTelemetryStore = create<TelemetryState>((set) => {
  // Bind external websocket events to store state
  wsClient.addStatusHandler((status: ConnectionStatus) => {
    set({ status, isConnected: status === 'CONNECTED' });
  });

  wsClient.addMessageHandler((data: TelemetryFrame) => {
    set((state) => {
      // Keep last 600 frames (10 seconds at 60Hz) or whatever is needed for full flight. 
      // For a 3 minute flight at 60Hz = 10,800 frames. We'll store all to show the full path.
      return { 
        data,
        history: [...state.history, data]
      };
    });
  });

  return {
    data: null,
    history: [],
    status: wsClient.status,
    isConnected: wsClient.status === 'CONNECTED',
    missionConfig: null,
    
    connect: () => {
      wsClient.connect();
    },
    
    disconnect: () => {
      wsClient.disconnect();
      set({ data: null, history: [] });
    },

    setMissionConfig: (config: any) => {
      set({ missionConfig: config });
    }
  };
});
