import React, { useState } from 'react';
import { 
  Dna, 
  CheckCircle2, 
  AlertCircle, 
  HelpCircle, 
  Sliders, 
  Layers, 
  Eye, 
  Maximize2,
  Sparkles
} from 'lucide-react';
import { 
  BarChart, 
  Bar, 
  XAxis, 
  YAxis, 
  Tooltip, 
  ResponsiveContainer, 
  Cell 
} from 'recharts';
import { AlgSqaMedDataOut, AlgSqaMedDataIn, SpermCell } from '../types';

interface MorphologyLabProps {
  metrics: AlgSqaMedDataOut;
  input: AlgSqaMedDataIn;
  cells: SpermCell[];
}

export const MorphologyLab: React.FC<MorphologyLabProps> = ({ metrics, input, cells }) => {
  const [selectedCategory, setSelectedCategory] = useState<'all' | 'normal' | 'head' | 'midpiece' | 'tail' | 'droplet'>('all');

  const spermCells = cells.filter(c => c.morphology !== 'round_cell');

  const filteredCells = spermCells.filter(c => {
    if (selectedCategory === 'normal') return c.morphology === 'normal';
    if (selectedCategory === 'head') return c.morphology === 'head_defect';
    if (selectedCategory === 'midpiece') return c.morphology === 'midpiece_defect';
    if (selectedCategory === 'tail') return c.morphology === 'tail_defect';
    if (selectedCategory === 'droplet') return c.morphology === 'cytoplasmic_droplet';
    return true;
  });

  const defectDistribution = [
    { name: 'Normal Sperm', value: metrics.dNormal, color: '#10b981' },
    { name: 'Head Defects', value: metrics.dHdefects, color: '#f59e0b' },
    { name: 'Midpiece Defects', value: metrics.dMdefects, color: '#ec4899' },
    { name: 'Tail Defects', value: metrics.dTdefects, color: '#8b5cf6' },
    { name: 'CR Cytoplasmic Droplets', value: metrics.dCRdefects, color: '#06b6d4' },
  ];

  return (
    <div className="space-y-6">
      {/* Top Morphology Indices & WHO Standards */}
      <div className="grid grid-cols-1 md:grid-cols-4 gap-4">
        {/* Normal Morphology % */}
        <div className="bg-slate-900 border border-slate-800 rounded-2xl p-4 shadow-lg">
          <div className="text-xs text-slate-400 font-medium">Normal Morphology (WHO 5th/6th)</div>
          <div className="mt-1 flex items-baseline gap-2">
            <span className={`text-3xl font-bold font-mono ${metrics.dNormal >= 4.0 ? 'text-emerald-400' : 'text-amber-400'}`}>
              {metrics.dNormal}%
            </span>
            <span className="text-xs bg-slate-800 text-slate-300 px-2 py-0.5 rounded font-mono">
              WHO Ref: ≥ 4.0%
            </span>
          </div>
          <div className="mt-2 text-xs text-slate-400">
            {metrics.dNormal >= 4.0 ? 'Normozoospermic morphology profile' : 'Teratozoospermic morphology profile'}
          </div>
        </div>

        {/* TZI (Teratozoospermia Index) */}
        <div className="bg-slate-900 border border-slate-800 rounded-2xl p-4 shadow-lg">
          <div className="text-xs text-slate-400 font-medium">Teratozoospermia Index (TZI)</div>
          <div className="mt-1 flex items-baseline gap-2">
            <span className={`text-3xl font-bold font-mono ${metrics.dTZI <= 1.6 ? 'text-emerald-400' : 'text-amber-400'}`}>
              {metrics.dTZI}
            </span>
            <span className="text-xs bg-slate-800 text-slate-300 px-2 py-0.5 rounded font-mono">
              Normal &lt; 1.60
            </span>
          </div>
          <div className="mt-2 text-xs text-slate-400">
            Defects per abnormal spermatozoon
          </div>
        </div>

        {/* SDI (Sperm Deformity Index) */}
        <div className="bg-slate-900 border border-slate-800 rounded-2xl p-4 shadow-lg">
          <div className="text-xs text-slate-400 font-medium">Sperm Deformity Index (SDI)</div>
          <div className="mt-1 flex items-baseline gap-2">
            <span className={`text-3xl font-bold font-mono ${metrics.dSDI <= 1.6 ? 'text-emerald-400' : 'text-amber-400'}`}>
              {metrics.dSDI}
            </span>
            <span className="text-xs bg-slate-800 text-slate-300 px-2 py-0.5 rounded font-mono">
              Normal &lt; 1.60
            </span>
          </div>
          <div className="mt-2 text-xs text-slate-400">
            Total defects divided by total counted sperm
          </div>
        </div>

        {/* Round Cells / Leukocytes */}
        <div className="bg-slate-900 border border-slate-800 rounded-2xl p-4 shadow-lg">
          <div className="text-xs text-slate-400 font-medium">Round Cells & Leukocytes</div>
          <div className="mt-1 flex items-baseline gap-2">
            <span className="text-3xl font-bold font-mono text-cyan-400">
              {metrics.dDensityRoundcells}
            </span>
            <span className="text-xs text-slate-400">M/mL</span>
          </div>
          <div className="mt-2 text-xs text-slate-400">
            WHO threshold: &lt; 1.0 M/mL (Leukocytospermia)
          </div>
        </div>
      </div>

      {/* Main Breakdown Section */}
      <div className="grid grid-cols-1 lg:grid-cols-12 gap-6">
        {/* Defect Distribution Chart (6 cols) */}
        <div className="lg:col-span-6 bg-slate-900 border border-slate-800 rounded-2xl p-5 shadow-xl">
          <div className="flex items-center justify-between pb-3 border-b border-slate-800">
            <h3 className="font-semibold text-slate-200 flex items-center gap-2">
              <Dna className="w-4 h-4 text-cyan-400" />
              Morphology Defect Classification (%)
            </h3>
            <span className="text-xs text-slate-400">Tygerberg Strict Criteria</span>
          </div>

          <div className="h-64 my-3">
            <ResponsiveContainer width="100%" height="100%">
              <BarChart data={defectDistribution} layout="vertical" margin={{ top: 10, right: 30, left: 40, bottom: 5 }}>
                <XAxis type="number" stroke="#64748b" domain={[0, 100]} />
                <YAxis dataKey="name" type="category" stroke="#94a3b8" fontSize={11} width={130} />
                <Tooltip
                  contentStyle={{ backgroundColor: '#0f172a', borderColor: '#334155', borderRadius: '8px', color: '#f8fafc' }}
                  formatter={(val: any) => [`${val}%`, 'Frequency']}
                />
                <Bar dataKey="value" radius={[0, 4, 4, 0]}>
                  {defectDistribution.map((entry, index) => (
                    <Cell key={`cell-${index}`} fill={entry.color} />
                  ))}
                </Bar>
              </BarChart>
            </ResponsiveContainer>
          </div>

          <div className="grid grid-cols-2 gap-2 text-xs pt-3 border-t border-slate-800">
            <div className="bg-slate-950 p-2.5 rounded-lg border border-slate-800">
              <div className="text-slate-500">Head Defect Subtypes</div>
              <div className="text-slate-300 font-medium mt-1">Tapered, Micro, Macro, Amorphous, Pyriform</div>
            </div>
            <div className="bg-slate-950 p-2.5 rounded-lg border border-slate-800">
              <div className="text-slate-500">Midpiece & Flagellum Subtypes</div>
              <div className="text-slate-300 font-medium mt-1">Bent neck, Asymmetric, Coiled, Short, Hairpin</div>
            </div>
          </div>
        </div>

        {/* Morphometric Thresholds & Boundaries (6 cols) */}
        <div className="lg:col-span-6 bg-slate-900 border border-slate-800 rounded-2xl p-5 shadow-xl flex flex-col justify-between">
          <div>
            <div className="flex items-center justify-between pb-3 border-b border-slate-800">
              <h3 className="font-semibold text-slate-200 flex items-center gap-2">
                <Sliders className="w-4 h-4 text-cyan-400" />
                Sperm Morphometry Threshold Windows
              </h3>
              <span className="text-xs font-mono text-cyan-400">COUNTSPERMMED.H</span>
            </div>

            <div className="space-y-3 mt-4 text-xs">
              <div className="bg-slate-950 p-3 rounded-xl border border-slate-800 flex items-center justify-between">
                <div>
                  <div className="font-semibold text-slate-200">Head Area Window (dArea)</div>
                  <div className="text-slate-500 text-[11px]">Pixel area bounds for normal spermatozoon</div>
                </div>
                <div className="font-mono text-cyan-400 bg-slate-900 px-3 py-1 rounded border border-slate-800">
                  {input.morpPara.dArea.min} - {input.morpPara.dArea.max} px²
                </div>
              </div>

              <div className="bg-slate-950 p-3 rounded-xl border border-slate-800 flex items-center justify-between">
                <div>
                  <div className="font-semibold text-slate-200">Head Length / Width Ratio (dShape)</div>
                  <div className="text-slate-500 text-[11px]">Elliptical length-to-width ratio</div>
                </div>
                <div className="font-mono text-cyan-400 bg-slate-900 px-3 py-1 rounded border border-slate-800">
                  {input.morpPara.dShape.min} - {input.morpPara.dShape.max}
                </div>
              </div>

              <div className="bg-slate-950 p-3 rounded-xl border border-slate-800 flex items-center justify-between">
                <div>
                  <div className="font-semibold text-slate-200">Major Axis Length (dLength)</div>
                  <div className="text-slate-500 text-[11px]">Normal head length dimension</div>
                </div>
                <div className="font-mono text-cyan-400 bg-slate-900 px-3 py-1 rounded border border-slate-800">
                  {input.morpPara.dLength.min} - {input.morpPara.dLength.max} µm
                </div>
              </div>

              <div className="bg-slate-950 p-3 rounded-xl border border-slate-800 flex items-center justify-between">
                <div>
                  <div className="font-semibold text-slate-200">Minor Axis Width (dWidth)</div>
                  <div className="text-slate-500 text-[11px]">Normal head width dimension</div>
                </div>
                <div className="font-mono text-cyan-400 bg-slate-900 px-3 py-1 rounded border border-slate-800">
                  {input.morpPara.dWidth.min} - {input.morpPara.dWidth.max} µm
                </div>
              </div>
            </div>
          </div>

          <div className="mt-4 p-3 bg-cyan-950/30 border border-cyan-900/40 rounded-xl text-xs text-cyan-300 flex items-center gap-2">
            <Sparkles className="w-4 h-4 text-cyan-400 shrink-0" />
            <span>
              Morphological parameters strictly calibrate the calculation of <code>dMorp</code> (normal %), <code>dTZI</code>, and <code>dSDI</code> in the C++ algorithm engine.
            </span>
          </div>
        </div>
      </div>

      {/* Interactive Microscopic Gallery Filter */}
      <div className="bg-slate-900 border border-slate-800 rounded-2xl p-5 shadow-xl">
        <div className="flex flex-wrap items-center justify-between gap-3 pb-3 border-b border-slate-800">
          <h3 className="font-semibold text-slate-200 flex items-center gap-2">
            <Layers className="w-4 h-4 text-cyan-400" />
            Sperm Morphological Cell Gallery ({filteredCells.length} cells in field)
          </h3>

          {/* Filter Pills */}
          <div className="flex items-center gap-1.5 bg-slate-950 p-1 rounded-lg border border-slate-800 text-xs">
            <button
              onClick={() => setSelectedCategory('all')}
              className={`px-3 py-1 rounded font-medium ${selectedCategory === 'all' ? 'bg-cyan-600 text-white' : 'text-slate-400 hover:text-slate-200'}`}
            >
              All ({spermCells.length})
            </button>
            <button
              onClick={() => setSelectedCategory('normal')}
              className={`px-3 py-1 rounded font-medium ${selectedCategory === 'normal' ? 'bg-emerald-600 text-white' : 'text-slate-400 hover:text-slate-200'}`}
            >
              Normal
            </button>
            <button
              onClick={() => setSelectedCategory('head')}
              className={`px-3 py-1 rounded font-medium ${selectedCategory === 'head' ? 'bg-amber-600 text-white' : 'text-slate-400 hover:text-slate-200'}`}
            >
              Head Defects
            </button>
            <button
              onClick={() => setSelectedCategory('midpiece')}
              className={`px-3 py-1 rounded font-medium ${selectedCategory === 'midpiece' ? 'bg-pink-600 text-white' : 'text-slate-400 hover:text-slate-200'}`}
            >
              Midpiece
            </button>
            <button
              onClick={() => setSelectedCategory('tail')}
              className={`px-3 py-1 rounded font-medium ${selectedCategory === 'tail' ? 'bg-purple-600 text-white' : 'text-slate-400 hover:text-slate-200'}`}
            >
              Tail Defects
            </button>
            <button
              onClick={() => setSelectedCategory('droplet')}
              className={`px-3 py-1 rounded font-medium ${selectedCategory === 'droplet' ? 'bg-cyan-700 text-white' : 'text-slate-400 hover:text-slate-200'}`}
            >
              CR Droplet
            </button>
          </div>
        </div>

        {/* Cells Grid */}
        <div className="grid grid-cols-2 sm:grid-cols-3 md:grid-cols-4 lg:grid-cols-6 gap-3 mt-4 max-h-96 overflow-y-auto pr-1">
          {filteredCells.map(cell => (
            <div key={cell.id} className="bg-slate-950 border border-slate-800/80 rounded-xl p-3 text-xs flex flex-col justify-between hover:border-slate-700 transition-colors">
              <div className="flex items-center justify-between mb-2">
                <span className="font-mono text-cyan-400 font-bold">#{cell.id}</span>
                <span className={`text-[10px] px-1.5 py-0.5 rounded font-semibold ${
                  cell.morphology === 'normal' ? 'bg-emerald-950 text-emerald-400 border border-emerald-800/60' :
                  'bg-rose-950 text-rose-400 border border-rose-800/60'
                }`}>
                  {cell.morphology === 'normal' ? 'Normal' : 'Abnormal'}
                </span>
              </div>
              <div className="space-y-1 text-[11px] text-slate-400">
                <div>Type: <span className="text-slate-200 capitalize">{cell.morphology.replace('_', ' ')}</span></div>
                {cell.headDefectType && <div className="text-amber-300 text-[10px]">{cell.headDefectType}</div>}
                {cell.midDefectType && <div className="text-pink-300 text-[10px]">{cell.midDefectType}</div>}
                {cell.tailDefectType && <div className="text-purple-300 text-[10px]">{cell.tailDefectType}</div>}
                <div className="pt-1 text-[10px] text-slate-500 font-mono">
                  {cell.length.toFixed(1)}x{cell.width.toFixed(1)}µm | {cell.vcl.toFixed(1)}µm/s
                </div>
              </div>
            </div>
          ))}
        </div>
      </div>
    </div>
  );
};
