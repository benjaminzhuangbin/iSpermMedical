import React, { useRef, useEffect, useState } from 'react';
import { 
  Play, 
  Pause, 
  RotateCcw, 
  ZoomIn, 
  ZoomOut, 
  Layers, 
  Eye, 
  Crosshair, 
  Maximize2, 
  Flame, 
  HelpCircle,
  Sparkles,
  RefreshCw,
  Info
} from 'lucide-react';
import { SpermCell, AlgSqaMedDataIn, AlgSqaMedDataOut } from '../types';

interface MicroscopeStageProps {
  cells: SpermCell[];
  setCells: React.Dispatch<React.SetStateAction<SpermCell[]>>;
  input: AlgSqaMedDataIn;
  metrics: AlgSqaMedDataOut;
  onRefreshSample: (mode?: 'Human' | 'Porcine' | 'Quality Control Particle') => void;
  selectedCellId: number | null;
  setSelectedCellId: (id: number | null) => void;
}

export const MicroscopeStage: React.FC<MicroscopeStageProps> = ({
  cells,
  setCells,
  input,
  metrics,
  onRefreshSample,
  selectedCellId,
  setSelectedCellId
}) => {
  const canvasRef = useRef<HTMLCanvasElement | null>(null);
  const [isPlaying, setIsPlaying] = useState(true);
  const [showTrails, setShowTrails] = useState(true);
  const [showVectors, setShowVectors] = useState(true);
  const [showBoxes, setShowBoxes] = useState(false);
  const [showGrid, setShowGrid] = useState(true);
  const [opticalMagnification, setOpticalMagnification] = useState<10 | 20 | 40>(20);
  const [sampleType, setSampleType] = useState<'Human' | 'Porcine' | 'Quality Control Particle'>('Human');
  const [backgroundTexture, setBackgroundTexture] = useState<'microscope' | 'raw_sample' | 'darkfield'>('microscope');

  // Animation frame loop
  useEffect(() => {
    let animationFrameId: number;
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    const render = () => {
      const width = canvas.width;
      const height = canvas.height;

      // Update positions if playing
      if (isPlaying) {
        setCells(prevCells => {
          return prevCells.map(cell => {
            if (cell.motilityClass === 'D' || cell.morphology === 'round_cell') {
              // Immotile Brownian slight jitter
              const jitterX = (Math.random() - 0.5) * 0.15;
              const jitterY = (Math.random() - 0.5) * 0.15;
              return {
                ...cell,
                x: Math.max(10, Math.min(width - 10, cell.x + jitterX)),
                y: Math.max(10, Math.min(height - 10, cell.y + jitterY))
              };
            }

            let newAngle = cell.angle;
            let vx = cell.vx;
            let vy = cell.vy;

            // Class C has higher angular rotation (circular / twitching)
            if (cell.motilityClass === 'C') {
              newAngle += cell.angularVelocity;
              const currentSpeed = Math.sqrt(vx * vx + vy * vy);
              vx = Math.cos(newAngle) * currentSpeed;
              vy = Math.sin(newAngle) * currentSpeed;
            } else {
              // Slight path deviation simulating flagellar beat
              newAngle += (Math.random() - 0.5) * 0.08;
              const currentSpeed = Math.sqrt(vx * vx + vy * vy);
              vx = Math.cos(newAngle) * currentSpeed;
              vy = Math.sin(newAngle) * currentSpeed;
            }

            let newX = cell.x + vx;
            let newY = cell.y + vy;

            // Boundary collision reflection
            if (newX < 15 || newX > width - 15) {
              vx = -vx;
              newAngle = Math.atan2(vy, vx);
              newX = Math.max(15, Math.min(width - 15, newX));
            }
            if (newY < 15 || newY > height - 15) {
              vy = -vy;
              newAngle = Math.atan2(vy, vx);
              newY = Math.max(15, Math.min(height - 15, newY));
            }

            // Update trail
            const newTrail = [...cell.trail, { x: newX, y: newY, t: Date.now() }];
            if (newTrail.length > 25) {
              newTrail.shift();
            }

            return {
              ...cell,
              x: newX,
              y: newY,
              vx,
              vy,
              angle: newAngle,
              trail: newTrail
            };
          });
        });
      }

      // Clear & draw background
      ctx.clearRect(0, 0, width, height);

      // Optical microscope field styling
      if (backgroundTexture === 'microscope') {
        const grad = ctx.createRadialGradient(width / 2, height / 2, 50, width / 2, height / 2, width / 1.5);
        grad.addColorStop(0, '#0c1a2e');
        grad.addColorStop(0.7, '#07101f');
        grad.addColorStop(1, '#030712');
        ctx.fillStyle = grad;
        ctx.fillRect(0, 0, width, height);
      } else if (backgroundTexture === 'darkfield') {
        ctx.fillStyle = '#020617';
        ctx.fillRect(0, 0, width, height);
      } else {
        ctx.fillStyle = '#1e293b';
        ctx.fillRect(0, 0, width, height);
      }

      // Draw Hemocytometer / Chamber grid if enabled
      if (showGrid) {
        ctx.strokeStyle = 'rgba(56, 189, 248, 0.08)';
        ctx.lineWidth = 1;
        const gridSize = 40;
        for (let x = 0; x < width; x += gridSize) {
          ctx.beginPath();
          ctx.moveTo(x, 0);
          ctx.lineTo(x, height);
          ctx.stroke();
        }
        for (let y = 0; y < height; y += gridSize) {
          ctx.beginPath();
          ctx.moveTo(0, y);
          ctx.lineTo(width, y);
          ctx.stroke();
        }

        // Chamber boundary guide lines (Triple lines for standard Neubauer / CASA grid)
        ctx.strokeStyle = 'rgba(14, 165, 233, 0.2)';
        ctx.lineWidth = 1.5;
        ctx.strokeRect(40, 40, width - 80, height - 80);
      }

      // Draw trajectory trails
      if (showTrails) {
        cells.forEach(cell => {
          if (cell.trail.length < 2 || cell.motilityClass === 'D') return;

          ctx.beginPath();
          ctx.moveTo(cell.trail[0].x, cell.trail[0].y);
          for (let i = 1; i < cell.trail.length; i++) {
            ctx.lineTo(cell.trail[i].x, cell.trail[i].y);
          }

          let trailColor = 'rgba(148, 163, 184, 0.3)';
          if (cell.motilityClass === 'A') trailColor = 'rgba(16, 185, 129, 0.6)';
          else if (cell.motilityClass === 'B') trailColor = 'rgba(56, 189, 248, 0.6)';
          else if (cell.motilityClass === 'C') trailColor = 'rgba(245, 158, 11, 0.5)';

          ctx.strokeStyle = trailColor;
          ctx.lineWidth = cell.id === selectedCellId ? 2.5 : 1.2;
          ctx.stroke();
        });
      }

      // Draw each cell
      cells.forEach(cell => {
        const isSelected = cell.id === selectedCellId;
        const isRound = cell.morphology === 'round_cell';

        // Draw Bounding Box if enabled
        if (showBoxes || isSelected) {
          ctx.strokeStyle = isSelected ? '#38bdf8' : 'rgba(100, 116, 139, 0.4)';
          ctx.lineWidth = isSelected ? 2 : 1;
          const boxSize = isRound ? 16 : 22;
          ctx.strokeRect(cell.x - boxSize / 2, cell.y - boxSize / 2, boxSize, boxSize);

          if (isSelected) {
            ctx.fillStyle = '#38bdf8';
            ctx.font = '10px monospace';
            ctx.fillText(`#${cell.id} [${cell.motilityClass}] ${cell.vcl.toFixed(1)}µm/s`, cell.x - 20, cell.y - 15);
          }
        }

        // Draw Velocity vector
        if (showVectors && cell.motilityClass !== 'D' && !isRound) {
          ctx.beginPath();
          ctx.moveTo(cell.x, cell.y);
          const vecLength = Math.min(25, cell.vcl * 0.4);
          ctx.lineTo(cell.x + Math.cos(cell.angle) * vecLength, cell.y + Math.sin(cell.angle) * vecLength);
          ctx.strokeStyle = cell.motilityClass === 'A' ? '#10b981' : (cell.motilityClass === 'B' ? '#38bdf8' : '#f59e0b');
          ctx.lineWidth = 1.5;
          ctx.stroke();
        }

        // Draw Sperm Head & Flagellum
        ctx.save();
        ctx.translate(cell.x, cell.y);
        ctx.rotate(cell.angle);

        if (isRound) {
          // Round cell / leukocyte / debris
          ctx.fillStyle = '#94a3b8';
          ctx.beginPath();
          ctx.arc(0, 0, 5, 0, Math.PI * 2);
          ctx.fill();
          ctx.strokeStyle = '#cbd5e1';
          ctx.lineWidth = 1;
          ctx.stroke();
        } else {
          // Flagellum tail with sine-wave lateral beat (ALH)
          ctx.beginPath();
          ctx.moveTo(-4, 0);
          const waveFreq = cell.bcf * 0.2;
          const waveAmp = (cell.alh || 2.5) * (cell.motilityClass === 'D' ? 0 : 0.8);
          const tNow = Date.now() * 0.01;

          for (let tx = -4; tx >= -22; tx -= 2) {
            const waveY = Math.sin((tx * waveFreq) + (isPlaying ? tNow : 0)) * waveAmp * ((tx + 4) / -18);
            ctx.lineTo(tx, waveY);
          }
          ctx.strokeStyle = cell.motilityClass === 'D' ? 'rgba(148, 163, 184, 0.4)' : 'rgba(203, 213, 225, 0.75)';
          ctx.lineWidth = 1;
          ctx.stroke();

          // Midpiece & Cytoplasmic droplet defect
          if (cell.morphology === 'cytoplasmic_droplet') {
            ctx.fillStyle = '#f59e0b';
            ctx.beginPath();
            ctx.arc(-3, 2, 2.2, 0, Math.PI * 2);
            ctx.fill();
          }

          // Oval Sperm Head
          ctx.beginPath();
          let headRx = 4.2;
          let headRy = 2.4;
          if (cell.headDefectType?.includes('Micro')) { headRx = 2.8; headRy = 1.8; }
          if (cell.headDefectType?.includes('Macro')) { headRx = 5.8; headRy = 3.5; }
          if (cell.headDefectType?.includes('Amorphous')) { headRx = 4.0; headRy = 3.8; }

          ctx.ellipse(0, 0, headRx, headRy, 0, 0, Math.PI * 2);

          // Head color coded by WHO motility classification
          let headColor = '#cbd5e1';
          if (cell.motilityClass === 'A') headColor = '#34d399'; // Emerald
          else if (cell.motilityClass === 'B') headColor = '#38bdf8'; // Sky cyan
          else if (cell.motilityClass === 'C') headColor = '#fbbf24'; // Amber
          else headColor = '#64748b'; // Slate immotile

          ctx.fillStyle = headColor;
          ctx.fill();
          ctx.strokeStyle = isSelected ? '#ffffff' : '#1e293b';
          ctx.lineWidth = isSelected ? 1.5 : 0.8;
          ctx.stroke();

          // Acrosome cap highlight
          ctx.beginPath();
          ctx.arc(1.5, 0, headRy * 0.75, -Math.PI / 2, Math.PI / 2, false);
          ctx.fillStyle = 'rgba(255, 255, 255, 0.4)';
          ctx.fill();
        }

        ctx.restore();
      });

      animationFrameId = requestAnimationFrame(render);
    };

    render();

    return () => {
      cancelAnimationFrame(animationFrameId);
    };
  }, [isPlaying, cells, showTrails, showVectors, showBoxes, showGrid, selectedCellId, backgroundTexture]);

  // Handle canvas click to select sperm cell
  const handleCanvasClick = (e: React.MouseEvent<HTMLCanvasElement>) => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const rect = canvas.getBoundingClientRect();
    const clickX = ((e.clientX - rect.left) / rect.width) * canvas.width;
    const clickY = ((e.clientY - rect.top) / rect.height) * canvas.height;

    // Find nearest cell
    let closestCell: SpermCell | null = null;
    let minDistance = 25; // 25px threshold

    cells.forEach(cell => {
      const dist = Math.hypot(cell.x - clickX, cell.y - clickY);
      if (dist < minDistance) {
        minDistance = dist;
        closestCell = cell;
      }
    });

    setSelectedCellId(closestCell ? (closestCell as SpermCell).id : null);
  };

  const selectedCell = cells.find(c => c.id === selectedCellId);

  return (
    <div className="grid grid-cols-1 lg:grid-cols-4 gap-6">
      {/* Microscopic Stage Canvas (3 cols) */}
      <div className="lg:col-span-3 bg-slate-900 border border-slate-800 rounded-2xl p-4 shadow-xl flex flex-col">
        {/* Stage Top Bar */}
        <div className="flex flex-wrap items-center justify-between gap-3 pb-3 border-b border-slate-800">
          <div className="flex items-center gap-3">
            <div className="flex items-center gap-1.5">
              <span className="w-2.5 h-2.5 rounded-full bg-emerald-400 animate-pulse"></span>
              <span className="font-semibold text-sm text-slate-200">CASA Microscopic Field</span>
            </div>
            <span className="text-xs bg-slate-800 text-slate-400 px-2 py-0.5 rounded font-mono">
              Depth: {input.dSampleDepth}µm | Res: {input.dRatioImg}µm/px | {input.dFrameRate} FPS
            </span>
          </div>

          {/* Sample Mode Picker */}
          <div className="flex items-center gap-2">
            <select
              id="select-sample-mode"
              value={sampleType}
              onChange={(e) => {
                const newMode = e.target.value as any;
                setSampleType(newMode);
                onRefreshSample(newMode);
              }}
              className="bg-slate-950 border border-slate-700 text-xs text-cyan-300 rounded-lg px-2.5 py-1.5 focus:outline-none focus:border-cyan-500 font-medium"
            >
              <option value="Human">Human Semen (WHO 6th)</option>
              <option value="Porcine">Porcine CASA (Boar AI)</option>
              <option value="Quality Control Particle">QC Standard Bead (0.5 Particle)</option>
            </select>

            <button
              id="btn-refresh-field"
              onClick={() => onRefreshSample(sampleType)}
              className="p-1.5 text-slate-400 hover:text-cyan-400 hover:bg-slate-800 rounded-lg transition-colors"
              title="Resample Field"
            >
              <RefreshCw className="w-4 h-4" />
            </button>
          </div>
        </div>

        {/* Canvas Stage */}
        <div className="relative flex-1 my-3 bg-black rounded-xl overflow-hidden border border-slate-800/80 cursor-crosshair min-h-[460px] flex items-center justify-center">
          <canvas
            ref={canvasRef}
            width={800}
            height={520}
            onClick={handleCanvasClick}
            className="w-full h-full object-contain"
          />

          {/* Quick HUD Overlays */}
          <div className="absolute top-3 left-3 bg-slate-950/80 backdrop-blur border border-slate-800 text-[11px] font-mono px-3 py-1.5 rounded-lg text-slate-300 flex items-center gap-3">
            <span>Sperm In Field: <strong className="text-cyan-400">{cells.filter(c => c.morphology !== 'round_cell').length}</strong></span>
            <span className="text-slate-600">|</span>
            <span>Motility (PR+NP): <strong className="text-emerald-400">{metrics.dActiveSpermRatio}%</strong></span>
            <span className="text-slate-600">|</span>
            <span>Avg VCL: <strong className="text-cyan-300">{metrics.dAveVCL} µm/s</strong></span>
          </div>

          {/* Magnification Badge */}
          <div className="absolute bottom-3 right-3 bg-slate-950/80 backdrop-blur border border-slate-800 text-xs font-mono px-2.5 py-1 rounded-md text-cyan-400 font-semibold">
            {opticalMagnification}x Optical Phase Contrast
          </div>
        </div>

        {/* Stage Controls Footer */}
        <div className="flex flex-wrap items-center justify-between gap-3 pt-3 border-t border-slate-800 text-xs">
          {/* Playback Controls */}
          <div className="flex items-center gap-2">
            <button
              id="btn-toggle-play"
              onClick={() => setIsPlaying(!isPlaying)}
              className={`flex items-center gap-1.5 px-3 py-1.5 rounded-lg font-medium transition-all ${
                isPlaying
                  ? 'bg-amber-600/20 text-amber-300 border border-amber-500/30 hover:bg-amber-600/30'
                  : 'bg-emerald-600 text-white shadow-md shadow-emerald-600/30'
              }`}
            >
              {isPlaying ? <Pause className="w-3.5 h-3.5" /> : <Play className="w-3.5 h-3.5" />}
              {isPlaying ? 'Pause Motion' : 'Play Live Motion'}
            </button>
            <button
              id="btn-reset-trails"
              onClick={() => setCells(prev => prev.map(c => ({ ...c, trail: [{ x: c.x, y: c.y, t: 0 }] })))}
              className="p-1.5 text-slate-400 hover:text-slate-200 hover:bg-slate-800 rounded-lg transition-colors"
              title="Clear Trajectory Trails"
            >
              <RotateCcw className="w-4 h-4" />
            </button>
          </div>

          {/* Visual Layer Toggles */}
          <div className="flex items-center gap-1.5 bg-slate-950 p-1 rounded-lg border border-slate-800">
            <button
              id="toggle-trails"
              onClick={() => setShowTrails(!showTrails)}
              className={`px-2.5 py-1 rounded text-xs font-medium transition-all ${
                showTrails ? 'bg-cyan-600 text-white' : 'text-slate-400 hover:text-slate-200'
              }`}
            >
              Trajectories
            </button>
            <button
              id="toggle-vectors"
              onClick={() => setShowVectors(!showVectors)}
              className={`px-2.5 py-1 rounded text-xs font-medium transition-all ${
                showVectors ? 'bg-cyan-600 text-white' : 'text-slate-400 hover:text-slate-200'
              }`}
            >
              Vectors
            </button>
            <button
              id="toggle-boxes"
              onClick={() => setShowBoxes(!showBoxes)}
              className={`px-2.5 py-1 rounded text-xs font-medium transition-all ${
                showBoxes ? 'bg-cyan-600 text-white' : 'text-slate-400 hover:text-slate-200'
              }`}
            >
              Boxes
            </button>
            <button
              id="toggle-grid"
              onClick={() => setShowGrid(!showGrid)}
              className={`px-2.5 py-1 rounded text-xs font-medium transition-all ${
                showGrid ? 'bg-cyan-600 text-white' : 'text-slate-400 hover:text-slate-200'
              }`}
            >
              Chamber Grid
            </button>
          </div>

          {/* Magnification Controls */}
          <div className="flex items-center gap-1 bg-slate-950 p-1 rounded-lg border border-slate-800">
            {([10, 20, 40] as const).map(mag => (
              <button
                key={mag}
                onClick={() => setOpticalMagnification(mag)}
                className={`px-2 py-0.5 rounded text-xs font-mono font-medium ${
                  opticalMagnification === mag ? 'bg-slate-800 text-cyan-400' : 'text-slate-500 hover:text-slate-300'
                }`}
              >
                {mag}x
              </button>
            ))}
          </div>
        </div>
      </div>

      {/* Side Inspector Panel (1 col) */}
      <div className="bg-slate-900 border border-slate-800 rounded-2xl p-4 shadow-xl flex flex-col justify-between space-y-4">
        <div>
          <div className="flex items-center justify-between pb-3 border-b border-slate-800">
            <h3 className="font-semibold text-sm text-slate-200 flex items-center gap-2">
              <Crosshair className="w-4 h-4 text-cyan-400" />
              Sperm Cell Inspector
            </h3>
            {selectedCell && (
              <button
                onClick={() => setSelectedCellId(null)}
                className="text-[11px] text-slate-400 hover:text-slate-200"
              >
                Clear Selection
              </button>
            )}
          </div>

          {selectedCell ? (
            <div className="mt-3 space-y-3">
              {/* Selected Cell Header */}
              <div className="bg-slate-950 p-3 rounded-xl border border-slate-800">
                <div className="flex items-center justify-between">
                  <span className="font-mono font-bold text-sm text-cyan-400">Target #{selectedCell.id}</span>
                  <span className={`text-xs px-2 py-0.5 rounded font-medium ${
                    selectedCell.motilityClass === 'A' ? 'bg-emerald-950 text-emerald-400 border border-emerald-800/60' :
                    selectedCell.motilityClass === 'B' ? 'bg-sky-950 text-sky-400 border border-sky-800/60' :
                    selectedCell.motilityClass === 'C' ? 'bg-amber-950 text-amber-400 border border-amber-800/60' :
                    'bg-slate-800 text-slate-400'
                  }`}>
                    Class {selectedCell.motilityClass} ({
                      selectedCell.motilityClass === 'A' ? 'Rapid PR' :
                      selectedCell.motilityClass === 'B' ? 'Slow PR' :
                      selectedCell.motilityClass === 'C' ? 'Non-Prog' : 'Immotile'
                    })
                  </span>
                </div>
                <div className="mt-1 text-xs text-slate-400">
                  Morphology: <span className="text-slate-200 capitalize font-medium">{selectedCell.morphology.replace('_', ' ')}</span>
                  {selectedCell.headDefectType && ` (${selectedCell.headDefectType})`}
                  {selectedCell.midDefectType && ` (${selectedCell.midDefectType})`}
                  {selectedCell.tailDefectType && ` (${selectedCell.tailDefectType})`}
                </div>
              </div>

              {/* Kinematics Grid */}
              <div className="grid grid-cols-2 gap-2 text-xs">
                <div className="bg-slate-950 p-2 rounded-lg border border-slate-800/80">
                  <div className="text-slate-500 text-[10px]">VCL (Curvilinear)</div>
                  <div className="text-sm font-mono font-semibold text-slate-100">{selectedCell.vcl.toFixed(1)} <span className="text-[10px] text-slate-500">µm/s</span></div>
                </div>
                <div className="bg-slate-950 p-2 rounded-lg border border-slate-800/80">
                  <div className="text-slate-500 text-[10px]">VSL (Straight Line)</div>
                  <div className="text-sm font-mono font-semibold text-slate-100">{selectedCell.vsl.toFixed(1)} <span className="text-[10px] text-slate-500">µm/s</span></div>
                </div>
                <div className="bg-slate-950 p-2 rounded-lg border border-slate-800/80">
                  <div className="text-slate-500 text-[10px]">VAP (Average Path)</div>
                  <div className="text-sm font-mono font-semibold text-slate-100">{selectedCell.vap.toFixed(1)} <span className="text-[10px] text-slate-500">µm/s</span></div>
                </div>
                <div className="bg-slate-950 p-2 rounded-lg border border-slate-800/80">
                  <div className="text-slate-500 text-[10px]">LIN (Linearity)</div>
                  <div className="text-sm font-mono font-semibold text-slate-100">{selectedCell.lin.toFixed(2)}</div>
                </div>
                <div className="bg-slate-950 p-2 rounded-lg border border-slate-800/80">
                  <div className="text-slate-500 text-[10px]">STR (Straightness)</div>
                  <div className="text-sm font-mono font-semibold text-slate-100">{selectedCell.str.toFixed(2)}</div>
                </div>
                <div className="bg-slate-950 p-2 rounded-lg border border-slate-800/80">
                  <div className="text-slate-500 text-[10px]">WOB (Wobble)</div>
                  <div className="text-sm font-mono font-semibold text-slate-100">{selectedCell.wob.toFixed(2)}</div>
                </div>
                <div className="bg-slate-950 p-2 rounded-lg border border-slate-800/80">
                  <div className="text-slate-500 text-[10px]">ALH (Lateral Amp)</div>
                  <div className="text-sm font-mono font-semibold text-slate-100">{selectedCell.alh.toFixed(2)} <span className="text-[10px] text-slate-500">µm</span></div>
                </div>
                <div className="bg-slate-950 p-2 rounded-lg border border-slate-800/80">
                  <div className="text-slate-500 text-[10px]">BCF (Beat Freq)</div>
                  <div className="text-sm font-mono font-semibold text-slate-100">{selectedCell.bcf.toFixed(1)} <span className="text-[10px] text-slate-500">Hz</span></div>
                </div>
              </div>

              {/* Head Dimensions */}
              <div className="bg-slate-950 p-2.5 rounded-lg border border-slate-800 text-xs space-y-1">
                <div className="text-slate-400 font-medium">Morphometry:</div>
                <div className="flex justify-between text-slate-300">
                  <span>Head Length: {selectedCell.length.toFixed(2)} µm</span>
                  <span>Width: {selectedCell.width.toFixed(2)} µm</span>
                </div>
                <div className="flex justify-between text-slate-300">
                  <span>Circularity: {selectedCell.circularity.toFixed(2)}</span>
                  <span>Area: {selectedCell.area.toFixed(1)} px²</span>
                </div>
              </div>
            </div>
          ) : (
            <div className="mt-6 text-center text-slate-500 text-xs py-10 px-4 border border-dashed border-slate-800 rounded-xl">
              <Crosshair className="w-8 h-8 mx-auto text-slate-600 mb-2 opacity-60" />
              Click on any swimming sperm in the stage to inspect full kinematics, trajectory vectors, and WHO defect grading.
            </div>
          )}
        </div>

        {/* WHO Classification Legend */}
        <div className="bg-slate-950 p-3 rounded-xl border border-slate-800 text-[11px] space-y-1.5">
          <div className="font-semibold text-slate-300 flex items-center gap-1.5 mb-1">
            <Info className="w-3.5 h-3.5 text-cyan-400" />
            WHO 6th Motility Criteria
          </div>
          <div className="flex items-center justify-between text-slate-300">
            <span className="flex items-center gap-1.5">
              <span className="w-2 h-2 rounded-full bg-emerald-400"></span>
              Class A (Rapid-PR ≥25µm/s)
            </span>
            <span className="font-mono text-emerald-400">{metrics.dRatioClassA}%</span>
          </div>
          <div className="flex items-center justify-between text-slate-300">
            <span className="flex items-center gap-1.5">
              <span className="w-2 h-2 rounded-full bg-sky-400"></span>
              Class B (Slow-PR 5-25µm/s)
            </span>
            <span className="font-mono text-sky-400">{metrics.dRatioClassB}%</span>
          </div>
          <div className="flex items-center justify-between text-slate-300">
            <span className="flex items-center gap-1.5">
              <span className="w-2 h-2 rounded-full bg-amber-400"></span>
              Class C (Non-Progressive)
            </span>
            <span className="font-mono text-amber-400">{metrics.dRatioClassC}%</span>
          </div>
          <div className="flex items-center justify-between text-slate-300">
            <span className="flex items-center gap-1.5">
              <span className="w-2 h-2 rounded-full bg-slate-500"></span>
              Class D (Immotile)
            </span>
            <span className="font-mono text-slate-400">{metrics.dRatioClassD}%</span>
          </div>
        </div>
      </div>
    </div>
  );
};
