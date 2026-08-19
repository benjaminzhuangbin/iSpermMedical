import React from 'react';
import { BookOpen, FileCode, CheckCircle2, Cpu, ShieldCheck, Activity, Dna, Info } from 'lucide-react';

export const AlgorithmSpecsView: React.FC = () => {
  return (
    <div className="space-y-6 max-w-5xl mx-auto">
      {/* Top Banner */}
      <div className="bg-slate-900 border border-slate-800 rounded-2xl p-6 shadow-xl">
        <div className="flex items-center gap-3">
          <div className="w-10 h-10 rounded-xl bg-cyan-600/20 text-cyan-400 border border-cyan-500/30 flex items-center justify-center">
            <BookOpen className="w-5 h-5" />
          </div>
          <div>
            <h2 className="text-lg font-bold text-white">iSperm Medical System Specifications & Mathematical Reference</h2>
            <p className="text-xs text-slate-400">
              Technical documentation for Computer-Assisted Sperm Analysis (CASA v3.1) and Nexus ADB Watchdog v2.4 Architecture
            </p>
          </div>
        </div>
      </div>

      {/* Grid of Docs */}
      <div className="grid grid-cols-1 md:grid-cols-2 gap-6">
        {/* CASA Kinematics Math */}
        <div className="bg-slate-900 border border-slate-800 rounded-2xl p-5 shadow-xl space-y-4">
          <div className="flex items-center gap-2 pb-3 border-b border-slate-800">
            <Activity className="w-4 h-4 text-cyan-400" />
            <h3 className="font-semibold text-sm text-slate-100">CASA Kinematics Formulas (WHO 6th Standard)</h3>
          </div>

          <div className="space-y-3 text-xs text-slate-300">
            <div className="bg-slate-950 p-3 rounded-xl border border-slate-800/80">
              <div className="font-mono text-cyan-300 font-semibold mb-1">VCL (Curvilinear Velocity, µm/s)</div>
              <p className="text-slate-400 text-[11px]">
                Total distance traveled along the actual point-to-point path divided by elapsed time:
              </p>
              <div className="mt-1 font-mono text-[11px] bg-slate-900 p-1.5 rounded text-slate-200">
                VCL = Σ √[(x_{'{i+1}'} - x_i)² + (y_{'{i+1}'} - y_i)²] / Δt
              </div>
            </div>

            <div className="bg-slate-950 p-3 rounded-xl border border-slate-800/80">
              <div className="font-mono text-emerald-300 font-semibold mb-1">VSL (Straight-Line Velocity, µm/s)</div>
              <p className="text-slate-400 text-[11px]">
                Straight-line net displacement from the first detected point to the last detected point:
              </p>
              <div className="mt-1 font-mono text-[11px] bg-slate-900 p-1.5 rounded text-slate-200">
                VSL = √[(x_N - x_0)² + (y_N - y_0)²] / Δt_total
              </div>
            </div>

            <div className="bg-slate-950 p-3 rounded-xl border border-slate-800/80">
              <div className="font-mono text-purple-300 font-semibold mb-1">LIN, STR, WOB (Path Ratios)</div>
              <ul className="list-disc pl-4 space-y-1 text-slate-400 text-[11px]">
                <li><strong>LIN (Linearity)</strong>: VSL / VCL (Net progression ratio)</li>
                <li><strong>STR (Straightness)</strong>: VSL / VAP (Linearity of smoothed average path)</li>
                <li><strong>WOB (Wobble)</strong>: VAP / VCL (Oscillation amplitude ratio)</li>
              </ul>
            </div>
          </div>
        </div>

        {/* Morphology & Teratozoospermia */}
        <div className="bg-slate-900 border border-slate-800 rounded-2xl p-5 shadow-xl space-y-4">
          <div className="flex items-center gap-2 pb-3 border-b border-slate-800">
            <Dna className="w-4 h-4 text-pink-400" />
            <h3 className="font-semibold text-sm text-slate-100">Morphometry & Deformity Indices</h3>
          </div>

          <div className="space-y-3 text-xs text-slate-300">
            <div className="bg-slate-950 p-3 rounded-xl border border-slate-800/80">
              <div className="font-mono text-pink-300 font-semibold mb-1">TZI (Teratozoospermia Index)</div>
              <p className="text-slate-400 text-[11px]">
                Calculates the average number of morphological defects per abnormal spermatozoon (normal value &lt; 1.60):
              </p>
              <div className="mt-1 font-mono text-[11px] bg-slate-900 p-1.5 rounded text-slate-200">
                TZI = (Head Def + Mid Def + Tail Def + CR Def) / Abnormal Count
              </div>
            </div>

            <div className="bg-slate-950 p-3 rounded-xl border border-slate-800/80">
              <div className="font-mono text-amber-300 font-semibold mb-1">SDI (Sperm Deformity Index)</div>
              <p className="text-slate-400 text-[11px]">
                Total defects across all evaluated sperm cells divided by total sperm count:
              </p>
              <div className="mt-1 font-mono text-[11px] bg-slate-900 p-1.5 rounded text-slate-200">
                SDI = Total Defects / Total Count
              </div>
            </div>

            <div className="bg-slate-950 p-3 rounded-xl border border-slate-800/80">
              <div className="font-mono text-cyan-300 font-semibold mb-1">nStatus Decision Rules (docs/nStatus-9)</div>
              <ul className="list-disc pl-4 space-y-1 text-slate-400 text-[11px]">
                <li><code>dShapeRatio &lt; 0.8</code>: QC standard particle mode → nStatus=-9</li>
                <li><code>dShapeRatio &gt; 0.9</code>: Human sperm mode → nStatus=1</li>
                <li><code>dShapeRatio ≥ 1.0</code>: Porcine/boar CASA mode → nStatus=1</li>
              </ul>
            </div>
          </div>
        </div>

        {/* Nexus ADB Watchdog Architecture */}
        <div className="bg-slate-900 border border-slate-800 rounded-2xl p-5 shadow-xl space-y-4 md:col-span-2">
          <div className="flex items-center gap-2 pb-3 border-b border-slate-800">
            <ShieldCheck className="w-4 h-4 text-emerald-400" />
            <h3 className="font-semibold text-sm text-slate-100">Nexus ADB Watchdog 2.4 Product Architecture (RK3288 Android 5.1.1)</h3>
          </div>

          <div className="grid grid-cols-1 md:grid-cols-3 gap-4 text-xs">
            <div className="bg-slate-950 p-3.5 rounded-xl border border-slate-800">
              <div className="font-bold text-slate-200 mb-1">Root Elevation via nexus_su</div>
              <p className="text-slate-400 text-[11px]">
                Stock Android <code>/system/xbin/su</code> rejects app UID 10053. The factory setuid binary <code>/system/xbin/nexus_su</code> (mode 06755 root:root) elevates the APK service to UID 0 without rooting frameworks like Magisk.
              </p>
            </div>

            <div className="bg-slate-950 p-3.5 rounded-xl border border-slate-800">
              <div className="font-bold text-slate-200 mb-1">Safe Active Session Guard</div>
              <p className="text-slate-400 text-[11px]">
                If <code>ESTABLISHED &gt; 0</code>, ADBD is NEVER restarted, even if CLOSE_WAIT socket leaks appear. This protects active physician screen-mirroring (QtScrcpy) and clinical telemetry.
              </p>
            </div>

            <div className="bg-slate-950 p-3.5 rounded-xl border border-slate-800">
              <div className="font-bold text-slate-200 mb-1">Rate-Limited Healing Window</div>
              <p className="text-slate-400 text-[11px]">
                Auto-recovery executes only on confirmed hard faults (ADBD dead or port 5555 closed). A 15-second cooldown window prevents restart storms or latch loops.
              </p>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
};
