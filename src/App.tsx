import React, { useState, useEffect } from 'react';
import { Navbar } from './components/Navbar';
import { MicroscopeStage } from './components/MicroscopeStage';
import { CASAAnalysisDashboard } from './components/CASAAnalysisDashboard';
import { MorphologyLab } from './components/MorphologyLab';
import { NexusWatchdogConsole } from './components/NexusWatchdogConsole';
import { AlgorithmSpecsView } from './components/AlgorithmSpecsView';
import { ClinicalReportModal } from './components/ClinicalReportModal';
import { SettingsModal } from './components/SettingsModal';
import { DEFAULT_CASA_INPUT, generateInitialSpermCells, computeCASAMetrics } from './lib/casaEngine';
import { SpermCell, AlgSqaMedDataIn, AlgSqaMedDataOut } from './types';

export function App() {
  const [activeTab, setActiveTab] = useState<'microscope' | 'dashboard' | 'morphology' | 'watchdog' | 'docs'>('microscope');
  const [input, setInput] = useState<AlgSqaMedDataIn>(DEFAULT_CASA_INPUT);
  const [cells, setCells] = useState<SpermCell[]>(() => generateInitialSpermCells(60, 800, 520, input.dRatioImg, 'Human'));
  const [selectedCellId, setSelectedCellId] = useState<number | null>(null);
  const [isReportOpen, setIsReportOpen] = useState(false);
  const [isSettingsOpen, setIsSettingsOpen] = useState(false);

  // Compute metrics in real-time based on cells and calibration inputs
  const metrics: AlgSqaMedDataOut = computeCASAMetrics(cells, input, 800, 520);

  const handleRefreshSample = (mode: 'Human' | 'Porcine' | 'Quality Control Particle' = 'Human') => {
    let count = 60;
    if (mode === 'Porcine') {
      count = 85;
      setInput(prev => ({ ...prev, dShapeRatio: 1.5, freshSpermShapeMax: 2.4 }));
    } else if (mode === 'Quality Control Particle') {
      count = 45;
      setInput(prev => ({ ...prev, dShapeRatio: 0.5 }));
    } else {
      setInput(prev => ({ ...prev, dShapeRatio: 1.15, freshSpermShapeMax: 2.1 }));
    }
    const newCells = generateInitialSpermCells(count, 800, 520, input.dRatioImg, mode);
    setCells(newCells);
    setSelectedCellId(null);
  };

  return (
    <div className="min-h-screen bg-slate-950 text-slate-100 flex flex-col">
      {/* Top Navigation */}
      <Navbar
        activeTab={activeTab}
        setActiveTab={setActiveTab}
        metrics={metrics}
        onOpenReport={() => setIsReportOpen(true)}
        onOpenSettings={() => setIsSettingsOpen(true)}
      />

      {/* Main Content Area */}
      <main className="flex-1 max-w-7xl w-full mx-auto p-4 sm:p-6 lg:p-8">
        {activeTab === 'microscope' && (
          <MicroscopeStage
            cells={cells}
            setCells={setCells}
            input={input}
            metrics={metrics}
            onRefreshSample={handleRefreshSample}
            selectedCellId={selectedCellId}
            setSelectedCellId={setSelectedCellId}
          />
        )}

        {activeTab === 'dashboard' && (
          <CASAAnalysisDashboard metrics={metrics} input={input} />
        )}

        {activeTab === 'morphology' && (
          <MorphologyLab metrics={metrics} input={input} cells={cells} />
        )}

        {activeTab === 'watchdog' && (
          <NexusWatchdogConsole />
        )}

        {activeTab === 'docs' && (
          <AlgorithmSpecsView />
        )}
      </main>

      {/* Footer */}
      <footer className="bg-slate-900 border-t border-slate-800 py-3 text-center text-xs text-slate-500">
        <div className="max-w-7xl mx-auto px-4 flex flex-wrap items-center justify-between gap-2">
          <span>iSperm Medical Instrument Suite • CASA Algorithm & Embedded Nexus ADB Daemon</span>
          <span className="font-mono text-[11px]">Build v2.4.2 • WHO 5th/6th Criteria • RK3288 Android 5.1.1 Target</span>
        </div>
      </footer>

      {/* Modals */}
      <ClinicalReportModal
        isOpen={isReportOpen}
        onClose={() => setIsReportOpen(false)}
        metrics={metrics}
        input={input}
      />

      <SettingsModal
        isOpen={isSettingsOpen}
        onClose={() => setIsSettingsOpen(false)}
        input={input}
        setInput={setInput}
      />
    </div>
  );
}

export default App;
