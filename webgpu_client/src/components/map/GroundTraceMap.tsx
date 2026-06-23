'use client';

import React, { useRef, useEffect, useState } from 'react';
import { useTelemetryStore } from '@/store/telemetryStore';

export default function GroundTraceMap() {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const { history, nominalTrajectory } = useTelemetryStore();
  
  // View matrix state
  const [scale, setScale] = useState(1.0);
  const [offset, setOffset] = useState({ x: 0, y: 0 });
  const isDragging = useRef(false);
  const lastMouse = useRef({ x: 0, y: 0 });
  const mapCenter = { lat: 13.733, lon: 80.235 }; // SDSC-SHAR
  
  const handleWheel = (e: React.WheelEvent) => {
    e.preventDefault();
    const zoomFactor = e.deltaY < 0 ? 1.1 : 0.9;
    setScale(prev => Math.min(Math.max(0.1, prev * zoomFactor), 50));
  };

  const handleMouseDown = (e: React.MouseEvent) => {
    isDragging.current = true;
    lastMouse.current = { x: e.clientX, y: e.clientY };
  };

  const handleMouseMove = (e: React.MouseEvent) => {
    if (!isDragging.current) return;
    const dx = e.clientX - lastMouse.current.x;
    const dy = e.clientY - lastMouse.current.y;
    setOffset(prev => ({ x: prev.x + dx, y: prev.y + dy }));
    lastMouse.current = { x: e.clientX, y: e.clientY };
  };

  const handleMouseUp = () => {
    isDragging.current = false;
  };

  // Convert lat/lon to local pixel coordinates
  const latLonToPixels = (lat: number, lon: number, width: number, height: number) => {
    // Basic Equirectangular relative to SHAR
    const dLon = lon - mapCenter.lon;
    const dLat = lat - mapCenter.lat;
    
    // Scale factor: 1 degree ~ 111km. Let's say 1 pixel = 1km at scale 1.0. 
    // So 1 degree = 111 pixels.
    const pxPerDegree = 111;
    
    const x = (width / 2) + (dLon * pxPerDegree * scale) + offset.x;
    const y = (height / 2) - (dLat * pxPerDegree * scale) + offset.y;
    
    return { x, y };
  };

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;
    
    let animationFrameId: number;
    
    const render = () => {
      // Handle resize
      if (canvas.width !== canvas.clientWidth || canvas.height !== canvas.clientHeight) {
        canvas.width = canvas.clientWidth;
        canvas.height = canvas.clientHeight;
      }
      
      const width = canvas.width;
      const height = canvas.height;
      
      // Clear background
      ctx.fillStyle = '#000015';
      ctx.fillRect(0, 0, width, height);
      
      // Draw Grid
      ctx.strokeStyle = 'rgba(0, 255, 255, 0.05)';
      ctx.lineWidth = 1;
      const gridSize = 50 * scale;
      const offsetX = offset.x % gridSize;
      const offsetY = offset.y % gridSize;
      
      ctx.beginPath();
      for (let x = offsetX; x < width; x += gridSize) {
        ctx.moveTo(x, 0); ctx.lineTo(x, height);
      }
      for (let y = offsetY; y < height; y += gridSize) {
        ctx.moveTo(0, y); ctx.lineTo(width, y);
      }
      ctx.stroke();

      // Draw Launchpad
      const pad = latLonToPixels(mapCenter.lat, mapCenter.lon, width, height);
      ctx.fillStyle = '#facc15';
      ctx.beginPath();
      ctx.arc(pad.x, pad.y, 5, 0, Math.PI * 2);
      ctx.fill();
      ctx.fillStyle = '#facc15';
      ctx.font = '10px monospace';
      ctx.fillText('SDSC-SHAR', pad.x + 8, pad.y + 4);

      // Draw Nominal Trajectory
      if (nominalTrajectory && nominalTrajectory.length > 0) {
        ctx.strokeStyle = 'rgba(34, 197, 94, 0.5)'; // Dotted green
        ctx.lineWidth = 2;
        ctx.setLineDash([5, 5]);
        ctx.beginPath();
        for (let i = 0; i < nominalTrajectory.length; i++) {
          const pt = latLonToPixels(nominalTrajectory[i].latitude, nominalTrajectory[i].longitude, width, height);
          if (i === 0) ctx.moveTo(pt.x, pt.y);
          else ctx.lineTo(pt.x, pt.y);
        }
        ctx.stroke();
        ctx.setLineDash([]);
      }

      // Draw Live Trajectory
      if (history && history.length > 0) {
        ctx.strokeStyle = '#f97316'; // Solid orange
        ctx.lineWidth = 3;
        ctx.beginPath();
        for (let i = 0; i < history.length; i++) {
          const pt = latLonToPixels(history[i].latitude, history[i].longitude, width, height);
          if (i === 0) ctx.moveTo(pt.x, pt.y);
          else ctx.lineTo(pt.x, pt.y);
        }
        ctx.stroke();
        
        // Draw current position ping
        const currentPt = latLonToPixels(history[history.length - 1].latitude, history[history.length - 1].longitude, width, height);
        ctx.fillStyle = '#f97316';
        ctx.beginPath();
        ctx.arc(currentPt.x, currentPt.y, 4, 0, Math.PI * 2);
        ctx.fill();
        
        // Pulse ring
        const time = Date.now() / 1000;
        const pulse = (time % 1) * 20;
        ctx.strokeStyle = `rgba(249, 115, 22, ${1 - pulse/20})`;
        ctx.beginPath();
        ctx.arc(currentPt.x, currentPt.y, pulse, 0, Math.PI * 2);
        ctx.stroke();
      }

      animationFrameId = requestAnimationFrame(render);
    };
    
    render();
    
    return () => {
      cancelAnimationFrame(animationFrameId);
    };
  }, [history, nominalTrajectory, scale, offset]);

  return (
    <div className="w-full h-full relative cursor-grab active:cursor-grabbing bg-[#000015]">
      <canvas
        ref={canvasRef}
        className="w-full h-full block"
        onWheel={handleWheel}
        onMouseDown={handleMouseDown}
        onMouseMove={handleMouseMove}
        onMouseUp={handleMouseUp}
        onMouseLeave={handleMouseUp}
      />
      <div className="absolute top-4 right-4 bg-black/80 border border-gray-800 p-2 text-xs font-mono text-gray-400 pointer-events-none">
        <div>PROJECTION: WGS84 EQUIRECTANGULAR</div>
        <div>SCALE: {scale.toFixed(2)}X</div>
        <div>DRAG TO PAN | SCROLL TO ZOOM</div>
      </div>
    </div>
  );
}
