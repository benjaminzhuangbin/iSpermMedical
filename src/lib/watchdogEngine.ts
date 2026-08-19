import { WatchdogStatus, WatchdogLogEntry, WatchdogDaemonStatus, SocketState } from '../types';

export class WatchdogEngine {
  private status: WatchdogStatus;
  private logs: WatchdogLogEntry[] = [];
  private listeners: Array<(status: WatchdogStatus, logs: WatchdogLogEntry[]) => void> = [];
  private timer: any = null;

  constructor() {
    this.status = {
      version: '2.4.2 (Product Stable)',
      daemonStatus: 'RUNNING',
      adbdRunning: true,
      port5555Listening: true,
      socketCount: 1,
      establishedClients: 1,
      closeWaitSockets: 0,
      timeWaitSockets: 0,
      rootOk: true,
      rootMethod: 'NEXUS_SU:/system/xbin/nexus_su',
      rootUid: 0,
      restartCount: 0,
      lastRestartReason: 'SYSTEM_BOOT',
      lastRestartTime: new Date(Date.now() - 3600000).toISOString().replace('T', ' ').substring(0, 19),
      cooldownRemainingSeconds: 0,
      uptimeSeconds: 3600,
      ipAddress: '192.168.31.11:5555',
      boardModel: 'Rockchip RK3288 (Medical Host)',
      androidVersion: 'Android 5.1.1 (API 22 Lollipop)'
    };

    this.addLog('INFO', 'Nexus ADB Watchdog 2.4 daemon initialized.');
    this.addLog('INFO', 'Root verified: NEXUS_SU (/system/xbin/nexus_su) UID=0');
    this.addLog('INFO', 'Listening on TCP 0.0.0.0:5555 for Ethernet ADB.');
    this.startLoop();
  }

  public subscribe(listener: (status: WatchdogStatus, logs: WatchdogLogEntry[]) => void) {
    this.listeners.push(listener);
    listener({ ...this.status }, [...this.logs]);
    return () => {
      this.listeners = this.listeners.filter(l => l !== listener);
    };
  }

  private notify() {
    this.listeners.forEach(l => l({ ...this.status }, [...this.logs]));
  }

  public addLog(level: 'INFO' | 'WARN' | 'ERROR' | 'RECOVERY' | 'DIAG', message: string) {
    const timestamp = new Date().toISOString().replace('T', ' ').substring(0, 19);
    this.logs.unshift({ timestamp, level, message });
    if (this.logs.length > 100) {
      this.logs.pop();
    }
    this.notify();
  }

  private startLoop() {
    this.timer = setInterval(() => {
      this.status.uptimeSeconds += 1;

      if (this.status.cooldownRemainingSeconds > 0) {
        this.status.cooldownRemainingSeconds -= 1;
        if (this.status.cooldownRemainingSeconds === 0) {
          this.status.daemonStatus = 'RUNNING';
          this.addLog('INFO', 'Cooldown window elapsed. Watchdog returning to active monitoring state.');
        }
      }

      // Check Watchdog 2.4 rules
      if (this.status.daemonStatus === 'RUNNING') {
        if (!this.status.adbdRunning) {
          this.triggerRecovery('CASE_A_ADBD_DEAD', 'ADBD daemon process terminated. Initiating setprop & ctl.start adbd.');
        } else if (!this.status.port5555Listening) {
          this.triggerRecovery('CASE_B_PORT_CLOSED', 'ADBD alive but TCP port 5555 not listening. Restarting adbd service.');
        }
      }

      this.notify();
    }, 1000);
  }

  private triggerRecovery(reason: string, details: string) {
    this.status.daemonStatus = 'RECOVERING';
    this.status.lastRestartReason = reason;
    this.status.lastRestartTime = new Date().toISOString().replace('T', ' ').substring(0, 19);
    this.status.restartCount += 1;

    this.addLog('RECOVERY', `[WATCHDOG RECOVERY] ${details}`);
    this.addLog('INFO', 'Executing: /system/xbin/nexus_su -c "setprop service.adb.tcp.port 5555"');

    setTimeout(() => {
      this.addLog('INFO', 'Executing: /system/xbin/nexus_su -c "setprop ctl.stop adbd"');
      setTimeout(() => {
        this.addLog('INFO', 'Executing: /system/xbin/nexus_su -c "setprop ctl.start adbd"');
        this.status.adbdRunning = true;
        this.status.port5555Listening = true;
        this.status.socketCount = 1;
        this.status.establishedClients = 1;
        this.status.closeWaitSockets = 0;
        this.status.daemonStatus = 'COOLDOWN';
        this.status.cooldownRemainingSeconds = 15;
        this.addLog('INFO', 'ADB service successfully recovered. Entering 15s rate-limit cooldown.');
        this.notify();
      }, 800);
    }, 600);
  }

  // Fault Injection Triggers
  public injectCaseA() {
    this.addLog('WARN', '[INJECT] Simulating Case A: Killing adbd daemon process (STOP_ADBD)...');
    this.status.adbdRunning = false;
    this.status.port5555Listening = false;
    this.status.establishedClients = 0;
    this.notify();
  }

  public injectCaseB() {
    this.addLog('WARN', '[INJECT] Simulating Case B: Port 5555 closed while adbd alive (BREAK_PORT)...');
    this.status.port5555Listening = false;
    this.notify();
  }

  public injectCaseC() {
    this.addLog('DIAG', '[INJECT] Simulating Case C: CLOSE_WAIT socket leak detected from remote client disconnect...');
    this.status.closeWaitSockets = 4;
    this.status.socketCount = 5;
    this.addLog('DIAG', 'Watchdog 2.4 Policy Check: ESTABLISHED=1 > 0 -> Diagnostics only, suppressing reboot to protect active session.');
    this.notify();
  }

  public injectCaseD() {
    this.addLog('INFO', '[INJECT] Simulating Case D: Remote ADB connection handshake (CNXN probe)...');
    this.status.establishedClients += 1;
    this.status.socketCount += 1;
    this.addLog('INFO', `New TCP client connected from 192.168.31.105. Active ESTABLISHED=${this.status.establishedClients}.`);
    this.notify();
  }

  public clearLogs() {
    this.logs = [];
    this.addLog('INFO', 'Watchdog log cleared.');
  }

  public getStatusText(): string {
    return `TIMESTAMP=${new Date().toISOString().replace('T', ' ').substring(0, 19)}
VERSION=${this.status.version}
DAEMON_STATUS=${this.status.daemonStatus}
ADBD_RUNNING=${this.status.adbdRunning ? 'YES' : 'NO'}
PORT5555_LISTENING=${this.status.port5555Listening ? 'YES' : 'NO'}
SOCKET_TOTAL=${this.status.socketCount}
CLIENTS_ESTABLISHED=${this.status.establishedClients}
SOCKETS_CLOSE_WAIT=${this.status.closeWaitSockets}
ROOT_OK=${this.status.rootOk ? '1' : '0'}
ROOT_METHOD=${this.status.rootMethod}
ROOT_UID=${this.status.rootUid}
RESTART_COUNT=${this.status.restartCount}
LAST_RESTART_REASON=${this.status.lastRestartReason}
LAST_RESTART_TIME=${this.status.lastRestartTime}
COOLDOWN_REMAINING=${this.status.cooldownRemainingSeconds}s
UPTIME=${Math.floor(this.status.uptimeSeconds / 60)}m ${this.status.uptimeSeconds % 60}s
DEVICE_IP=${this.status.ipAddress}
TARGET_BOARD=${this.status.boardModel}`;
  }
}

export const globalWatchdog = new WatchdogEngine();
