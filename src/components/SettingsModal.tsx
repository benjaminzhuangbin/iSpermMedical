import React from 'react';
import { X, Sliders, CheckCircle2, RotateCcw, HelpCircle } from 'lucide-react';
import { AlgSqaMedDataIn } from '../types';
import { DEFAULT_CASA_INPUT } from '../lib/casaEngine';

interface SettingsModalProps {
  isOpen: boolean;
  onClose: () => void;
  input: AlgSqaMedDataIn;
  setInput: React.Dispatch<React.SetStateAction<AlgSqaMedDataIn>>;
}

export const SettingsModal: React.FC<SettingsModalProps> = ({
  isOpen,
  onClose,
  input,
  setInput
}) => {
  if (!isOpen) return null;

  const handleReset = () => {
    setInput({ ...DEFAULT_CASA_INPUT });
  };

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/80 backdrop-blur-sm p-4 overflow-y-auto">
      <div className="bg-slate-900 border border-slate-700 rounded-2xl w-full max-w-2xl shadow-2xl overflow-hidden my-auto flex flex-col max-h-[90vh]">
        {/* Header */}
        <div className="flex items-center justify-between px-6 py-4 border-b border-slate-800 bg-slate-950">
          <div className="flex items-center gap-2">
            <Sliders className="w-5 h-5 text-cyan-400" />
            <h2 className="text-base font-bold text-white">CASA Algorithm & Calibration Parameters</h2>
          </div>
          <div className="flex items-center gap-2">
            <button
              onClick={handleReset}
              className="flex items-center gap-1 text-xs text-slate-400 hover:text-slate-200 bg-slate-800 px-2.5 py-1.5 rounded-lg transition-colors"
            >
              <RotateCcw className="w-3.5 h-3.5" />
              Reset Defaults
            </button>
            <button
              onClick={onClose}
              className="p-1.5 text-slate-400 hover:text-white rounded-lg hover:bg-slate-800 transition-colors"
            >
              <X className="w-5 h-5" />
            </button>
          </div>
        </div>

        {/* Form Body */}
        <div className="p-6 overflow-y-auto space-y-5 text-xs">
          {/* Chamber & Optical Calibration */}
          <div className="space-y-3">
            <h3 className="font-semibold text-slate-200 text-sm border-b border-slate-800 pb-1">
              1. Optical & Chamber Dimensions (<code>tag_algsqamed_data_in</code>)
            </h3>

            <div className="grid grid-cols-1 sm:grid-cols-2 gap-4">
              {/* Optical Ratio */}
              <div className="bg-slate-950 p-3 rounded-xl border border-slate-800">
                <label className="block text-slate-400 font-medium mb-1">
                  Optical Ratio (<code>dRatioImg</code>, µm/px)
                </label>
                <input
                  type="number"
                  step="0.01"
                  value={input.dRatioImg}
                  onChange={(e) => setInput({ ...input, dRatioImg: parseFloat(e.target.value) || 0.65 })}
                  className="w-full bg-slate-900 border border-slate-700 rounded-lg px-3 py-1.5 text-cyan-300 font-mono focus:outline-none focus:border-cyan-500"
                />
                <span className="text-[10px] text-slate-500 block mt-1">Typical: 0.65 µm/px (10x objective)</span>
              </div>

              {/* Sample Depth */}
              <div className="bg-slate-950 p-3 rounded-xl border border-slate-800">
                <label className="block text-slate-400 font-medium mb-1">
                  Chamber Depth (<code>dSampleDepth</code>, µm)
                </label>
                <input
                  type="number"
                  step="1"
                  value={input.dSampleDepth}
                  onChange={(e) => setInput({ ...input, dSampleDepth: parseFloat(e.target.value) || 10 })}
                  className="w-full bg-slate-900 border border-slate-700 rounded-lg px-3 py-1.5 text-cyan-300 font-mono focus:outline-none focus:border-cyan-500"
                />
                <span className="text-[10px] text-slate-500 block mt-1">Standard: 10 µm (Leja / Makler)</span>
              </div>

              {/* Slide Chamber Type */}
              <div className="bg-slate-950 p-3 rounded-xl border border-slate-800">
                <label className="block text-slate-400 font-medium mb-1">
                  Slide Type (<code>dPlateType</code>)
                </label>
                <select
                  value={input.dPlateType}
                  onChange={(e) => setInput({ ...input, dPlateType: parseInt(e.target.value) || 1 })}
                  className="w-full bg-slate-900 border border-slate-700 rounded-lg px-3 py-1.5 text-cyan-300 font-mono focus:outline-none focus:border-cyan-500"
                >
                  <option value={1}>1: 6-Chamber Blue Slide (Standard)</option>
                  <option value={0}>0: 4-Chamber White Slide</option>
                </select>
              </div>

              {/* Density Correction Factor */}
              <div className="bg-slate-950 p-3 rounded-xl border border-slate-800">
                <label className="block text-slate-400 font-medium mb-1">
                  Density Factor (<code>dDSDensk</code>)
                </label>
                <input
                  type="number"
                  step="0.05"
                  value={input.dDSDensk}
                  onChange={(e) => setInput({ ...input, dDSDensk: parseFloat(e.target.value) || 1.0 })}
                  className="w-full bg-slate-900 border border-slate-700 rounded-lg px-3 py-1.5 text-cyan-300 font-mono focus:outline-none focus:border-cyan-500"
                />
                <span className="text-[10px] text-slate-500 block mt-1">Range: 0.1 &lt; k ≤ 10.0</span>
              </div>
            </div>
          </div>

          {/* nStatus-9 Relaxation Settings per docs */}
          <div className="space-y-3">
            <h3 className="font-semibold text-slate-200 text-sm border-b border-slate-800 pb-1 flex items-center gap-1.5">
              <span>2. Mode Switch & nStatus-9 Thresholds (<code>docs/nStatus-9-relax-howto.md</code>)</span>
            </h3>

            <div className="grid grid-cols-1 sm:grid-cols-2 gap-4">
              {/* dShapeRatio Mode Switch */}
              <div className="bg-slate-950 p-3 rounded-xl border border-slate-800 sm:col-span-2">
                <div className="flex justify-between items-center mb-1">
                  <label className="text-slate-300 font-medium">
                    Mode Switch (<code>dShapeRatio</code>): <span className="font-mono text-cyan-400 font-bold">{input.dShapeRatio}</span>
                  </label>
                  <span className={`text-[10px] px-2 py-0.5 rounded font-mono ${
                    input.dShapeRatio < 0.8 ? 'bg-amber-950 text-amber-400' : 'bg-emerald-950 text-emerald-400'
                  }`}>
                    {input.dShapeRatio < 0.8 ? '0.5 Standard QC Particle (nStatus=-9)' : '>0.9 Human / ≥1.0 Boar (nStatus=1)'}
                  </span>
                </div>
                <input
                  type="range"
                  min="0.4"
                  max="2.5"
                  step="0.05"
                  value={input.dShapeRatio}
                  onChange={(e) => setInput({ ...input, dShapeRatio: parseFloat(e.target.value) })}
                  className="w-full accent-cyan-500"
                />
                <div className="flex justify-between text-[10px] text-slate-500 mt-1">
                  <span>0.5 (QC Bead)</span>
                  <span>0.8 (Boundary)</span>
                  <span>1.15 (Human Standard)</span>
                  <span>≥1.5 (Boar Sperm)</span>
                </div>
              </div>

              {/* FRESH_SPERM_AREA_MIN / MAX */}
              <div className="bg-slate-950 p-3 rounded-xl border border-slate-800">
                <label className="block text-slate-400 font-medium mb-1">
                  Sperm Area Window (<code>FRESH_SPERM_AREA</code>, px²)
                </label>
                <div className="flex items-center gap-2">
                  <input
                    type="number"
                    value={input.freshSpermAreaMin}
                    onChange={(e) => setInput({ ...input, freshSpermAreaMin: parseFloat(e.target.value) || 8 })}
                    className="w-1/2 bg-slate-900 border border-slate-700 rounded px-2 py-1 text-cyan-300 font-mono text-xs"
                    placeholder="Min (8)"
                  />
                  <span className="text-slate-500">to</span>
                  <input
                    type="number"
                    value={input.freshSpermAreaMax}
                    onChange={(e) => setInput({ ...input, freshSpermAreaMax: parseFloat(e.target.value) || 50 })}
                    className="w-1/2 bg-slate-900 border border-slate-700 rounded px-2 py-1 text-cyan-300 font-mono text-xs"
                    placeholder="Max (50)"
                  />
                </div>
                <span className="text-[10px] text-slate-500 block mt-1">Lower min / raise max to relax particle acceptance</span>
              </div>

              {/* FRESH_SPERM_SHAPE_MIN / MAX */}
              <div className="bg-slate-950 p-3 rounded-xl border border-slate-800">
                <label className="block text-slate-400 font-medium mb-1">
                  Sperm Shape Window (<code>FRESH_SPERM_SHAPE</code>, L/W)
                </label>
                <div className="flex items-center gap-2">
                  <input
                    type="number"
                    step="0.05"
                    value={input.freshSpermShapeMin}
                    onChange={(e) => setInput({ ...input, freshSpermShapeMin: parseFloat(e.target.value) || 1.05 })}
                    className="w-1/2 bg-slate-900 border border-slate-700 rounded px-2 py-1 text-cyan-300 font-mono text-xs"
                    placeholder="Min (1.05)"
                  />
                  <span className="text-slate-500">to</span>
                  <input
                    type="number"
                    step="0.05"
                    value={input.freshSpermShapeMax}
                    onChange={(e) => setInput({ ...input, freshSpermShapeMax: parseFloat(e.target.value) || 2.10 })}
                    className="w-1/2 bg-slate-900 border border-slate-700 rounded px-2 py-1 text-cyan-300 font-mono text-xs"
                    placeholder="Max (2.10)"
                  />
                </div>
                <span className="text-[10px] text-slate-500 block mt-1">Expand max to 2.4+ for elongated boar spermatozoa</span>
              </div>
            </div>
          </div>
        </div>

        {/* Footer */}
        <div className="px-6 py-3 bg-slate-950 border-t border-slate-800 flex justify-end">
          <button
            onClick={onClose}
            className="bg-cyan-600 hover:bg-cyan-500 text-white font-semibold px-4 py-2 rounded-xl text-xs transition-colors shadow-lg shadow-cyan-600/30"
          >
            Apply & Close
          </button>
        </div>
      </div>
    </div>
  );
};
