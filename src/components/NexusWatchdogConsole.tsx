import React, { useState, useEffect } from 'react';
import { 
  ShieldCheck, 
  Terminal, 
  Play, 
  AlertTriangle, 
  RefreshCw, 
  Server, 
  Wifi, 
  CheckCircle2, 
  XCircle, 
  Cpu, 
  Sliders,
  Copy,
  Check,
  Radio,
  FileText
} from 'lucide-react';
import { WatchdogStatus, WatchdogLogEntry } from '../types';
import { globalWatchdog } from '../lib/watchdogEngine';

export const NexusWatchdogConsole: React.FC = () => {
  const [status, setStatus] = useState<WatchdogStatus | null>(null);
  const [logs, setLogs] = useState<WatchdogLogEntry[]>([]);
  const [copied, setCopied] = useState(false);
  const [customCommand, setCustomCommand] = useState('adb shell "/system/xbin/nexus_su -c id"');
  const [commandOutput, setCommandOutput] = useState<string | null>(null);

  useEffect(() => {
    const unsubscribe = globalWatchdog.subscribe((newStatus, newLogs) => {
      setStatus(newStatus);
      setLogs(newLogs);
    });
    return unsubscribe;
  }, []);

  const handleCopyStatus = () => {
    if (!status) return;
    const text = globalWatchdog.getStatusText();
    navigator.clipboard.writeText(text);
    setCopied(true);
    setTimeout(() => setCopied(false), 2000);
  };

  const handleExecuteCommand = (e: React.FormEvent) => {
    e.preventDefault();
    if (customCommand.includes('id')) {
      setCommandOutput('uid=0(root) gid=0(root) context=u:r:nexus_su:s0');
    } else if (customCommand.includes('watchdog.status')) {
      setCommandOutput(globalWatchdog.getStatusText());
    } else if (customCommand.includes('ps') || customCommand.includes('adbd')) {
      setCommandOutput('root      1204  1     4520   1284  ffffffff 0001a4e0 S /sbin/adbd');
    } else if (customCommand.includes('netstat')) {
      setCommandOutput('tcp        0      0 0.0.0.0:5555            0.0.0.0:*               LISTEN\ntcp        0      0 192.168.31.11:5555      192.168.31.105:43892    ESTABLISHED');
    } else {
      setCommandOutput(`Execution complete (exit code 0). Command sent to /system/xbin/nexus_su.`);
    }
  };

  if (!status) return null;

  return (
    <div className="space-y-6">
      {/* Top Header Card */}
      <div className="bg-slate-900 border border-slate-800 rounded-2xl p-5 shadow-xl flex flex-wrap items-center justify-between gap-4">
        <div className="flex items-center gap-4">
          <div className="w-12 h-12 rounded-xl bg-gradient-to-tr from-emerald-600 to-teal-500 flex items-center justify-center shadow-lg shadow-emerald-500/20">
            <ShieldCheck className="w-7 h-7 text-white" />
          </div>
          <div>
            <div className="flex items-center gap-2">
              <h2 className="text-lg font-bold text-white">Nexus ADB Watchdog 2.4 & APK Service</h2>
              <span className="text-xs bg-emerald-950 text-emerald-400 border border-emerald-800/60 font-mono px-2 py-0.5 rounded-full font-medium">
                DAEMON {status.daemonStatus}
              </span>
            </div>
            <p className="text-xs text-slate-400">
              Native C Daemon + Android APK Service for Rockchip RK3288 / Android 5.1.1 (API 22) Medical Instrument
            </p>
          </div>
        </div>

        <div className="flex items-center gap-2">
          <button
            onClick={handleCopyStatus}
            className="flex items-center gap-1.5 bg-slate-800 hover:bg-slate-700 text-slate-200 px-3 py-1.5 rounded-lg text-xs font-medium transition-colors"
          >
            {copied ? <Check className="w-3.5 h-3.5 text-emerald-400" /> : <Copy className="w-3.5 h-3.5" />}
            {copied ? 'Copied status' : 'Copy watchdog.status'}
          </button>
        </div>
      </div>

      {/* Status Grid Cards */}
      <div className="grid grid-cols-2 sm:grid-cols-3 lg:grid-cols-6 gap-4">
        {/* ADBD Process */}
        <div className="bg-slate-900 border border-slate-800 rounded-2xl p-4 shadow-lg">
          <div className="text-xs text-slate-400 font-medium">ADBD Process</div>
          <div className="mt-1 flex items-center gap-2">
            {status.adbdRunning ? (
              <CheckCircle2 className="w-5 h-5 text-emerald-400" />
            ) : (
              <XCircle className="w-5 h-5 text-rose-400" />
            )}
            <span className="text-lg font-bold font-mono text-white">
              {status.adbdRunning ? 'ALIVE' : 'STOPPED'}
            </span>
          </div>
          <div className="mt-2 text-[11px] text-slate-500 font-mono">
            PID: {status.adbdRunning ? '1204 (root)' : 'None'}
          </div>
        </div>

        {/* TCP Port 5555 */}
        <div className="bg-slate-900 border border-slate-800 rounded-2xl p-4 shadow-lg">
          <div className="text-xs text-slate-400 font-medium">TCP Port 5555</div>
          <div className="mt-1 flex items-center gap-2">
            {status.port5555Listening ? (
              <Wifi className="w-5 h-5 text-emerald-400" />
            ) : (
              <AlertTriangle className="w-5 h-5 text-rose-400" />
            )}
            <span className="text-lg font-bold font-mono text-white">
              {status.port5555Listening ? 'LISTEN' : 'CLOSED'}
            </span>
          </div>
          <div className="mt-2 text-[11px] text-slate-500 font-mono">
            {status.ipAddress}
          </div>
        </div>

        {/* Real Root (nexus_su) */}
        <div className="bg-slate-900 border border-slate-800 rounded-2xl p-4 shadow-lg">
          <div className="text-xs text-slate-400 font-medium">Elevated Root</div>
          <div className="mt-1 flex items-center gap-2">
            <span className="text-lg font-bold font-mono text-emerald-400">UID=0</span>
            <span className="text-xs bg-emerald-950 text-emerald-400 px-1.5 py-0.5 rounded font-mono">OK</span>
          </div>
          <div className="mt-2 text-[11px] text-slate-500 truncate font-mono">
            {status.rootMethod.split(':')[1] || 'nexus_su'}
          </div>
        </div>

        {/* Active ADB Clients */}
        <div className="bg-slate-900 border border-slate-800 rounded-2xl p-4 shadow-lg">
          <div className="text-xs text-slate-400 font-medium">Active ADB Clients</div>
          <div className="mt-1 flex items-baseline gap-1">
            <span className="text-2xl font-bold font-mono text-cyan-400">{status.establishedClients}</span>
            <span className="text-xs text-slate-400">sessions</span>
          </div>
          <div className="mt-2 text-[11px] text-slate-500">
            CLOSE_WAIT: {status.closeWaitSockets}
          </div>
        </div>

        {/* Recoveries / Restarts */}
        <div className="bg-slate-900 border border-slate-800 rounded-2xl p-4 shadow-lg">
          <div className="text-xs text-slate-400 font-medium">Auto Recoveries</div>
          <div className="mt-1 flex items-baseline gap-1">
            <span className="text-2xl font-bold font-mono text-amber-400">{status.restartCount}</span>
            <span className="text-xs text-slate-400">events</span>
          </div>
          <div className="mt-2 text-[11px] text-slate-500 truncate">
            Last: {status.lastRestartReason}
          </div>
        </div>

        {/* Rate Limit Cooldown */}
        <div className="bg-slate-900 border border-slate-800 rounded-2xl p-4 shadow-lg">
          <div className="text-xs text-slate-400 font-medium">Cooldown Window</div>
          <div className="mt-1 flex items-baseline gap-1">
            <span className="text-2xl font-bold font-mono text-slate-100">{status.cooldownRemainingSeconds}</span>
            <span className="text-xs text-slate-400">sec</span>
          </div>
          <div className="mt-2 text-[11px] text-slate-500">
            Uptime: {Math.floor(status.uptimeSeconds / 60)}m
          </div>
        </div>
      </div>

      {/* Main Section: Fault Injection & Diagnostics */}
      <div className="grid grid-cols-1 lg:grid-cols-12 gap-6">
        {/* Fault Injection Panel & Test Cases (5 cols) */}
        <div className="lg:col-span-5 bg-slate-900 border border-slate-800 rounded-2xl p-5 shadow-xl space-y-4">
          <div className="pb-3 border-b border-slate-800">
            <h3 className="font-semibold text-slate-200 flex items-center gap-2">
              <Play className="w-4 h-4 text-emerald-400" />
              Watchdog 2.4 Fault Recovery Simulator
            </h3>
            <p className="text-xs text-slate-400 mt-1">
              Inject hardware/software faults to verify auto-healing state machine behavior without breaking active sessions.
            </p>
          </div>

          <div className="space-y-3">
            {/* Case A */}
            <div className="bg-slate-950 p-3.5 rounded-xl border border-slate-800 flex items-center justify-between">
              <div>
                <div className="font-semibold text-xs text-slate-200">Inject Case A: ADBD Process Killed</div>
                <div className="text-[11px] text-slate-500">Triggers setprop & ctl.start adbd recovery</div>
              </div>
              <button
                onClick={() => globalWatchdog.injectCaseA()}
                className="bg-rose-900/40 hover:bg-rose-800/60 text-rose-300 border border-rose-700/50 px-3 py-1.5 rounded-lg text-xs font-semibold transition-all"
              >
                Kill ADBD
              </button>
            </div>

            {/* Case B */}
            <div className="bg-slate-950 p-3.5 rounded-xl border border-slate-800 flex items-center justify-between">
              <div>
                <div className="font-semibold text-xs text-slate-200">Inject Case B: Port 5555 Closed</div>
                <div className="text-[11px] text-slate-500">Daemon alive, port dead &rarr; adbd restart cycle</div>
              </div>
              <button
                onClick={() => globalWatchdog.injectCaseB()}
                className="bg-amber-900/40 hover:bg-amber-800/60 text-amber-300 border border-amber-700/50 px-3 py-1.5 rounded-lg text-xs font-semibold transition-all"
              >
                Break Port
              </button>
            </div>

            {/* Case C */}
            <div className="bg-slate-950 p-3.5 rounded-xl border border-slate-800 flex items-center justify-between">
              <div>
                <div className="font-semibold text-xs text-slate-200">Inject Case C: CLOSE_WAIT Socket Leak</div>
                <div className="text-[11px] text-slate-500">ESTABLISHED &gt; 0 protects active hospital session</div>
              </div>
              <button
                onClick={() => globalWatchdog.injectCaseC()}
                className="bg-sky-900/40 hover:bg-sky-800/60 text-sky-300 border border-sky-700/50 px-3 py-1.5 rounded-lg text-xs font-semibold transition-all"
              >
                Leak Socket
              </button>
            </div>

            {/* Case D */}
            <div className="bg-slate-950 p-3.5 rounded-xl border border-slate-800 flex items-center justify-between">
              <div>
                <div className="font-semibold text-xs text-slate-200">Inject Case D: Client Handshake</div>
                <div className="text-[11px] text-slate-500">Connects new remote QtScrcpy / ADB client</div>
              </div>
              <button
                onClick={() => globalWatchdog.injectCaseD()}
                className="bg-emerald-900/40 hover:bg-emerald-800/60 text-emerald-300 border border-emerald-700/50 px-3 py-1.5 rounded-lg text-xs font-semibold transition-all"
              >
                Connect Client
              </button>
            </div>
          </div>

          {/* Watchdog Hard Rules Card */}
          <div className="p-3 bg-slate-950 border border-slate-800 rounded-xl text-[11px] space-y-1.5 text-slate-400">
            <div className="font-semibold text-slate-200">Product Hard Rules (Native 2.4 &amp; APK):</div>
            <div>✓ Fault recovery only — never periodic adbd restart</div>
            <div>✓ ESTABLISHED &gt; 0 &rarr; never restart/stop adbd (active session safe)</div>
            <div>✓ ESTABLISHED = 0 &rarr; normal idle, not a fault</div>
            <div>✓ Setuid helper <code>/system/xbin/nexus_su</code> guarantees root UID 0 for APK</div>
          </div>
        </div>

        {/* Live Terminal & Log Stream (7 cols) */}
        <div className="lg:col-span-7 bg-slate-900 border border-slate-800 rounded-2xl p-5 shadow-xl flex flex-col justify-between">
          <div>
            <div className="flex items-center justify-between pb-3 border-b border-slate-800">
              <h3 className="font-semibold text-slate-200 flex items-center gap-2">
                <Terminal className="w-4 h-4 text-emerald-400" />
                Live Log Stream (<code>/sdcard/NexusADBWatchdog/watchdog.log</code>)
              </h3>
              <button
                onClick={() => globalWatchdog.clearLogs()}
                className="text-xs text-slate-400 hover:text-slate-200"
              >
                Clear Log
              </button>
            </div>

            {/* Terminal Window */}
            <div className="bg-slate-950 rounded-xl p-3 my-3 font-mono text-xs text-slate-300 h-64 overflow-y-auto space-y-1 border border-slate-800/80">
              {logs.map((log, index) => (
                <div key={index} className="flex items-start gap-2 leading-relaxed">
                  <span className="text-slate-600 select-none text-[10px]">{log.timestamp.split(' ')[1]}</span>
                  <span className={`text-[10px] px-1 rounded font-bold ${
                    log.level === 'RECOVERY' ? 'bg-rose-950 text-rose-400 border border-rose-800/60' :
                    log.level === 'WARN' ? 'bg-amber-950 text-amber-400' :
                    log.level === 'DIAG' ? 'bg-purple-950 text-purple-400' :
                    'bg-slate-800 text-slate-400'
                  }`}>
                    {log.level}
                  </span>
                  <span className={log.level === 'RECOVERY' ? 'text-rose-200 font-semibold' : 'text-slate-300'}>
                    {log.message}
                  </span>
                </div>
              ))}
            </div>
          </div>

          {/* ADB Shell Interactive Command Line */}
          <form onSubmit={handleExecuteCommand} className="space-y-2 border-t border-slate-800 pt-3">
            <div className="flex items-center gap-2">
              <span className="text-xs font-mono text-emerald-400 font-bold">$</span>
              <input
                type="text"
                value={customCommand}
                onChange={(e) => setCustomCommand(e.target.value)}
                placeholder="Enter ADB command (e.g. adb shell nexus_su -c id)"
                className="flex-1 bg-slate-950 border border-slate-800 rounded-lg px-3 py-1.5 text-xs font-mono text-cyan-300 focus:outline-none focus:border-cyan-500"
              />
              <button
                type="submit"
                className="bg-emerald-600 hover:bg-emerald-500 text-white text-xs font-semibold px-3 py-1.5 rounded-lg transition-colors"
              >
                Run
              </button>
            </div>
            {commandOutput && (
              <div className="bg-slate-950 p-2 rounded-lg border border-slate-800 text-[11px] font-mono text-slate-300 whitespace-pre-wrap">
                {commandOutput}
              </div>
            )}
          </form>
        </div>
      </div>
    </div>
  );
};
