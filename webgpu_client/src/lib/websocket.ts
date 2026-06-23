export type ConnectionStatus = 'DISCONNECTED' | 'CONNECTING' | 'CONNECTED' | 'RECONNECTING';

type MessageHandler = (data: any) => void;
type StatusHandler = (status: ConnectionStatus) => void;

export class PrakshepWebSocket {
  private ws: WebSocket | null = null;
  private url: string;
  private reconnectAttempts = 0;
  private maxReconnectAttempts = 10;
  private baseDelay = 1000; // 1 second
  private maxDelay = 30000; // 30 seconds
  private intentionalClose = false;
  
  private onMessageHandlers: MessageHandler[] = [];
  private onStatusHandlers: StatusHandler[] = [];
  
  public status: ConnectionStatus = 'DISCONNECTED';

  constructor() {
    // Dynamically choose URL based on environment.
    if (typeof window !== 'undefined') {
      const isLocalhost = window.location.hostname === 'localhost' || window.location.hostname === '127.0.0.1';
      const defaultWsUrl = isLocalhost 
        ? 'ws://127.0.0.1:8000/ws/telemetry' 
        : 'wss://prakshep-api.onrender.com/ws/telemetry';
      this.url = process.env.NEXT_PUBLIC_WS_URL || defaultWsUrl;
    } else {
      this.url = 'wss://prakshep-api.onrender.com/ws/telemetry'; // SSR fallback
    }
  }

  public connect() {
    if (this.ws && (this.ws.readyState === WebSocket.OPEN || this.ws.readyState === WebSocket.CONNECTING)) {
      return;
    }

    this.intentionalClose = false;
    this.updateStatus(this.reconnectAttempts > 0 ? 'RECONNECTING' : 'CONNECTING');
    
    if (!this.url) {
      console.error("[WS] WebSocket URL is empty or undefined. Aborting connection.");
      this.updateStatus('DISCONNECTED');
      return;
    }

    try {
      this.ws = new WebSocket(this.url);
      
      this.ws.onopen = this.handleOpen.bind(this);
      this.ws.onmessage = this.handleMessage.bind(this);
      this.ws.onclose = this.handleClose.bind(this);
      this.ws.onerror = this.handleError.bind(this);
    } catch (e) {
      console.error("Failed to construct WebSocket:", e);
      this.scheduleReconnect();
    }
  }

  public disconnect() {
    this.intentionalClose = true;
    if (this.ws) {
      this.ws.close();
      this.ws = null;
    }
    this.updateStatus('DISCONNECTED');
    this.reconnectAttempts = 0;
  }

  public addMessageHandler(handler: MessageHandler) {
    this.onMessageHandlers.push(handler);
  }

  public addStatusHandler(handler: StatusHandler) {
    this.onStatusHandlers.push(handler);
  }

  private handleOpen() {
    console.log(`[WS] Connected securely to ${this.url}`);
    this.reconnectAttempts = 0;
    this.updateStatus('CONNECTED');
  }

  private handleMessage(event: MessageEvent) {
    try {
      const data = JSON.parse(event.data);
      this.onMessageHandlers.forEach(handler => handler(data));
    } catch (e) {
      console.error('[WS] Failed to parse message', e);
    }
  }

  private handleClose(event: CloseEvent) {
    console.warn(`[WS] Connection closed. Code: ${event.code}, Reason: ${event.reason}`);
    this.ws = null;
    
    if (!this.intentionalClose) {
      this.scheduleReconnect();
    }
  }

  private handleError(error: Event) {
    console.error('[WS] Error occurred:', error);
    // onclose will fire immediately after onerror, so we handle reconnect there
  }

  private scheduleReconnect() {
    if (this.reconnectAttempts >= this.maxReconnectAttempts) {
      console.error('[WS] Maximum reconnection attempts reached. Giving up.');
      this.updateStatus('DISCONNECTED');
      return;
    }

    // Exponential backoff with full jitter
    const delay = Math.min(
      this.maxDelay,
      this.baseDelay * Math.pow(2, this.reconnectAttempts)
    );
    const jitter = Math.random() * delay;
    
    this.reconnectAttempts++;
    this.updateStatus('RECONNECTING');
    
    console.log(`[WS] Scheduling reconnect attempt ${this.reconnectAttempts} in ${Math.round(jitter)}ms`);
    setTimeout(() => this.connect(), jitter);
  }

  private updateStatus(newStatus: ConnectionStatus) {
    if (this.status !== newStatus) {
      this.status = newStatus;
      this.onStatusHandlers.forEach(handler => handler(newStatus));
    }
  }
}

// Singleton instance export
export const wsClient = new PrakshepWebSocket();
