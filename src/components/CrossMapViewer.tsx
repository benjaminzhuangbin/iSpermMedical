import React, { useState, useRef, useEffect, useMemo } from 'react';
import { 
  Crosshair, 
  Download, 
  Upload, 
  FileCode, 
  ZoomIn, 
  ZoomOut, 
  RotateCcw, 
  Layers, 
  Eye, 
  Info, 
  Check, 
  Copy,
  Sliders,
  Filter,
  Maximize2
} from 'lucide-react';
import { DEFAULT_SPERM_RECORDS, SpermCrossRecord } from '../data/defaultSpermPoints';

interface CrossMapViewerProps {
  // Optional props if needed
}

export const CrossMapViewer: React.FC<CrossMapViewerProps> = () => {
  const [records, setRecords] = useState<SpermCrossRecord[]>(DEFAULT_SPERM_RECORDS);
  const [selectedRecord, setSelectedRecord] = useState<SpermCrossRecord | null>(null);
  const [hoveredRecord, setHoveredRecord] = useState<SpermCrossRecord | null>(null);
  const [mousePos, setMousePos] = useState<{ x: number; y: number } | null>(null);
  
  // Display settings
  const [bgMode, setBgMode] = useState<'black' | 'dark' | 'white'>('black');
  const [lineWidth, setLineWidth] = useState<number>(1);
  const [crossColor, setCrossColor] = useState<string>('#ff1a1a');
  const [showCenterDot, setShowCenterDot] = useState<boolean>(true);
  const [showLabels, setShowLabels] = useState<boolean>(false);
  const [showGrid, setShowGrid] = useState<boolean>(true);
  const [zoomScale, setZoomScale] = useState<number>(1);
  const [viewMode, setViewMode] = useState<'fit' | 'actual'>('fit');
  
  // Filter settings
  const [searchTerm, setSearchTerm] = useState<string>('');
  const [minShapeFilter, setMinShapeFilter] = useState<number>(1.0);
  const [maxShapeFilter, setMaxShapeFilter] = useState<number>(4.0);

  // Active view tab in modal or subpanel
  const [activeSubTab, setActiveSubTab] = useState<'canvas' | 'code' | 'table'>('canvas');
  const [copiedCode, setCopiedCode] = useState(false);

  const canvasRef = useRef<HTMLCanvasElement | null>(null);
  const containerRef = useRef<HTMLDivElement | null>(null);

  // Filtered records
  const filteredRecords = useMemo(() => {
    return records.filter(r => {
      if (searchTerm) {
        const term = searchTerm.trim();
        if (!r.id.toString().includes(term)) return false;
      }
      if (r.shape < minShapeFilter || r.shape > maxShapeFilter) return false;
      return true;
    });
  }, [records, searchTerm, minShapeFilter, maxShapeFilter]);

  // Statistics
  const stats = useMemo(() => {
    if (records.length === 0) return { count: 0, avgMaj: 0, avgMin: 0, avgShape: 0 };
    const total = records.length;
    const avgMaj = records.reduce((s, r) => s + r.maj, 0) / total;
    const avgMin = records.reduce((s, r) => s + r.min, 0) / total;
    const avgShape = records.reduce((s, r) => s + r.shape, 0) / total;
    return { count: total, avgMaj, avgMin, avgShape };
  }, [records]);

  // Canvas Drawing Routine (Strict 1280x720)
  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    const width = 1280;
    const height = 720;
    canvas.width = width;
    canvas.height = height;

    // 1. Draw Background
    if (bgMode === 'black') {
      ctx.fillStyle = '#000000';
      ctx.fillRect(0, 0, width, height);
    } else if (bgMode === 'dark') {
      ctx.fillStyle = '#080d1a';
      ctx.fillRect(0, 0, width, height);
    } else {
      ctx.fillStyle = '#ffffff';
      ctx.fillRect(0, 0, width, height);
    }

    // 2. Draw Subtle Optical Grid (if enabled)
    if (showGrid) {
      ctx.strokeStyle = bgMode === 'white' ? 'rgba(0, 0, 0, 0.05)' : 'rgba(255, 255, 255, 0.06)';
      ctx.lineWidth = 1;
      const gridSize = 80;
      for (let x = 0; x <= width; x += gridSize) {
        ctx.beginPath();
        ctx.moveTo(x, 0);
        ctx.lineTo(x, height);
        ctx.stroke();
      }
      for (let y = 0; y <= height; y += gridSize) {
        ctx.beginPath();
        ctx.moveTo(0, y);
        ctx.lineTo(width, y);
        ctx.stroke();
      }

      // Draw Center Axis Crosshair on canvas
      ctx.strokeStyle = bgMode === 'white' ? 'rgba(0, 0, 0, 0.12)' : 'rgba(255, 255, 255, 0.12)';
      ctx.setLineDash([4, 4]);
      ctx.beginPath();
      ctx.moveTo(width / 2, 0);
      ctx.lineTo(width / 2, height);
      ctx.moveTo(0, height / 2);
      ctx.lineTo(width, height / 2);
      ctx.stroke();
      ctx.setLineDash([]);
    }

    // 3. Draw All Sperm Crosses
    // User Specification:
    // 每个点画成一个十字:
    // - 十字横线为 dMinAxsLen (水平短轴)
    // - 十字竖线为 dMajAxsLen (垂直长轴)
    // - 位置为 dPosX (对应 1280), dPosY (对应 720)
    filteredRecords.forEach(rec => {
      const isSelected = selectedRecord?.id === rec.id;
      const isHovered = hoveredRecord?.id === rec.id;

      const cx = rec.x;
      const cy = rec.y;
      const halfW = rec.min / 2.0;
      const halfH = rec.maj / 2.0;

      // Color
      if (isSelected) {
        ctx.strokeStyle = '#06b6d4'; // Cyan highlight for selected
      } else if (isHovered) {
        ctx.strokeStyle = '#facc15'; // Yellow highlight for hovered
      } else {
        ctx.strokeStyle = crossColor; // User selected (default Red #ff1a1a)
      }

      ctx.lineWidth = isSelected ? lineWidth + 1.5 : (isHovered ? lineWidth + 1 : lineWidth);

      // Horizontal line: length = dMinAxsLen
      ctx.beginPath();
      ctx.moveTo(cx - halfW, cy);
      ctx.lineTo(cx + halfW, cy);
      ctx.stroke();

      // Vertical line: length = dMajAxsLen
      ctx.beginPath();
      ctx.moveTo(cx, cy - halfH);
      ctx.lineTo(cx, cy + halfH);
      ctx.stroke();

      // Center dot (optional)
      if (showCenterDot) {
        ctx.fillStyle = isSelected ? '#38bdf8' : (isHovered ? '#fef08a' : (bgMode === 'white' ? '#990000' : '#ff8888'));
        ctx.beginPath();
        ctx.arc(cx, cy, Math.max(0.7, lineWidth * 0.7), 0, Math.PI * 2);
        ctx.fill();
      }

      // Selected ring
      if (isSelected) {
        ctx.strokeStyle = '#06b6d4';
        ctx.lineWidth = 1.5;
        ctx.beginPath();
        ctx.arc(cx, cy, Math.max(halfH, halfW) + 5, 0, Math.PI * 2);
        ctx.stroke();
      }

      // Particle ID Labels (optional)
      if (showLabels) {
        ctx.fillStyle = bgMode === 'white' ? '#1e293b' : '#94a3b8';
        ctx.font = '9px monospace';
        ctx.fillText(`${rec.id}`, cx + halfW + 2, cy - 2);
      }
    });

  }, [filteredRecords, bgMode, lineWidth, crossColor, showCenterDot, showLabels, showGrid, selectedRecord, hoveredRecord]);

  // Handle canvas mouse move for interactive inspection
  const handleCanvasMouseMove = (e: React.MouseEvent<HTMLCanvasElement>) => {
    const canvas = canvasRef.current;
    if (!canvas) return;

    const rect = canvas.getBoundingClientRect();
    const scaleX = 1280 / rect.width;
    const scaleY = 720 / rect.height;

    const mouseX = (e.clientX - rect.left) * scaleX;
    const mouseY = (e.clientY - rect.top) * scaleY;
    setMousePos({ x: Math.round(mouseX), y: Math.round(mouseY) });

    // Find nearest point within 10 pixels radius
    let nearest: SpermCrossRecord | null = null;
    let minDist = 12; // 12px hit threshold

    for (const r of filteredRecords) {
      const dx = r.x - mouseX;
      const dy = r.y - mouseY;
      const dist = Math.sqrt(dx * dx + dy * dy);
      if (dist < minDist) {
        minDist = dist;
        nearest = r;
      }
    }
    setHoveredRecord(nearest);
  };

  const handleCanvasClick = () => {
    if (hoveredRecord) {
      setSelectedRecord(hoveredRecord);
    } else {
      setSelectedRecord(null);
    }
  };

  // Download high-resolution 1280x720 PNG
  const handleDownloadPNG = () => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const link = document.createElement('a');
    link.download = `sperm_cross_map_1280x720_${bgMode}.png`;
    link.href = canvas.toDataURL('image/png');
    link.click();
  };

  // Upload custom CSV
  const handleFileUpload = (e: React.ChangeEvent<HTMLInputElement>) => {
    const file = e.target.files?.[0];
    if (!file) return;

    const reader = new FileReader();
    reader.onload = (evt) => {
      const text = evt.target?.result as string;
      if (!text) return;

      const lines = text.split(/\r?\n/);
      const parsed: SpermCrossRecord[] = [];
      let startIdx = 0;
      if (lines[0] && lines[0].includes('dPosX')) {
        startIdx = 1;
      }

      for (let i = startIdx; i < lines.length; i++) {
        const line = lines[i].trim();
        if (!line) continue;
        const cols = line.split(',');
        if (cols.length >= 5) {
          const id = parseInt(cols[0], 10) || parsed.length + 1;
          const x = parseFloat(cols[1]);
          const y = parseFloat(cols[2]);
          const maj = parseFloat(cols[3]);
          const min = parseFloat(cols[4]);
          const shape = cols.length > 5 ? parseFloat(cols[5]) : (min > 0 ? parseFloat((maj / min).toFixed(2)) : 1);
          const area = cols.length > 6 ? parseFloat(cols[6]) : 0;
          const circ = cols.length > 7 ? parseFloat(cols[7]) : 0;

          if (!isNaN(x) && !isNaN(y) && !isNaN(maj) && !isNaN(min)) {
            parsed.push({ id, x, y, maj, min, shape, area, circ });
          }
        }
      }

      if (parsed.length > 0) {
        setRecords(parsed);
        setSelectedRecord(null);
      }
    };
    reader.readAsText(file);
  };

  // Python Code Text to display/copy
  const pythonScriptCode = `#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
iSperm Medical - 精子十字形态位置图绘制脚本
尺寸: 1280x720 | 十字横线: dMinAxsLen | 十字竖线: dMajAxsLen | 中心: (dPosX, dPosY)
"""
import os, sys, csv, zlib, struct

DEFAULT_WIDTH = 1280
DEFAULT_HEIGHT = 720
COLOR_RED = (255, 0, 0)
COLOR_BLACK = (0, 0, 0)

def draw_sperm_crosses(csv_file, out_png="sperm_cross_map_1280x720.png", bg_white=False):
    width, height = DEFAULT_WIDTH, DEFAULT_HEIGHT
    bg = (255, 255, 255) if bg_white else (0, 0, 0)
    pixels = bytearray(width * height * 3)
    
    # 填充背景色
    for i in range(0, len(pixels), 3):
        pixels[i], pixels[i+1], pixels[i+2] = bg

    def set_px(x, y, color):
        if 0 <= x < width and 0 <= y < height:
            idx = (y * width + x) * 3
            pixels[idx], pixels[idx+1], pixels[idx+2] = color

    # 读取 CSV 并画十字
    with open(csv_file, 'r', encoding='utf-8-sig') as f:
        reader = csv.reader(f)
        header = next(reader)
        count = 0
        for row in reader:
            if len(row) < 5: continue
            try:
                cx = float(row[1])      # dPosX
                cy = float(row[2])      # dPosY
                maj = float(row[3])     # dMajAxsLen (竖线)
                min_len = float(row[4]) # dMinAxsLen (横线)
                
                # 绘制横线 (长度为 dMinAxsLen)
                hw = max(0.5, min_len / 2.0)
                for x in range(round(cx - hw), round(cx + hw) + 1):
                    set_px(x, round(cy), COLOR_RED)
                
                # 绘制竖线 (长度为 dMajAxsLen)
                hh = max(0.5, maj / 2.0)
                for y in range(round(cy - hh), round(cy + hh) + 1):
                    set_px(round(cx), y, COLOR_RED)
                count += 1
            except ValueError:
                continue

    # 原生输出标准 PNG (利用 zlib + struct)
    def chunk(tag, data):
        return struct.pack('>I', len(data)) + tag + data + struct.pack('>I', zlib.crc32(tag + data) & 0xffffffff)
    
    raw = bytearray()
    row_bytes = width * 3
    for y in range(height):
        raw.append(0)
        raw.extend(pixels[y * row_bytes : (y + 1) * row_bytes])
    
    png = b'\\x89PNG\\r\\n\\x1a\\n' + \\
          chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0)) + \\
          chunk(b'IDAT', zlib.compress(bytes(raw), 9)) + \\
          chunk(b'IEND', b'')
          
    with open(out_png, 'wb') as f:
        f.write(png)
    print(f"成功绘制 {count} 个标记点，输出图像: {out_png}")

if __name__ == "__main__":
    csv_path = sys.argv[1] if len(sys.argv) > 1 else "AllSpermInfor-100023.csv"
    draw_sperm_crosses(csv_path, "sperm_cross_map_black_bg.png", bg_white=False)
    draw_sperm_crosses(csv_path, "sperm_cross_map_white_bg.png", bg_white=True)
`;

  const copyCodeToClipboard = () => {
    navigator.clipboard.writeText(pythonScriptCode);
    setCopiedCode(true);
    setTimeout(() => setCopiedCode(false), 2000);
  };

  return (
    <div className="space-y-6">
      {/* Top Header Card */}
      <div className="bg-slate-900 border border-slate-800 rounded-xl p-5 shadow-xl">
        <div className="flex flex-col lg:flex-row items-start lg:items-center justify-between gap-4">
          <div className="flex items-center gap-3">
            <div className="w-12 h-12 rounded-xl bg-gradient-to-tr from-red-600 via-rose-500 to-amber-500 flex items-center justify-center shadow-lg shadow-red-500/20">
              <Crosshair className="w-6 h-6 text-white" />
            </div>
            <div>
              <div className="flex items-center gap-2">
                <h1 className="text-xl font-bold text-white tracking-tight">
                  精子十字形态位置图 (AllSpermInfor Cross Map)
                </h1>
                <span className="text-xs bg-red-950 text-red-400 border border-red-800/60 font-mono px-2.5 py-0.5 rounded-full font-medium">
                  1280 × 720 HD
                </span>
                <span className="text-xs bg-cyan-950 text-cyan-400 border border-cyan-800/60 font-mono px-2 py-0.5 rounded-full">
                  627 Particles
                </span>
              </div>
              <p className="text-xs text-slate-400 mt-1">
                根据 <code className="text-cyan-400">AllSpermInfor-100023.csv</code> 内部数据精准绘制：
                横线为 <span className="text-amber-300 font-mono">dMinAxsLen</span>，
                竖线为 <span className="text-emerald-300 font-mono">dMajAxsLen</span>，
                中心为 <span className="text-rose-300 font-mono">(dPosX, dPosY)</span>。
              </p>
            </div>
          </div>

          {/* Quick Action Buttons */}
          <div className="flex flex-wrap items-center gap-2">
            <label className="flex items-center gap-1.5 px-3 py-1.5 rounded-lg text-xs font-medium bg-slate-800 hover:bg-slate-700 text-slate-200 border border-slate-700 cursor-pointer transition-colors">
              <Upload className="w-3.5 h-3.5 text-cyan-400" />
              <span>导入 CSV</span>
              <input type="file" accept=".csv" onChange={handleFileUpload} className="hidden" />
            </label>

            <button
              onClick={handleDownloadPNG}
              className="flex items-center gap-1.5 px-3.5 py-1.5 rounded-lg text-xs font-medium bg-gradient-to-r from-red-600 to-rose-600 hover:from-red-500 hover:to-rose-500 text-white shadow-md shadow-red-600/25 transition-all"
            >
              <Download className="w-3.5 h-3.5" />
              <span>下载 1280×720 PNG</span>
            </button>

            <a
              href="/AllSpermInfor-100023.csv"
              download="AllSpermInfor-100023.csv"
              className="flex items-center gap-1.5 px-3 py-1.5 rounded-lg text-xs font-medium bg-slate-800 hover:bg-slate-700 text-slate-200 border border-slate-700 transition-colors"
            >
              <Download className="w-3.5 h-3.5 text-slate-400" />
              <span>下载 CSV</span>
            </a>

            <a
              href="/draw_sperm_crosses.py"
              download="draw_sperm_crosses.py"
              className="flex items-center gap-1.5 px-3 py-1.5 rounded-lg text-xs font-medium bg-slate-800 hover:bg-slate-700 text-cyan-300 border border-slate-700 transition-colors"
            >
              <FileCode className="w-3.5 h-3.5 text-cyan-400" />
              <span>下载 Python 脚本</span>
            </a>
          </div>
        </div>

        {/* Metric Badges Summary */}
        <div className="grid grid-cols-2 sm:grid-cols-4 gap-3 mt-4 pt-4 border-t border-slate-800/80">
          <div className="bg-slate-950/50 p-2.5 rounded-lg border border-slate-800">
            <span className="text-[11px] text-slate-400 block">标记颗粒总数</span>
            <span className="text-lg font-bold font-mono text-cyan-400">{filteredRecords.length} / {stats.count}</span>
          </div>
          <div className="bg-slate-950/50 p-2.5 rounded-lg border border-slate-800">
            <span className="text-[11px] text-slate-400 block">平均竖线长轴 (dMajAxsLen)</span>
            <span className="text-lg font-bold font-mono text-emerald-400">{stats.avgMaj.toFixed(2)} px</span>
          </div>
          <div className="bg-slate-950/50 p-2.5 rounded-lg border border-slate-800">
            <span className="text-[11px] text-slate-400 block">平均横线短轴 (dMinAxsLen)</span>
            <span className="text-lg font-bold font-mono text-amber-400">{stats.avgMin.toFixed(2)} px</span>
          </div>
          <div className="bg-slate-950/50 p-2.5 rounded-lg border border-slate-800">
            <span className="text-[11px] text-slate-400 block">平均长宽形态比 (dShape)</span>
            <span className="text-lg font-bold font-mono text-rose-400">{stats.avgShape.toFixed(2)}</span>
          </div>
        </div>
      </div>

      {/* Control Toolbar & Sub-tabs */}
      <div className="flex flex-wrap items-center justify-between gap-3 bg-slate-900 border border-slate-800 p-3 rounded-xl">
        {/* View Toggle Tabs */}
        <div className="flex items-center gap-1 bg-slate-950 p-1 rounded-lg border border-slate-800">
          <button
            onClick={() => setActiveSubTab('canvas')}
            className={`flex items-center gap-1.5 px-3 py-1 rounded-md text-xs font-medium transition-all ${
              activeSubTab === 'canvas'
                ? 'bg-red-600 text-white shadow-sm'
                : 'text-slate-400 hover:text-slate-200'
            }`}
          >
            <Crosshair className="w-3.5 h-3.5" />
            画布视窗 (1280×720)
          </button>
          <button
            onClick={() => setActiveSubTab('code')}
            className={`flex items-center gap-1.5 px-3 py-1 rounded-md text-xs font-medium transition-all ${
              activeSubTab === 'code'
                ? 'bg-red-600 text-white shadow-sm'
                : 'text-slate-400 hover:text-slate-200'
            }`}
          >
            <FileCode className="w-3.5 h-3.5" />
            Python 程序与 EXE 说明
          </button>
          <button
            onClick={() => setActiveSubTab('table')}
            className={`flex items-center gap-1.5 px-3 py-1 rounded-md text-xs font-medium transition-all ${
              activeSubTab === 'table'
                ? 'bg-red-600 text-white shadow-sm'
                : 'text-slate-400 hover:text-slate-200'
            }`}
          >
            <Sliders className="w-3.5 h-3.5" />
            数据表格 ({filteredRecords.length})
          </button>
        </div>

        {/* Canvas Adjustments */}
        {activeSubTab === 'canvas' && (
          <div className="flex flex-wrap items-center gap-3 text-xs text-slate-300">
            {/* Background Selector */}
            <div className="flex items-center gap-1 bg-slate-950 px-2 py-1 rounded-lg border border-slate-800">
              <span className="text-slate-400 text-[11px]">背景:</span>
              <button
                onClick={() => setBgMode('black')}
                className={`px-2 py-0.5 rounded text-[11px] font-medium transition-colors ${
                  bgMode === 'black' ? 'bg-slate-800 text-cyan-400 font-bold border border-cyan-800/60' : 'text-slate-400 hover:text-slate-200'
                }`}
              >
                深黑 (经典)
              </button>
              <button
                onClick={() => setBgMode('dark')}
                className={`px-2 py-0.5 rounded text-[11px] font-medium transition-colors ${
                  bgMode === 'dark' ? 'bg-slate-800 text-cyan-400 font-bold border border-cyan-800/60' : 'text-slate-400 hover:text-slate-200'
                }`}
              >
                显微视野
              </button>
              <button
                onClick={() => setBgMode('white')}
                className={`px-2 py-0.5 rounded text-[11px] font-medium transition-colors ${
                  bgMode === 'white' ? 'bg-slate-200 text-slate-950 font-bold' : 'text-slate-400 hover:text-slate-200'
                }`}
              >
                纯白 (报告)
              </button>
            </div>

            {/* Line width */}
            <div className="flex items-center gap-1.5 bg-slate-950 px-2.5 py-1 rounded-lg border border-slate-800">
              <span className="text-slate-400 text-[11px]">十字粗细:</span>
              {[1, 2, 3].map(w => (
                <button
                  key={w}
                  onClick={() => setLineWidth(w)}
                  className={`w-5 h-5 rounded text-[11px] font-mono font-medium transition-colors ${
                    lineWidth === w ? 'bg-red-600 text-white font-bold' : 'text-slate-400 hover:text-white bg-slate-800'
                  }`}
                >
                  {w}
                </button>
              ))}
            </div>

            {/* Checkboxes */}
            <label className="flex items-center gap-1.5 cursor-pointer select-none">
              <input
                type="checkbox"
                checked={showCenterDot}
                onChange={e => setShowCenterDot(e.target.checked)}
                className="rounded border-slate-700 text-red-600 focus:ring-0 bg-slate-950"
              />
              <span className="text-[11px] text-slate-300">中心定位点</span>
            </label>

            <label className="flex items-center gap-1.5 cursor-pointer select-none">
              <input
                type="checkbox"
                checked={showGrid}
                onChange={e => setShowGrid(e.target.checked)}
                className="rounded border-slate-700 text-red-600 focus:ring-0 bg-slate-950"
              />
              <span className="text-[11px] text-slate-300">标尺网格</span>
            </label>

            <label className="flex items-center gap-1.5 cursor-pointer select-none">
              <input
                type="checkbox"
                checked={showLabels}
                onChange={e => setShowLabels(e.target.checked)}
                className="rounded border-slate-700 text-red-600 focus:ring-0 bg-slate-950"
              />
              <span className="text-[11px] text-slate-300">颗粒序号</span>
            </label>

            {/* View scale toggle */}
            <button
              onClick={() => setViewMode(prev => prev === 'fit' ? 'actual' : 'fit')}
              className="flex items-center gap-1 bg-slate-950 px-2 py-1 rounded-lg border border-slate-800 text-[11px] text-slate-300 hover:text-white"
            >
              <Maximize2 className="w-3 h-3 text-cyan-400" />
              <span>{viewMode === 'fit' ? '100% 原始比例' : '适应屏幕'}</span>
            </button>
          </div>
        )}
      </div>

      {/* Main Content Area */}
      {activeSubTab === 'canvas' && (
        <div className="space-y-4">
          {/* Canvas Wrapper */}
          <div 
            ref={containerRef}
            className={`relative rounded-xl border border-slate-800 overflow-hidden shadow-2xl bg-black ${
              viewMode === 'actual' ? 'overflow-auto max-h-[760px]' : ''
            }`}
          >
            {/* Top Coordinate & Hover Overlay */}
            <div className="absolute top-3 left-3 z-10 flex items-center gap-2 bg-slate-900/85 backdrop-blur-md px-3 py-1.5 rounded-lg border border-slate-700 text-xs">
              <span className="font-mono text-cyan-400">
                X: {mousePos ? mousePos.x : '---'} px | Y: {mousePos ? mousePos.y : '---'} px
              </span>
              <span className="text-slate-600">|</span>
              <span className="text-slate-400">分辨率: 1280 × 720 px</span>
              {hoveredRecord && (
                <>
                  <span className="text-slate-600">|</span>
                  <span className="text-amber-300 font-medium">
                    #{hoveredRecord.id} (Maj: {hoveredRecord.maj}px, Min: {hoveredRecord.min}px, Shape: {hoveredRecord.shape})
                  </span>
                </>
              )}
            </div>

            {/* Selected Particle Overlay Badge */}
            {selectedRecord && (
              <div className="absolute top-3 right-3 z-10 bg-slate-900/90 backdrop-blur-md px-3.5 py-2 rounded-lg border border-cyan-500/60 shadow-lg text-xs space-y-1">
                <div className="flex items-center justify-between gap-4 font-semibold text-cyan-300">
                  <span>精子编号 #{selectedRecord.id}</span>
                  <button 
                    onClick={() => setSelectedRecord(null)}
                    className="text-slate-400 hover:text-white text-xs"
                  >
                    ✕
                  </button>
                </div>
                <div className="grid grid-cols-2 gap-x-3 gap-y-0.5 text-[11px] text-slate-300 font-mono">
                  <span>坐标: ({selectedRecord.x}, {selectedRecord.y})</span>
                  <span>长宽比: {selectedRecord.shape}</span>
                  <span>竖线(长轴): {selectedRecord.maj} px</span>
                  <span>横线(短轴): {selectedRecord.min} px</span>
                  <span>面积: {selectedRecord.area} px²</span>
                  <span>圆度: {selectedRecord.circ}</span>
                </div>
              </div>
            )}

            {/* Canvas Element */}
            <div className={`flex items-center justify-center ${viewMode === 'actual' ? 'w-[1280px] h-[720px]' : 'w-full aspect-[16/9]'}`}>
              <canvas
                ref={canvasRef}
                onMouseMove={handleCanvasMouseMove}
                onClick={handleCanvasClick}
                onMouseLeave={() => { setHoveredRecord(null); setMousePos(null); }}
                className={`cursor-crosshair ${viewMode === 'fit' ? 'w-full h-auto max-h-[720px] object-contain' : ''}`}
                style={{ imageRendering: 'pixelated' }}
              />
            </div>
          </div>

          {/* Explanation Banner */}
          <div className="bg-slate-900/60 border border-slate-800 rounded-xl p-4 text-xs text-slate-300 flex flex-col md:flex-row items-start md:items-center justify-between gap-3">
            <div className="flex items-start gap-2.5">
              <Info className="w-4 h-4 text-cyan-400 mt-0.5 shrink-0" />
              <div>
                <p className="font-medium text-slate-200">
                  形态几何逻辑核对说明：
                </p>
                <p className="text-slate-400 mt-0.5">
                  每一个标记点绘制为正十字线：横向水平线段长度为 <strong className="text-amber-300">dMinAxsLen</strong>（短轴），竖向垂直线段长度为 <strong className="text-emerald-300">dMajAxsLen</strong>（长轴）。
                  十字交点严格对准中心坐标 <strong className="text-rose-300">(dPosX, dPosY)</strong>。鼠标悬停或点击即可查看具体颗粒参数。
                </p>
              </div>
            </div>
            <div className="flex items-center gap-2 shrink-0">
              <button
                onClick={() => setBgMode(prev => prev === 'black' ? 'white' : 'black')}
                className="px-3 py-1.5 rounded-lg bg-slate-800 hover:bg-slate-700 text-slate-200 border border-slate-700 text-xs transition-colors"
              >
                切换黑/白底
              </button>
              <button
                onClick={handleDownloadPNG}
                className="px-3 py-1.5 rounded-lg bg-red-600 hover:bg-red-500 text-white font-medium text-xs transition-colors flex items-center gap-1.5"
              >
                <Download className="w-3.5 h-3.5" />
                保存高清图
              </button>
            </div>
          </div>
        </div>
      )}

      {/* Code & Program Documentation Tab */}
      {activeSubTab === 'code' && (
        <div className="space-y-6">
          <div className="bg-slate-900 border border-slate-800 rounded-xl p-5 shadow-xl space-y-4">
            <div className="flex items-center justify-between">
              <div>
                <h3 className="text-base font-bold text-white flex items-center gap-2">
                  <FileCode className="w-5 h-5 text-cyan-400" />
                  已创建的独立绘图程序 (位于本地 AllSpermInfor 文件夹)
                </h3>
                <p className="text-xs text-slate-400 mt-1">
                  采用纯 Python 3 原生标准库实现，零第三方依赖 (免装 pip/pillow/cv2)，在任何 Windows、Mac 或 Linux 上即开即用。
                </p>
              </div>
              <button
                onClick={copyCodeToClipboard}
                className="flex items-center gap-1.5 px-3 py-1.5 rounded-lg text-xs font-medium bg-slate-800 hover:bg-slate-700 text-cyan-300 border border-slate-700 transition-colors"
              >
                {copiedCode ? <Check className="w-3.5 h-3.5 text-emerald-400" /> : <Copy className="w-3.5 h-3.5" />}
                <span>{copiedCode ? '已复制源码' : '复制 Python 代码'}</span>
              </button>
            </div>

            {/* Folder Structure */}
            <div className="bg-slate-950 p-4 rounded-lg border border-slate-800 font-mono text-xs text-slate-300 space-y-1">
              <div className="text-cyan-400 font-semibold mb-2">📁 AllSpermInfor/ 文件组织结构：</div>
              <div className="pl-2">├── <span className="text-emerald-400 font-bold">draw_sperm_crosses.py</span>  ← 核心绘图程序 (支持命令行参数、生成高清 PNG/BMP)</div>
              <div className="pl-2">├── <span className="text-amber-400 font-bold">run_draw.bat</span>          ← Windows 一键双击运行脚本</div>
              <div className="pl-2">├── <span className="text-cyan-300">AllSpermInfor-100023.csv</span>  ← 627 颗精子分析源数据</div>
              <div className="pl-2">├── <span className="text-rose-400">sperm_cross_map_black_bg.png</span> (1280x720 黑色底图，12 KB)</div>
              <div className="pl-2">├── <span className="text-rose-400">sperm_cross_map_white_bg.png</span> (1280x720 白色底图，13 KB)</div>
              <div className="pl-2">├── <span className="text-slate-400">sperm_cross_map_black_bg.bmp</span> (24-bit 无损 BMP，2.7 MB)</div>
              <div className="pl-2">└── <span className="text-slate-400">README.md</span>                   ← 完整使用说明文档</div>
            </div>

            {/* Code Box */}
            <div className="relative">
              <pre className="bg-slate-950 text-slate-200 p-4 rounded-xl border border-slate-800 text-xs font-mono overflow-x-auto max-h-[460px] leading-relaxed">
                {pythonScriptCode}
              </pre>
            </div>

            {/* How to run instructions */}
            <div className="grid grid-cols-1 md:grid-cols-2 gap-4 pt-2">
              <div className="bg-slate-950/60 p-4 rounded-lg border border-slate-800 text-xs space-y-2">
                <span className="font-semibold text-emerald-400 block">方式 1: Windows 直接双击运行</span>
                <p className="text-slate-400">
                  在 Windows 资源管理器中打开 <code className="text-cyan-300">AllSpermInfor</code> 文件夹，直接双击 <code className="text-amber-300">run_draw.bat</code>，程序将自动调用 Python 执行绘制并在同目录下输出 PNG 与 BMP 图片。
                </p>
              </div>
              <div className="bg-slate-950/60 p-4 rounded-lg border border-slate-800 text-xs space-y-2">
                <span className="font-semibold text-cyan-400 block">方式 2: 命令行或指定参数运行</span>
                <p className="text-slate-400 font-mono">
                  cd AllSpermInfor<br/>
                  python draw_sperm_crosses.py --linewidth 2<br/>
                  python draw_sperm_crosses.py --csv 你的文件.csv
                </p>
              </div>
            </div>
          </div>
        </div>
      )}

      {/* Data Table Tab */}
      {activeSubTab === 'table' && (
        <div className="bg-slate-900 border border-slate-800 rounded-xl p-5 shadow-xl space-y-4">
          <div className="flex flex-col sm:flex-row sm:items-center justify-between gap-3">
            <div>
              <h3 className="text-base font-bold text-white">
                AllSpermInfor-100023 颗粒特征数据列表
              </h3>
              <p className="text-xs text-slate-400">
                共计 {records.length} 个样本点，当前过滤显示 {filteredRecords.length} 个
              </p>
            </div>

            <div className="flex items-center gap-2">
              <input
                type="text"
                placeholder="搜索序号 (例如 10)..."
                value={searchTerm}
                onChange={e => setSearchTerm(e.target.value)}
                className="bg-slate-950 border border-slate-700 px-3 py-1.5 rounded-lg text-xs text-white placeholder-slate-500 focus:outline-none focus:border-cyan-500"
              />
              {searchTerm && (
                <button
                  onClick={() => setSearchTerm('')}
                  className="text-xs text-slate-400 hover:text-white px-2 py-1"
                >
                  清除
                </button>
              )}
            </div>
          </div>

          {/* Table Container */}
          <div className="overflow-x-auto max-h-[500px] border border-slate-800 rounded-lg">
            <table className="w-full text-left text-xs">
              <thead className="bg-slate-950 text-slate-400 sticky top-0 border-b border-slate-800 font-mono">
                <tr>
                  <th className="py-2.5 px-3">i+1</th>
                  <th className="py-2.5 px-3">dPosX (X)</th>
                  <th className="py-2.5 px-3">dPosY (Y)</th>
                  <th className="py-2.5 px-3 text-emerald-400">dMajAxsLen (竖线)</th>
                  <th className="py-2.5 px-3 text-amber-400">dMinAxsLen (横线)</th>
                  <th className="py-2.5 px-3">dShape (长宽比)</th>
                  <th className="py-2.5 px-3">dArea (面积)</th>
                  <th className="py-2.5 px-3">dCircularity (圆度)</th>
                  <th className="py-2.5 px-3 text-right">操作</th>
                </tr>
              </thead>
              <tbody className="divide-y divide-slate-800/60 font-mono">
                {filteredRecords.slice(0, 100).map(rec => {
                  const isSelected = selectedRecord?.id === rec.id;
                  return (
                    <tr
                      key={rec.id}
                      onClick={() => {
                        setSelectedRecord(rec);
                        setActiveSubTab('canvas');
                      }}
                      className={`hover:bg-slate-800/50 cursor-pointer transition-colors ${
                        isSelected ? 'bg-cyan-950/40 text-cyan-300' : 'text-slate-300'
                      }`}
                    >
                      <td className="py-2 px-3 font-bold text-white">#{rec.id}</td>
                      <td className="py-2 px-3">{rec.x.toFixed(1)}</td>
                      <td className="py-2 px-3">{rec.y.toFixed(1)}</td>
                      <td className="py-2 px-3 text-emerald-400 font-semibold">{rec.maj.toFixed(1)} px</td>
                      <td className="py-2 px-3 text-amber-400 font-semibold">{rec.min.toFixed(1)} px</td>
                      <td className="py-2 px-3">
                        <span className={`px-1.5 py-0.5 rounded text-[10px] ${
                          rec.shape <= 1.25 ? 'bg-emerald-950 text-emerald-300 border border-emerald-800' :
                          rec.shape <= 2.2 ? 'bg-cyan-950 text-cyan-300 border border-cyan-800' :
                          'bg-amber-950 text-amber-300 border border-amber-800'
                        }`}>
                          {rec.shape.toFixed(2)}
                        </span>
                      </td>
                      <td className="py-2 px-3">{rec.area.toFixed(1)}</td>
                      <td className="py-2 px-3">{rec.circ.toFixed(2)}</td>
                      <td className="py-2 px-3 text-right">
                        <button
                          onClick={(e) => {
                            e.stopPropagation();
                            setSelectedRecord(rec);
                            setActiveSubTab('canvas');
                          }}
                          className="text-[11px] text-cyan-400 hover:underline"
                        >
                          在画布定位
                        </button>
                      </td>
                    </tr>
                  );
                })}
              </tbody>
            </table>
          </div>
          {filteredRecords.length > 100 && (
            <p className="text-center text-xs text-slate-500">
              为保证流畅性，表格仅预览前 100 条记录；可使用搜索栏快速检索指定编号。
            </p>
          )}
        </div>
      )}
    </div>
  );
};
