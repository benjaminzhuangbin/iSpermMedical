import React from 'react';
import { 
  Activity, 
  TrendingUp, 
  PieChart as PieIcon, 
  BarChart3, 
  Zap, 
  Compass, 
  Layers, 
  AlertCircle,
  FileDown,
  Gauge
} from 'lucide-react';
import { 
  BarChart, 
  Bar, 
  XAxis, 
  YAxis, 
  Tooltip, 
  ResponsiveContainer, 
  PieChart, 
  Pie, 
  Cell, 
  Legend 
} from 'recharts';
import { AlgSqaMedDataOut, AlgSqaMedDataIn } from '../types';

interface CASAAnalysisDashboardProps {
  metrics: AlgSqaMedDataOut;
  input: AlgSqaMedDataIn;
}

export const CASAAnalysisDashboard: React.FC<CASAAnalysisDashboardProps> = ({ metrics, input }) => {
  // Motility Pie Chart Data
  const motilityData = [
    { name: 'Class A (Rapid-PR)', value: metrics.dRatioClassA, count: metrics.dDensityClassA, color: '#10b981' },
    { name: 'Class B (Slow-PR)', value: metrics.dRatioClassB, count: metrics.dDensityClassB, color: '#38bdf8' },
    { name: 'Class C (Non-Prog NP)', value: metrics.dRatioClassC, count: metrics.dDensityClassC, color: '#f59e0b' },
    { name: 'Class D (Immotile IM)', value: metrics.dRatioClassD, count: metrics.dDensityClassD, color: '#64748b' }
  ];

  // Velocity Histograms Data
  const histLabels = ['0-10', '10-20', '20-30', '30-40', '40-50', '50-60', '60-70', '70-80', '80-90', '90-100+'];
  const histogramData = histLabels.map((label, idx) => ({
    bin: label,
    VCL: metrics.dHistVCL[idx] || 0,
    VSL: metrics.dHistVSL[idx] || 0,
    VAP: metrics.dHistVAP[idx] || 0
  }));

  return (
    <div className="space-y-6">
      {/* Top Clinical Summary Cards */}
      <div className="grid grid-cols-2 sm:grid-cols-3 lg:grid-cols-6 gap-4">
        {/* Total Concentration */}
        <div className="bg-slate-900 border border-slate-800 rounded-2xl p-4 shadow-lg relative overflow-hidden">
          <div className="text-xs text-slate-400 font-medium">Sperm Concentration</div>
          <div className="mt-1 flex items-baseline gap-1">
            <span className="text-2xl font-bold font-mono text-cyan-400">{metrics.dTotaSpermDensity}</span>
            <span className="text-xs text-slate-400">M/mL</span>
          </div>
          <div className="mt-2 text-[11px] text-slate-500">
            Total count: {(metrics.dTotaSpermDensity * input.dVolume).toFixed(1)} M
          </div>
          <div className="absolute top-3 right-3 text-cyan-500/20">
            <Activity className="w-8 h-8" />
          </div>
        </div>

        {/* Total Motility (PR + NP) */}
        <div className="bg-slate-900 border border-slate-800 rounded-2xl p-4 shadow-lg relative overflow-hidden">
          <div className="text-xs text-slate-400 font-medium">Total Motility (PR+NP)</div>
          <div className="mt-1 flex items-baseline gap-1">
            <span className={`text-2xl font-bold font-mono ${metrics.dActiveSpermRatio >= 40 ? 'text-emerald-400' : 'text-amber-400'}`}>
              {metrics.dActiveSpermRatio}%
            </span>
            <span className="text-xs text-slate-400">Active</span>
          </div>
          <div className="mt-2 text-[11px] text-slate-500">
            WHO ref: ≥ 42% (Normal)
          </div>
          <div className="absolute top-3 right-3 text-emerald-500/20">
            <Zap className="w-8 h-8" />
          </div>
        </div>

        {/* Progressive Motility (PR) */}
        <div className="bg-slate-900 border border-slate-800 rounded-2xl p-4 shadow-lg relative overflow-hidden">
          <div className="text-xs text-slate-400 font-medium">Progressive (PR = A+B)</div>
          <div className="mt-1 flex items-baseline gap-1">
            <span className={`text-2xl font-bold font-mono ${metrics.dRatioClassPR >= 32 ? 'text-emerald-400' : 'text-amber-400'}`}>
              {metrics.dRatioClassPR}%
            </span>
            <span className="text-xs text-slate-400">Prog</span>
          </div>
          <div className="mt-2 text-[11px] text-slate-500">
            WHO ref: ≥ 30% (5th/6th)
          </div>
          <div className="absolute top-3 right-3 text-sky-500/20">
            <TrendingUp className="w-8 h-8" />
          </div>
        </div>

        {/* Average VCL */}
        <div className="bg-slate-900 border border-slate-800 rounded-2xl p-4 shadow-lg relative overflow-hidden">
          <div className="text-xs text-slate-400 font-medium">Avg Curvilinear (VCL)</div>
          <div className="mt-1 flex items-baseline gap-1">
            <span className="text-2xl font-bold font-mono text-cyan-300">{metrics.dAveVCL}</span>
            <span className="text-xs text-slate-400">µm/s</span>
          </div>
          <div className="mt-2 text-[11px] text-slate-500">
            Straight-line VSL: {metrics.dAveVSL} µm/s
          </div>
          <div className="absolute top-3 right-3 text-cyan-500/20">
            <Gauge className="w-8 h-8" />
          </div>
        </div>

        {/* Linearity & Straightness */}
        <div className="bg-slate-900 border border-slate-800 rounded-2xl p-4 shadow-lg relative overflow-hidden">
          <div className="text-xs text-slate-400 font-medium">Linearity / STR</div>
          <div className="mt-1 flex items-baseline gap-1">
            <span className="text-2xl font-bold font-mono text-slate-100">{metrics.dLIN}</span>
            <span className="text-xs text-slate-400">/ {metrics.dSTR}</span>
          </div>
          <div className="mt-2 text-[11px] text-slate-500">
            Wobble (WOB): {metrics.dWOB}
          </div>
          <div className="absolute top-3 right-3 text-purple-500/20">
            <Compass className="w-8 h-8" />
          </div>
        </div>

        {/* Normal Morphology */}
        <div className="bg-slate-900 border border-slate-800 rounded-2xl p-4 shadow-lg relative overflow-hidden">
          <div className="text-xs text-slate-400 font-medium">Normal Morphology</div>
          <div className="mt-1 flex items-baseline gap-1">
            <span className={`text-2xl font-bold font-mono ${metrics.dNormal >= 4.0 ? 'text-emerald-400' : 'text-amber-400'}`}>
              {metrics.dNormal}%
            </span>
            <span className="text-xs text-slate-400">Normal</span>
          </div>
          <div className="mt-2 text-[11px] text-slate-500">
            TZI: {metrics.dTZI} | SDI: {metrics.dSDI}
          </div>
          <div className="absolute top-3 right-3 text-emerald-500/20">
            <Layers className="w-8 h-8" />
          </div>
        </div>
      </div>

      {/* Main Charts & Detailed Tables */}
      <div className="grid grid-cols-1 lg:grid-cols-12 gap-6">
        {/* Motility Grade Donut & Densities (5 cols) */}
        <div className="lg:col-span-5 bg-slate-900 border border-slate-800 rounded-2xl p-5 shadow-xl flex flex-col justify-between">
          <div>
            <div className="flex items-center justify-between pb-3 border-b border-slate-800">
              <h3 className="font-semibold text-slate-200 flex items-center gap-2">
                <PieIcon className="w-4 h-4 text-cyan-400" />
                WHO Motility Classification
              </h3>
              <span className="text-xs font-mono text-slate-400">Sample Depth: {input.dSampleDepth} µm</span>
            </div>

            <div className="h-64 my-2">
              <ResponsiveContainer width="100%" height="100%">
                <PieChart>
                  <Pie
                    data={motilityData}
                    cx="50%"
                    cy="50%"
                    innerRadius={60}
                    outerRadius={90}
                    paddingAngle={4}
                    dataKey="value"
                  >
                    {motilityData.map((entry, index) => (
                      <Cell key={`cell-${index}`} fill={entry.color} />
                    ))}
                  </Pie>
                  <Tooltip
                    contentStyle={{ backgroundColor: '#0f172a', borderColor: '#334155', borderRadius: '8px', color: '#f8fafc' }}
                    formatter={(value: any, name: any) => [`${value}%`, name]}
                  />
                </PieChart>
              </ResponsiveContainer>
            </div>
          </div>

          {/* Breakdown List */}
          <div className="space-y-2 text-xs border-t border-slate-800 pt-3">
            {motilityData.map((item, i) => (
              <div key={i} className="flex items-center justify-between bg-slate-950 px-3 py-2 rounded-lg border border-slate-800/80">
                <div className="flex items-center gap-2">
                  <span className="w-2.5 h-2.5 rounded-full" style={{ backgroundColor: item.color }}></span>
                  <span className="text-slate-300 font-medium">{item.name}</span>
                </div>
                <div className="flex items-center gap-4 font-mono">
                  <span className="text-slate-100 font-bold">{item.value}%</span>
                  <span className="text-slate-400 text-[11px]">{item.count} M/mL</span>
                </div>
              </div>
            ))}
          </div>
        </div>

        {/* Velocity Distribution Histograms (7 cols) */}
        <div className="lg:col-span-7 bg-slate-900 border border-slate-800 rounded-2xl p-5 shadow-xl flex flex-col justify-between">
          <div>
            <div className="flex items-center justify-between pb-3 border-b border-slate-800">
              <h3 className="font-semibold text-slate-200 flex items-center gap-2">
                <BarChart3 className="w-4 h-4 text-cyan-400" />
                Kinematic Velocity Histograms (0 - 100+ µm/s)
              </h3>
              <div className="flex items-center gap-3 text-xs">
                <span className="flex items-center gap-1.5 text-cyan-400"><span className="w-2 h-2 rounded bg-cyan-400"></span>VCL</span>
                <span className="flex items-center gap-1.5 text-emerald-400"><span className="w-2 h-2 rounded bg-emerald-400"></span>VSL</span>
                <span className="flex items-center gap-1.5 text-purple-400"><span className="w-2 h-2 rounded bg-purple-400"></span>VAP</span>
              </div>
            </div>

            <div className="h-72 my-2">
              <ResponsiveContainer width="100%" height="100%">
                <BarChart data={histogramData} margin={{ top: 20, right: 20, left: -10, bottom: 5 }}>
                  <XAxis dataKey="bin" stroke="#64748b" fontSize={11} />
                  <YAxis stroke="#64748b" fontSize={11} />
                  <Tooltip
                    contentStyle={{ backgroundColor: '#0f172a', borderColor: '#334155', borderRadius: '8px', color: '#f8fafc' }}
                  />
                  <Bar dataKey="VCL" fill="#38bdf8" radius={[4, 4, 0, 0]} />
                  <Bar dataKey="VSL" fill="#10b981" radius={[4, 4, 0, 0]} />
                  <Bar dataKey="VAP" fill="#a855f7" radius={[4, 4, 0, 0]} />
                </BarChart>
              </ResponsiveContainer>
            </div>
          </div>

          {/* Kinematics Reference Table */}
          <div className="grid grid-cols-2 sm:grid-cols-4 gap-2 text-xs border-t border-slate-800 pt-3">
            <div className="bg-slate-950 p-2.5 rounded-lg border border-slate-800">
              <div className="text-slate-500 text-[10px]">Head Amplitude (ALH)</div>
              <div className="text-base font-mono font-bold text-slate-200">{metrics.dALH} <span className="text-xs text-slate-500">µm</span></div>
            </div>
            <div className="bg-slate-950 p-2.5 rounded-lg border border-slate-800">
              <div className="text-slate-500 text-[10px]">Beat Frequency (BCF)</div>
              <div className="text-base font-mono font-bold text-slate-200">{metrics.dBCF} <span className="text-xs text-slate-500">Hz</span></div>
            </div>
            <div className="bg-slate-950 p-2.5 rounded-lg border border-slate-800">
              <div className="text-slate-500 text-[10px]">Mean Angular (MAD)</div>
              <div className="text-base font-mono font-bold text-slate-200">{metrics.dMAD} <span className="text-xs text-slate-500">deg</span></div>
            </div>
            <div className="bg-slate-950 p-2.5 rounded-lg border border-slate-800">
              <div className="text-slate-500 text-[10px]">Round Cells Count</div>
              <div className="text-base font-mono font-bold text-slate-200">{metrics.nRoundcellsCount} <span className="text-xs text-slate-500">({metrics.dDensityRoundcells} M/mL)</span></div>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
};
