import { create } from 'zustand';

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
  isConnected: boolean;
  connect: () => void;
  disconnect: () => void;
}

export const useTelemetryStore = create<TelemetryState>((set, get) => {
  let ws: WebSocket | null = null;

  return {
    data: null,
    isConnected: false,
    
    connect: () => {
      if (ws && ws.readyState === WebSocket.OPEN) return;
      
      const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
      const host = window.location.host;
      const wsUrl = process.env.NEXT_PUBLIC_WS_URL || `${protocol}//${host}/ws/telemetry`;
      
      ws = new WebSocket(wsUrl);
      
      ws.onopen = () => {
        console.log('WebSocket Connected');
        set({ isConnected: true });
      };
      
      ws.onmessage = (event) => {
        try {
          const frame = JSON.parse(event.data);
          set({ data: frame });
        } catch (e) {
          console.error("Failed to parse telemetry frame", e);
        }
      };
      
      ws.onclose = () => {
        console.log('WebSocket Disconnected');
        set({ isConnected: false });
        // Auto reconnect
        setTimeout(() => get().connect(), 1000);
      };
      
      ws.onerror = (error) => {
        console.error('WebSocket Error:', error);
      };
    },
    
    disconnect: () => {
      if (ws) {
        ws.close();
        ws = null;
      }
    }
  };
});
