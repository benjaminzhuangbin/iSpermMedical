import React from 'react';
import { 
  Activity, 
  Microscope, 
  Dna, 
  ShieldCheck, 
  FileSpreadsheet, 
  Sliders, 
  BookOpen, 
  CheckCircle2, 
  AlertTriangle 
} from 'lucide-react';
import { AlgSqaMedDataOut } from '../types';

interface NavbarProps {
  activeTab: 'microscope' | 'dashboard' | 'morphology' | 'watchdog' | 'docs';
  setActiveTab: (tab: 'microscope' | 'dashboard' | 'morphology' | 'watchdog' | 'docs') => void;
  metrics: AlgSqaMedDataOut;
  onOpenReport: () => void;
  onOpenSettings: () => void;
}

export const Navbar: React.FC<NavbarProps> = ({
  activeTab,
  setActiveTab,
  metrics,
  onOpenReport,
  onOpenSettings
}) => {
  return (
    <header className="bg-slate-900/90 backdrop-blur border-b border-slate-800 sticky top-0 z-40">
      <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
        <div className="flex items-center justify-between h-16">
          {/* Brand Logo & Name */}
          <div className="flex items-center gap-3">
            <div className="w-10 h-10 rounded-lg bg-gradient-to-tr from-cyan-600 to-blue-500 flex items-center justify-center shadow-lg shadow-cyan-500/20">
              <Microscope className="w-6 h-6 text-white" />
            </div>
            <div>
              <div className="flex items-center gap-2">
                <span className="font-bold text-lg text-white tracking-tight">iSperm Medical</span>
                <span className="text-xs bg-cyan-950 text-cyan-400 border border-cyan-800/60 font-mono px-2 py-0.5 rounded-full font-medium">
                  CASA v3.1 + Watchdog 2.4
                </span>
              </div>
              <p className="text-xs text-slate-400">Clinical CASA Semen Analyzer & Embedded Instrument Daemon</p>
            </div>
          </div>

          {/* Navigation Tabs */}
          <nav className="hidden md:flex items-center gap-1 bg-slate-950/60 p-1 rounded-xl border border-slate-800/80">
            <button
              id="nav-tab-microscope"
              onClick={() => setActiveTab('microscope')}
              className={`flex items-center gap-2 px-3.5 py-1.5 rounded-lg text-sm font-medium transition-all ${
                activeTab === 'microscope'
                  ? 'bg-cyan-600 text-white shadow-md shadow-cyan-600/30'
                  : 'text-slate-400 hover:text-slate-200 hover:bg-slate-800/50'
              }`}
            >
              <Microscope className="w-4 h-4" />
              Live Stage
            </button>
            <button
              id="nav-tab-dashboard"
              onClick={() => setActiveTab('dashboard')}
              className={`flex items-center gap-2 px-3.5 py-1.5 rounded-lg text-sm font-medium transition-all ${
                activeTab === 'dashboard'
                  ? 'bg-cyan-600 text-white shadow-md shadow-cyan-600/30'
                  : 'text-slate-400 hover:text-slate-200 hover:bg-slate-800/50'
              }`}
            >
              <Activity className="w-4 h-4" />
              CASA Kinematics
            </button>
            <button
              id="nav-tab-morphology"
              onClick={() => setActiveTab('morphology')}
              className={`flex items-center gap-2 px-3.5 py-1.5 rounded-lg text-sm font-medium transition-all ${
                activeTab === 'morphology'
                  ? 'bg-cyan-600 text-white shadow-md shadow-cyan-600/30'
                  : 'text-slate-400 hover:text-slate-200 hover:bg-slate-800/50'
              }`}
            >
              <Dna className="w-4 h-4" />
              Morphology Lab
            </button>
            <button
              id="nav-tab-watchdog"
              onClick={() => setActiveTab('watchdog')}
              className={`flex items-center gap-2 px-3.5 py-1.5 rounded-lg text-sm font-medium transition-all ${
                activeTab === 'watchdog'
                  ? 'bg-cyan-600 text-white shadow-md shadow-cyan-600/30'
                  : 'text-slate-400 hover:text-slate-200 hover:bg-slate-800/50'
              }`}
            >
              <ShieldCheck className="w-4 h-4" />
              Nexus Watchdog
            </button>
            <button
              id="nav-tab-docs"
              onClick={() => setActiveTab('docs')}
              className={`flex items-center gap-2 px-3.5 py-1.5 rounded-lg text-sm font-medium transition-all ${
                activeTab === 'docs'
                  ? 'bg-cyan-600 text-white shadow-md shadow-cyan-600/30'
                  : 'text-slate-400 hover:text-slate-200 hover:bg-slate-800/50'
              }`}
            >
              <BookOpen className="w-4 h-4" />
              Algorithm Specs
            </button>
          </nav>

          {/* Action Buttons & Status Badge */}
          <div className="flex items-center gap-3">
            {/* nStatus indicator */}
            <div className={`hidden lg:flex items-center gap-1.5 text-xs px-2.5 py-1 rounded-full border ${
              metrics.nStatus === 1 
                ? 'bg-emerald-950/60 text-emerald-400 border-emerald-800/60'
                : 'bg-amber-950/60 text-amber-400 border-amber-800/60'
            }`}>
              {metrics.nStatus === 1 ? (
                <CheckCircle2 className="w-3.5 h-3.5 text-emerald-400" />
              ) : (
                <AlertTriangle className="w-3.5 h-3.5 text-amber-400" />
              )}
              <span className="font-mono font-medium">nStatus={metrics.nStatus}</span>
            </div>

            <button
              id="btn-settings"
              onClick={onOpenSettings}
              className="p-2 rounded-lg text-slate-400 hover:text-slate-200 hover:bg-slate-800 transition-colors"
              title="Calibration & Settings"
            >
              <Sliders className="w-5 h-5" />
            </button>

            <button
              id="btn-clinical-report"
              onClick={onOpenReport}
              className="flex items-center gap-2 bg-gradient-to-r from-cyan-500 to-blue-600 hover:from-cyan-400 hover:to-blue-500 text-white px-3.5 py-1.5 rounded-lg text-sm font-semibold shadow-md shadow-cyan-500/20 transition-all active:scale-95"
            >
              <FileSpreadsheet className="w-4 h-4" />
              <span className="hidden sm:inline">WHO Report</span>
            </button>
          </div>
        </div>

        {/* Mobile Navigation Tabs */}
        <div className="flex md:hidden overflow-x-auto pb-2 pt-1 gap-1 text-xs no-scrollbar">
          <button
            onClick={() => setActiveTab('microscope')}
            className={`px-3 py-1.5 rounded-lg whitespace-nowrap font-medium ${
              activeTab === 'microscope' ? 'bg-cyan-600 text-white' : 'text-slate-400 bg-slate-800/50'
            }`}
          >
            Live Stage
          </button>
          <button
            onClick={() => setActiveTab('dashboard')}
            className={`px-3 py-1.5 rounded-lg whitespace-nowrap font-medium ${
              activeTab === 'dashboard' ? 'bg-cyan-600 text-white' : 'text-slate-400 bg-slate-800/50'
            }`}
          >
            CASA Kinematics
          </button>
          <button
            onClick={() => setActiveTab('morphology')}
            className={`px-3 py-1.5 rounded-lg whitespace-nowrap font-medium ${
              activeTab === 'morphology' ? 'bg-cyan-600 text-white' : 'text-slate-400 bg-slate-800/50'
            }`}
          >
            Morphology
          </button>
          <button
            onClick={() => setActiveTab('watchdog')}
            className={`px-3 py-1.5 rounded-lg whitespace-nowrap font-medium ${
              activeTab === 'watchdog' ? 'bg-cyan-600 text-white' : 'text-slate-400 bg-slate-800/50'
            }`}
          >
            Watchdog
          </button>
          <button
            onClick={() => setActiveTab('docs')}
            className={`px-3 py-1.5 rounded-lg whitespace-nowrap font-medium ${
              activeTab === 'docs' ? 'bg-cyan-600 text-white' : 'text-slate-400 bg-slate-800/50'
            }`}
          >
            Specs
          </button>
        </div>
      </div>
    </header>
  );
};
