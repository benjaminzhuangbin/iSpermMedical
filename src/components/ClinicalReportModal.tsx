import React, { useState } from 'react';
import { X, Printer, Download, CheckCircle2, AlertTriangle, FileSpreadsheet, Building2, User } from 'lucide-react';
import { AlgSqaMedDataOut, AlgSqaMedDataIn, PatientInfo } from '../types';

interface ClinicalReportModalProps {
  isOpen: boolean;
  onClose: () => void;
  metrics: AlgSqaMedDataOut;
  input: AlgSqaMedDataIn;
}

export const ClinicalReportModal: React.FC<ClinicalReportModalProps> = ({
  isOpen,
  onClose,
  metrics,
  input
}) => {
  const [patient, setPatient] = useState<PatientInfo>({
    patientId: 'PT-2026-08819',
    patientName: 'Chen, Wei',
    age: 32,
    abstinenceDays: 3,
    collectionTime: new Date(Date.now() - 3600000).toISOString().replace('T', ' ').substring(0, 16),
    analysisTime: new Date().toISOString().replace('T', ' ').substring(0, 16),
    sampleType: 'Human',
    referringPhysician: 'Dr. Sarah Lin, MD (Reproductive Endocrinology)',
    operatorName: 'Lab Tech #419 (CASA Certified)',
    notes: 'Sample liquefied within 30 minutes at 37°C. Viscosity normal, pH 7.6.'
  });

  if (!isOpen) return null;

  const handlePrint = () => {
    window.print();
  };

  const handleExportJSON = () => {
    const dataStr = "data:text/json;charset=utf-8," + encodeURIComponent(JSON.stringify({ patient, input, metrics }, null, 2));
    const downloadAnchor = document.createElement('a');
    downloadAnchor.setAttribute("href", dataStr);
    downloadAnchor.setAttribute("download", `CASA_Report_${patient.patientId}.json`);
    document.body.appendChild(downloadAnchor);
    downloadAnchor.click();
    downloadAnchor.remove();
  };

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/80 backdrop-blur-sm p-4 overflow-y-auto">
      <div className="bg-slate-900 border border-slate-700 rounded-2xl w-full max-w-4xl shadow-2xl max-h-[90vh] flex flex-col overflow-hidden my-auto">
        {/* Header */}
        <div className="flex items-center justify-between px-6 py-4 border-b border-slate-800 bg-slate-950">
          <div className="flex items-center gap-2">
            <FileSpreadsheet className="w-5 h-5 text-cyan-400" />
            <h2 className="text-base font-bold text-white">WHO 6th Edition Standard Semen Analysis Report</h2>
          </div>
          <div className="flex items-center gap-2">
            <button
              onClick={handlePrint}
              className="flex items-center gap-1.5 bg-slate-800 hover:bg-slate-700 text-slate-200 px-3 py-1.5 rounded-lg text-xs font-semibold transition-colors"
            >
              <Printer className="w-3.5 h-3.5" />
              Print Report
            </button>
            <button
              onClick={handleExportJSON}
              className="flex items-center gap-1.5 bg-cyan-600 hover:bg-cyan-500 text-white px-3 py-1.5 rounded-lg text-xs font-semibold transition-colors"
            >
              <Download className="w-3.5 h-3.5" />
              Export JSON
            </button>
            <button
              onClick={onClose}
              className="p-1.5 text-slate-400 hover:text-white rounded-lg hover:bg-slate-800 transition-colors"
            >
              <X className="w-5 h-5" />
            </button>
          </div>
        </div>

        {/* Printable Report Canvas Area */}
        <div className="p-6 overflow-y-auto space-y-6 bg-slate-900 text-slate-100 font-sans text-xs">
          {/* Clinic Header */}
          <div className="flex justify-between items-start border-b border-slate-800 pb-4">
            <div>
              <div className="text-lg font-bold text-cyan-400">iSperm Medical CASA Diagnostic Laboratory</div>
              <div className="text-slate-400 text-[11px]">Computer-Assisted Semen Analysis System v3.1</div>
              <div className="text-slate-500 text-[10px]">Certified ISO 15189 / WHO 5th & 6th Edition Compliance</div>
            </div>
            <div className="text-right text-[11px] text-slate-400">
              <div>Report ID: <strong className="text-slate-200 font-mono">{patient.patientId}-R1</strong></div>
              <div>Date: {patient.analysisTime}</div>
              <div>Instrument: iSperm RK3288-04</div>
            </div>
          </div>

          {/* Patient Details */}
          <div className="grid grid-cols-2 sm:grid-cols-4 gap-3 bg-slate-950 p-4 rounded-xl border border-slate-800">
            <div>
              <span className="text-slate-500 block text-[10px]">Patient Name</span>
              <strong className="text-slate-200 text-sm">{patient.patientName}</strong>
            </div>
            <div>
              <span className="text-slate-500 block text-[10px]">Patient ID</span>
              <span className="font-mono text-cyan-400">{patient.patientId}</span>
            </div>
            <div>
              <span className="text-slate-500 block text-[10px]">Age / Abstinence</span>
              <span className="text-slate-200">{patient.age} yrs / {patient.abstinenceDays} days</span>
            </div>
            <div>
              <span className="text-slate-500 block text-[10px]">Ejaculate Volume</span>
              <span className="text-slate-200 font-bold">{input.dVolume} mL</span>
            </div>
          </div>

          {/* Primary CASA Findings Table */}
          <div>
            <h3 className="font-bold text-sm text-cyan-400 mb-2">1. Macroscopic & Primary CASA Parameters</h3>
            <table className="w-full text-left border border-slate-800 rounded-lg overflow-hidden">
              <thead className="bg-slate-950 text-slate-400 text-[11px]">
                <tr>
                  <th className="p-2 border-b border-slate-800">Parameter</th>
                  <th className="p-2 border-b border-slate-800">Result</th>
                  <th className="p-2 border-b border-slate-800">WHO 6th Lower Reference Limit</th>
                  <th className="p-2 border-b border-slate-800">Status</th>
                </tr>
              </thead>
              <tbody className="divide-y divide-slate-800 font-mono">
                <tr>
                  <td className="p-2 text-slate-300 font-sans">Total Sperm Concentration</td>
                  <td className="p-2 font-bold text-cyan-300">{metrics.dTotaSpermDensity} M/mL</td>
                  <td className="p-2 text-slate-400">≥ 16.0 M/mL</td>
                  <td className="p-2">
                    <span className={`text-xs px-2 py-0.5 rounded font-sans font-semibold ${
                      metrics.dTotaSpermDensity >= 16 ? 'bg-emerald-950 text-emerald-400' : 'bg-amber-950 text-amber-400'
                    }`}>
                      {metrics.dTotaSpermDensity >= 16 ? 'Normal' : 'Oligozoospermia'}
                    </span>
                  </td>
                </tr>
                <tr>
                  <td className="p-2 text-slate-300 font-sans">Total Sperm Count / Ejaculate</td>
                  <td className="p-2 font-bold text-slate-200">{(metrics.dTotaSpermDensity * input.dVolume).toFixed(1)} M</td>
                  <td className="p-2 text-slate-400">≥ 39.0 M</td>
                  <td className="p-2">
                    <span className="text-emerald-400 font-sans text-xs">Pass</span>
                  </td>
                </tr>
                <tr>
                  <td className="p-2 text-slate-300 font-sans">Total Motility (PR + NP)</td>
                  <td className="p-2 font-bold text-emerald-400">{metrics.dActiveSpermRatio}%</td>
                  <td className="p-2 text-slate-400">≥ 42%</td>
                  <td className="p-2">
                    <span className={`text-xs px-2 py-0.5 rounded font-sans font-semibold ${
                      metrics.dActiveSpermRatio >= 42 ? 'bg-emerald-950 text-emerald-400' : 'bg-amber-950 text-amber-400'
                    }`}>
                      {metrics.dActiveSpermRatio >= 42 ? 'Normal' : 'Asthenozoospermia'}
                    </span>
                  </td>
                </tr>
                <tr>
                  <td className="p-2 text-slate-300 font-sans">Progressive Motility (PR = Class A + B)</td>
                  <td className="p-2 font-bold text-sky-400">{metrics.dRatioClassPR}%</td>
                  <td className="p-2 text-slate-400">≥ 30%</td>
                  <td className="p-2">
                    <span className={`text-xs px-2 py-0.5 rounded font-sans font-semibold ${
                      metrics.dRatioClassPR >= 30 ? 'bg-emerald-950 text-emerald-400' : 'bg-amber-950 text-amber-400'
                    }`}>
                      {metrics.dRatioClassPR >= 30 ? 'Normal' : 'Low PR'}
                    </span>
                  </td>
                </tr>
                <tr>
                  <td className="p-2 text-slate-300 font-sans">Normal Sperm Morphology</td>
                  <td className="p-2 font-bold text-purple-400">{metrics.dNormal}%</td>
                  <td className="p-2 text-slate-400">≥ 4.0% (Strict Criteria)</td>
                  <td className="p-2">
                    <span className={`text-xs px-2 py-0.5 rounded font-sans font-semibold ${
                      metrics.dNormal >= 4.0 ? 'bg-emerald-950 text-emerald-400' : 'bg-amber-950 text-amber-400'
                    }`}>
                      {metrics.dNormal >= 4.0 ? 'Normal' : 'Teratozoospermia'}
                    </span>
                  </td>
                </tr>
              </tbody>
            </table>
          </div>

          {/* Kinematics Section */}
          <div>
            <h3 className="font-bold text-sm text-cyan-400 mb-2">2. Advanced Kinematic Velocity Profiles</h3>
            <div className="grid grid-cols-3 sm:grid-cols-6 gap-2 text-center font-mono">
              <div className="bg-slate-950 p-2.5 rounded-lg border border-slate-800">
                <span className="text-[10px] text-slate-500 block font-sans">VCL</span>
                <span className="font-bold text-cyan-300">{metrics.dAveVCL} µm/s</span>
              </div>
              <div className="bg-slate-950 p-2.5 rounded-lg border border-slate-800">
                <span className="text-[10px] text-slate-500 block font-sans">VSL</span>
                <span className="font-bold text-emerald-300">{metrics.dAveVSL} µm/s</span>
              </div>
              <div className="bg-slate-950 p-2.5 rounded-lg border border-slate-800">
                <span className="text-[10px] text-slate-500 block font-sans">VAP</span>
                <span className="font-bold text-purple-300">{metrics.dAveVAP} µm/s</span>
              </div>
              <div className="bg-slate-950 p-2.5 rounded-lg border border-slate-800">
                <span className="text-[10px] text-slate-500 block font-sans">LIN</span>
                <span className="font-bold text-slate-200">{metrics.dLIN}</span>
              </div>
              <div className="bg-slate-950 p-2.5 rounded-lg border border-slate-800">
                <span className="text-[10px] text-slate-500 block font-sans">STR</span>
                <span className="font-bold text-slate-200">{metrics.dSTR}</span>
              </div>
              <div className="bg-slate-950 p-2.5 rounded-lg border border-slate-800">
                <span className="text-[10px] text-slate-500 block font-sans">ALH</span>
                <span className="font-bold text-slate-200">{metrics.dALH} µm</span>
              </div>
            </div>
          </div>

          {/* Morphology Breakdown */}
          <div>
            <h3 className="font-bold text-sm text-cyan-400 mb-2">3. WHO Strict Morphology & Teratozoospermia Analysis</h3>
            <div className="grid grid-cols-2 sm:grid-cols-4 gap-3 bg-slate-950 p-3 rounded-xl border border-slate-800 text-slate-300 font-mono">
              <div>Head Defects: <strong className="text-amber-400">{metrics.dHdefects}%</strong></div>
              <div>Midpiece Defects: <strong className="text-pink-400">{metrics.dMdefects}%</strong></div>
              <div>Tail Defects: <strong className="text-purple-400">{metrics.dTdefects}%</strong></div>
              <div>TZI Index: <strong className="text-emerald-400">{metrics.dTZI}</strong> (Ref: &lt;1.60)</div>
            </div>
          </div>

          {/* Signature & Sign-off Block */}
          <div className="grid grid-cols-2 gap-6 pt-6 border-t border-slate-800 text-slate-400">
            <div>
              <div className="text-[10px]">Medical Laboratory Operator:</div>
              <div className="font-semibold text-slate-200 mt-1">{patient.operatorName}</div>
            </div>
            <div className="text-right">
              <div className="text-[10px]">Attending Physician / Director Sign-off:</div>
              <div className="font-semibold text-slate-200 mt-1">{patient.referringPhysician}</div>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
};
